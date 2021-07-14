#pragma once

#include <cstddef>
#include <cstdint>

#include "crypt/encrypt_package.h"

namespace avocado {
  struct CryptContext {
    PacketSsl const & cipher;

    CryptContext(PacketSsl const & cipher) : cipher(cipher) {}

    bool decrypt(uint8_t * dst, uint8_t const * src, size_t n) const {
      return cipher.decrypt(dst, src, n);
    }

    bool encrypt(uint8_t * dst, uint8_t const * src, size_t n) const {
      return cipher.encrypt(dst, src, n);
    }

    [[gnu::pure, nodiscard]]
    static constexpr size_t get_message_size(size_t n) noexcept { return PacketSsl::get_message_size(n); }
    [[gnu::pure, nodiscard]]
    static constexpr size_t get_buffer_size(size_t n) noexcept { return PacketSsl::get_buffer_size(n); }
  };
} //avocado
