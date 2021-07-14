#pragma once

#include <memory>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "../slab_allocator/allocator.h"
#include "../protocol/abd/lamport.h"

#include "crypt/encrypt_openssl.h"

namespace avocado {

struct MetaData {
  using Ptr_t = slab::UniquePtrWrap<slab::DynamicLockLessAllocator>::Ptr_t;
  uint8_t mac[CipherSsl::MacSize];
  size_t  data_size;
  Lamport clock;
  Ptr_t data;

  MetaData() = default;

  MetaData(CipherSsl const & cipher, size_t data_size, uint8_t const * plain_value, Lamport clock, Ptr_t && data) : data_size(data_size), clock(clock), data(std::move(data)) {
    cipher.encrypt(reinterpret_cast<uint8_t*>(this->data.get()), plain_value, data_size, mac); 
  }
};

template<int key_size>
struct Node {
  char key[key_size];
  std::shared_ptr<MetaData> meta;

  Node() = default;
  Node(uint8_t const * key) {
    std::memcpy(this->key, key, key_size);
  }

  Node(uint8_t const * key, std::shared_ptr<MetaData> & meta) : meta(meta) {
    std::memcpy(this->key, key, key_size);
  }

  bool operator<(Node const & other) const {
    return std::memcmp(key, other.key, key_size) < 0;
  }
};

struct VarKeySizeNode {
  size_t key_size;
  std::unique_ptr<char[]> key;
  std::shared_ptr<MetaData> meta;

  VarKeySizeNode() = default;
  VarKeySizeNode(VarKeySizeNode &&) = default;
  VarKeySizeNode & operator=(VarKeySizeNode &&) = default;

  VarKeySizeNode(VarKeySizeNode const & other) : key_size(other.key_size), key(std::make_unique<char[]>(other.key_size)), meta(other.meta) {
    std::memcpy(key.get(), other.key.get(), key_size);
  }
  VarKeySizeNode(size_t key_size, uint8_t const * key) : key_size(key_size), key(std::make_unique<char[]>(key_size)) {
    std::memcpy(this->key.get(), key, key_size);
  }

  VarKeySizeNode(size_t key_size, uint8_t const * key, std::shared_ptr<MetaData> && meta) : key_size(key_size), key(std::make_unique<char[]>(key_size)), meta(meta) {
    std::memcpy(this->key.get(), key, key_size);
  }

  bool operator<(VarKeySizeNode const & other) const {
    if (key_size < other.key_size) return true;
    if (key_size > other.key_size) return false;
    return std::memcmp(key.get(), other.key.get(), key_size) < 0;
  }
};

} //avocado
