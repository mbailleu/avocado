#pragma once

#include <cstddef>
#include <cstdint>
#include <tuple>
#include <utility>

unsigned long get_time();

inline uint64_t get_tsc() { //should be inlined to remove function call overhead
    uint32_t eax;
    uint32_t edx;
    asm volatile ("rdtsc" : "=a" (eax), "=d" (edx));
    return (static_cast<uint64_t>(edx) << 32) | eax;
}

uint64_t get_time_in_ms(size_t tsc_value);

long double get_time_in_s(size_t tsc_value);

uint64_t get_time(size_t cpu_cycles);


template<class F, class ... Args>
auto take_time(F && f, Args && ... args)
  -> std::tuple<uint64_t, uint64_t, decltype(std::declval<F>().operator()(std::forward<Args>(args)...))> {
    auto start = get_tsc();
    decltype(auto) res = f(std::forward<Args>(args)...);
    auto end = get_tsc();
    return {start, end, res};
}

template<class F, class ... Args>
auto take_time_diff(F && f, Args && ... args)
  -> std::tuple<int64_t, decltype(std::declval<F>().operator()(std::forward<Args>(args)...))> {
    auto && [start, end, res] = take_time(std::move(f), std::forward<Args>(args)...);
    return {end - start, res};
}

//TODO add version with void return type
