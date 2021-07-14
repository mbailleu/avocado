#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "docopt/docopt.h"

#include "memtable/allocator.h"
#include "memtable/memtable.h"
#include "version.h"
#include "common.h"
#include "generate_traces.h"
#include <fmt/format.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <utility>
#include <vector>
#include <tuple>
#include <algorithm>
#include <iterator>
#include "slab_allocator/size_allocator.h"

#include "rpc.h"

#include "protocol/abd/protocol.h"
#include <sys/time.h>

#if BENCHMARKING
#include <utility>
#include <future>
#include <boost/property_tree/ptree.hpp>
#endif //BENCHMARKING

//#if COLLECT_STATS
#include "stats/core.h"
//#endif //COLLECT_STATS

struct RateLimit {
  std::atomic<size_t> n_aquire{0};
  std::atomic<size_t> n_release{0};
  std::atomic<size_t> request{0};
  size_t max_request{1024};
  //static constexpr size_t print_request = 10; // 10000ULL;
  static constexpr size_t print_request = 10000ULL;

  void release() {
    request.fetch_add(-1, std::memory_order_relaxed);
    auto n = n_release.fetch_add(1, std::memory_order_relaxed);
    if (0 && n % print_request == 0) {
      fmt::print("Release nr: {}\n", n);
    }
  }

  template<class F>
  void aquire(F const & f) {
    auto n = n_aquire.fetch_add(1, std::memory_order_relaxed);
    if (n % print_request == 0) {
      fmt::print("Request nr: {}\n", n);
    }
    for (;;) {
      auto r = request.load(std::memory_order_relaxed);
      while (r < max_request && !request.compare_exchange_weak(r, r + 1, std::memory_order_release, std::memory_order_relaxed)) {}
      if (r < max_request) {
        return;
      }
      f();
    }
  }

} rate;

void find_and_replace_all(std::string & res, std::string const & search, std::string const & replace) {
  for (auto pos = res.find(search);
       pos != std::string::npos;
       pos = res.find(search, pos + replace.size())) {
    res.replace(pos, search.size(), replace);
  }
}

using namespace avocado;

static std::string Name("Avocado");
static std::string Name_vers = Name + " (Ver:";

static const char USAGE[] = 
R"( 

  Usage:
)"
"    avocado [-n <threads>] -u <uri> (-a <other>)... [-p <port>] [-v <size>] (-k <number> | -m <size>) [-d] "
#if BENCHMARKING
"[--key_size <key_size>] [--value_size <value_size>] --n_cmds <n_cmds> --n_keys <n_keys> [--permille_reads <permille_reads>] [--output <results>]"
#endif //BENCHMARKING
R"(

  Options:
    -h --help                              Show this screen
    --version                              Show version
    -n <threads>                           Number of threads to use [default: 1]
    -u <uri>                               Server URI
    -a <other>                             Add replica (format ip_addrs:port:threads)
    -p <port>                              Physical port [default: 0]
    -v --max-value-size <size>             Max value size per key [default:64]
    -k --number-of-distinct-keys <number>  Number of distinct keys (used to estimate host-memory usage) [default: 0]
    -m --host-memory <size>                Amount of host memory to allocate (overrides Number of distinct keys) [default: 0]
    -d --dump-config                       Print the current config
)"
#if BENCHMARKING
R"(    --key_size <key_size>                  set key size for benchmarking (currently ignored) [default: 8]
    --value_size <value_size>              set value size for benchmaking (if zero take value from -vsz) [default: 0]
    --n_cmds <n_cmds>                      set number of commands
    --n_keys <n_keys>                      set number of keys
    --permille_reads <reads>               set the ratio of reads in permille [default: 500]
    --output <results>                     File to write benchmark results to)"
#endif //BENCHMARKING
;


struct Args {
  size_t n_thread;
  std::string uri;

