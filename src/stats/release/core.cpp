#if !BENCHMARKING

#include "stats/core.h"

Stats::~Stats() {}

Stats stats;

#else //BENCHMARKING

#include "../debug/core.cpp"

#endif //BENCHMARKING
