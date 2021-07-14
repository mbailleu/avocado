#include <memory>
#include <iostream>

size_t requested_size = 0;
size_t n_request = 0;

template<class T>
class SharedPtrAllocator {
public:
  using value_type = T;

  SharedPtrAllocator() noexcept = default;

  template<class U>
  SharedPtrAllocator(SharedPtrAllocator<U> const &) noexcept {};

  value_type * allocate(std::size_t n) {
    n_request = n;
    requested_size = n * sizeof(value_type);
    return static_cast<value_type *>(::operator new (requested_size));
  }

  void deallocate(value_type * p, std::size_t) noexcept {
    ::operator delete(p);
  }
};

template<class T, class U>
bool operator==(SharedPtrAllocator<T> const &, SharedPtrAllocator<U> const &) noexcept { return true; }

template<class T, class U>
bool operator!=(SharedPtrAllocator<T> const & a, SharedPtrAllocator<U> const & b) noexcept { return !(a == b); }

struct S {
  uint64_t a[16];
};

int main() {
  SharedPtrAllocator<S> alloc;
  auto ptr = std::allocate_shared<S>(alloc);
  std::cout <<
    "#pragma once\n"
    "\n"
    "static constexpr size_t shared_ptr_ctl_block_sz = " << requested_size - sizeof(S) << ';' << std::endl;
  return 0;
}
