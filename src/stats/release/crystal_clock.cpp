#include "stats/crystal_clock.h"

#if !BENCHMARKING

CrystalClockFactors::~CrystalClockFactors() {}

#else //BENCHMARKING

#include "stats/core.h"

#include <boost/property_tree/ptree.hpp>

CrystalClockFactors::~CrystalClockFactors() {
  using ptree = boost::property_tree::ptree;
  ptree res;
#if defined(SCONE)
  res.put("enclave", "scone");
#else //SCONE
  res.put("enclave", "native");
#endif //SCONE
  stats.add("CrystalClock", res);
}

#endif //BENCHMARKING
