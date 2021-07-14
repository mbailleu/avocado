#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include "debug_print.h"

namespace avocado {

template<class Span>
class Msg {
public:
  struct Encoded_msg {
    size_t key_size;
    size_t size;
//Flexiable array size member
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
    uint8_t buf[];
#pragma GCC diagnostic pop
  };
  std::unique_ptr<uint8_t[]> ptr;

  inline Encoded_msg * get_ptr() const noexcept {
    return reinterpret_cast<Encoded_msg*>(ptr.get());
  }

public:

  Msg(std::unique_ptr<uint8_t[]> && ptr) : ptr(std::move(ptr)) {}
  Msg(size_t size) : ptr(std::make_unique<uint8_t[]>(size)) {}
  Msg() = default;

  uint8_t * buf() const noexcept { return ptr.get(); }
  
  size_t get_key_size() const noexcept {
    auto ptr = get_ptr();
    return ptr->key_size;
  }

  size_t get_value_size() const noexcept {
    auto ptr = get_ptr();
    if (ptr->size > ptr->key_size) {
      return ptr->size - ptr->key_size;
    }
    return 0;
  }

  static constexpr size_t adjust_size(size_t size) {
    return size + sizeof(Encoded_msg);
  }

  size_t get_size() const noexcept {
    auto ptr = get_ptr();
    return ptr->size;
  }

  Span get_key() const noexcept {
    auto ptr = get_ptr();
    return {ptr->key_size, static_cast<uint8_t *>(ptr->buf)};
  }

  Span get_value() const noexcept {
    auto ptr = get_ptr();
    if (ptr->size > ptr->key_size) {
      return {ptr->size - ptr->key_size, static_cast<uint8_t *>(ptr->buf) + ptr->key_size};
    }
    return {0, nullptr};
  }

  bool has_value() const noexcept {
    auto ptr = get_ptr();
    return ptr->size > ptr->key_size;
  }
};

template<class Span>
class MsgBuilder {
  Msg<Span> msg;

public:
  MsgBuilder(size_t max_size) : msg(msg.adjust_size(max_size)) {
    auto ptr = msg.get_ptr();
    ptr->key_size = 0;
    ptr->size = 0;
  }

  template<class T>
  void add_key(T const * key, size_t size = 1) {
    size *= sizeof(T);
    auto ptr = msg.get_ptr();
    memcpy(ptr->buf + ptr->key_size, key, size);
    ptr->key_size += size;
    ptr->size += size;
  }

  template<class T>
  void add_value(T const * buf, size_t size = 1) {
    size *= sizeof(T);
    auto ptr = msg.get_ptr();
    memcpy(ptr->buf + ptr->size, buf, size);
    ptr->size += size;
  }

  Msg<Span> finish() {
    return std::move(msg);
  }

};

#if !defined(NDEBUG)

#include <iosfwd>

template<class T>
std::ostream & operator<<(std::ostream & os, Msg<T> const & obj) {
  auto ptr = obj.get_ptr();
  return os << "Msg::key_size(" << ptr->key_size << ") Msg::size(" << ptr->size << ") Msg::buf(" << static_cast<void const *>(ptr->buf) << ')';
}

#endif

} //avocado
