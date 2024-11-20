#include "../inc/slowdb.h"
#include "../config.h"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>

#include <fcntl.h>
#ifdef _WIN32 
# include <io.h>
#endif

typedef struct {
    int32_t hash;
    uint16_t entid;
} slowdb_hashtab_ent;

typedef struct {
    slowdb_hashtab_ent * items;
    size_t count;
} slowdb_hashtab_bucket;

struct slowdb {
    FILE* fp;

    size_t* ents;

    size_t hashtab_buckets;
    slowdb_hashtab_bucket * hashtab;

    size_t ents_len;
};

#define SLOWDB__ENT_MAGIC (0b10101010)

typedef enum : char {
    COMPRESS_NONE = 0,
    COMPRESS_STRPACK = 1,
#if _SLOWDB_WITH_UNISHOX2
    COMPRESS_UNISHOX2 = 2,
#endif 
#if _SLOWDB_WITH_LZ4
    COMPRESS_LZ4 = 3,
#endif 
} slowdb__compress;

typedef struct {
    /** SLOWDB__ENT_MAGIC | valid */
    char valid;
    /** only for data, not key */
    slowdb__compress compress;

    uint16_t key_len;

    /** size of the data in compressed form */
    uint16_t data_len;
} __attribute__((packed)) slowdb_ent_header;

extern char slowdb__header_magic[8];

typedef struct {
    char magic[sizeof(slowdb__header_magic)];
} __attribute__((packed)) slowdb_header;

int32_t slowdb__hash(const unsigned char * data, int len);
void slowdb__add_ent_idx(slowdb* db, size_t where, int32_t hash);

/** returns 1 if valid */
static int slowdb__iter_seek_and_get_header(slowdb_iter* iter, slowdb_ent_header* header);

slowdb__compress slowdb__select_compress(void const* data, size_t len);
void * slowdb__comp(slowdb__compress algo, void const* src, size_t len, size_t *actual_len_out);
void * slowdb__decomp(slowdb__compress algo, void const* src, size_t len, size_t *actual_len_out);

#define malloca(dest, size) { if ((size) > 1024) dest = malloc((size)); else { char buf[size]; dest = (void*) (&buf); }  }
#define freea(var, size)    { if ((size) > 1024) free((var)); }

// 0 = no next 
#define slowdb__iterPotentialEnt(db, hashh, idVar, fn) \
{ \
    slowdb_hashtab_bucket bucket = \
        (db)->hashtab[((size_t) hashh) % (db)->hashtab_buckets]; \
\
    for (size_t __buck = 0; __buck < bucket.count; __buck ++) \
    { \
        if (bucket.items[__buck].hash == (hashh)) \
        { \
            uint16_t idVar = bucket.items[__buck].entid; \
            fn; \
        } \
    } \
}
