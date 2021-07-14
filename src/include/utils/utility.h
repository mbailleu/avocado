#pragma once

#include <cstddef>
#include <optional>
#include <cassert>

namespace avocado {

inline bool compare_exchange_128(std::byte * dst, std::byte * src, std::byte * old) {
  assert((reinterpret_cast<uintptr_t>(dst) & 0xF) == 0); //Check that alignment is correct
  assert(dst != old); //Must be distinct to get expected result //In theory nothing is wrong with both having the same value but it might not be what you expect since the registers having suddenly a different value from the memory, while both represent the same memory
  struct Val {
    uint64_t low;
    uint64_t high;
  };
  auto old_val = reinterpret_cast<Val *>(old);
  auto src_val = reinterpret_cast<Val *>(src);
  auto dst_val = reinterpret_cast<Val *>(dst);
  uint8_t flag;
  asm /*volatile*/ (
      "lock cmpxchg16b %[ptr]\n"
      : "+a" (old_val->low),
        "+d" (old_val->high),
        "=@cce" (flag)
      : "b" (src_val->low),
        "c" (src_val->high),
        [ptr] "m" (*dst_val)
  );
  return flag;
}

inline void atomic_load_128(std::byte * dst, std::byte const * src) {
  assert((reinterpret_cast<uintptr_t>(src) & 0xF) == 0); //See compare_exchange_128
  assert(dst != src); //See compare_exchange_128
  struct Val {
    uint64_t low;
    uint64_t high;
  } * dst_val = reinterpret_cast<Val *>(dst);
  asm volatile (
      "lock cmpxchg16b %[ptr]\n"
      : "=a" (dst_val->low),
        "=d" (dst_val->high)
      : "a" (0ULL),
        "b" (0ULL),
        "c" (0ULL),
        "d" (0ULL),
        [ptr] "m" (src)
  );
}

template<class T>
void fetch_and_add_128(std::byte * orig, std::byte * local, T val) {
  std::byte new_value[16];
  {
    *reinterpret_cast<__uint128_t*>(new_value) = *reinterpret_cast<__uint128_t*>(local) + val;
  } while (!compare_exchange_128(orig, new_value, local));
}

template<class T>
void findAndReplaceAll(T & res, T const & search, T const & replace) {
  for (auto pos = res.find(search); pos != T::npos; pos = res.find(search, pos + replace.size())) {
    res.replace(pos, search.size(), replace);
  }
}

// __builtin_clzll
template<class T>
std::optional<T> pos_most_significant_bit(T const val) noexcept {
    if (val == 0) {
        return std::nullopt; 
    }
    T ret;
    asm (
        "BSR %[val], %[ret]"
        : [ret] "=r" (ret)
        : [val] "rm" (val)
    );
    return ret;
}

//  __builtin_ctzll
template<class T>
std::optional<T> pos_least_significant_bit(T const val) noexcept {
    if (val == 0) {
        return std::nullopt;
    }
    T ret;
    asm (
        "BSF %[val], %[ret]"
        : [ret] "=r" (ret)
        : [val] "rm" (val)
    );
    return ret;
}

template<class T1, class T2>
T1 reset_bit(T1 val, T2 const pos) noexcept {
    asm (
        "BTR %[val], %[pos]"
        : [val] "+rm" (val)
        : [pos] "r" (pos)
    );
    return val;
}

template<class T1, class T2>
T1 set_bit(T1 val, T2 const pos) noexcept {
    asm (
        "BTS %[val], %[pos]"
        : [val] "+rm" (val)
        : [pos] "r" (pos)
    );
    return val;
}

template<class T1>
bool test_bit(T1 const & val, T1 const & pos) noexcept {
#if 0
    uint8_t res{0};
    asm (
        "BT %[val], %[pos] \n"
        "SETC %[res]       \n"
        : [res] "=r" (res)
        : [val] "rm" (val),
          [pos] "r" (pos)
    );
    return res;
#endif
    return 0;
}

} //namespace avocado
