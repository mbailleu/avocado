#include <fmt/format.h>
#include <thread>
#include <vector>
#include <memory>

#include "memtable/allocator.h"
#include "memtable/memtable.h"
#include "crypt/encrypt_package.h"
#include "../generate_traces.h"

using namespace avocado;

static int 	kNumThreads 	= 8;
static size_t 	kValueSize	= 16;
static std::string kValue	= "generateFormula";

using Traces = std::unique_ptr<::avocado::Trace_cmd, ::avocado::unmap_trace>;

Traces traces;

class thread_args {
	public:
	thread_args(int id, avocado::KV_store* kv_ptr, Lamport c, Traces::pointer b, Traces::pointer e)
		: thread_id(id), store(kv_ptr), clock(c), begin(b), end(e) {
			assert(id < kNumThreads);
			assert(kv_ptr != nullptr);
		}
	int thread_id;
	avocado::KV_store *store = nullptr;
	Lamport clock;
	Traces::pointer begin;
	Traces::pointer end;
};

void thread_func(void* _args) {
	auto args = static_cast<class thread_args*>(_args);
  fmt::print("Thread {}\n", args->thread_id);
	while (args->begin != args->end) {
		if (args->begin->op == Trace_cmd::Put) {
			const uint8_t* ptr = reinterpret_cast<const uint8_t*>(args->begin->key_hash);
			bool ok = args->store->put(args->begin->key_size, ptr, kValueSize, reinterpret_cast<const uint8_t*>(kValue.c_str()), args->clock);
		}
		else {
			const uint8_t* ptr = reinterpret_cast<const uint8_t*>(args->begin->key_hash);
			KV_store::Ret_value ret = args->store->get(args->begin->key_size, ptr);
      fmt::print("{}\n", ret.value.get());
		}
		args->begin++;
	}
	return;
}

int main(int argc, char ** argv) {
	traces = ::avocado::trace_init(0, 1000, 2, 50, 1);

	auto alloc = create_allocator(1073741824, kValueSize, true);
	if (alloc == nullptr) {
		return -1;
	}
	uint8_t key[] = {0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8,0x9,0xa,0xb,0xc,0xd,0xe,0xf};
	CipherSsl crypt(key, key);
	PacketSsl packet = PacketSsl::create(crypt);
	avocado::KV_store store(std::move(crypt), std::move(alloc));
	Lamport clock(10);
	std::vector<std::thread> threads(kNumThreads);
	std::vector<std::unique_ptr<class thread_args>> arguments(kNumThreads);
	auto step_size = 1000/kNumThreads;
	for (int i = 0; i < kNumThreads; i++) {
		auto begin = traces.get();
		std::advance(begin, i * step_size);
		auto end = begin;
		std::advance(end, step_size);
		arguments[i] = std::make_unique<class thread_args>(i, &store, clock, begin, end);
		threads[i] = std::thread(thread_func, arguments[i].get());
	}
	for (auto & t : threads) {
		t.join();   
	}
	return 0;
}
