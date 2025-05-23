#include "internal.h"

unsigned char *slowdb_get(slowdb *instance, const unsigned char *key, int keylen, int *vallen)
{
    slowdb__hash_t keyhs = slowdb__hash(key, keylen);

    slowdb__compress comp;
    size_t found = 0;
    slowdb__iterPotentialEnt(instance, keyhs, entid)
        slowdb_hashtab_ent* ent = slowdb__ent(instance, entid);
        fseek(instance->fp, ent->where, SEEK_SET);
        slowdb_ent_header header;
        fread(&header, 1, sizeof(header), instance->fp);
        if (!(header.valid & 1) || header.key_len != keylen)
            continue;
        unsigned char * k = (unsigned char *) malloc(header.key_len);
        if (k == NULL) continue;
        fread(k, 1, header.key_len, instance->fp);
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
        fread(res, 1, found, instance->fp);
    }

    size_t decomp = 0;
    unsigned char * actual = NULL;
    if (found > 0) {
        actual = slowdb__decomp(comp, res, found, &decomp);
        if (actual == NULL) return NULL;
    }
    if (vallen)
        *vallen = decomp;
    free(res);
    return actual;
}

static void slowdb__rm(slowdb *instance, size_t where)
{
    fseek(instance->fp, where, SEEK_SET);
    slowdb_ent_header header;
    fread(&header, 1, sizeof(header), instance->fp);
    if (!(header.valid & 1))
        return;
    fseek(instance->fp, where, SEEK_SET);
    header.valid = (char) SLOWDB__ENT_MAGIC;
    fwrite(&header, 1, sizeof(header), instance->fp);
	if (where + sizeof(header) + header.key_len + header.data_len == instance->next_new)
		instance->next_new = where;
}

// TODO: get rid of that in the future because of compression
int slowdb_replaceOrPut(slowdb *instance, const unsigned char *key, int keylen, const unsigned char *val, int vallen)
{
    slowdb__hash_t keyhs = slowdb__hash(key, keylen);
    slowdb__iterPotentialEnt(instance, keyhs, entid)
        slowdb_hashtab_ent* ent = slowdb__ent(instance, entid);
        fseek(instance->fp, ent->where, SEEK_SET);
        slowdb_ent_header header;
        fread(&header, 1, sizeof(header), instance->fp);
        if (!(header.valid & 1) || header.key_len != keylen)
            continue;
        unsigned char * k = (unsigned char *) malloc(header.key_len);
        if (k == NULL) continue;
        fread(k, 1, header.key_len, instance->fp);
        if (!memcmp(key, k, header.key_len)) {
            free(k);
            if (header.compress != COMPRESS_NONE) {
                // because compression might make new data len != old data len, remove this entry and add new
                slowdb__rm(instance, ent->where);
				slowdb__rem_ent_idx(instance, entid);
                break;
            }
            else {
                assert(header.data_len == vallen && "cannot slowdb_replaceOrPut() with different vallen than before");
                fseek(instance->fp, ent->where + sizeof(header) + header.key_len, SEEK_SET);
                fwrite(val, 1, vallen, instance->fp);
                return 0;
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
        fseek(instance->fp, ent->where, SEEK_SET);
        slowdb_ent_header header;
        fread(&header, 1, sizeof(header), instance->fp);
        if (!(header.valid & 1) || header.key_len != keylen)
            continue;
        unsigned char * k = (unsigned char *) malloc(header.key_len);
        if (k == NULL) continue;
        fread(k, 1, header.key_len, instance->fp);
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
		fprintf(stderr, "key len does not fit in u16\n");
		exit(1);
	}

	if ((size_t) (uint16_t) vallen != vallen) {
		fprintf(stderr, "val len does not fit in u16\n");
		exit(1);
	}

    slowdb__compress algo = slowdb__select_compress(val, vallen);
    val = slowdb__comp(algo, val, vallen, &vallen);
    if (val == NULL && vallen) return 1;

    size_t where = slowdb__get_free_space(instance, keylen, vallen);
	fseek(instance->fp, where, SEEK_SET);

    slowdb_ent_header header;
    header.key_len = keylen;
    header.data_len = vallen;
    header.valid = (char) (SLOWDB__ENT_MAGIC | 1);
    header.compress = algo;

    fwrite(&header, 1, sizeof(header), instance->fp);
    fseek(instance->fp, where + sizeof(header), SEEK_SET);
    fwrite(key, 1, header.key_len, instance->fp);
    fseek(instance->fp, where + sizeof(header) + header.key_len, SEEK_SET);
    fwrite(val, 1, header.data_len, instance->fp);

    slowdb__hash_t hash = slowdb__hash(key, header.key_len);
    slowdb__add_ent_idx(instance, where, hash);

    free((unsigned char *) val);

    return 0;
}
