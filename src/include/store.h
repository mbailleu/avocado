#pragma once

#include <stddef.h>
#include <stdint.h>
#include <memory.h>
#include <cstring>
#include <memory>
#include <vector>
#include <forward_list>
#include <utility>
#include <functional>
#include <tuple>
#include <shared_mutex>

#include "list.h"

#include "crypt/encrypt_openssl.h"
#include "utils/enumerate.h"

#if defined(PUSH_FRONT)
static constexpr bool Put_front = true;
#else
static constexpr bool Put_front = false;
#endif

struct BitKey {
  static bool equal(uint8_t const * const lhs, uint8_t const * const rhs, size_t const size) noexcept {
    return !memcmp(lhs, rhs, size);
  }

  static constexpr size_t adjust_size(size_t const size) noexcept {
    return size;
  }
  
  static constexpr uint8_t const * adjust_key_buf(uint8_t const * const key) noexcept {
    return key;
  }

};

struct MAC {
  static constexpr size_t Size = CipherSsl::MacSize;
  uint8_t mac[Size];

  MAC(uint8_t const * m) {
    memcpy(mac, m, Size);
  }

  MAC(uint8_t const (&m)[Size]) {
    memcpy(mac, m , Size);
  }

  MAC() : mac{} {}
  MAC(MAC const & other) {
    memcpy(mac, other.mac, Size);
  }

  operator uint8_t* () {
    return mac;
  }

  operator uint8_t const * () const {
    return mac;
  }
};

void mac_push_back([[maybe_unused]] std::vector<MAC> & macs, [[maybe_unused]] uint8_t const * mac) {
  if constexpr (Put_front) {
    macs.emplace_back(mac);
  }
}

template<class KEY_ACCESS = BitKey>
class Avocado {
 
    //TODO WTF is this, does this even do a useful hash?
    template<uint64_t INIT, uint64_t MULT>
    static uint64_t hash_(size_t const size, uint8_t const * const buf) noexcept {
        uint64_t hashval = INIT;
        for (auto i = 0U; i < size; ++i) {
            hashval = hashval * MULT + buf[i];
        }
        return hashval;
    }

public:
    constexpr static size_t Mac_size = CipherSsl::MacSize;

    struct Entry {
        uint32_t const key_size;
        uint32_t const val_size;
        uint8_t const key_hash;
        std::unique_ptr<uint8_t[]> key_val; //TODO maybe this should be a vector
        MAC mac;

        // Takes ownership of key_val
        Entry(CipherSsl & cipher,
                uint32_t const key_size,
                uint32_t const val_size,
                uint8_t const * plain_key_val) noexcept
                : key_size(key_size)
                , val_size(val_size)
                , key_hash(hash(key_size, plain_key_val))
                , key_val(std::make_unique<uint8_t[]>(cipher.adjust_size(key_size + val_size))) {
            cipher.encrypt(key_val.get(), plain_key_val, cipher.adjust_size(key_size + val_size), mac);
        }

        static uint8_t hash(size_t const key_size, uint8_t const * const key) noexcept {
            return hash_<7, 11>(KEY_ACCESS::adjust_size(key_size), KEY_ACCESS::adjust_key_buf(key)) & 0xFF; //TODO MAGIC NUMBERS
        }

        std::unique_ptr<uint8_t[]> compare(CipherSsl const & cipher, uint8_t const * const key) const noexcept {
            auto const size = cipher.adjust_size(key_size);
            auto res = std::make_unique<uint8_t[]>(size);
            uint8_t mac[Mac_size];
            cipher.decrypt(res.get(), key_val.get(), size, mac);
            if (KEY_ACCESS::equal(res.get(), key, key_size))
                return get(cipher); //This could be improved...
            return nullptr;
        }

        bool get(CipherSsl const & cipher, size_t buf_size, uint8_t * buf) const noexcept {
          auto const size = cipher.adjust_size(key_size + val_size);
          return cipher.decrypt(buf, key_val.get(), std::min(buf_size, size), mac);
        }

        std::unique_ptr<uint8_t[]> get(CipherSsl const & cipher) const noexcept {
            auto const size = get_size(cipher);
            auto res = std::make_unique<uint8_t[]>(size);
            if (get(cipher, size, res.get())) {
                return res;
            }
            return nullptr;
        }

       constexpr size_t get_size(CipherSsl const & cipher) const noexcept {
         return cipher.adjust_size(key_size + val_size);
       }

    };

