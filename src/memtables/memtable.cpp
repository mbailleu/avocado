#include "memtable/memtable.h"

#include <memory>

#include "memtable/meta_data.h"

namespace avocado {

bool KV_store::put(VarKeySizeNode & new_node) {
  Accessor kv(skip_list);
  auto [node, found] = kv.addOrGetData(new_node);
  if (found) { //There was no node with the same key, we are done here
    return true;
  }
  auto clock = new_node.meta->clock;
  auto old_meta = std::atomic_load_explicit(&(node->meta), std::memory_order_relaxed);
  while (old_meta->clock < clock && !std::atomic_compare_exchange_weak_explicit(&(node->meta), &old_meta, new_node.meta, std::memory_order_release, std::memory_order_relaxed)) {}
  return old_meta->clock < clock;
}

bool KV_store::put(size_t key_size, uint8_t const * key, size_t value_size, uint8_t const * value, Lamport clock) {
  auto node = create_node(key_size, key, value_size, value, clock);
  return put(node);
}

KV_store::Ret_value KV_store::decrypt_node(Node const & node) const {
  auto meta = std::atomic_load_explicit(&(node.meta), std::memory_order_relaxed);
  auto ptr = std::make_unique<uint8_t[]>(meta->data_size);
  if (!cipher.decrypt(ptr.get(), reinterpret_cast<uint8_t*>(meta->data.get()), meta->data_size, meta->mac)) {
    return {Lamport{0}, 0, nullptr};
  }
  return {meta->clock, meta->data_size, std::move(ptr)};
}

KV_store::Ret_value KV_store::get(Node const & node) const {
  Accessor kv(skip_list);
  auto iter = kv.find(node);
  if (iter == kv.end()) {
    return {Lamport{0}, 0, nullptr};
  }

  return decrypt_node(*iter);
}

KV_store::Ret_value KV_store::get(size_t const key_size, uint8_t const * key) const {
  return get(Node(key_size, key));
}

} //avocado
