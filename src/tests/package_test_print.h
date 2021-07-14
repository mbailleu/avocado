#include <iostream>
#include "crypt/encrypt_package.h"

void _print_buffer(std::byte * buf, size_t buf_size) { 
  for (auto i = 0ULL; i < buf_size; ++i) {
    uint8_t out;
    memcpy(&out, &buf[i], sizeof(out));
    if (out < 0x10) {
      std::cout << '0';
    }
    std::cout << +out;
  }
}

void print_buffer(std::string const & prefix, std::byte * buf, size_t buf_size) {
  std::cout << prefix <<   ":\nsize: " << std::dec << buf_size << std::hex << "\nIV:  0x";
  _print_buffer(buf, PacketSsl::IvSize);
  std::cout << "\nMAC: 0x";
  _print_buffer(buf + (buf_size - (PacketSsl::MacSize)), PacketSsl::MacSize);
  std::cout << "\nBuf: 0x";
  _print_buffer(buf + PacketSsl::IvSize, PacketSsl::get_message_size(buf_size));
  std::cout << '\n';
}
