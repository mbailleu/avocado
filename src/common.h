#pragma once

#include <string>
#include <vector>

extern std::string SvrName;
extern std::vector<std::string> ClnNames;

static constexpr uint16_t UDP_port = 31850;
static constexpr uint8_t Req_type = 2;
static constexpr size_t Msg_size = 16;

void parse_args(int argc, char ** argv);
