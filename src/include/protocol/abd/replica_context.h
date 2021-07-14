#pragma once

#include <cstddef>
#include <cstdint>
#include <atomic>
#include <array>
#include <vector>
#include <memory>

#include "msg_buffer.h"
#include "rpc.h"

#include "rpc_context.h"

#include "debug_print.h"

#include "crypt/encrypt_package.h"
#include "span.h"
#include "msg.h"
#include "replica.h"

namespace avocado {

template<class KV_store>
class ReplicaContext;

template<class KV_store>
struct NodeReplicaContext {
  using Parent = ReplicaContext<KV_store>;
  std::shared_ptr<Parent> parent;
  size_t n;
  erpc::MsgBuffer req;
  erpc::MsgBuffer resp;

  NodeReplicaContext(std::shared_ptr<Parent> && parent, size_t n, size_t request_size, size_t response_size)
    : parent(parent)
      , n(n)
      , req(parent->alloc_msg_buffer(request_size))
      , resp(parent->alloc_msg_buffer(response_size)) {}

  void remove() {
    if (!parent)
      return;
    if (req.buf != nullptr)
      parent->free_msg_buffer(req);
    if (resp.buf != nullptr)
      parent->free_msg_buffer(resp);
    parent.reset();
  }

  ~NodeReplicaContext() {
    if (!parent)
      return;
    if (req.buf != nullptr)
      parent->free_msg_buffer(req);
    if (resp.buf != nullptr)
      parent->free_msg_buffer(resp);
  }

  void decrypt() {
    parent->add_to_cache(this);
  }
};

struct NodeDeleter {
  template<class T>
  void operator()(T * node) const {
    node->remove();
  }
};

template<class KV_store>
class ReplicaContext : public RpcContext {
public:
  using Span = ::avocado::Span<erpc::MsgBuffer, typename KV_store::Ret_value>;
  using Msg = ::avocado::Msg<Span>;
  using Node = ::avocado::NodeReplicaContext<KV_store>;

  size_t max_package_size;
  std::weak_ptr<ReplicaContext> self;
  erpc::ReqHandle * handle;


  std::atomic<size_t> n_resp{0};
  size_t n_msgs{0};
  std::atomic_flag done = ATOMIC_FLAG_INIT;
  std::vector<erpc::MsgBuffer> cache;
  size_t max_resp_size{0};
  std::vector<Node> nodes;
  //Msg data;
  typename KV_store::Node data;

  enum {
    Req,
    Resp
  };

//  typename KV_store::Ret_value local_resp;
//  std::unique_ptr<std::byte[]> key;
//  size_t key_size;

  ReplicaContext(size_t max_package_size, PacketSsl const & cipher, erpc::ReqHandle * handle) : RpcContext(cipher), max_package_size(max_package_size), handle(handle) {}


public:
  ReplicaContext(ReplicaContext &&) = default;
  ~ReplicaContext() {
    for (auto & c : cache) {
      free_msg_buffer(c);
    }
  }

  template<class ... Args>
  static std::shared_ptr<ReplicaContext> create(size_t max_package_size, erpc::Rpc<erpc::CTransport> * rpc, PacketSsl const & cipher, Args && ... args) {
    auto res = std::make_shared<ReplicaContext>(max_package_size, cipher, std::forward<Args>(args)...);
    res->rpc = reinterpret_cast<Rpc_wrapper *>(rpc);
    res->self = res;
    return res;
  }

  void create_buffers(size_t request_size, size_t response_size) {
    nodes.reserve(n_msgs);
    for (auto i = 0ULL; i < n_msgs; ++i) {
      nodes.emplace_back(self.lock(), i, get_buffer_size(request_size), get_buffer_size(response_size));
    }
  }

  void fill_req_buffers(uint8_t const * buf, size_t size) noexcept {
    assert(buf != nullptr);

    for (auto const & node : nodes) {
      assert(node.req.buf != nullptr);
      assert(node.resp.buf != nullptr);
    }

    encrypt(nodes[0].req.buf, buf, size);
    for (auto i = 1ULL; i < n_msgs; ++i) {
      memcpy(nodes[i].req.buf, nodes[0].req.buf, get_buffer_size(size));
    }
  }

  template<class Replicas, class F>
  void send_and_encrypt_msg(Replicas const & replicas, int cont_id, F const & cont_f, uint8_t * buf, size_t size) {
    n_msgs = replicas.size();
    cache.reserve(n_msgs);
    create_buffers(size, max_package_size);
    fill_req_buffers(buf, size);
    for (auto i = 0ULL; i < n_msgs; ++i) {
      rpc->enqueue_request(replicas[i].session_id
                          , cont_id
                          , &(nodes[i].req)
                          , &(nodes[i].resp)
                          , cont_f
                          , &nodes[i]
          );
    }
  }

  template<class Replicas, class F>
  void send_and_encrypt_msg(Replicas const & replicas, int cont_id, F const & cont_f, Msg const & msg) {
    return send_and_encrypt_msg(replicas, cont_id, cont_f, msg.buf(), Msg::adjust_size(msg.get_size()));
  }

//  template<class F>
//  void map_nodes(F const & f) {
//    for (auto & node : nodes) {
//      f(node);
//    }
//  }

  template<class F>
  void map_resp(F const & f) {
    Msg msg(max_resp_size);
    for (auto & c : cache) {
     // f(local_resp);
      decrypt(msg.buf(), c.buf, c.get_data_size());
      f(msg);
    }
  }

  template<class F>
  void map_last(F const & f) {
    auto & last_cached = cache.back();
    auto size = get_message_size(last_cached.get_data_size());
    Msg msg(size);
    decrypt(msg.buf(), last_cached.buf, last_cached.get_data_size());
    f(msg);
  }

  template<class F, class T>
  auto reduce_resp(F const & f, T const & init) const -> T { 
    auto res = init;
    Msg msg(max_resp_size);
    for (auto const & c : cache) {
      decrypt(msg.buf(), c.buf, c.get_data_size());
      res = f(res, msg);
    }
    return res;
  }

  void add_to_cache(Node * node) {
    auto const & resp = node->resp;
    auto size = get_message_size(resp.get_data_size());
    if (size > max_resp_size) {
      max_resp_size = size;
    }
    cache.emplace_back(resp);
    node->resp.buf = nullptr;
    //auto const size = resp.get_data_size();
    //Msg res(get_message_size(size));
    //decrypt(res.buf(), resp.buf, size);
    //cache.emplace_back(std::move(res));
  }

};

} //avocado