  std::vector<std::string> replica_info;
  size_t port;
  size_t n_distinct_keys;
  size_t host_memory;
  size_t max_value_size;

#if BENCHMARKING
  size_t key_size = ::avocado::Trace_cmd::key_size;
  size_t value_size;
  size_t n_cmds;
  size_t n_keys;
  size_t permille_reads;
  std::string output;
#endif //BENCHMARKING

#if COLLECT_STATS || BENCHMARKING
  void dump() {
    using ptree = boost::property_tree::ptree;
    ptree args;
    args.put("thread", n_thread);
    args.put("uri", uri);
    {
      ptree replicas;
      for (auto const & r : replica_info) {
        ptree tmp;
        tmp.put("", r);
        replicas.push_back(std::make_pair("", tmp));
      }
      args.add_child("replicas", replicas);
    }
    args.put("port", port);
    args.put("distinct_keys", n_distinct_keys);
    args.put("host_memory", host_memory);
    args.put("max_value_size", max_value_size);
#if BENCHMARKING
    args.put("key_size", key_size);
    args.put("value_size", value_size);
    args.put("n_cmds", n_cmds);
    args.put("n_keys", n_keys);
    args.put("ratio", permille_reads);
#endif //BENCHMARKING
    stats.add("arguments", args);
  }
#endif //COLLECT_STATS

  void dump_arguments() {
    fmt::print(
        "{1: <{0}}{2}\n"
        "{3: <{0}}{4}\n"
        "{5: <{0}}{6}\n"
        "{7: <{0}}{8}\n"
        "{9: <{0}}{10}\n"
        , 25
        , "Number of Threads:"
        , n_thread
        , "Server URI:"
        , uri
        , "Phys Port Nr:"
        , port
        , "Host Memory Size:"
        , host_memory
        , "Max Value Size:"
        , max_value_size
    );

    for (auto i = 0ULL; i < replica_info.size(); ++i) {
      fmt::memory_buffer buf;
      format_to(buf, "Replica info[{}]:", i); 
      fmt::print("{0: <{1}}{2}\n", to_string(buf), 25, replica_info[i]);
    }

#if BENCHMARKING
    fmt::print(
        "\n"
        "Benchmark flags:\n"
        "{1: <{0}}{2}\n"
        "{3: <{0}}{4}\n"
        "{5: <{0}}{6}\n"
        "{7: <{0}}{8}\n"
        "{9: <{0}}{10}\n"
        "{11: <{0}}{12}\n\n"
        , 25
        , "Key Size:"
        , key_size
        , "Value Size:"
        , value_size
        , "Number of Commands:"
        , n_cmds
        , "Number of Distinct Keys:"
        , n_keys
        , "Read ratio:"
        , permille_reads/1000.0
        , "Result Output:"
        , output
    );
#endif //BENCHMARKING
  }

  Args(std::map<std::string, docopt::value> args) {
    n_thread = args["-n"].asLong();
    uri = args["-u"].asString();
    auto replicas = args["-a"].asStringList();
    replica_info.reserve(replicas.size());
    for (auto const & r : replicas) {
      replica_info.emplace_back(r);
    }
    port = args["-p"].asLong();
    n_distinct_keys = args["--number-of-distinct-keys"].asLong();
    host_memory = args["--host-memory"].asLong();
    max_value_size = args["--max-value-size"].asLong();
    if (host_memory == 0) {
      host_memory = n_distinct_keys * (max_value_size + sizeof(ABD::KV_store));
      host_memory *= 2;
    }
#if BENCHMARKING
    fmt::print("Avocado is compiled for benchmarking, please recompile it without BENCHMARKING flag if you want to use it for something else.\n");
    value_size = args["--value_size"].asLong();
    n_cmds = args["--n_cmds"].asLong();
    n_keys = args["--n_keys"].asLong();
    permille_reads = args["--permille_reads"].asLong();
    if (args["--output"]) {
      output = args["--output"].asString();
    }
#endif //BENCHMARKING

    if (args["--dump-config"].asBool()) {
      dump_arguments();
    }
  }

};