    template<class Allocator>
    class EHashtable {
        struct Bucket {
           using List = avocado::List<Entry, Allocator>;
           using Hash = uint8_t[Mac_size];

           Bucket() = default;
           Bucket(Allocator const & alloc) : list(alloc) {}
           Bucket(Bucket const &) = delete;
           Bucket(Bucket &&) = default;

           List list;
           Hash hash = {0};
           std::unique_ptr<std::shared_mutex> mutex_ = std::make_unique<std::shared_mutex>();
        };

        std::vector<Bucket> table;
        CipherSsl           cipher;

        uint64_t hash(size_t const key_size, uint8_t const * const key) const noexcept {
            assert(table.size() > 0);
            return hash_<7, 61>(KEY_ACCESS::adjust_size(key_size), KEY_ACCESS::adjust_key_buf(key)) % table.size();
        }

    public:

        CipherSsl const & get_cipher() const noexcept {
          return cipher;
        }

        struct Ret_value {
          typename Bucket::List::const_iterator idx;
          size_t size;
          std::unique_ptr<uint8_t[]> kv;

          Ret_value() = default;
          Ret_value(Ret_value &&) = default;
          Ret_value & operator=(Ret_value &&) = default;
        };

        EHashtable(CipherSsl && cipher, size_t const size = 10, Allocator const & alloc = Allocator()) : cipher(cipher) {
          table.reserve(size);
          for (auto i = 0U; i < size; ++i) {
            table.emplace_back(alloc);
          }
        }

        template<class ITER, class ITER2 = ITER>
        struct SafeIter {
          using iterator_category = typename ITER::iterator_category;
          using value_type        = typename ITER::value_type;
          using difference_type   = typename ITER::difference_type;
          using pointer           = typename ITER::pointer;
          using reference         = typename ITER::reference;

          ITER iter;
          ITER2 end;
          HashSsl _hash;
          uint8_t const * mac;
          bool & ok;
          bool reset_{false};

          SafeIter(ITER && iter, ITER2 && end, HashSsl && _hash, uint8_t const * mac, bool & ok)
            : iter(std::move(iter))
            , end(std::move(end))
            , _hash(std::move(_hash))
            , mac(mac)
            , ok(ok) {
            ok = false;
          }

          SafeIter(SafeIter && other) : iter(std::move(other.iter)), end(std::move(other.end)), _hash(std::move(other._hash)), mac(other.mac), ok(other.ok), reset_(other.reset_) {
            other.reset_ = true;
          }

          ~SafeIter() {
            if (reset_) return;
            for(; iter != end; ++*this);
            ok = _hash.equal(mac); 
          }

          SafeIter & operator++() noexcept(noexcept(std::declval<ITER>().operator++())) {
            _hash.update(iter->mac, 1);
            iter.operator++();
            return *this;
          }

          SafeIter & operator++(int) noexcept(noexcept(std::declval<ITER>().operator++(0))) {
            auto res = *this;
            ++*this;
            return res;
          }

          void reset() { reset_ = true; }

          reference operator*() const noexcept(noexcept(std::declval<ITER>().operator*())) {
            return iter.operator*();
          }

          pointer operator->() const noexcept(noexcept(std::declval<ITER>().operator->())) {
            return iter.operator->();
          }

          decltype(auto) get_ptr() {
            return iter.get_ptr();
          }

          template<class T>
          decltype(auto) operator== (T const & other) const noexcept(noexcept(std::declval<ITER>().operator==(std::declval<T>()))) {
            return iter == other;
          }

          template<class T>
          decltype(auto) operator!=(T const & other) const noexcept(noexcept(std::declval<ITER>().operator!=(std::declval<T>()))) {
            return iter != other;
          }
        };


    private:

        Ret_value get(bool & ok
                     , size_t const key_size
                     , uint8_t const * const key
                     , typename Bucket::List const & bucket
                     , typename Bucket::Hash const & bucket_hash)
                     const noexcept {
          auto const key_hash = Entry::hash(key_size, key);

          for (auto [entry, last] = std::tuple{
                                        SafeIter(bucket.cbegin() //I claim this is legal in C++17 else ...
                                                , bucket.cend()
                                                , HashSsl(cipher)
                                                , bucket_hash
                                                , ok)
                                        , bucket.cbefore_begin()};
              entry != bucket.cend();
              ++entry, ++last) {
            if ((*entry).key_val != nullptr
               && KEY_ACCESS::adjust_size((*entry).key_size) == KEY_ACCESS::adjust_size(key_size)
               && (*entry).key_hash == key_hash) {
               if (auto plain_key = (*entry).compare(cipher, key); plain_key != nullptr) {
                 return {last, (*entry).key_size + (*entry).val_size, std::move(plain_key)};
               }
            }
          }
          return {bucket.cend(), 0, nullptr};
        }

