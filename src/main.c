#include "internal.h"
#include <assert.h>

#define OPEN_OPTS_MAGIC (0b11110011)

void logfn__default(
        slowdb_log_level level,
        char const* cstr,
        size_t cstrln,
        void* userdata)
{
    char const* lvlstr;

    switch (level)
    {
        case SLOWDB_LOG_LEVEL__VERBOSE:
            return;

        case SLOWDB_LOG_LEVEL__WARNING:
            lvlstr = "WARN ";
            break;

        case SLOWDB_LOG_LEVEL__ERROR:
            lvlstr = "ERROR";
            break;
    }

    fprintf(stderr, "[%s] %s\n", lvlstr, cstr);
}

void slowdb_open_opts_default(slowdb_open_opts* opts)
{
    memset(opts, 0, sizeof(*opts));
    opts->__internal__magic = OPEN_OPTS_MAGIC;
    opts->__internal__magic2 = OPEN_OPTS_MAGIC;

    opts->logfn = logfn__default;
    opts->index_num_buckets = 32;
    opts->create_if_not_exist = 1;
    opts->try_recover_header_offsets = 1;
}

slowdb *slowdb_open(const char *filename)
{
    slowdb_open_opts opts;
    slowdb_open_opts_default(&opts);
    return slowdb_openx(filename, &opts);
}

static slowdb *slowdb__open_inner(slowdb* db, const char *filename, slowdb_open_opts const* opts)
{
    assert(opts->__internal__magic == OPEN_OPTS_MAGIC);
    assert(opts->__internal__magic2 == OPEN_OPTS_MAGIC);

    db->logfn = opts->logfn;
    db->logfn_userdata =
        opts->logfn ? opts->logfn_userdata : 0; // for when the user does GC, and didn't zero logfn_userdata

    int oflags = O_RDWR;
    if (opts->create_if_not_exist)
        oflags |= O_CREAT;

    int fd = open(filename, oflags, 0666);
    db->fp = (fd == -1) ? NULL : fdopen(fd, "rb+");
    if (db->fp == NULL) {
        slowdb__logf(db, SLOWDB_LOG_LEVEL__ERROR, "could not open file");
        free(db);
        return NULL;
    }
    safe_fseek_set(db->fp, 0);

    db->hashtab_buckets = opts->index_num_buckets;
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
		safe_fseek_set(db->fp, 0);
        memcpy(header.magic, slowdb__header_magic, sizeof(slowdb__header_magic));
        safe_fwrite(&header, sizeof(header), db->fp);
    } else {
        if ( memcmp(header.magic, slowdb__header_magic, sizeof(slowdb__header_magic)) ){
            fclose(db->fp);
            free(db->hashtab);
            free(db);
            slowdb__logf(db, SLOWDB_LOG_LEVEL__ERROR, "db file magic seq does not match");
            return NULL;
        }

		// TODO: WHY REQUIRED
		safe_fseek_set(db->fp, sizeof(header));

        while (1)
        {
            size_t where = ftell(db->fp);
            slowdb_ent_header eh;
            if ( fread(&eh, 1, sizeof(eh), db->fp) != sizeof(eh) )
                break;
			if (! (eh.valid == (char) SLOWDB__ENT_MAGIC || eh.valid == (char) (SLOWDB__ENT_MAGIC | 1))) {
                if (opts->try_recover_header_offsets) {
                    int recovered = 0;
                    for (size_t i = 0; i < sizeof(eh); i++) {
                        if ((((char *) (void*) &eh)[i] & 0b11111110) == SLOWDB__ENT_MAGIC) {
                            slowdb__logf(db, SLOWDB_LOG_LEVEL__WARNING,
                                    "Corrupted entry header at %zu: recovered with offset of +%zu bytes", where, i);
                            where += i;
                            safe_fseek_set(db->fp, where);
                            safe_fread(&eh, sizeof(eh), db->fp);
                            recovered = 1;
                            break;
                        }
                    }
                    if (!recovered && where > sizeof(eh)) {
                        safe_fseek_set(db->fp, where - sizeof(eh));
                        safe_fread(&eh, sizeof(eh), db->fp);
                        for (size_t i = 0; i < sizeof(eh); i++) {
                            if ((((char *) (void*) &eh)[i] & 0b11111110) == SLOWDB__ENT_MAGIC) {
                                size_t off = sizeof(eh) - i;
                                slowdb__logf(db, SLOWDB_LOG_LEVEL__WARNING,
                                        "Corrupted entry header at %zu: recovered with offset of -%zu bytes", where, off);
                                where -= off;
                                safe_fseek_set(db->fp, where);
                                safe_fread(&eh, sizeof(eh), db->fp);
                                recovered = 1;
                                break;
                            }
                        }
                    }
                    if (!recovered) {
                        slowdb__logf(db, SLOWDB_LOG_LEVEL__ERROR, "Corrupted entry header at %zu: could not recover!", where);
                        slowdb_close(db);
                        return NULL;
                    }
                } else
                {
                    slowdb__logf(db, SLOWDB_LOG_LEVEL__ERROR, "Corrupted entry header at %zu!", where);
                    slowdb_close(db);
                    return NULL;
                }
			}

            slowdb_open_entcb_info info;
            info.valid = eh.valid & 1;
            info.compressed_keylen.valid = eh.key_len;
            info.compressed_vallen.valid = eh.data_len;

            if (eh.valid & 1)
            {
                if (opts->on_read.valid_ent)
                    opts->on_read.valid_ent(db, &info, opts->on_read.valid_ent_userdata);

                unsigned char * k = (unsigned char *) malloc(eh.key_len);
                if (k) {
                    safe_fread(k, eh.key_len, db->fp);
                    int32_t hash = slowdb__hash(k, eh.key_len);
                    free(k);
                    slowdb__add_ent_idx(db, where, hash);
                }
				db->next_new = where + sizeof(slowdb_ent_header) + eh.data_len + eh.key_len;
            } else
            {
                if (opts->on_read.invalid_ent)
                    opts->on_read.invalid_ent(db, &info, opts->on_read.invalid_ent_userdata);
            }

            safe_fseek_set(db->fp, where + sizeof(slowdb_ent_header) + eh.data_len + eh.key_len);
        }
    }

    return db;
}

slowdb *slowdb_openx(const char *filename, slowdb_open_opts const* opts)
{
    slowdb* db = (slowdb*) malloc(sizeof(slowdb));
    if (db == NULL) return NULL;
    return slowdb__open_inner(db, filename, opts);
}

static void slowdb__close_inner(slowdb *instance)
{
    fclose(instance->fp);
    for (size_t i = 0; i < instance->hashtab_buckets; i ++)
        free(instance->hashtab[i].items);
    free(instance->hashtab);
    instance->logfn_userdata = 0; // if the user uses GC on that
}

void slowdb_close(slowdb *instance)
{
    slowdb__close_inner(instance);
    free(instance);
}

/** this is completely safe */
int slowdb_reopen(slowdb *instance, const char *filename, slowdb_open_opts const* opts)
{
    slowdb__close_inner(instance);
    if ( slowdb__open_inner(instance, filename, opts) == NULL )
        return 1;
    return 0;
}
