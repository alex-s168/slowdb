#include "internal.h"

slowdb *slowdb_open(const char *filename)
{
    slowdb* db = (slowdb*) malloc(sizeof(slowdb));
    if (db == NULL) return NULL;

    int fd = open(filename, O_RDWR | O_CREAT, 0666);
    db->fp = (fd == -1) ? NULL : fdopen(fd, "rb+");
    if (db->fp == NULL) {
        free(db);
        return NULL;
    }
    fseek(db->fp, 0, SEEK_SET);

    db->hashtab_buckets = 32;
    db->hashtab = (slowdb_hashtab_bucket *)
        malloc(db->hashtab_buckets * sizeof(slowdb_hashtab_bucket));
    if (db->hashtab == NULL) {
        fclose(db->fp);
        free(db);
        return NULL;
    }
    memset(db->hashtab, 0, db->hashtab_buckets * sizeof(slowdb_hashtab_bucket));

    slowdb_header header;
    db->next_new = sizeof(header);
	if ( fread(&header, 1, sizeof(header), db->fp) != sizeof(header) ) {
		fseek(db->fp, 0, SEEK_SET);
        memcpy(header.magic, slowdb__header_magic, sizeof(slowdb__header_magic));
        fwrite(&header, 1, sizeof(header), db->fp);
    } else {
        if ( memcmp(header.magic, slowdb__header_magic, sizeof(slowdb__header_magic)) ){
            fclose(db->fp);
            free(db);
			fprintf(stderr, "db file magic seq does not match\n");
            return NULL;
        }

		// TODO: WHY REQUIRED
		fseek(db->fp, sizeof(header), SEEK_SET);

        while (1)
        {
            size_t where = ftell(db->fp);
            slowdb_ent_header eh;
            if ( fread(&eh, 1, sizeof(eh), db->fp) != sizeof(eh) )
                break;
			if (! (eh.valid == (char) SLOWDB__ENT_MAGIC || eh.valid == (char) (SLOWDB__ENT_MAGIC | 1))) {
				fprintf(stderr, "corrupted header in database at %zu: ", where);
				char recovered = 0;
				for (size_t i = 0; i < sizeof(eh); i++) {
					if ((((char *) (void*) &eh)[i] & 0b11111110) == SLOWDB__ENT_MAGIC) {
						fprintf(stderr, "recovered with off of %zu bytes\n", i);
						where += i;
						fseek(db->fp, where, SEEK_SET);
            			fread(&eh, 1, sizeof(eh), db->fp);
						recovered = 1;
						break;
					}
				}
				if (!recovered && where > sizeof(eh)) {
					fseek(db->fp, where - sizeof(eh), SEEK_SET);
					fread(&eh, 1, sizeof(eh), db->fp);
					for (size_t i = 0; i < sizeof(eh); i++) {
						if ((((char *) (void*) &eh)[i] & 0b11111110) == SLOWDB__ENT_MAGIC) {
							size_t off = sizeof(eh) - i;
							fprintf(stderr, "recovered with off of -%zu bytes\n", off);
							where -= off;
							fseek(db->fp, where, SEEK_SET);
							fread(&eh, 1, sizeof(eh), db->fp);
							recovered = 1;
							break;
						}
					}
				}
				if (!recovered) {
					fprintf(stderr, "could not recover\n");
					return NULL;
				}
			}

            if (eh.valid & 1) {
                unsigned char * k = (unsigned char *) malloc(eh.key_len);
                if (k) {
                    fread(k, 1, eh.key_len, db->fp);
                    int32_t hash = slowdb__hash(k, eh.key_len);
                    free(k);
                    slowdb__add_ent_idx(db, where, hash);
                }
				db->next_new = where + sizeof(slowdb_ent_header) + eh.data_len + eh.key_len;
            }

            fseek(db->fp, where + sizeof(slowdb_ent_header) + eh.data_len + eh.key_len, SEEK_SET);
        }
    }

    return db;
}

void slowdb_close(slowdb *instance)
{
    fclose(instance->fp);
    for (size_t i = 0; i < instance->hashtab_buckets; i ++)
        free(instance->hashtab[i].items);
    free(instance->hashtab);
    free(instance);
}
