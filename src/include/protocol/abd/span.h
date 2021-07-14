#pragma once

#include <cstdint>
#include <cstddef>

namespace avocado {

template<class NET_MSG, class KV_BUF>
struct Span {
  size_t size;
  uint8_t * buf;

  Span(NET_MSG const & msg) : size(msg.get_data_size()), buf(msg.buf) {}
  Span(KV_BUF const & msg) : size(msg.size), buf(msg.value.get()) {}
  Span(size_t size, uint8_t * buf) : size(size), buf(buf) {}
};

} //avocado
