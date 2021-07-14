#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>

#include <cassert>

#include "lamport.h"

namespace avocado {

struct KeyAccess {
  [[nodiscard]] static bool equal(uint8_t const * const lhs, uint8_t const * const rhs, size_t const size) noexcept {
    return !memcmp(lhs, rhs, size - sizeof(Lamport));
  }

  [[nodiscard]] static constexpr size_t adjust_size(size_t const size) noexcept {
    return size - meta_size();
  }

  [[nodiscard]] static constexpr uint8_t const * adjust_key_buf(uint8_t const * const key) noexcept {
    return key;
  }

  [[nodiscard]] static Lamport get_lamport(size_t const size, uint8_t * const key) noexcept {
    assert(size >= meta_size());
    if (key != nullptr)
      return *reinterpret_cast<Lamport *>(key + adjust_size(size));
    return Lamport(0,~0);
  }

  [[nodiscard]] static constexpr size_t add_meta_size(size_t const size) noexcept {
    return size + meta_size();
  }

  [[nodiscard]] static constexpr size_t sub_meta_size(size_t const size) noexcept {
    return size - meta_size();
  }

  [[nodiscard]] static constexpr size_t meta_size() noexcept {
    return sizeof(Lamport);
  }
};

} //namespace avocado
