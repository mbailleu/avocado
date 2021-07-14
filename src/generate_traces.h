#pragma once

#include <cstdint>
#include <functional>
#include <vector>
#include <string>
#include <memory>

namespace avocado {

struct Trace_cmd;

struct unmap_trace {
  size_t size;
  void operator() (Trace_cmd * ptr);
};

using Trace_ptr = std::unique_ptr<Trace_cmd, unmap_trace>;

struct Trace_cmd {
    enum {
      Put,
      Get
    } op;

    static constexpr size_t key_size = 8;

    std::function<void(Trace_cmd & cmd)> call_;
    uint8_t key_hash[key_size];

    void call() { call_(*this); }

    explicit Trace_cmd(uint32_t key_id, int read_permille = 500);
    explicit Trace_cmd(std::string const & s, int read_permille);

    private:
    void init(uint32_t key_id, int read_permille);
  };


  Trace_ptr trace_init(uint16_t t_id, size_t Trace_size, size_t N_keys, int read_permille = 500, int rand_start = 0);
  Trace_ptr trace_init_zipfian(uint16_t t_id, size_t Trace_size, size_t N_keys, int read_permille = 500, int rand_start = 0, double alpha = 1.0);

//  std::vector<Trace_cmd> trace_init(uint16_t t_id, std::string path);
//  std::vector<Trace_cmd> trace_init(std::string path, int read_permille);

} //avocado
