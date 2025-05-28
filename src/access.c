#include "internal.h"

#ifdef _WIN32
#else
# include <unistd.h>
#endif

unsigned char *slowdb_get(slowdb *instance, const unsigned char *key, int keylen, int *vallen)
{
    slowdb__hash_t keyhs = slowdb__hash(key, keylen);

    slowdb__compress comp;
    size_t found = 0;
    slowdb__iterPotentialEnt(instance, keyhs, entid)
        slowdb_hashtab_ent* ent = slowdb__ent(instance, entid);
        safe_fseek_set(instance->fp, ent->where);
        slowdb_ent_header header;
        safe_fread(&header, sizeof(header), instance->fp);
        if (!(header.valid & 1) || header.key_len != keylen)
            continue;
        unsigned char * k = (unsigned char *) malloc(header.key_len);
        if (k == NULL) continue;
        safe_fread(k, header.key_len, instance->fp);
        if (!memcmp(key, k, header.key_len)) {
            found = header.data_len + 1;
            comp = header.compress;
            free(k);
            break;
        }
        free(k);
    }}

    if (!found) return NULL;
	found --;

    unsigned char * res = NULL;
    if (found > 0) {
        res = (unsigned char*) malloc(found);
        if (res == NULL) return NULL;
        safe_fread(res, found, instance->fp);
    }

    size_t decomp = 0;
    unsigned char * actual = NULL;
    if (found > 0) {
        actual = slowdb__decomp(comp, res, found, &decomp);
        if (actual == NULL) {
            free(res);
            return NULL;
        }
    }
    if (vallen)
        *vallen = decomp;
    free(res);
    return actual;
}

// including the header + content bytes of the given location
static size_t slowdb__continuos_count_dead_ent_bytes_from(slowdb* instance, size_t filesize, int force_first_as_free, size_t max_size, size_t* num_ents, size_t where)
{
    slowdb_ent_header header;

    size_t out = 0;
    size_t nents = 0;
    while (where + sizeof(header) < filesize)
    {
        safe_fseek_set(instance->fp, where);

        safe_fread(&header, sizeof(header), instance->fp);
        if ( (header.valid & 1) && !force_first_as_free )
            break;
        force_first_as_free = 0;

        size_t thissize = sizeof(header) + header.data_len + header.key_len;
        if (out + thissize > max_size)
            break;
        out += thissize;
        nents ++;
        where += thissize;
    }

    if (num_ents)
        *num_ents = nents;

    return out;
}

// TODO: can truncate more than we can merge!
// TODO: truncate on open

static void slowdb__rm(slowdb *instance, size_t where)
{
    fseek(instance->fp, 0, SEEK_END);
    size_t filesize = ftell(instance->fp);

    slowdb_ent_header header;
    size_t num_ents;
    size_t numfree = slowdb__continuos_count_dead_ent_bytes_from(instance, filesize, 1, sizeof(header) + UINT16_MAX * 2, &num_ents, where);
    size_t const num_total_free = numfree;
    numfree -= sizeof(header);
    size_t num_key = numfree; if (num_key > UINT16_MAX) num_key = UINT16_MAX;
    numfree -= num_key;
    size_t num_val = numfree; if (num_val > UINT16_MAX) num_val = UINT16_MAX;
    numfree -= num_val;
    assert(numfree == 0);

    header.compress = COMPRESS_NONE;
    header.key_len = num_key;
    header.data_len = num_val;
    header.valid = (char) SLOWDB__ENT_MAGIC;
    safe_fseek_set(instance->fp, where);
    safe_fwrite(&header, sizeof(header), instance->fp);

    safe_flush(instance->fp);

    if (num_ents > 1)
    {
        slowdb__logf(instance, SLOWDB_LOG_LEVEL__VERBOSE, "Merged %zu dead entries", num_ents);
    }

    if (where + num_total_free >= instance->next_new) {
        instance->next_new = where;

        size_t const num_too_big = filesize - where;
        if (num_too_big >= instance->min_bytes_for_trunc)
        {
#ifdef _WIN32
            // TODO: find windows alternative
#else
            safe_fseek_set(instance->fp, 0);
            int fd = fileno(instance->fp);
            runtime_assert(fd != -1);
            runtime_assert(0 == ftruncate(fd, where));
            slowdb__logf(instance, SLOWDB_LOG_LEVEL__VERBOSE, "Truncated database file by %zu bytes", num_too_big);
#endif
        }
    }
}

