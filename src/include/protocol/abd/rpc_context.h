#pragma once

#include <cstddef>
#include <cstdint>

#include "rpc.h"

#include "crypt_context.h"
#include "msg.h"

#include "stats/packet_stats.h"

namespace avocado {

struct RpcContext : public CryptContext {
  Rpc_wrapper * rpc;

  RpcContext(PacketSsl const & cipher) : CryptContext(cipher) {}

  [[nodiscard]] erpc::MsgBuffer alloc_msg_buffer(size_t size) {
    for (;;) {
      auto res = rpc->alloc_msg_buffer(size);
      if (res.buf != nullptr) {
        return res;
      }
      for (auto i = 0ULL; i < 1000; ++i) {
        rpc->run_event_loop_once();
      }
    }
  }

  void free_msg_buffer(erpc::MsgBuffer & buf) {
    if (buf.buf == nullptr)
      return;
    rpc->free_msg_buffer(buf);
    buf.buf = nullptr;
  }

  void encrypt_and_resp(erpc::ReqHandle * req, uint8_t const * buf, size_t size) {
    assert(rpc != nullptr);
    auto & resp = [&] () -> erpc::MsgBuffer & {
      if (get_buffer_size(size) <= 968) {
        auto & res = req->pre_resp_msgbuf;
        rpc->resize_msg_buffer(&res, get_buffer_size(size));
        return res;
      }
      auto & res = req->dyn_resp_msgbuf;
      res = alloc_msg_buffer(get_buffer_size(size));
      return res;
    }();
    encrypt(resp.buf, buf, size);
    rpc->enqueue_response(req, &resp);
  }

  template<class T>
  void encrypt_and_resp(erpc::ReqHandle * req, Msg<T> const & msg) {
    return encrypt_and_resp(req, msg.buf(), Msg<T>::adjust_size(msg.get_size()));
  }
};

} //shiedlstore_
