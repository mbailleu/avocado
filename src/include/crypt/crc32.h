/* crc32c.c -- compute CRC-32C using the Intel crc32 instruction
 * Copyright (C) 2013 Mark Adler
 * Version 1.1  1 Aug 2013  Mark Adler
 */

/*
  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the author be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Mark Adler
  madler@alumni.caltech.edu
 */

/* Use hardware CRC instruction on Intel SSE 4.2 processors.  This computes a
   CRC-32C, *not* the CRC-32 used by Ethernet and zip, gzip, etc.  A software
   version is provided as a fall-back, as well as for speed comparisons. */

/* Version history:
   1.0  10 Feb 2013  First version
   1.1   1 Aug 2013  Correct comments on why three crc instructions in parallel
 */


#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

namespace checksum {


/* Tbale for quadword-at-a-time software crc.*/

class CRC32 {
public:
    constexpr static size_t LONG = 8192;
    constexpr static size_t SHORT = 256;
    constexpr static uint32_t poly = 0x82f63b78;
    static uint32_t l[][256];
    static uint32_t s[][256];

private:

    [[gnu::pure]] static inline uint32_t shift(uint32_t z[][256], uint32_t crc) noexcept {
        return z[0][crc & 0xff] ^ z[1][(crc >> 8) & 0xff] ^ z[2][(crc >> 16) & 0xff] ^ z[3][crc >> 24];
    }

    template<unsigned int SIZE>
    inline void crc_aligned(uint32_t mat[][256], uint64_t & crc0, uint8_t const * & next, size_t & len) const noexcept {
        while (len >= SIZE * 3) {
            uint32_t crc1 = 0;
            uint32_t crc2 = 0;
            auto const end = next + SIZE;
            do {
                asm ("crc32q (%[next]) %[crc0]\n"
                     "crc32q %[sx1](%[next]) %[crc1]\n"
                     "crc32q %[sx2](%[next]) %[crc2]\n"
                     : [crc0] "+r" (crc0), [crc1] "+r" (crc1), [crc2] "+r" (crc2)
                     : [next] "r" (next), [sx1] "i" (SIZE), [sx2] "i" (SIZE * 2));
                next += 8;
            } while (next < end);
            crc0 = shift(mat, crc0) ^ crc1;
            crc0 = shift(mat, crc0) ^ crc2;
            next += SIZE * 2;
            len -= SIZE * 3;
        }
    }

    template<bool test(size_t, uint8_t const *)>
    inline void crc_unaligned(uint64_t & crc0, uint8_t const * & next, size_t & len) const noexcept {
        while(test(len, next)) {
            asm("crc32b (%[next]) %[crc0]"
                : [crc0] "+r" (crc0)
                : [next] "r" (next));
            ++next;
            --len;
        }
    }

    [[gnu::pure]] constexpr inline static bool test_first_align(size_t len, uint8_t const * next) noexcept {
        return len && ((reinterpret_cast<uintptr_t>(next) & 0x7) != 0);
    }

    [[gnu::pure]] constexpr inline static bool test_last_align(size_t len, uint8_t const *) noexcept {
        return len;
    }

public:
    [[gnu::pure]] inline uint32_t checksum(uint32_t crc, uint8_t const * buf, size_t len) const noexcept {
        uint64_t crc0 = crc ^ 0xffffffff;
        uint8_t const * next = buf;
        
        crc_unaligned<test_first_align>(crc0, next, len);

        crc_aligned<LONG>(l, crc0, next, len);
        crc_aligned<SHORT>(s, crc0, next, len);
        auto const end = next + (len - (len & 0x7));
        while (next < end) {
            asm("crc32q (%[next]) %[crc0]"
                : [crc0] "+r" (crc0)
                : [next] "r" (next));
            next += 8;
        }

        len &= 0x7;

        crc_unaligned<test_last_align>(crc0, next, len);

        return (uint32_t)crc0 ^ 0xffffffff;
    }

    static CRC32 create(); 

private:

    CRC32(){}
};

} //namespace checksum
