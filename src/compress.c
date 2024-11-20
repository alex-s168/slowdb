#include "internal.h"

slowdb__compress slowdb__select_compress(void const* data, size_t len)
{
    if (len > 1024) {
        // TODO: LZ4
        return COMPRESS_NONE;
    }

    bool any_wide = false;
    bool any_null = false;

    for (size_t i = 0; i < len; i ++) {
        char c = ((char const*) data)[i];
        if (c == 0) {
            any_null = true;
            break;
        }
        if (c & 0b10000000) {
            any_wide = true;
        }
    }

    if (!any_wide && !any_null)
        return COMPRESS_STRPACK;
#if _SLOWDB_WITH_UNISHOX2
    else if (!any_null)
        return COMPRESS_UNISHOX2;
#endif

    // TODO: LZ4
    return COMPRESS_NONE;
}

#include <math.h>
#include <stdlib.h>
#ifndef max
#define max(a, b) ((a > b) ? a : b)
#endif
#define STRPACK_IMPLEMENT
#include "strpack/strpack.h"

#if _SLOWDB_WITH_UNISHOX2
#include "unishox2/unishox2.h"
#endif

size_t slowdb__comp(slowdb__compress algo, void* dest, void const* src, size_t len)
{
    switch (algo) {
        case COMPRESS_NONE: {
            memcpy(dest, src, len);
            return len;
        }

        case COMPRESS_STRPACK: {
            return (size_t) strpack_compress(src, dest, NULL);
        }

#if _SLOWDB_WITH_UNISHOX2
        case COMPRESS_UNISHOX2: {
            return (size_t) unishox2_compress_simple(src, (int) len, dest);
        }
#endif

        default: {
            fprintf(stderr, "don't know compress algo %i\n", algo);
            exit(1);
        }
    }
}

size_t slowdb__decomp(slowdb__compress algo, void* dest, void const* src, size_t len)
{
    switch (algo) {
        case COMPRESS_NONE: {
            memcpy(dest, src, len);
            return len;
        }

        case COMPRESS_STRPACK: {
            return (size_t) strpack_decompress(src, dest);
        }

#if _SLOWDB_WITH_UNISHOX2
        case COMPRESS_UNISHOX2: {
            return (size_t) unishox2_decompress_simple(src, len, dest);
        }
#endif

        default: {
            fprintf(stderr, "don't know compress algo %i\n", algo);
            exit(1);
        }
    }
}
