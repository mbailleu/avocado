#pragma once

namespace stats_ {
#include "crypt/release/encrypt_openssl.h"
} //namespace stats_

#include <string>
#include <utility>

#include "stats/crypt_stats.h"
#include "stats/rdtsc.h"

class CipherSslStats : public CryptStats {
public:
  CipherSslStats() : CryptStats() {
    name_ = "CipherSslStats";
  }
};

class HashSslStats {
public:
  CryptStats::Data data;

  std::string name() const { return "HashSslStats"; }

  virtual ~HashSslStats();
};

extern CipherSslStats cipher_ssl_stats;
extern HashSslStats hash_ssl_stats;

class [[nodiscard]] CipherSsl : public stats_::CipherSsl {
  using Base = stats_::CipherSsl;

public:
  template<class ... Args>
  CipherSsl(Args && ... args) : Base(std::forward<Args>(args)...) {}

  template<class DST, class SRC>
  auto encrypt(DST * dst, SRC * src, size_t src_size, uint8_t * mac) const noexcept {
    auto start = get_tsc();
    auto res = Base::encrypt(dst, src, src_size, mac);
    auto end = get_tsc();
    cipher_ssl_stats.encrypt.add(src_size, end - start);
    return res;
  }

  template<class DST, class SRC>
  auto decrypt(DST * dst, SRC * src, size_t src_size, uint8_t const * mac) const noexcept {
    auto start = get_tsc();
    auto res = Base::decrypt(dst, src, src_size, mac);
    auto end = get_tsc();
    cipher_ssl_stats.decrypt.add(src_size, end - start);
    return res;
  }
};

namespace {
  inline void update_encrypt(size_t bytes, size_t time) {
    cipher_ssl_stats.encrypt.add(bytes, time);
  }

  inline void update_decrypt(size_t bytes, size_t time) {
    cipher_ssl_stats.decrypt.add(bytes, time);
  }
}

template<class BASE,
          void update_impl(size_t, size_t)>
struct [[nodiscard]] SplitCipherSsl : public BASE {
  using Base = BASE;

  size_t bytes{0};
  size_t time{0};

  template<class ... Args>
  SplitCipherSsl(Args && ... args) : Base(std::forward<Args>(args)...) {}

  template<class DST, class SRC>
  [[nodiscard]] size_t update(DST * dst, SRC const * src, size_t const n) noexcept {
    auto start = get_tsc();
    auto res = Base::update(dst, src, n);
    auto end = get_tsc();
    bytes += n;
    time += end - start;
    return res;
  }

  template<class DST>
  bool finalize(DST * dst, uint8_t * mac) noexcept {
    auto start = get_tsc();
    auto res = Base::finalize(dst, mac);
    auto end = get_tsc();
    time += end - start;
    update(bytes, time);
    return res;
  }

};

using EncryptSsl = SplitCipherSsl<stats_::EncryptSsl, update_encrypt>;
using DecryptSsl = SplitCipherSsl<stats_::DecryptSsl, update_decrypt>;

struct [[nodiscard]] HashSsl : public stats_::HashSsl {
  using Base = stats_::HashSsl;

  size_t bytes{0};
  size_t time{0};

  template<class ... Args>
  HashSsl(Args && ... args) : Base(std::forward<Args>(args)...) {}

  template<class T>
  bool update(T const * src, size_t const n) noexcept {
    auto start = get_tsc();
    auto res = Base::update(src, n);
    auto end = get_tsc();
    bytes += n;
    time += end - start;
    return res;
  }

  template<class T>
  bool update(T const & src, size_t const n) noexcept {
    auto start = get_tsc();
    auto res = Base::update(src, n);
    auto end = get_tsc();
    bytes += n;
    time += end - start;
    return res;
  }

  bool finalize() noexcept {
    auto start = get_tsc();
    auto res = Base::finalize();
    auto end = get_tsc();
    time += end - start;
    hash_ssl_stats.data.add(bytes, time);
    return res;
  }
};
