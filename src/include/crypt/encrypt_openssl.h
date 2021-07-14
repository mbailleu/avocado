#pragma once

#include <type_traits>
#include <algorithm>

#include "utils/final_act.h"

#include "openssl/aes.h"
#include "openssl/evp.h"
#include "openssl/err.h"
#include "utils/constexpr_math.h"

#if !defined(NDEBUG)

#include <cassert>
#include <cstring>

#endif //NDEBUG

#if COLLECT_STATS

#include "stats/encrypt_openssl.h"

#else //COLLECT_STATS

#include "crypt/release/encrypt_openssl.h"

#endif //COLLECT_STATS
