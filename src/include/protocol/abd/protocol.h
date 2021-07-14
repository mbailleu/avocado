#pragma once

#include <memory>
#include <atomic>
#include <vector>
#include <string>
#include <functional>
#include <cstdint>

#include "rpc.h"
#include "lamport.h"
#include "key_access.h"
//#include "header.h"
#include "span.h"
#include "msg.h"
#include "replica.h"
#include "svr_context.h"
#include "replica_context.h"
#include "memtable/memtable.h"
#include "crypt/encrypt_package.h"
#include "debug_print.h"
#include <fmt/core.h>

#define BENCHMARKING 1
#define SCALA 1

namespace avocado {


class ABD {

  static constexpr uint8_t ACK[] = "ACK";
  static constexpr size_t ACK_SZ = sizeof(ACK);

private:
  size_t max_value_size;
  size_t max_package_size; //MaxValueSize + sizeof(Lamport) + PacketSsl::MetaDataSize;
  std::vector<Replica_info> replicas_infos;
#if SCALA
  std::vector<std::vector<Replica>> replicas;
#else
  std::vector<Replica> replicas;
#endif

  size_t const local_id;
  PacketSsl cipher;

public:;
  using KV_store = ::avocado::KV_store;

private:
  KV_store & kv;
  //static size_t constexpr MaxPacketSize = erpc::CTransport::kMaxDataPerPkt;
  Lamport::M_id_t m_id{0};

public:
  enum ReqType {
    Put,
    Get,
    GetTimeStamp,
    PutReplica,
    GetValueTimeStamp,
    Cmd,
    EndReq
  };

  enum ContType {
    GetTime,
    AckPutTime,
    AckPut,
    RetValue,
    RetGetPutUpdate,
    CmdRet,
    EndCont
  };

public:

  using Span = ::avocado::Span<erpc::MsgBuffer, typename KV_store::Ret_value>;
  using Msg  = ::avocado::Msg<Span>;
  using MsgBuilder = ::avocado::MsgBuilder<Span>;
  using ReplicaContext = ::avocado::ReplicaContext<KV_store>;
  using Node = ::avocado::NodeReplicaContext<KV_store>;

private:
  template<class ... Args>
  std::shared_ptr<ReplicaContext> create_rep_context(Args && ... args) {
    return ReplicaContext::create(max_package_size, rpc, cipher, std::forward<Args>(args)...);
  }

  void delete_rep_context(ReplicaContext * ptr) {
    delete ptr;
  }

public:

  SvrContext create_svr_context() {
    auto res = SvrContext(cipher);
    res.parent = this;
    return res;
  }

  ABD(size_t MaxValueSize, Lamport::M_id_t m_id, KV_store & kv, std::vector<Replica_info> const && r_uri, size_t local_id, PacketSsl cipher)
    : max_value_size(MaxValueSize)
    , max_package_size(MaxValueSize + sizeof(Lamport) + PacketSsl::MetaDataSize)
    , replicas_infos(std::move(r_uri))
    , local_id(local_id)
    , cipher(std::move(cipher))
    , kv(kv)
    , m_id(m_id) {};

  erpc::Rpc<erpc::CTransport> * rpc;

  std::function<Msg (Msg const &)> cmd;
  std::function<void (size_t, Msg &)> cmd_ret;
  std::atomic<size_t> connected{0};

#if BENCHMARKING
  std::function<void (uint8_t const * buf, size_t const size)> put_ret;
  std::function<void (uint8_t const * buf, size_t const size)> get_ret;
  std::function<void (uint8_t const * buf, size_t const size)> skip_ret;
#endif //BENCMARKING

#if !defined(NDEBUG)
  void print_retries(size_t i, Replica_info const & r) {
    if (i % 100000 == 0) {
      debug_print("Trying to connect to: ", r.uri, " with id: ", r.session_id, " connection attempt: ", i);
    }
  }
#endif

