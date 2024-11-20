#include "internal.h"

char slowdb__header_magic[8] = {':', 's', 'l', 'o', 'w', 'd', 'b', '1'};

int32_t slowdb__hash(const unsigned char * data, int len)
{
    int32_t r = 0;
    for (int i = 0; i < len; i ++) {
        r = data[i] + (r << 6) + (r << 16) - r;
    }
    return r;
}

void slowdb__add_ent_idx(slowdb* db, size_t where, int32_t hash)
{
    db->ents = (size_t*)
        realloc(db->ents, sizeof(size_t) * (db->ents_len + 1));
    db->ents[db->ents_len] = where;

    slowdb_hashtab_bucket * bucket = &db->hashtab[((size_t ) hash) % db->hashtab_buckets];

    slowdb_hashtab_ent htent;
    htent.hash = hash;
    htent.entid = db->ents_len;
    bucket->items = (slowdb_hashtab_ent *)
        realloc(bucket->items, sizeof(slowdb_hashtab_ent) * (bucket->count + 1));
    bucket->items[bucket->count ++] = htent;

    db->ents_len ++;
}