std::ostream & operator<<(std::ostream & os, Args const & args) {
  os << "n_threads(size_t:" << args.n_thread << ") uri(std::string:" << args.uri << ") ";
  for (auto i = 0ULL; i < args.replica_info.size(); ++i) {
    os << "replica_uri[" << i << "](std::string:" << args.replica_info[i] << ") ";
  }
  os << "port(size_t:" << args.port << ") ";
  os << "host_memory(size_t:" << args.host_memory << ") ";
  os << "max_value_size(size_t:" << args.max_value_size << ") ";
#if BENCHMARKING
  os << "key_size(size_t:" << args.key_size << ") value_size(size_t:" << args.value_size << ") n_cmds(size_t:" << args.n_cmds << ") n_keys(size_t:"
    << args.n_keys << ") permille_reads(size_t:" << args.permille_reads << ") ";
#endif //BENCHMARKING
  return os;
}

template<int CALL>
void register_functions_(erpc::Nexus & nexus);

template<>
void register_functions_<ABD::ReqType::EndReq>(erpc::Nexus &) {}

template<int CALL>
void register_functions_(erpc::Nexus & nexus) {
  nexus.register_req_func(CALL, ABD::request<CALL>);
  register_functions_<CALL + 1>(nexus);
}

void register_functions(erpc::Nexus & nexus) {
  register_functions_<0>(nexus);
}

static std::string get_uri_from_server_info(std::string const & svr_info) {
  return svr_info.substr(0, svr_info.rfind(':'));
}

static size_t get_id_from_addr(std::string const & uri) {
  if constexpr (0) {
    auto fpos = uri.find(':');
    auto rpos = uri.rfind('.', fpos);
    return std::atoll(uri.c_str() + rpos);
  }
  return std::atoll(uri.c_str() + uri.rfind('.', uri.find(':')) + 1);
}

static size_t get_n_threads_from_svr_info(std::string const & svr_info) {
  return get_id_from_addr(svr_info);
}

#if 0
static size_t get_id_from_server_info(std::string const & svr_info) {
  return get_id_from_addr(get_uri_from_server_info(svr_info));
}
#endif //0

static size_t get_id_from_addr(Args const & args) {
  return get_id_from_addr(args.uri);
}

static size_t constexpr get_session_id_from_id(size_t const id) noexcept {
  return id << 24;
}

template<class T>
static size_t get_session_id_from_addr(T const & arg) {
  return get_session_id_from_id(get_id_from_addr(arg));
}

namespace {
  void sm_handler([[maybe_unused]] int a, erpc::SmEventType evtT, erpc::SmErrType errT, void * handle) {
    if (evtT == erpc::SmEventType::kConnected && errT == erpc::SmErrType::kNoError) {
      ABD * abd = reinterpret_cast<avocado::SvrContext *>(handle)->parent;
      abd->connected.fetch_add(1, std::memory_order_relaxed);
    }
  }
}


#if !BENCHMARKING

using Thread_info = std::thread;

void join(Thread_info & t) {
  t.join();
}

void server_thread(erpc::Nexus *, size_t, size_t, ABD::KV_store *, size_t, Args const &, PacketSsl);

void run_event_loop(erpc::Rpc<erpc::CTransport> & rpc) {
  for(;;) {
    rpc.run_event_loop(100000);
  }
}

void create_threads(std::vector<Thread_info> & threads, erpc::Nexus * nexus, ABD::KV_store * store, size_t base_id, Args const & args, PacketSsl & cipher) {
  auto i{0ULL};
  std::generate_n(threads.begin(), args.n_thread, [&] -> std::thread {
      return std::thread(server_thread, nexus, base_id, i++, store, args.port, args, cipher);
  });
}

void write_result(std::vector<Thread_info> &) {}

#else //!BENCHMARKING

using Thread_info = std::pair<std::thread, std::future<long double>>;

void join(Thread_info & t) {
  t.first.join();
}