int slowdb_replaceOrPut(slowdb *instance, const unsigned char *key, int keylen, const unsigned char *val, int vallen)
{
    slowdb__hash_t keyhs = slowdb__hash(key, keylen);
    slowdb__iterPotentialEnt(instance, keyhs, entid)
        slowdb_hashtab_ent* ent = slowdb__ent(instance, entid);
        safe_fseek_set(instance->fp, ent->where);
        slowdb_ent_header header;
        safe_fread(&header, sizeof(header), instance->fp);
        if (!(header.valid & 1) || header.key_len != keylen)
            continue;
        unsigned char * k = (unsigned char *) malloc(header.key_len);
        if (k == NULL) continue;
        safe_fread(k, header.key_len, instance->fp);
        if (!memcmp(key, k, header.key_len)) {
            free(k);
            if (header.compress == COMPRESS_NONE && header.data_len == vallen) {
                safe_fseek_set(instance->fp, ent->where + sizeof(header) + header.key_len);
                safe_fwrite(val, vallen, instance->fp);
                safe_flush(instance->fp);
                return 0;
            } else {
                // because compression might make new data len != old data len, remove this entry and add new
                slowdb__rm(instance, ent->where);
				slowdb__rem_ent_idx(instance, entid);
                break;
            }
        }
        free(k);
    }}

    return slowdb_put(instance, key, keylen, val, vallen);
}

int slowdb_remove(slowdb *instance, const unsigned char *key, int keylen)
{
    int code = 1;
    slowdb__hash_t keyhs = slowdb__hash(key, keylen);
    slowdb__iterPotentialEnt(instance, keyhs, entid)
        slowdb_hashtab_ent* ent = slowdb__ent(instance, entid);
        safe_fseek_set(instance->fp, ent->where);
        slowdb_ent_header header;
        safe_fread(&header, sizeof(header), instance->fp);
        if (!(header.valid & 1) || header.key_len != keylen)
            continue;
        unsigned char * k = (unsigned char *) malloc(header.key_len);
        if (k == NULL) continue;
        safe_fread(k, header.key_len, instance->fp);
        if (!memcmp(key, k, header.key_len)) {
            slowdb__rm(instance, ent->where);
            slowdb__rem_ent_idx(instance, entid);
            free(k);
            code = 0;
            break;
        }
        free(k);
    }}
    return code;
}

static size_t slowdb__get_free_space(slowdb* instance, size_t keylen, size_t vallen)
{
	size_t where = instance->next_new;
    instance->next_new += keylen + vallen + sizeof(slowdb_ent_header);
    return where;
}

int slowdb_put(slowdb *instance, const unsigned char *key, size_t keylen, const unsigned char *val, size_t vallen)
{
	if ((size_t) (uint16_t) keylen != keylen) {
		slowdb__logf(instance, SLOWDB_LOG_LEVEL__ERROR, "key len does not fit in u16");
		return 1;
	}

	if ((size_t) (uint16_t) vallen != vallen) {
		slowdb__logf(instance, SLOWDB_LOG_LEVEL__ERROR,  "val len does not fit in u16");
		return 1;
	}

    slowdb__compress algo = slowdb__select_compress(val, vallen);
    val = slowdb__comp(algo, val, vallen, &vallen);
    if (val == NULL && vallen) return 1;

    size_t where = slowdb__get_free_space(instance, keylen, vallen);
	safe_fseek_set(instance->fp, where);

    slowdb_ent_header header;
    header.key_len = keylen;
    header.data_len = vallen;
    header.valid = (char) (SLOWDB__ENT_MAGIC | 1);
    header.compress = algo;

    safe_fwrite(&header, sizeof(header), instance->fp);
    safe_fseek_set(instance->fp, where + sizeof(header));
    safe_fwrite(key, header.key_len, instance->fp);
    safe_fseek_set(instance->fp, where + sizeof(header) + header.key_len);
    safe_fwrite(val, header.data_len, instance->fp);

    safe_flush(instance->fp);

    slowdb__hash_t hash = slowdb__hash(key, header.key_len);
    slowdb__add_ent_idx(instance, where, hash);

    free((unsigned char *) val);

    return 0;
}
