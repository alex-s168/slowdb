#include "internal.h"

slowdb_iter slowdb_iter_new(slowdb* slowdb)
{
    return (slowdb_iter) { ._db = slowdb, ._ent_id = SIZE_MAX };
}

static int slowdb__iter_seek_and_get_header(slowdb_iter* iter, slowdb_ent_header* header)
{
    fseek(iter->_db->fp, iter->_db->ents[iter->_ent_id], SEEK_SET);
    fread(header, 1, sizeof(*header), iter->_db->fp);
    return (header->valid & 1);
}

unsigned char * slowdb_iter_get_key(slowdb_iter* iter, int* lenout)
{
    slowdb_ent_header header;
	if (!slowdb__iter_seek_and_get_header(iter, &header)) return NULL;

    unsigned char * k = (unsigned char *) malloc(header.key_len);
    if (k == NULL) return NULL;
    fread(k, 1, header.key_len, iter->_db->fp);
    if (lenout) *lenout = header.key_len;
    return k;
}

unsigned char * slowdb_iter_get_val(slowdb_iter* iter, int* lenout)
{
    slowdb_ent_header header;
	if (!slowdb__iter_seek_and_get_header(iter, &header)) return NULL;

	fseek(iter->_db->fp, header.key_len, SEEK_CUR);

    unsigned char * v = (unsigned char *) malloc(header.data_len);
    if (v == NULL) return NULL;
    fread(v, 1, header.data_len, iter->_db->fp);

    size_t decomp;
    unsigned char * actual = slowdb__decomp(header.compress, v, header.data_len, &decomp);
    if (actual == NULL) return NULL;
    free(v);
    if (lenout) *lenout = decomp;
    return actual;
}

// returns 1 if there was a next
int slowdb_iter_next(slowdb_iter* iter)
{
    iter->_ent_id ++;
    return iter->_ent_id < iter->_db->ents_len;
}

void slowdb_stats_get(slowdb* db, slowdb_stats* out)
{
    memset(out, 0, sizeof(slowdb_stats));

	slowdb_iter iter = slowdb_iter_new(db);
	while (slowdb_iter_next(&iter))
	{
		slowdb_ent_header header;

        if ( slowdb__iter_seek_and_get_header(&iter, &header) ) {
            out->num_alive_ents ++;
            out->bytes_alive_ents += header.key_len + header.data_len;
        }
        else {
            out->num_dead_ents ++;
            out->bytes_dead_ents += header.key_len + header.data_len;
        }
	}

    for (size_t i = 0; i < db->hashtab_buckets; i ++)
    {
        slowdb_hashtab_bucket bucket = db->hashtab[i];

        for (size_t j = 0; j < bucket.count; j ++)
            for (size_t k = 0; k < j; k ++)
                if (bucket.items[k].hash == bucket.items[j].hash)
                    out->num_hash_coll ++;
    }
}
