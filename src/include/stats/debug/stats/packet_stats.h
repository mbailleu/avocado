#pragma once
#include <atomic>
#include <cstdint>

#include "rpc.h"
#include "stats/rdtsc.h"

#include "size_events.h"

struct RpcStats {
  struct Alloc : public SizeEventData {
    std::atomic<size_t> stalls;

    void add_stall() {
      stalls.fetch_add(1, std::memory_order_relaxed);
    }
  };

  Alloc alloc;

  EventTime free_calls;

  Event enqueue_request;
  Event enqueue_response;

  ~RpcStats();
};

extern RpcStats rpc_stats;

#define ENQUEUE(fn) \
  template<class ... Args> \
  void fn(Args && ... args) { \
    Base:: fn(std::forward<Args>(args)...); \
    rpc_stats. fn .add(); \
  }

class Rpc_wrapper : public erpc::Rpc<erpc::CTransport> {
public:
  using Base = erpc::Rpc<erpc::CTransport>;

  template<class ... Args>
  Rpc_wrapper(Args && ... args) : Base(std::forward<Args>(args)...) {}

  inline erpc::MsgBuffer alloc_msg_buffer(size_t max_data_size) {
    auto [time, res] = take_time_diff([&] { return Base::alloc_msg_buffer(max_data_size); });
    if (res.buf == nullptr) {
      rpc_stats.alloc.add_stall();
      max_data_size = 0;
    }
    rpc_stats.alloc.add(max_data_size, time);
    return res;
  }

  inline void free_msg_buffer(erpc::MsgBuffer msg_buffer) {
    auto [time, res] = take_time_diff([&] { return Base::free_msg_buffer(msg_buffer), true; });
    rpc_stats.free_calls.add(time);
  }

#if 0
  template<class ... Args>
  void enqueue_request(Args && ... args) {
    auto res = Base::enqueue_request(std::forward<Args>(args)...);
    rpc_stats.enqueue_response.fetch_add(1, std::memory_order_relaxed);
    return res;
  }

  template<class ... Args>
  void enqueue_response(Args && ... args) {
    auto res = Base::enqueue_response(std::forward<Args>(args)...);
    rpc_stats.enqueue_request.fetch_add(1, std::memory_order_relaxed);
    return res;
  }
#else
  ENQUEUE(enqueue_request)
  ENQUEUE(enqueue_response)
#endif
};

#undef ENQUEUE
