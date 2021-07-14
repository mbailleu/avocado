#include "crypt/encrypt_package.h"

#include <string>
#include <iostream>

#include "package_test_print.h"

int main() {
  uint8_t key[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
  auto cipher = PacketSsl::create(reinterpret_cast<std::byte*>(key), reinterpret_cast<std::byte*>(key));

  std::string msg = "Hello World! How are you?";
  auto buf_size = PacketSsl::get_buffer_size(msg.size() + 1);
  std::byte * buf = new std::byte[buf_size];
  assert(cipher.encrypt(buf, msg.c_str(), msg.size() + 1));
  print_buffer("test:", buf, buf_size);
  char de_msg[PacketSsl::get_message_size(buf_size)];
  assert(cipher.decrypt(de_msg, buf, buf_size));
  std::cout << de_msg << '\n';
}
