#include "stats/core.h"

#include <iostream>
#include <fstream>

#include <boost/property_tree/json_parser.hpp>

namespace pt = boost::property_tree;

Stats::~Stats() {
  if (fileName.empty()) {
    pt::write_json(std::cout, root);
    return;
  }
  std::ofstream out(fileName);
  pt::write_json(out, root);
  out.close();
}

Stats stats;