//using Traces = std::vector<::avocado::Trace_cmd>;
using Traces = ::avocado::Trace_ptr;
using ::avocado::Trace_cmd;

size_t n_put_ret{0};
size_t n_get_ret{0};
std::atomic<size_t> n_skipped_ret{0};

Traces traces;// = ::avocado::trace_init(0, n_cmds, 10);
size_t trace_size;

#if SCALA
std::vector<std::tuple<bool, size_t, size_t>> replica_order;
#endif

void server_thread(erpc::Nexus *, size_t, size_t, ABD::KV_store *, size_t, Args const &, PacketSsl, Trace_cmd * begin, Trace_cmd * end, std::promise<long double> && result);

void put_ret([[maybe_unused]] uint8_t const * buf, [[maybe_unused]] size_t const size) {
  ++n_put_ret;
  rate.release();
  //assert(strncmp("ACK", reinterpret_cast<char const *>(buf), size) == 0);
}

void get_ret([[maybe_unused]] uint8_t const * buf, [[maybe_unused]] size_t const size) {
  ++n_get_ret;
  rate.release();
  //assert(strncmp("ACK", reinterpret_cast<char const *>(buf), size) == 0);
}

void skip_ret([[maybe_unused]] uint8_t const * buf, [[maybe_unused]] size_t const size) {
    n_skipped_ret.fetch_add(1, std::memory_order_relaxed);
    rate.release();
}


std::atomic<size_t> others_ack_d{0};
std::atomic<size_t> others_finished_d{0};
std::atomic<size_t> self_finished{0};

long double run_event_loop(erpc::Rpc<erpc::CTransport> & rpc, ABD & abd, Args const & args, Trace_cmd * begin, Trace_cmd * end) {
  assert(begin != end);
  size_t other_finished{0};
  size_t others = args.replica_info.size();
  abd.put_ret = put_ret;
  abd.get_ret = get_ret;
  abd.skip_ret = skip_ret;
  abd.cmd = [&other_finished] (auto & msg) -> ABD::Msg {
    auto key = msg.get_key();
    if (strncmp("DONE", reinterpret_cast<char const *>(key.buf), key.size) == 0) {
      auto tmp = others_finished_d.fetch_add(1) + 1;
      fmt::print("OTHER FINISHED: {}\n", tmp);
      ++other_finished;
      ABD::MsgBuilder builder(sizeof("ACK") + 1);
      builder.add_key("ACK", sizeof("ACK"));
      return builder.finish();
    }
    ABD::MsgBuilder builder(sizeof("ERR") + 1);
    builder.add_key("ERR", sizeof("ERR"));
    return builder.finish();
  };

  size_t others_ack{0};
  abd.cmd_ret = [&others_ack] (size_t n_resp, auto & msg) -> void {
    others_ack = n_resp;
    auto tmp = others_ack_d.fetch_add(1) + 1;
    auto key = msg.get_key();
    if (strncmp("ACK", reinterpret_cast<char const *>(key.buf), key.size) == 0) {
      fmt::print("OTHER ACK {}\n", tmp);
      return;
    }
    fmt::print("OTHER ERR\n");
  };

  abd.init();
#if defined(SCALA)
  abd.replica_ordering(replica_order);
#endif

  //auto val = std::make_unique<uint8_t[]>(args.value_size);

  timeval start_time, end_time;
  
  fmt::print("BEGIN\n");
  gettimeofday(&start_time, nullptr);
  while (begin != end) {
    rate.aquire([&rpc](){ rpc.run_event_loop_once(); });
    auto msg = ABD::get_msg(args.key_size, args.value_size);
    auto ptr = msg.get_ptr();
    auto & trace = *begin;
    std::copy(trace.key_hash, trace.key_hash + trace.key_size, ptr->buf);
    if (trace.op == ::avocado::Trace_cmd::Put) {
      abd.benchmark_put(std::move(msg));
    } else {
      abd.benchmark_get(std::move(msg));
    }
    ++begin;
    //TODO run more then once...
    rpc.run_event_loop_once();
  }

  while (abd.load_traces() > 0) {
    for (auto i = 0; i < 10; ++i) {
      rpc.run_event_loop_once();
    }
  }

  abd.send_cmd("DONE");
  auto selff = self_finished.fetch_add(1) + 1;
  fmt::print("SELF: DONE {}\n", selff);

  while (other_finished < others || others_ack < others) {
    rpc.run_event_loop_once();
  }
  rpc.run_event_loop_once();
  gettimeofday(&end_time, nullptr);
  rpc.run_event_loop(10);

  long double x = (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_usec - start_time.tv_usec) / 1000000.L;
  return x;
}

