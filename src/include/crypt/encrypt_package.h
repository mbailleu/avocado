#pragma once

#include "crypt/encrypt_openssl.h"

#include "utils/utility.h"
#include <cassert>
#include <memory>
#include <utility>
#include <iostream>

#if COLLECT_STATS

#include "stats/encrypt_package.h"

#else //COLLECT_STATS

#include "crypt/release/encrypt_package.h"

#endif //COLLECT_STATS