  void init() {
    for (auto & r : replicas_infos) {
      IF_DEBUG(size_t i{0ULL});
      r.session_id = rpc->create_session(r.uri, r.base_id);
      while (!rpc->is_connected(r.session_id)) {
        i++;
    //    IF_DEBUG(print_retries(i++, r));
        rpc->run_event_loop_once();
      }
#if !defined(SCALA)
      replicas.emplace_back(Replica{r.session_id});
#endif
      debug_print("Connect to: ", r.uri, " with id: ", r.session_id, " at attempt: ", i);
    }
    while(connected.load(std::memory_order_relaxed) != replicas_infos.size()) {
      rpc->run_event_loop_once();
    }
  }

#if defined(SCALA)
  void replica_ordering(std::vector<std::tuple<bool, size_t, size_t>> const & order) {
    for (auto [us, a, b] : order) {
      if (us) {
        Replica id_a{replicas_infos[a].session_id};
        Replica id_b{replicas_infos[b].session_id};
        replicas.emplace_back(std::vector<Replica>{id_a, id_b});
      } else {
        replicas.emplace_back();
      }
    }
  }
#endif


private:
  static Msg get_data(erpc::ReqHandle * handle, SvrContext * context) noexcept {
    auto const & msg_buf = handle->get_req_msgbuf();
    auto const size = msg_buf->get_data_size();
    Msg res(context->get_message_size(size));
    context->decrypt(res.buf(), msg_buf->buf, size);
    return res;
  }

  auto begin_request(erpc::ReqHandle * handle, void * context_handle) const noexcept -> std::pair<erpc::ReqHandle *,SvrContext *> {
    return std::make_pair(handle, reinterpret_cast<SvrContext *>(context_handle));
  }

public:
  template<int CALL>
  static void request(erpc::ReqHandle * handle, void * tag);

  template<int CALL>
  static void cont(void * handle, void * tag);

  void send_cmd(std::string const & command) {
    MsgBuilder msg(command.size() + 1);
    msg.add_key(command.c_str(), command.size() + 1);
    auto rep_context = create_rep_context(nullptr);
  //  rep_context->data = msg.finish();
    rep_context->send_and_encrypt_msg(replicas_infos, ReqType::Cmd, cont<ContType::CmdRet>, msg.finish());
    //rep_context->local_resp 
  }

#if BENCHMARKING

  static Msg get_msg(size_t key_size, size_t value_size) {
    Msg res(key_size + value_size + sizeof(Msg::Encoded_msg));
    auto ptr = res.get_ptr();
    ptr->key_size = key_size;
    ptr->size = key_size + value_size;
    return res;
  }

  std::atomic<size_t> current_traces{0};

  template<int i>
  constexpr auto add_traces() {
    current_traces.fetch_add(i, std::memory_order_relaxed);
  }

  constexpr auto increment_traces() {
    return add_traces<1>();
  }

  constexpr auto decrement_traces() {
    return add_traces<-1>();
  }

  auto load_traces() {
    return current_traces.load(std::memory_order_relaxed);
  }

  template<class T>
  size_t get_idx_(uint8_t const * buf) const noexcept {
    return *(reinterpret_cast<T const *>(buf)) % replicas.size();
  }

  size_t get_idx(Span const & key) const noexcept {
    if (key.size >= 8) {
      return get_idx_<uint64_t>(key.buf + (key.size - 8));
    }
    if (key.size >= 4) {
      return get_idx_<uint32_t>(key.buf + (key.size - 4));
    }
    if (key.size >= 2) {
      return get_idx_<uint16_t>(key.buf + (key.size - 2));
    }
    return get_idx_<uint8_t>(key.buf);
  }

#if defined(SCALA)

  template<class F>
  void send_and_encrypt_msg(ReplicaContext * rep_context, Span const & key, int cont_id, F const & cont_f, uint8_t * buf, size_t size) {
    auto const & reps = replicas[get_idx(key)];
    if (reps.size() > 0) {
      rep_context->send_and_encrypt_msg(reps, cont_id, cont_f, buf, size);
    } else {
      decrement_traces();
      skip_ret(nullptr, 0);
    }
  }

  template<class F>
  void send_and_encrypt_msg(ReplicaContext * rep_context, Span const & key, int cont_id, F const & cont_f, Msg const & msg) {
    auto const & reps = replicas[get_idx(key)];
    if (reps.size() > 0) {
      rep_context->send_and_encrypt_msg(reps, cont_id, cont_f, msg);
    } else {
      decrement_traces();
      skip_ret(nullptr, 0);
    }
  }

#else
  
  template<class F>
  void send_and_encrypt_msg(ReplicaContext * rep_context, Span const &, int cont_id, F const & cont_f, uint8_t * buf, size_t size) {
    rep_context->send_and_encrypt_msg(replicas, cont_id, cont_f, buf, size);
  }

  template<class F>
  void send_and_encrypt_msg(ReplicaContext * rep_context, Span const &, int cont_id, F const & cont_f, Msg const & msg) {
    rep_context->send_and_encrypt_msg(replicas, cont_id, cont_f, msg);
  }

#endif

