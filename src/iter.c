#include "internal.h"

slowdb_iter* slowdb_iter_new(slowdb* slowdb)
{
    slowdb_iter* iter = malloc(sizeof(slowdb_iter));
    if (!iter) return NULL;
    iter->_db = slowdb;
    iter->_ent_id.bucket = 0;
    iter->_ent_id.inbuck = (half_size_t) SIZE_MAX;
    return iter;
}

void slowdb_iter_delete(slowdb_iter* iter)
{
    free(iter);
}

int slowdb__iter_seek_and_get_header(slowdb_iter* iter, slowdb_ent_header* header)
{
    slowdb_hashtab_ent* ent = slowdb__ent(iter->_db, iter->_ent_id);
    safe_fseek_set(iter->_db->fp, ent->where);
    safe_fread(header, sizeof(slowdb_ent_header), iter->_db->fp);
    return (header->valid & 1);
}

unsigned char * slowdb_iter_get_key(slowdb_iter* iter, int* lenout)
{
    slowdb_ent_header header;
	if (!slowdb__iter_seek_and_get_header(iter, &header)) return NULL;

    unsigned char * k = (unsigned char *) malloc(header.key_len);
    if (k == NULL) return NULL;
    safe_fread(k, header.key_len, iter->_db->fp);
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
    safe_fread(v, header.data_len, iter->_db->fp);

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
    iter->_ent_id.inbuck ++;
    while (iter->_ent_id.inbuck >= iter->_db->hashtab[iter->_ent_id.bucket].count) {
        iter->_ent_id.inbuck = 0;
        iter->_ent_id.bucket ++;
        if (iter->_ent_id.bucket >= iter->_db->hashtab_buckets)
            return 0;
    }
    return 1;
}

typedef struct {
    slowdb_stats* stats;
    slowdb_open_opts const* old_opts;
} readent_valid_cb_ctx;

static void readent_valid_cb(slowdb* instance, slowdb_open_entcb_info const* info, void* userdata)
{
    readent_valid_cb_ctx* ctx = (readent_valid_cb_ctx *) userdata;

    // this is not legal outside of slowdb implementation!
    size_t sz = sizeof(slowdb_ent_header) + info->compressed_keylen.valid + info->compressed_vallen.valid;
    if (info->valid) {
        ctx->stats->num_alive_ents += 1;
        ctx->stats->bytes_alive_ents += sz;

        if (ctx->old_opts->on_read.valid_ent)
            ctx->old_opts->on_read.valid_ent(instance, info, ctx->old_opts->on_read.valid_ent_userdata);
    } else {
        ctx->stats->num_dead_ents += 1;
        ctx->stats->bytes_dead_ents += sz;

        if (ctx->old_opts->on_read.invalid_ent)
            ctx->old_opts->on_read.invalid_ent(instance, info, ctx->old_opts->on_read.invalid_ent_userdata);
    }
}

slowdb* slowdb_stats_get(char const* path, slowdb_open_opts const* opts_in, slowdb_stats* out)
{
    memset(out, 0, sizeof(slowdb_stats));

    slowdb_open_opts opts = *opts_in;

    readent_valid_cb_ctx ctx;
    ctx.stats = out;
    ctx.old_opts = opts_in;

    opts.on_read.valid_ent = readent_valid_cb;
    opts.on_read.valid_ent_userdata = &ctx;

    opts.on_read.invalid_ent = readent_valid_cb;
    opts.on_read.invalid_ent_userdata = &ctx;

    slowdb* db = slowdb_openx(path, &opts);
    if (!db) return NULL;

    for (size_t i = 0; i < db->hashtab_buckets; i ++)
    {
        slowdb_hashtab_bucket bucket = db->hashtab[i];
        out->num_lookup_entries += bucket.count;

        for (size_t j = 0; j < bucket.count; j ++) {
            for (size_t k = 0; k < j; k ++) {
                if (bucket.items[k].hash == bucket.items[j].hash) {
                    out->num_hash_coll ++;
                }
            }
        }
    }

    return db;
}
