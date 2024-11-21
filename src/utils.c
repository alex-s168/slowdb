#include "internal.h"

char slowdb__header_magic[8] = {':', 's', 'l', 'o', 'w', 'd', 'b', '1'};

void slowdb__add_ent_idx(slowdb* db, size_t where, int32_t hash)
{
    slowdb_hashtab_bucket * bucket = &db->hashtab[((size_t ) hash) % db->hashtab_buckets];

    slowdb_hashtab_ent htent;
    htent.hash = hash;
    htent.where = where;
    bucket->items = (slowdb_hashtab_ent *)
        realloc(bucket->items, sizeof(slowdb_hashtab_ent) * (bucket->count + 1));
    bucket->items[bucket->count ++] = htent;
}

void slowdb__rem_ent_idx(slowdb* db, slowdb__ent_id id)
{
    slowdb_hashtab_bucket *buck = &db->hashtab[id.bucket];
    // we do not make list in bucket smaller because of perf reasons
    memcpy(&buck->items[id.inbuck],
          &buck->items[id.inbuck + 1],
          sizeof(slowdb_hashtab_ent) * (buck->count - id.inbuck));
    buck->count --;
}

#define FNV1A(type, prime, offset, dest, value, valueSize) { \
    type hash = offset;                                      \
    for (size_t i = 0; i < valueSize; i ++) {                \
        uint8_t byte = ((uint8_t *)value)[i];                \
        hash ^= byte;                                        \
        hash *= prime;                                       \
    }                                                        \
    *(type *)dest = hash;                                    \
}

slowdb__hash_t slowdb__hash(const unsigned char * data, int len)
{
    slowdb__hash_t res;
    FNV1A(slowdb__hash_t, 0x100000001B3, 0xcbf29ce484222325, &res, data, len);
    return res;
}
