#include "stats/crypt_stats.h"

#include "stats/core.h"

#include <string>
#include <boost/property_tree/ptree.hpp>

#include "crypt/encrypt_openssl.h"
#include "crypt/encrypt_package.h"

namespace {
  using ptree = boost::property_tree::ptree;

  void add(std::string const & name, ptree & child, CryptStats::Data const & data) {
    auto n_events = data.n_events.load(std::memory_order_relaxed);
    auto n_bytes = data.n_bytes.load(std::memory_order_relaxed);
    auto total_time = data.total_time.load(std::memory_order_relaxed);
    ptree res;
    res.put("events", n_events);
    res.put("bytes", n_bytes);
    res.put("time", total_time);
    child.add_child(name, res);
  }
}

CryptStats::~CryptStats() {
  ptree res;
  add("encrypt", res, encrypt);
  add("decrypt", res, decrypt);
  stats.add(name(), res);
}

HashSslStats::~HashSslStats() {
  add("hash", stats.root, data);
}

CipherSslStats cipher_ssl_stats;
HashSslStats hash_ssl_stats;
PacketSslStats packet_ssl_stats;