void create_threads(std::vector<Thread_info> & threads
                   , erpc::Nexus * nexus
                   , ABD::KV_store * store
                   , size_t base_id
                   , Args const & args
                   , PacketSsl & cipher
                   ) {
  auto step_size = args.n_cmds / args.n_thread;
  auto max = traces.get() + trace_size;
  for (auto i = 0ULL; i < args.n_thread; ++i) {
    auto begin = traces.get();
    std::advance(begin, i * step_size);
    auto end = begin;
    std::advance(end, step_size);
    end = end > max ? max : end;
    std::promise<long double> p;
    auto f = p.get_future();
    //auto t = std::thread(server_thread, nexus, base_id, i, store, args.port, args, cipher, begin, end, std::move(p));
    threads[i] = Thread_info(std::thread(server_thread, nexus, base_id, i, store, args.port, args, cipher, begin, end, std::move(p)), std::move(f));
    fmt::print("Thread created {}\n", i);
  }
}

void write_result(std::vector<Thread_info> & info) {
  using ptree = boost::property_tree::ptree;
  ptree child;
  for (auto & [_, f] : info) {
    ptree tmp;
    tmp.put("", f.get());
    child.push_back(std::make_pair("", tmp));
  }
  stats.add("elapsed", child);
  stats.put("skipped", n_skipped_ret.load());
}

#endif //!BENCMARKING

void server_thread(erpc::Nexus * nexus
                  , size_t base_id
                  , size_t local_id
                  , ABD::KV_store * store
                  , size_t port
                  , Args const & args
                  , PacketSsl cipher
#if BENCHMARKING
                  , Trace_cmd * begin
                  , Trace_cmd * end
                  , std::promise<long double> && result
#endif //BENCHMARKING
                  ) {
  std::vector<Replica_info> replicas;
  replicas.reserve(args.replica_info.size());
  for (auto const & r_info : args.replica_info) {
    replicas.emplace_back(Replica_info{0, local_id % get_n_threads_from_svr_info(r_info), get_uri_from_server_info(r_info)});
  }

  ABD abd(args.value_size, base_id, *store, std::move(replicas), local_id, cipher);
  auto svr_context = abd.create_svr_context();
  erpc::Rpc<erpc::CTransport> rpc(nexus, &svr_context, local_id, sm_handler, port);
  rpc.retry_connect_on_invalid_rpc_id = true;
  abd.rpc = &rpc;
  svr_context.rpc = reinterpret_cast<Rpc_wrapper *>(&rpc);
  

#if !BENCHMARKING
  abd.init();
  run_event_loop(rpc);
#else //!BENCHMARKING
  result.set_value(run_event_loop(rpc, abd, args, begin, end));
//alloc_max(args.n_thread);
#if 0
  for (;;) {
    rpc.run_event_loop_once();
  }
#endif
#endif //BENCHMARKING
}

#if 0
#include "openssl/crypto.h"
#endif //0