  void benchmark_put(Msg msg) {
    increment_traces();
    auto const & key = msg.get_key();
    MsgBuilder replica_msg(key.size);
    replica_msg.add_key(key.buf, key.size);
    std::shared_ptr<ReplicaContext> rep_context = create_rep_context(nullptr);
    auto value = msg.get_value();
    rep_context->data = kv.create_node(key.size, key.buf, value.size, value.buf, Lamport::min());
    send_and_encrypt_msg(rep_context.get(), key, ReqType::GetTimeStamp, cont<ContType::GetTime>, replica_msg.finish());
    //rep_context->
    //rep_context->local_resp = kv.get(rep_context->data.get_key().size, rep_context->data.get_key().buf);
  }

  void benchmark_get(Msg msg) {
    increment_traces();
    auto rep_context = create_rep_context(nullptr);
    auto key = msg.get_key();
    rep_context->data = kv.create_node(key.size, key.buf, 0, nullptr, Lamport::min());
    send_and_encrypt_msg(rep_context.get(), key, ReqType::GetValueTimeStamp, cont<ContType::RetValue>, msg);
    //rep_context->local_resp = kv.get(rep_context->data.get_key().size, rep_context->data.get_key().buf);
  }
#endif

private:
  struct True_if_time {
    size_t const size;
    Lamport const clock;

    True_if_time(size_t const size, Lamport const clock) : size(size), clock(clock) {}
    True_if_time(Msg const & data) : size(data.get_key_size()), clock(KeyAccess::get_lamport(size, data.get_key().buf)) {}

    bool operator()(KV_store::Ret_value const & key) const noexcept {
#if defined(NDEBUG)
      return key.size == 0 || KeyAccess::get_lamport(size, key.kv.get()) < clock;
#else //NDEBUG
      if (key.size == 0)
        return true;

      return key.lamport < clock;
#endif
    }
  };

