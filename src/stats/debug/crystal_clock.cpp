#include "stats/crystal_clock.h"

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
  res.put("clock_fq", crystal_clock_fq);
  res.put("k", k);
  res.put("ART_fq", ART_fq);
  res.put("denominator", denominator);
  res.put("numerator", numerator);
  res.put("base_fq", base_freq);
  res.put("max_fq", max_freq);
  res.put("bus_fq", bus_freq);
  stats.add("CrystalClock", res);
}
