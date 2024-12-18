#include "../inc/slowdb.h"
#include "../config.h"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include <limits.h>

#include <fcntl.h>
#ifdef _WIN32 
# include <io.h>
#endif

typedef enum {
    COMPRESS_NONE = 0,
    COMPRESS_STRPACK = 1,
#if _SLOWDB_WITH_UNISHOX2
    COMPRESS_UNISHOX2 = 2,
#endif 
#if _SLOWDB_WITH_LZ4
    COMPRESS_LZ4 = 3,
#endif 
} slowdb__compress;

#if __SIZE_WIDTH__ >= 64
# define half_size_t uint32_t
#elif __SIZE_WIDTH__ >= 32 
# define half_size_t uint16_t 
#else 
# define half_size_t uint8_t
#endif

#define expand(x,y) y(x)
#define uint(bits) uint##bits##_t

#define SLOWDB__HASH_BITS 64
#define slowdb__hash_t expand(SLOWDB__HASH_BITS, uint)

typedef struct {
    slowdb__hash_t hash;
    size_t  where;
} slowdb_hashtab_ent;

typedef struct {
    slowdb_hashtab_ent * items;
    size_t count;
} slowdb_hashtab_bucket;

typedef struct {
    half_size_t bucket;
    half_size_t inbuck;
} slowdb__ent_id;

struct slowdb {
    FILE* fp;

    size_t hashtab_buckets;
    slowdb_hashtab_bucket * hashtab;

    size_t next_new;
};

static slowdb_hashtab_ent* slowdb__ent(slowdb* db, slowdb__ent_id id) {
    return &db->hashtab[id.bucket].items[id.inbuck];
}

#define SLOWDB__ENT_MAGIC (0b10101010)

typedef struct {
    /** SLOWDB__ENT_MAGIC | valid */
    char valid;
    /** only for data, not key */
    char compress; // compress to slowdb__compress

    uint16_t key_len;

    /** size of the data in compressed form */
    uint16_t data_len;
} __attribute__((packed)) slowdb_ent_header;

extern char slowdb__header_magic[8];

typedef struct {
    char magic[sizeof(slowdb__header_magic)];
} __attribute__((packed)) slowdb_header;

slowdb__hash_t slowdb__hash(const unsigned char * data, int len);
void slowdb__add_ent_idx(slowdb* db, size_t where, int32_t hash);
void slowdb__rem_ent_idx(slowdb* db, slowdb__ent_id id);

/** returns 1 if valid */
static int slowdb__iter_seek_and_get_header(slowdb_iter* iter, slowdb_ent_header* header);

slowdb__compress slowdb__select_compress(void const* data, size_t len);
void * slowdb__comp(slowdb__compress algo, void const* src, size_t len, size_t *actual_len_out);
void * slowdb__decomp(slowdb__compress algo, void const* src, size_t len, size_t *actual_len_out);

#define malloca(dest, size) { if ((size) > 1024) dest = malloc((size)); else { char buf[size]; dest = (void*) (&buf); }  }
#define freea(var, size)    { if ((size) > 1024) free((var)); }

// 0 = no next 
#define slowdb__iterPotentialEnt(db, hashh, idVar) \
{ \
    size_t buckn = ((size_t) hashh) % (db)->hashtab_buckets; \
    slowdb_hashtab_bucket bucket = \
        (db)->hashtab[buckn]; \
\
    for (size_t __buck = 0; __buck < bucket.count; __buck ++) \
    { \
        if (bucket.items[__buck].hash != (hashh)) continue; \
        slowdb__ent_id idVar = (slowdb__ent_id) { .bucket = (half_size_t) buckn, .inbuck = (half_size_t) __buck } ; \

struct slowdb_iter {
    /** INTERNAL! */
    slowdb *_db;
    /** INTERNAL! */
    slowdb__ent_id _ent_id;
};
