#pragma once

#include <memory>
#include "meta_data.h"

#include "../slab_allocator/allocator.h"

#include "crypt/encrypt_openssl.h"

#include <folly/ConcurrentSkipList.h>

namespace avocado {

class KV_store {
  using Skip_list = folly::ConcurrentSkipList<VarKeySizeNode>;
  using Accessor = Skip_list::Accessor;
  using Allocator = slab::UniquePtrWrap<slab::DynamicLockLessAllocator>;

  CipherSsl cipher;
  std::shared_ptr<Allocator> alloc;
  std::shared_ptr<Skip_list> skip_list;
  
public:

  using Node = VarKeySizeNode;

  struct Ret_value {
    Lamport lamport{Lamport::min()};
    size_t size;
    std::unique_ptr<uint8_t[]> value;
  };

  KV_store(CipherSsl && cipher, std::shared_ptr<Allocator> && alloc) : cipher(cipher), alloc(alloc), skip_list(Skip_list::createInstance(12)) {}

  Node create_node(size_t key_size, uint8_t const * key, size_t value_size, uint8_t const * value, Lamport clock) {
    return Node(key_size, key, std::make_shared<MetaData>(cipher, value_size, value, clock, alloc->alloc()));
  }

  Ret_value decrypt_node(Node const & node) const;

  bool put(Node & node);
  bool put(size_t key_size, uint8_t const * key, size_t value_size, uint8_t const * value, Lamport clock);
  Ret_value get(size_t const key_size, uint8_t const * key) const;
  Ret_value get(Node const & node) const;

};

} //avocado