std::vector<std::tuple<bool, size_t, size_t>> permutations(Args & args) {
  std::sort(args.replica_info.begin(), args.replica_info.end());
  std::vector<std::tuple<int64_t, std::string>> tmp;
  tmp.emplace_back(-1, args.uri);
  {
    int64_t i = 0;
    for (auto const & r_info : args.replica_info) {
      tmp.emplace_back(i, r_info);
      ++i;
    }
  }
  struct {
        bool operator()(std::tuple<int64_t, std::string> const & lhs, std::tuple<int64_t, std::string> const & rhs) const {
          return std::get<1>(lhs).compare(std::get<1>(rhs)) < 0;
        }
  } customLess;
  std::sort(tmp.begin(), tmp.end(), customLess);
  for (auto const & [i, r] : tmp) {
    fmt::print("{} {}\n", i, r);
  }
  std::vector<std::tuple<bool, size_t, size_t>> res;
  //TODO make it configurable
  for (auto i = 0ULL; i < tmp.size(); ++i) {
    for (auto j = i + 1; j < tmp.size(); ++j) {
      for (auto k = j + 1; k < tmp.size(); ++k) {
        auto a = std::get<0>(tmp[i]);
        auto b = std::get<0>(tmp[j]);
        auto c = std::get<0>(tmp[k]);
        if (a == -1) {
          res.push_back(std::make_tuple(true, std::min(b, c), std::max(b, c)));
          continue;
        }
        if (b == -1) {
          res.push_back(std::make_tuple(true, std::min(a, c), std::max(a, c)));
          continue;
        }
        if (c == -1) {
          res.push_back(std::make_tuple(true, std::min(a, b), std::max(a, b)));
          continue;
        }
        res.push_back(std::make_tuple(false, std::min(a, b), std::max(a, b)));
      }
    }
  }
  for (auto const & [t, a, b] : res) {
    fmt::print("{}   {}  {}\n", t, args.replica_info[a], args.replica_info[b]);
  }
  return res;
}

int main(int argc, char ** argv) {
  Name_vers = Name_vers + version() + ")";
  auto usage = Name_vers + USAGE;
  auto doc_args = docopt::docopt(usage, {argv + 1, argv + argc}, true, Name_vers);
#if 0
  {
    unsigned long * opensslflags = OPENSSL_ia32cap_loc();
    *opensslflags |= (1UL << 19) | (1UL << 23) | (1UL << 24) | (1UL << 25) | (1UL << 26) | (1UL << 41) | (1UL << 57) | (1UL << 60);
  }
#endif //0
  //find_and_replace_all(usage, replace_name, argv[0]);
  Args args(doc_args);
  args.dump();
  auto base_id = get_id_from_addr(args);
#if BENCHMARKING
#if SCALA
  replica_order = permutations(args);
#endif
  if (!args.output.empty()) {
    stats.change_file_name(args.output);
  }
  trace_size = args.n_cmds;
//traces = ::avocado::trace_init(0, args.n_cmds, args.n_keys, args.permille_reads, base_id);
  traces = ::avocado::trace_init_zipfian(0, args.n_cmds, args.n_keys, args.permille_reads, base_id, 1.0);
#endif //BENCHMARKING
  fmt::print("NEXUS\n");
  erpc::Nexus nexus(args.uri, 0, 0);

  fmt::print("REGISTER\n");
  register_functions(nexus);

  fmt::print("Alloc\n");
  auto alloc = create_allocator(args.host_memory, args.max_value_size, true);
  if (alloc == nullptr) {
    return -1;
  }
  uint8_t key[] = {0x0, 0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8,0x9,0xa,0xb,0xc,0xd,0xe,0xf};
  CipherSsl crypt(key, key); 
  PacketSsl packet = PacketSsl::create(crypt);
  fmt::print("Store\n");
  ABD::KV_store store(std::move(crypt), std::move(alloc));

  std::vector<Thread_info> threads(args.n_thread);

  fmt::print("THREADS\n");
  create_threads(threads, &nexus, &store, base_id, args, packet);


  fmt::print("JOIN\n");
  for (auto & t : threads) {
    join(t);
  }

  write_result(threads);

  return 0;
}