  template<int TYPE> void req_handlers(erpc::ReqHandle *, void *);
  template<int TYPE> void req_conts_impl(size_t n_rep, ReplicaContext * rep_context, SvrContext * svr_context);
  template<int TYPE> void req_conts(void *, void *);
};

template<>
void ABD::req_handlers<ABD::ReqType::Put>(erpc::ReqHandle * req_h, void * context_handle) {
  auto [req_handle, context] = begin_request(req_h, context_handle);
  auto rep_context = create_rep_context(req_handle);
  auto data = get_data(req_handle, context);
  auto key = data.get_key();
  auto value = data.get_value();
  rep_context->data = kv.create_node(key.size, key.buf, value.size, value.buf, Lamport::min());
#if !BENCHMARKING
  rep_context->send_and_encrypt_msg(replicas, ReqType::GetTimeStamp, cont<ContType::GetTime>, data);
#else
  send_and_encrypt_msg(rep_context.get(), key, ReqType::GetTimeStamp, cont<ContType::GetTime>, data);
#endif
  //rep_context->local_resp = kv.get(rep_context->data.get_key().size, rep_context->data.get_key().buf);
}

template<>
void ABD::req_handlers<ABD::ReqType::Get>(erpc::ReqHandle * req_h, void * context_handle) {
  auto [req_handle, context] = begin_request(req_h, context_handle);
  auto rep_context = create_rep_context(req_handle);
  auto data = get_data(req_handle, context);
  auto key = data.get_key();
  rep_context->data = kv.create_node(key.size, key.buf, 0, nullptr, Lamport::min());
#if !BENCHMARKING
  rep_context->send_and_encrypt_msg(replicas, ReqType::GetValueTimeStamp, cont<ContType::RetValue>, data);
#else
  send_and_encrypt_msg(rep_context.get(), key, ReqType::GetValueTimeStamp, cont<ContType::RetValue>, data);
#endif

  //rep_context->local_resp = kv.get(rep_context->data.get_key().size, rep_context->data.get_key().buf);
}

template<>
void ABD::req_handlers<ABD::ReqType::GetTimeStamp>(erpc::ReqHandle * req_h, void * context_handle) {
  auto [req_handle, context] = begin_request(req_h, context_handle);
  auto data = get_data(req_handle, context);
  auto res = kv.get(data.get_key_size(), data.get_key().buf);
  MsgBuilder msg(sizeof(res.lamport));
  msg.add_key(&(res.lamport));
  context->encrypt_and_resp(req_handle, msg.finish());
  //What is this code doing?
  //auto key = std::make_unique<uint8_t[]>(data.get_key_size());
  //memcpy(key.get(), data.get_key().buf, data.get_key_size());
}

template<>
void ABD::req_handlers<ABD::ReqType::PutReplica>(erpc::ReqHandle * req_handle, void * context_handle) {
  auto [req, context] = begin_request(req_handle, context_handle);
  auto data = get_data(req, context);
  auto clock = KeyAccess::get_lamport(data.get_key_size(), data.get_key().buf);
  [[maybe_unused]] auto res = kv.put(KeyAccess::sub_meta_size(data.get_key_size())
                                    , data.get_key().buf
                                    , data.get_value_size()
                                    , data.get_value().buf
                                    , clock);
  MsgBuilder msg(ACK_SZ);
  msg.add_key(ACK);
  context->encrypt_and_resp(req_handle, msg.finish());
}

template<>
void ABD::req_handlers<ABD::ReqType::GetValueTimeStamp>(erpc::ReqHandle * req_handle, void * context_handle) {
  auto [req, context] = begin_request(req_handle, context_handle);
  auto data = get_data(req_handle, context);
  auto key = data.get_key();
  auto res = kv.get(key.size, key.buf);
  if (res.value != nullptr) {
    MsgBuilder msg(KeyAccess::add_meta_size(key.size) + res.size);
    msg.add_key(key.buf, key.size);
    msg.add_key(&(res.lamport));
    msg.add_value(res.value.get(), res.size); //We might be able to optimize this away
    context->encrypt_and_resp(req_handle, msg.finish());
    return;
  }
  auto clock = Lamport::min();
  MsgBuilder msg(KeyAccess::add_meta_size(key.size));
  msg.add_key(key.buf, key.size);
  msg.add_key(&clock);
  context->encrypt_and_resp(req_handle, msg.finish());
}

template<>
void ABD::req_handlers<ABD::ReqType::Cmd>(erpc::ReqHandle * req_handle, void * context_handle) {
  if (!cmd) return;
  auto [req, context] = begin_request(req_handle, context_handle);
  auto data = get_data(req_handle, context);
  auto msg = cmd(data);
  context->encrypt_and_resp(req_handle, msg);
}

template<int CALL>
void ABD::req_conts(void * context_handle, void * tag) {
  //reverse type deletion, because fucking lib
  auto svr_context = reinterpret_cast<SvrContext *>(context_handle);
  std::unique_ptr<Node, NodeDeleter> node_context(reinterpret_cast<Node *>(tag));
  node_context->decrypt();
  auto * rep_context = node_context->parent.get();
  
  //Get the number of responses
  auto n_resp = rep_context->n_resp.fetch_add(1, std::memory_order_relaxed);
  n_resp += 1;

  if constexpr (CALL == ABD::ContType::CmdRet) {
    return req_conts_impl<CALL>(n_resp, rep_context, svr_context);
  }
  //If we do not have perfectly half of the responses, we just return
  //if ((n_resp + 1) < (rep_context->n_msgs + 1) / 2) {
  //The below is just the conversion from above, the compiler doesn't do the transformation for some reason
  if (2 * n_resp + 1 < rep_context->n_msgs) {
    return;
  }
  
  if (rep_context->done.test_and_set(std::memory_order_acquire)) {
    return;
  }
  req_conts_impl<CALL>(n_resp, rep_context, svr_context);
}

template<>
void ABD::req_conts_impl<ABD::ContType::GetTime>(size_t, ReplicaContext * rep_context, SvrContext *) {
  //Find the maximum clock in the responses
  
  auto & node = rep_context->data;
  auto local_resp = kv.get(node);
  auto max = rep_context->reduce_resp(
    [] (auto const & prior, Msg const & buf) -> Lamport::Count_t {
      auto const size = buf.get_size();
      if (size < sizeof(Lamport)) {
        return prior;
      }
      auto key = buf.get_key();
      auto const clock = KeyAccess::get_lamport(key.size, key.buf);
      return prior >= clock.get_count() ?
             prior :
             clock.get_count();
    }
    , local_resp.lamport.get_count()
  );

  //Generating put message
  auto put_context = create_rep_context(rep_context->handle);

  auto const msg_size = KeyAccess::add_meta_size(node.key_size) + node.meta->data_size;

  MsgBuilder builder(msg_size); 
  Lamport clock(max + 1, m_id);
  auto [_, value_size, value] = kv.decrypt_node(node);
  builder.add_key(node.key.get(), node.key_size);
  builder.add_key(&clock);
  builder.add_value(value.get(), value_size);
  auto msg = builder.finish();
#if !defined(BENCHMARKING)
  put_context->send_and_encrypt_msg(replicas, ReqType::PutReplica, cont<ContType::AckPutTime>, msg);
#else
  send_and_encrypt_msg(put_context.get(), Span(node.key_size, reinterpret_cast<uint8_t *>(node.key.get())), ReqType::PutReplica, cont<ContType::AckPutTime>, msg);
#endif
  node.meta->clock = clock;
  [[maybe_unused]] auto res = kv.put(node);
}

template<>
void ABD::req_conts_impl<ABD::ContType::AckPutTime>(size_t, ReplicaContext *, [[maybe_unused]] SvrContext * svr_context) {
  //TODO test if majority is ACK

#if !BENCHMARKING
  svr_context->encrypt_and_resp(rep_context->handle, ACK, ACK_SZ);
#else //!BENCHMARKING
  decrement_traces();
  put_ret(ACK, ACK_SZ);
#endif
}

template<>
void ABD::req_conts_impl<ABD::ContType::RetValue>(size_t, ReplicaContext * rep_context, [[maybe_unused]] SvrContext * svr_context) { 
  std::map<Lamport,size_t> counts;
  auto const key_size = KeyAccess::add_meta_size(rep_context->data.key_size);
  auto & node = rep_context->data;
  auto local_resp = kv.get(node);
  auto clock = local_resp.lamport;
      
#if 0
  for (auto i = 0ULL; i < replicas.size(); ++i) {
    auto clock = *KeyAccess::get_lamport(key_size, rep_context->buffers[i].ptr.get());
  }
#endif

  Msg tmp(rep_context->max_package_size);

  auto max_clock_value = rep_context->reduce_resp(
    [key_size, &counts, &tmp] (auto max, Msg const & buf) -> std::pair<Lamport, Span> {
      auto clock = KeyAccess::get_lamport(key_size, buf.get_key().buf);
      auto el = counts.find(clock);
      if (el == counts.end()) {
        counts.insert({clock, 1});
      } else {
        el->second++;
      }
              
      if (clock > max.first) {
        memcpy(tmp.buf(), buf.buf(), buf.get_size());
        return std::make_pair<Lamport, Span>(std::move(clock), tmp.get_key());
      }
      return max;
    }
    , std::make_pair<Lamport,Span>(std::move(clock), local_resp)
  );

  bool consens{false};

  for (auto const & c : counts) {
    if (c.second > rep_context->n_msgs / 2) {
      clock = c.first;
      consens = true;
      break;
    }
  }

  if (!consens && max_clock_value.second.size > 0) {
    auto msg_size = KeyAccess::add_meta_size(node.key_size) + max_clock_value.second.size;
    MsgBuilder msg(msg_size);
    msg.add_key(node.key.get(), node.key_size);
    msg.add_key(&max_clock_value.first);
    msg.add_value(max_clock_value.second.buf, max_clock_value.second.size);
    auto put_context = create_rep_context(rep_context->handle);
#if !BENCHMARKING
    put_context->send_and_encrypt_msg(replicas, ReqType::PutReplica, cont<ContType::RetGetPutUpdate>, msg.finish());
#else
    send_and_encrypt_msg(put_context.get(), Span(node.key_size, reinterpret_cast<uint8_t *>(node.key.get())), ReqType::PutReplica, cont<ContType::RetGetPutUpdate>, msg.finish());
#endif
  }
#if !BENCHMARKING
  svr_context->encrypt_and_resp(rep_context->handle, max_clock_value.second.buf, max_clock_value.second.size);
#else
  decrement_traces();
  get_ret(max_clock_value.second.buf, max_clock_value.second.size);
#endif
}

template<>
void ABD::req_conts_impl<ABD::ContType::RetGetPutUpdate>(size_t, ReplicaContext *, SvrContext *) {
}

template<>
void ABD::req_conts_impl<ABD::ContType::CmdRet>(size_t n_resp, ReplicaContext * rep_context, SvrContext *) {
  if (!cmd_ret) return;
  rep_context->map_last([this, n_resp] (auto & msg) { cmd_ret(n_resp, msg); });
}

template<int CALL>
void ABD::request(erpc::ReqHandle * handle, void * tag) {
  reinterpret_cast<SvrContext *>(tag)->parent->req_handlers<CALL>(handle, tag);
}

template<int CALL>
void ABD::cont(void * handle, void * tag) {
  reinterpret_cast<SvrContext *>(handle)->parent->req_conts<CALL>(handle, tag);
}

}
