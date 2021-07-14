#pragma once

#define BENCHMARKING 1

#if BENCHMARKING
#include "../../debug/stats/core.h"
#else
class Stats {
public:
  Stats() {}
  template<class STR>
  Stats(STR const &) {}
  ~Stats();
  template<class STR>
  void change_file_name(STR const &) {}

  template<class STR, class PTREE>
  void add(STR const &, PTREE const &) {}
  template<class STR, class T>
  void put(STR const &, T &&) {}
};

extern Stats stats;
#endif
