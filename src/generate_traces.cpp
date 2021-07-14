#include "generate_traces.h"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <sys/mman.h>
#include <memory>
#include <cmath>

#include <city.h>

namespace avocado {
  namespace {
#if SCONE || 1
#ifndef SYS_untrusted_mmap
#define SYS_untrusted_mmap 1025
#endif
    
    void * scone_kernel_mmap(void * addr, size_t length, int prot, int flags, int fd, off_t offset) {
      return (void *)syscall(SYS_untrusted_mmap, addr, length, prot, flags, fd, offset);
    }

    void * mmap_helper(size_t size) {
      return scone_kernel_mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    }

#else

    void * mmap_helper(size_t size) {
      return mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    }

#endif
#if 0
    std::vector<std::string> split(std::string const & s, char delimiter) {
      std::vector<std::string> tokens;
      std::string token;
      std::istringstream tokenStream(s);
      while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
      }
      return tokens;
    }

    std::vector<Trace_cmd> parse_trace(uint16_t, std::string path, int read_permille) {
      std::ifstream file(path);
      if (!file) {
        return {};
      }

      std::vector<Trace_cmd> res;
      std::string line;
      while (std::getline(file, line)) {
        if (line.length() == 0) continue;

        if (line[line.length() - 1] == '\n') {
          line[line.length() - 1] = 0;
        }

        auto elements = split(line, ' ');
        if (elements.size() < 1) {
          continue;
        }
        res.emplace_back(elements[0], read_permille);
      }
      return res;
    }
#endif

    Trace_ptr manufacture_trace(uint16_t, size_t Trace_size, size_t N_keys, int read_permille) {
      auto res = reinterpret_cast<Trace_cmd*>(mmap_helper(Trace_size * sizeof(Trace_cmd)));
      for (auto i = 0ULL; i < Trace_size; ++i) {
        res[i] = Trace_cmd(static_cast<uint32_t>(rand() % N_keys), read_permille);
      }
      return Trace_ptr(res, unmap_trace{Trace_size * sizeof(Trace_cmd)});
    }

    Trace_ptr manufacture_trace_zipfian(uint16_t, size_t trace_size, size_t n_keys, int read_permille, double alpha = 1.0) {
      
      struct del {
        size_t size;
        void operator() (double * ptr) {
          if (ptr) {
            munmap(ptr, size + sizeof(size_t));
          }
        }
      } d{n_keys};
      
      double c = 0;
      auto sum_probs = std::unique_ptr<double[], del>(reinterpret_cast<double *>(mmap_helper((n_keys + 1) * sizeof(size_t))), d);
      for (auto i = 1ULL; i < n_keys; ++i) {
        c = c + (1.0 / std::pow(static_cast<double>(i), alpha));
      }

      c = 1.0 / c;
      sum_probs[0] = 0;
      for (auto i = 1ULL; i < n_keys; ++i) {
        sum_probs[i] = sum_probs[i - 1] + c / pow(static_cast<double>(i), alpha);
      }

      auto res = reinterpret_cast<Trace_cmd*>(mmap_helper(trace_size * sizeof(Trace_cmd)));

      for (auto i = 0ULL; i < trace_size; ++i) {
        auto z = static_cast<double>(rand()) / RAND_MAX;
        size_t low = 1;
        size_t high = n_keys;
        size_t zip_value;
        do {
          size_t mid = std::floor((low + high) / 2);
          if (sum_probs[mid] >= z && sum_probs[mid - 1] < z) {
            zip_value = mid;
            break;
          }
          if (sum_probs[mid] >= z) {
            high = mid - 1;
            continue;
          }
          low = mid + 1;
        } while (low <= high);
        res[i] = Trace_cmd(static_cast<uint32_t>(zip_value), read_permille);
      }
      return Trace_ptr(res, unmap_trace{trace_size * sizeof(Trace_cmd)});
    }

  } //namespace

  void Trace_cmd::init(uint32_t key_id, int read_permille) {
    op = (rand() % 1000) < read_permille ? Get : Put;
    auto key_hash_ = CityHash128((char *) &(key_id), sizeof(key_id));
    memcpy(key_hash, &key_hash_, key_size);
  }

  Trace_cmd::Trace_cmd(uint32_t key_id, int read_permille) {
    init(key_id, read_permille);
  }

  Trace_cmd::Trace_cmd(std::string const & s, int read_permille) {
    init(static_cast<uint32_t>(strtoul(s.c_str(), nullptr, 10)), read_permille);
  }

#if 0
  std::vector<Trace_cmd> trace_init(uint16_t t_id, std::string path) {
    return parse_trace(t_id, path, 500);
  }

  std::vector<Trace_cmd> trace_init(std::string file_path, int read_permile) {
    return parse_trace(0, file_path, read_permile);

  }
#endif

  void unmap_trace::operator() (Trace_cmd * ptr) {
    munmap(ptr, size);
  }

  Trace_ptr trace_init(uint16_t t_id, size_t Trace_size, size_t N_keys, int read_permille, int rand_start) {
    srand(rand_start);
    return manufacture_trace(t_id, Trace_size, N_keys, read_permille);
  }

  Trace_ptr trace_init_zipfian(uint16_t t_id, size_t Trace_size, size_t N_keys, int read_permille, int rand_start, double alpha) {
    srand(rand_start);
    return manufacture_trace_zipfian(t_id, Trace_size, N_keys, read_permille, alpha);
  }

} //namespace avocado
