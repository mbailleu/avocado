#include "crypt/encrypt_package.h"

#include <string>
#include <iostream>
#include <cassert>

#include "package_test_print.h"

int main() {
#if 1
  uint8_t key[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
  auto cipher_stats = PacketSsl::create(reinterpret_cast<std::byte*>(key), reinterpret_cast<std::byte*>(key));

  auto cipher = stats_::PacketSsl::create(reinterpret_cast<std::byte*>(key), reinterpret_cast<std::byte*>(key));
  
  std::string msg = "Hello World! How are you?";
  auto buf_stats_size = PacketSsl::get_buffer_size(msg.size() + 1);
  auto buf_size = stats_::PacketSsl::get_buffer_size(msg.size() + 1);
  std::byte * buf_stats = new std::byte[buf_stats_size];
  std::byte * buf = new std::byte[buf_size];
  assert(cipher_stats.encrypt(buf_stats, msg.c_str(), msg.size() + 1));
  assert(cipher.encrypt(buf, msg.c_str(), msg.size() + 1));
  print_buffer("Stats", buf_stats, buf_stats_size);
  print_buffer("normal", buf, buf_size);
  assert(buf_size == buf_stats_size);
  assert(!memcmp(buf, buf_stats, buf_size));
  char de_msg[stats_::PacketSsl::get_message_size(buf_size)];
  char de_msg_stats[PacketSsl::get_message_size(buf_stats_size)];
  assert(cipher.decrypt(de_msg, buf, buf_size));
  assert(cipher_stats.decrypt(de_msg_stats, buf_stats, buf_stats_size));
  std::cout << "Normal: " << de_msg << "\nStats: " << de_msg_stats << '\n';
#endif
}
