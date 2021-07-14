#pragma once

#include <string>

#include "size_events.h"

class CryptStats {
public:
  std::string name_ = "CryptStats";

  using Data = SizeEventData;

  Data encrypt;
  Data decrypt;

  std::string name() const { return name_; };

  virtual ~CryptStats();
};
