#include "stats/packet_stats.h"

#include "stats/core.h"

#include <string>
#include <boost/property_tree/ptree.hpp>

namespace {
  using ptree = boost::property_tree::ptree;
}

RpcStats::~RpcStats() {
  ptree res;
  {
    ptree alloc_stats;
    alloc_stats.put("events", alloc.n_events.load(std::memory_order_relaxed));
    alloc_stats.put("bytes", alloc.n_bytes.load(std::memory_order_relaxed));
    alloc_stats.put("time", alloc.total_time.load(std::memory_order_relaxed));
    alloc_stats.put("stalls", alloc.stalls.load(std::memory_order_relaxed));
    res.add_child("alloc", alloc_stats);
  }
  {
    ptree free_stats;
    free_stats.put("events", free_calls.n_events.load(std::memory_order_relaxed));
    free_stats.put("time", free_calls.total_time.load(std::memory_order_relaxed));
    res.add_child("free", free_stats);
  }
  res.put("requests", enqueue_request.n_events.load(std::memory_order_relaxed));
  res.put("responses", enqueue_response.n_events.load(std::memory_order_relaxed));

  stats.add("rpc", res);
}

RpcStats rpc_stats;
