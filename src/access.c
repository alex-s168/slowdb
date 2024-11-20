#include "internal.h"

unsigned char *slowdb_get(slowdb *instance, const unsigned char *key, int keylen, int *vallen)
{
    int32_t keyhs = slowdb__hash(key, keylen);

    int found = 0;
    slowdb__iterPotentialEnt(instance, keyhs, entid, ({
        fseek(instance->fp, instance->ents[entid], SEEK_SET);
        slowdb_ent_header header;
        fread(&header, 1, sizeof(header), instance->fp);
        if (!(header.valid & 1) || header.key_len != keylen)
            continue;
        unsigned char * k = (unsigned char *) malloc(header.key_len);
        if (k == NULL) continue;
        fread(k, 1, header.key_len, instance->fp);
        if (!memcmp(key, k, header.key_len)) {
            found = header.data_len;
            free(k);
            break;
        }
        free(k);
    }));

    if (vallen)
        *vallen = found;

    if (!found) return NULL;

    unsigned char * res = (unsigned char*) malloc(found);
    if (res == NULL) return NULL;

    fread(res, 1, found, instance->fp);

    return res;
}

void slowdb_replaceOrPut(slowdb *instance, const unsigned char *key, int keylen, const unsigned char *val, int vallen)
{
    int32_t keyhs = slowdb__hash(key, keylen);
    slowdb__iterPotentialEnt(instance, keyhs, entid, ({
        fseek(instance->fp, instance->ents[entid], SEEK_SET);
        slowdb_ent_header header;
        fread(&header, 1, sizeof(header), instance->fp);
        if (!(header.valid & 1) || header.key_len != keylen)
            continue;
        unsigned char * k = (unsigned char *) malloc(header.key_len);
        if (k == NULL) continue;
        fread(k, 1, header.key_len, instance->fp);
        if (!memcmp(key, k, header.key_len)) {
            assert(header.data_len == vallen && "cannot slowdb_replaceOrPut() with different vallen than before");
            fwrite(val, 1, vallen, instance->fp);
            free(k);
            return;
        }
        free(k);
    }));

    (void) slowdb_put(instance, key, keylen, val, vallen);
}

void slowdb_remove(slowdb *instance, const unsigned char *key, int keylen)
{
    int32_t keyhs = slowdb__hash(key, keylen);
    slowdb__iterPotentialEnt(instance, keyhs, entid, ({
        fseek(instance->fp, instance->ents[entid], SEEK_SET);
        slowdb_ent_header header;
        fread(&header, 1, sizeof(header), instance->fp);
        if (!(header.valid & 1) || header.key_len != keylen)
            continue;
        unsigned char * k = (unsigned char *) malloc(header.key_len);
        if (k == NULL) continue;
        fread(k, 1, header.key_len, instance->fp);
        if (!memcmp(key, k, header.key_len)) {
            fseek(instance->fp, instance->ents[entid], SEEK_SET);
            header.valid = (char) SLOWDB__ENT_MAGIC;
            fwrite(&header, 1, sizeof(header), instance->fp);
            free(k);
            break;
        }
        free(k);
    }));
}

void slowdb_put(slowdb *instance, const unsigned char *key, int keylen, const unsigned char *val, int vallen)
{
	size_t where;

	if (instance->ents_len == 0) {
		where = sizeof(slowdb_header);
	} else {
		size_t where_last = instance->ents[instance->ents_len - 1];

		fseek(instance->fp, where_last, SEEK_SET);
        slowdb_ent_header header;
        fread(&header, 1, sizeof(header), instance->fp);

        if (header.valid & 1) {
        	where = where_last + sizeof(slowdb_ent_header) + header.key_len + header.data_len;
		} else {
			where = where_last;
		}
	}

	fseek(instance->fp, where, SEEK_SET);

    slowdb_ent_header header;
    header.key_len = keylen;
    header.data_len = vallen;
    header.valid = (char) (SLOWDB__ENT_MAGIC | 1);

    fwrite(&header, 1, sizeof(header), instance->fp);
    fwrite(key, 1, header.key_len, instance->fp);
    fwrite(val, 1, header.data_len, instance->fp);

    int32_t hash = slowdb__hash(key, header.key_len);
    slowdb__add_ent_idx(instance, where, hash);
}
