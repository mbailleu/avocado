#pragma once

namespace stats_ {
#include "crypt/release/encrypt_package.h"
}

#include <string>
#include <utility>

#include "stats/crypt_stats.h"
#include "stats/rdtsc.h"

#if 1
class PacketSslStats : public CryptStats {
public:
  PacketSslStats() : CryptStats() {
    name_ = "PacketSslStats";
  }

  virtual ~PacketSslStats() {
    decrypt.n_bytes = decrypt.n_bytes - (decrypt.n_events * stats_::PacketSsl::MetaDataSize);
  }
};

extern PacketSslStats packet_ssl_stats;

class [[nodiscard]] PacketSsl : public stats_::PacketSsl {
  using Base = stats_::PacketSsl;

public:
  template<class ... Args>
  PacketSsl(Args && ... args) : Base(std::forward<Args>(args)...) {}

  static PacketSsl create(std::byte const * key, std::byte const * iv) {
    return PacketSsl(std::make_shared<stats_::KeyIV>(key, iv));
  }

  static PacketSsl create(CipherSsl const & cipher) {
    return PacketSsl(std::make_shared<stats_::KeyIV>(reinterpret_cast<std::byte const *>(cipher._key), reinterpret_cast<std::byte const *>(cipher._iv)));
  }

  template<class DST, class SRC>
  auto cipher(DST * dst, SRC * src, size_t src_size, Func func) const noexcept {
    auto start = get_tsc();
    auto res = Base::cipher<DST, SRC>(dst, src, src_size, func);
    auto end = get_tsc();
    auto & data = (func == Base::ENCRYPT) ? packet_ssl_stats.encrypt : packet_ssl_stats.decrypt;
    data.add(src_size, end - start);
    return res;
  }

  template<class DST, class SRC>
  auto encrypt(DST * dst, SRC * src, size_t src_size) const noexcept {
#if 0
    auto && [time, res] = take_time_diff([&] { return Base::encrypt(dst, src, src_size); });
    packet_ssl_stats.encrypt.add(src_size, time);
    return res;
#else
    auto start = get_tsc();
    auto res = Base::encrypt(dst, src, src_size);
    auto end = get_tsc();
    packet_ssl_stats.encrypt.add(src_size, end - start);
    return res;
#endif
  }

  template<class DST, class SRC>
  auto decrypt(DST * dst, SRC * src, size_t src_size) const noexcept {
    auto start = get_tsc();
    auto res = Base::decrypt(dst, src, src_size);
    auto end = get_tsc();
    packet_ssl_stats.decrypt.add(src_size, end - start);
    return res;
  }
};
#else

struct PacketSslStats {};
using PacketSsl = stats_::PacketSsl;

#endif
