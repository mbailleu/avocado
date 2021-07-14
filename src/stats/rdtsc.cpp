#include "stats/crystal_clock.h"
#include "stats/core.h"

#include <cstdint>


static CrystalClockFactors const clock_info;

template<class T, size_t NUMERATOR, size_t DENUMERATOR>
T get_time_(size_t tsc_value) {
    typedef unsigned int uint128_t __attribute__((mode(TI)));
    auto ART_Value = (static_cast<uint128_t>(tsc_value) * clock_info.denominator) / clock_info.numerator;
    T time = (ART_Value * NUMERATOR) / (static_cast<T>(clock_info.ART_fq) * DENUMERATOR); //HOWEVER this breaks with a big tsc_value
    return time;
}

long double get_time_in_s(size_t tsc_value) {
    return get_time_<long double, 1, 1>(tsc_value);
}

uint64_t get_time_in_ms(size_t tsc_value) {
    return get_time_<uint64_t, 1000, 1>(tsc_value);
}

uint64_t get_time(size_t tsc_value) {
	return get_time_in_ms(tsc_value);
}