        auto find(size_t const key_size, uint8_t const * key, typename Bucket::List const & bucket, [[maybe_unused]] std::vector<MAC> & macs, HashSsl & hash_) -> std::tuple<bool, decltype(bucket.cend()), Ret_value> {
          auto const key_hash = Entry::hash(key_size, key);
          auto last = bucket.cbefore_begin();
          for (auto entry = bucket.cbegin(); entry != bucket.cend(); ++entry, ++last) {
            mac_push_back(macs, entry->mac);
            if ((*entry).key_val != nullptr
                && KEY_ACCESS::adjust_size((*entry).key_size) == KEY_ACCESS::adjust_size(key_size)
                && (*entry).key_hash == key_hash) {
              if (auto plain_key = (*entry).compare(cipher, key); plain_key != nullptr) {
                return {true, entry, Ret_value{last, (*entry).key_size + (*entry).val_size, std::move(plain_key)}};
              }
            }
            hash_.update(entry->mac, 1);
          }

          return {false, bucket.cend(), Ret_value{last, 0, nullptr}};
        }

        void put(bool & ok
                , size_t const key_size
                , size_t const value_size
                , uint8_t const * key_value
                , typename Bucket::List & bucket
                , typename Bucket::Hash & bucket_hash
                , std::function<bool(Ret_value const & e)> where)
                noexcept {
//TODO We do not need the mac in most cases
          HashSsl hash_(cipher);
          [[maybe_unused]] std::vector<MAC> macs;
          auto [updated, entry, ret_value] = find(key_size, key_value, bucket, macs, hash_);
          if (!where(ret_value)) {
            ok = false;
            return;
          }
          if (entry == bucket.cend()) {
            if constexpr (Put_front) {
              hash_.finalize();
              if (!hash_.equal(bucket_hash)) {
                ok = false;
                return;
              }
              HashSsl new_hash(cipher);
              Entry new_entry(cipher, key_size, value_size, key_value);
              bucket.emplace_front(new_entry);
              hash_.update(new_entry.mac);
              new_hash.update(macs.data(), macs.size());
              new_hash.finalize();
              memcpy(bucket_hash, new_hash.mac, Mac_size);
              ok = true;
              return;
            }

            HashSsl new_hash = hash_;
            hash_.finalize();
            if (!hash_.equal(bucket_hash)) {
              ok = false;
              return;
            }
            Entry new_entry(cipher, key_size, value_size, key_value);
            new_hash.update(&(new_entry.mac), 1);
            bucket.insert_after(ret_value.idx, std::move(new_entry));
            new_hash.finalize();
            memcpy(bucket_hash, new_hash.mac, 1);
            ok = true;
            return;
          }

          auto new_hash = hash_;
          Entry new_entry(cipher, key_size, value_size, key_value);
          new_hash.update(&(new_entry.mac), 1);
          hash_.update(&(entry->mac), 1);
          bucket.erase_after(ret_value.idx);
          entry = bucket.insert_after(ret_value.idx, std::move(new_entry));
          ++entry;
          uint8_t tmp[Mac_size];
          for (; entry != bucket.cend(); ++entry) {
            memcpy(tmp, entry->mac, Mac_size);
            hash_.update(tmp, Mac_size);
            new_hash.update(tmp, Mac_size);
          }

          hash_.finalize();
          new_hash.finalize();
          ok = hash_.equal(bucket_hash);
          assert(ok);
          memcpy(bucket_hash, new_hash.mac, Mac_size);
          return;
        }

    public:
        Ret_value get(size_t const key_size, uint8_t const * const key, [[maybe_unused]] std::function<bool(Ret_value const &)> where = [] (Ret_value const &) { return true; }) const noexcept {
          bool ok{false};
          auto & [bucket, bucket_hash, m] = table[hash(key_size, key)];
          
          std::shared_lock lock(*m);
          auto res = get(ok, key_size, key, bucket, bucket_hash);
          
          if (ok) return res;
          return {bucket.cend(), 0, nullptr};
        }

        using Get_ret = Ret_value;

