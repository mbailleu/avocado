#pragma once

namespace constexpr_math {

    static constexpr size_t log(size_t n) noexcept {
        size_t res = 0;
        while ((n & 1) == 0) { n >>= 1; ++res;}
        return res;
    }

} //namespace constexpr_math
