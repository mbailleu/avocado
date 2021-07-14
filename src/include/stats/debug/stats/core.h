#pragma once

#include <string>

#include <boost/property_tree/ptree.hpp>

class Stats {
public:
  using ptree = boost::property_tree::ptree;
  std::string fileName;
  ptree root;
  Stats() {}
  Stats(std::string const & fileName) : fileName(fileName) {}
  ~Stats();

  inline void change_file_name(std::string const & fileName) { this->fileName = fileName; }
  void add(std::string const & name, ptree const & child) { root.add_child(name, child); }
  template<class T>
  void put(std::string const & name, T && val) { root.put(name, val); }
};

extern Stats stats;
