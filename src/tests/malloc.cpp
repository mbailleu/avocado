#include <cstdint>
#include <vector>
#include <iostream>
#include <cstdlib>
#include <cstddef>
#include <memory>
#include <thread>

#include "crypt/encrypt_package.h"

#include "../generate_traces.h"

constexpr size_t n_cmds = 500000;
constexpr size_t n_keys = 1000;
constexpr size_t n_threads = 8;
constexpr size_t key_size = 8;
constexpr size_t meta_size = 16;
constexpr size_t value_size = 1024;
constexpr size_t msg_size = key_size + meta_size + value_size;


using Traces = std::vector<::avocado::Trace_cmd>;

Traces traces;

void loop(PacketSsl cipher, Traces::iterator begin, Traces::iterator end) {
  assert(begin != end);
  while (begin != end) {
    auto msg = std::make_unique<uint8_t[]>(msg_size);
    auto & trace = *begin;
    std::copy(&key_size, (&key_size) + sizeof(key_size), msg.get());
    std::copy(&value_size, (&value_size) + sizeof(value_size), msg.get() + sizeof(key_size));
    std::copy(trace.key_hash, trace.key_hash + key_size, msg.get() + sizeof(key_size) + sizeof(value_size));
    auto buf = std::make_unique<uint8_t[]>(cipher.get_buffer_size(msg_size));
    cipher.encrypt(buf.get(), msg.get(), msg_size);
    auto new_msg = std::make_unique<uint8_t[]>(cipher.get_message_size(cipher.get_buffer_size(msg_size)));
    if (!cipher.decrypt(new_msg.get(), buf.get(), cipher.get_buffer_size(msg_size))) {
      std::cerr << "Decryption failed\n";
    }
    if (memcmp(msg.get(), new_msg.get(), msg_size) != 0) {
      std::cerr << "Message does not match\n";
    }
    ++begin;
  }
}

void create_threads(std::vector<std::thread> & threads, PacketSsl & packet) {
  auto step_size = n_cmds / n_threads;
  for (auto i = 0ULL; i < n_threads; ++i) {
    auto begin = traces.begin();
    std::advance(begin, i * step_size);
    auto end = begin;
    std::advance(end, step_size);
    threads[i] = std::thread(loop, packet, begin, end);
  }
}

int main() {

  uint8_t key[] = {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf};
  CipherSsl crypt(key, key);
  PacketSsl packet = PacketSsl::create(crypt);
  traces = ::avocado::trace_init(0, n_cmds, n_keys, 800, 54);
  
  std::vector<std::thread> threads(n_threads);
  create_threads(threads, packet);

  for (auto & t : threads) {
    t.join();
  }

}
