#pragma once

#include <cstddef>
#include <string>

namespace avocado {

struct Replica {
  int session_id;
};

struct Replica_info {
  int session_id;
  size_t base_id;
  std::string uri;
};

} //avocado
