#include  "common.h"

#include <string>
#include <vector>
#include <cstring>

std::string SvrName;
std::vector<std::string> ClnNames;

void parse_args(int argc, char ** argv) {
    for (auto i = 0; i < argc; ++i) {
        auto str = argv[i];
        if (i == 0) {
            SvrName = str;
        } else {
            ClnNames.push_back(str);
        }
    }
}
