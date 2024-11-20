#include "internal.h"

slowdb__compress slowdb__select_compress(void const* data, size_t len)
{
    if (len == 0) return COMPRESS_NONE;

    if (((const char *) data)[len-1] == '\0') {
        len --;
    }

    if (len < 6)
        return COMPRESS_NONE;

    if (len > 1024) {
#if _SLOWDB_WITH_LZ4
        return COMPRESS_LZ4;
#else
        return COMPRESS_NONE;
#endif
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

#if _SLOWDB_WITH_LZ4
    return COMPRESS_LZ4;
#else
    return COMPRESS_NONE;
#endif
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

#if _SLOWDB_WITH_LZ4
#include <lz4.h>
#endif

void * slowdb__comp(slowdb__compress algo, void const* src, size_t len, size_t *actual_len_out)
{
    if (len == 0) return NULL;

    if (algo == COMPRESS_STRPACK 
#if _SLOWDB_WITH_UNISHOX2
     || algo == COMPRESS_UNISHOX2
#endif
        )
        len --;

    switch (algo) {
        case COMPRESS_NONE: {
            void * dest = malloc(len);
            if (dest == NULL) return NULL;
            *actual_len_out = len;
            memcpy(dest, src, len);
            return dest;
        }

        case COMPRESS_STRPACK: {
            size_t dest_cap = len;
            void * dest = malloc(dest_cap);
            if (dest == NULL) return NULL;

            size_t was = 0;

            while (( was = (size_t) strpack_compress(src, dest, dest_cap, NULL) ) == 0) {
                dest_cap += len;
                void *new = realloc(dest, dest_cap);
                if (new == NULL) {
                    free(dest);
                    return NULL;
                }
                dest = new;
            }

            *actual_len_out = was;
            return dest;
        }

        // TODO: make safe
#if _SLOWDB_WITH_UNISHOX2
        case COMPRESS_UNISHOX2: {
            size_t dest_cap = len;
            void * dest = malloc(dest_cap);
            if (dest == NULL) return NULL;
            size_t was = (size_t) unishox2_compress_simple(src, (int) len, dest);
            *actual_len_out = was;
            return dest;
        }
#endif

#if _SLOWDB_WITH_LZ4
        case COMPRESS_LZ4: {
            size_t dest_cap = len >> 1;
            void * dest = malloc(dest_cap);
            if (dest == NULL) return NULL;

            size_t was = 0;

            while (( was = (size_t) LZ4_compress_fast(src, dest, len, dest_cap, 200) ) == 0) {
                dest_cap += len;
                void *new = realloc(dest, dest_cap);
                if (new == NULL) {
                    free(dest);
                    return NULL;
                }
                dest = new;
            }

            *actual_len_out = was;
            return dest;
        }
#endif

        default: {
            fprintf(stderr, "don't know compress algo %i\n", algo);
            exit(1);
        }
    }
}

void * slowdb__decomp(slowdb__compress algo, void const* src, size_t len, size_t *actual_len_out)
{
    if (len == 0) return NULL;

    switch (algo) {
        case COMPRESS_NONE: {
            void * dest = malloc(len);
            if (dest == NULL) return NULL;
            *actual_len_out = len;
            memcpy(dest, src, len);
            return dest;
        }

        case COMPRESS_STRPACK: {
            size_t dest_cap = len * 2;
            void * dest = malloc(dest_cap + 1);
            if (dest == NULL) return NULL;

            size_t was = 0;

            while (( was = (size_t) strpack_decompress(src, dest, dest_cap) ) == 0) {
                dest_cap += len;
                void *new = realloc(dest, dest_cap + 1);
                if (new == NULL) {
                    free(dest);
                    return NULL;
                }
                dest = new;
            }

            ((char*) dest)[was] = '\0';
            *actual_len_out = was + 1;
            return dest;
        }

        // TODO: make safe 
#if _SLOWDB_WITH_UNISHOX2
        case COMPRESS_UNISHOX2: {
            size_t dest_cap = len * 2;
            void * dest = malloc(dest_cap + 1);
            if (dest == NULL) return NULL;
            size_t was = (size_t) unishox2_decompress_simple(src, len, dest);
            ((char*) dest)[was] = '\0';
            *actual_len_out = was + 1;
            return dest;
        }
#endif

#if _SLOWDB_WITH_LZ4
        case COMPRESS_LZ4: {
            size_t dest_cap = len * 2;
            void * dest = malloc(dest_cap);
            if (dest == NULL) return NULL;

            size_t was = 0;

            while (( was = (size_t) LZ4_decompress_safe(src, dest, len, dest_cap) ) == 0) {
                dest_cap += len;
                void *new = realloc(dest, dest_cap);
                if (new == NULL) {
                    free(dest);
                    return NULL;
                }
                dest = new;
            }

            *actual_len_out = was;
            return dest;
        }
#endif

        default: {
            fprintf(stderr, "don't know compress algo %i\n", algo);
            exit(1);
        }
    }
}