        bool put(size_t key_size, size_t value_size, uint8_t const * key_value, std::function<bool(Ret_value const &)> where = [] (Ret_value const &) { return true;} ) noexcept {
          bool ok{false};
          auto & [bucket, bucket_hash, m] = table[hash(key_size, key_value)];
          {
            std::unique_lock lock(*m);
            put(ok, key_size, value_size, key_value, bucket, bucket_hash, where);
          }
          return ok;
        }
        
    };

#if 0
    template<class Allocator>
    class Hashtable {
        using BucketList = avocado::List<Entry, Allocator>;
        using BucketHash = uint8_t[Mac_size];

        struct Bucket {
          BucketList data;
          BucketHash hash = {0};
          Bucket(Allocator const & alloc) : data(alloc) {}
        };

        std::vector<Bucket> table;
        CipherSsl           cipher;

        //TODO we ignore the possible division by 0 bug if table size is 0 (We might have to fix this)
        uint64_t hash(size_t const key_size, uint8_t const * const key) const noexcept {
            assert(table.size() > 0);
            return hash_<7, 61>(KEY_ACCESS::adjust_size(key_size), KEY_ACCESS::adjust_key_buf(key)) % table.size();
        }

    public:

        CipherSsl const & get_cipher() const noexcept {
          return cipher;
        }

        struct Get_ret {
            typename BucketList::const_iterator idx;
            Entry const * entry;
            std::unique_ptr<uint8_t[]> key;

            Get_ret() = default;
            Get_ret(Get_ret &&) = default;
            Get_ret & operator=(Get_ret && other) {
              idx = std::move(other.idx);
              entry = other.entry;
              key = std::move(other.key);
              return *this;
            }
        };
        
        Hashtable(CipherSsl && cipher, size_t const size = 10, Allocator const & alloc = Allocator()) : cipher(cipher) {
            table.reserve(size);
            for (auto i = 0U; i < size; ++i) {
                table.emplace_back(alloc);
            }
        }

        Get_ret get(size_t const key_size, uint8_t const * const key) const noexcept {
            auto const bin = hash(key_size, key);
            auto const key_hash = Entry::hash(key_size, key);

            auto & [bucket, bucket_hash] = table[bin];
            for (auto entry = bucket.cbegin(), last = bucket.cbefore_begin(); entry != bucket.cend(); entry++, last++) {
                if ((*entry).key_val != nullptr
                    && KEY_ACCESS::adjust_size((*entry).key_size) == KEY_ACCESS::adjust_size(key_size)
                    && (*entry).key_hash == key_hash) {
                    if (auto plain_key = (*entry).compare(cipher, key); plain_key != nullptr) {
                        return {last, std::addressof(*entry), std::move(plain_key)};
                    }
                }
            }
            return {bucket.cend(), nullptr, nullptr};
        }

        std::unique_ptr<uint8_t[]> key_value(Get_ret & ret) const {
            if (ret.entry != nullptr) {
                return ret.entry->get(cipher);
            }
            return nullptr;
        }

        bool put_if(size_t const key_size, size_t const val_size, uint8_t const * key_val, std::function<bool(Get_ret const & key)> const & test) {
          auto const bin = hash(key_size, key_val);
          auto const key_hash = Entry::hash(key_size, key_val);

          auto & bucket = table[bin].data;
          for (auto entry = bucket.cbegin(), last = bucket.cbefore_begin(); entry != bucket.cend(); entry++, last++) {
            if ((*entry).key_val != nullptr
                && KEY_ACCESS::adjust_size((*entry).key_size) == KEY_ACCESS::adjust_size(key_size)
                && (*entry).key_hash == key_hash) {
                if (auto plain_key = (*entry).compare(cipher, key_val); plain_key != nullptr) {
                    Get_ret found{last, std::addressof(*entry), std::move(plain_key)};
                    if (test(found)) {
                      bucket.erase_after(last);
                      bucket.emplace_after(last, cipher, key_size, val_size, key_val);
                      return true;
                    }
                    return false;
                }
            }
          }
          return false;
        }

        void put(size_t const key_size, size_t const val_size, uint8_t const * key_val) {
            auto const bin = hash(key_size, key_val);
            auto & bucket = table[bin].data;
            bucket.emplace_front(cipher, key_size, val_size, key_val); 
        }

        void put(typename BucketList::const_iterator idx, size_t const key_size, size_t const val_size, uint8_t const * key_val) {
            auto const bin = hash(key_size, key_val);
            auto & bucket = table[bin].data;
            bucket.erase_after(idx);
            bucket.emplace_after(idx, cipher, key_size, val_size, key_val);
        }
    };
#endif
};

