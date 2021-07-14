#pragma once

#include <cstddef>
#include <cstdint>

struct CrystalClockFactors {
  static constexpr size_t M_factor = 1'000'000;
  size_t crystal_clock_fq;
  static constexpr size_t k = 1; //This might be different, in theory we should be able to fetch it from MSR_PLATFORM_INFO, but 1 works for me
  size_t ART_fq; //= crystal_clock_fq * k;
  uint32_t denominator; //AKA CPUID.15H.EAX[31:0]
  uint32_t numerator;  //AKA CPUID.15H.EBX[31:0]
  size_t base_freq{0};
  size_t max_freq{0};
  size_t bus_freq{0};

  CrystalClockFactors() {
    uint32_t a = 0x15;
    uint32_t c = 0x0;
    asm volatile ("cpuid" : "=a" (denominator), "=b" (numerator), "=c" (crystal_clock_fq) : "a" (a), "c" (c) : "edx");
    if (crystal_clock_fq != 0x0) {
      ART_fq = crystal_clock_fq * k;
      return;
    }
    a = 0x16;
    c = 0x0;
    uint32_t base_freq;
    uint32_t max_freq;
    uint32_t bus_freq;
    asm volatile ("cpuid" : "=a" (base_freq), "=b" (max_freq), "=c" (bus_freq) : "a" (a), "c" (c) : "edx");
    crystal_clock_fq = static_cast<size_t>(base_freq) * M_factor * denominator / numerator;
    ART_fq = crystal_clock_fq * k;
    this->base_freq = static_cast<size_t>(base_freq) * M_factor;
    this->max_freq = static_cast<size_t>(max_freq) * M_factor;
    this->bus_freq = static_cast<size_t>(bus_freq) * M_factor;
  }

	~CrystalClockFactors();
};
