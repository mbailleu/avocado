#ifndef HAVE_STRLCPY

#include <stddef.h>
#include <string.h>

size_t strlcpy(char * dst, char const * src, size_t size) {
    size_t srclen = strlen(src);
    --size;
    srclen = srclen > size ? size : srclen;

    memcpy(dst, src, srclen);
    dst[srclen] = '\0';
    return srclen;
}

#endif //HAVE_STRLCPY

#ifndef HAVE_STRLCAT

#include <stddef.h>
#include <string.h>

size_t strlcat(char * dst, char const * src, size_t size) {
    size_t dstlen = strlen(dst);
    if (size <= dstlen + 1)
        return dstlen;

    size -= dstlen;

    size_t srclen = strlen(src);

    srclen = srclen > size ? size : srclen;
    memcpy(dst + dstlen, src, srclen);
    dst[dstlen + srclen] = '\0';
    return dstlen + srclen;
}

#endif //HAVE_STRLCAT
