#include <city.h>

#include <iostream>
#include <cstring>

int main(int argc, char ** argv) {
  for (auto i = 0; i < argc; ++i) {
    auto [x, y] = CityHash128(argv[i], std::strlen(argv[i]));
    std::cout << x << ':' << y << '\n';
  }
}
