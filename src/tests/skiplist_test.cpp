#include <iostream>

#include <folly/ConcurrentSkipList.h>

struct S {
  uint64_t id;
  uint64_t value;

  S() = default;
  S(uint64_t id, uint64_t value) : id(id), value(value) {}

  S(S const & other) : id(other.id), value(other.value) {
    std::cout << "COPY " << id << ' ' << value <<  std::endl;
  }

  bool operator<(S const & other) const {
    return id < other.id;
  }
};

folly::ConcurrentSkipList<S>::Accessor kv = folly::ConcurrentSkipList<S>::create(12);

int main() {
  S a(2,0);
  S b(2,1);
  auto && [v, found] = kv.addOrGetData(a);
  std::cout << found << " " << v->value << '\n';
  auto && [v1, found1] = kv.addOrGetData(b);
  std::cout << found1 << " " << v1->value << '\n';
  v->value = 2;
  auto && [v2, found2] = kv.addOrGetData(b);
  std::cout << found2 << " " << v2->value << '\n';
}
