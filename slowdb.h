// define SLOWDB_IMPL and include

/*

Copyright 2024 Alexander Nutz

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 */ 

#ifndef _SLOWDB_H
#define _SLOWDB_H

#include <stddef.h>

typedef struct slowdb slowdb;

// Add a new record, returns slowdb_error on fail
void slowdb_put(slowdb *instance, const unsigned char *key, int keylen, const unsigned char *val, int vallen);

// Get a HEAP ALLOCATED pointer to an existing record. *vallen contains the size of the returned value 
// in bytes and can be NULL if you are not interested in it.
unsigned char *slowdb_get(slowdb *instance, const unsigned char *key, int keylen, int *vallen);

void slowdb_remove(slowdb *instance, const unsigned char *key, int keylen);

// replace data with new data of SAME VAL LEN 
void slowdb_replaceOrPut(slowdb *instance, const unsigned char *key, int keylen, const unsigned char *val, int vallen);

// Open existing, or create new
slowdb *slowdb_open(const char *filename);

// Close. Must be called to ensure all changes are written to disk
void slowdb_close(slowdb *instance);

typedef struct {
    slowdb *_db;
    size_t _ent_id;
} slowdb_iter;

slowdb_iter slowdb_iter_new(slowdb* slowdb);
// if this returns NULL, it means that the current item is not valid and you need to go to the next item
unsigned char * slowdb_iter_get_key(slowdb_iter* iter, int* lenout);
// if this returns NULL, it means that the current item is not valid and you need to go to the next item
unsigned char * slowdb_iter_get_val(slowdb_iter* iter, int* lenout);
// returns 1 if there was a next; ALSO HAS TO BE CALLED BEFORE THE FIRST ELEM
int slowdb_iter_next(slowdb_iter* iter);

typedef struct {
    size_t num_alive_ents;
    size_t bytes_alive_ents;

    size_t num_dead_ents;
    size_t bytes_dead_ents;

    size_t num_hash_coll;
} slowdb_stats;
void slowdb_stats_get(slowdb* db, slowdb_stats* out);

/* ========================================================================== */

#define SLOWDB_IMPL // TODO: REMOVE

#ifdef SLOWDB_IMPL

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

struct slowdb_hashtab_ent {
    int32_t hash;
    uint16_t entid;
};

struct slowdb_hashtab_bucket {
    struct slowdb_hashtab_ent * items;
    size_t count;
};

struct slowdb {
    FILE* fp;

    size_t*  ents;

    size_t hashtab_buckets;
    struct slowdb_hashtab_bucket * hashtab;

    size_t   ents_len;
};

#define SLOWDB__ENT_MAGIC (0b10101010)

struct slowdb_ent_header {
    char valid;
    uint16_t key_len;
    uint16_t data_len;
} __attribute__((packed));

static char slowdb_header_magic[8] = {':', 's', 'l', 'o', 'w', 'd', 'b', '1'};

struct slowdb_header {
    char magic[sizeof(slowdb_header_magic)];
} __attribute__((packed));

static int32_t slowdb__hash(const unsigned char * data, int len)
{
    int32_t r = 0;
    for (int i = 0; i < len; i ++) {
        r = data[i] + (r << 6) + (r << 16) - r;
    }
    return r;
}

static void slowdb__dap(slowdb* db, size_t where, int32_t hash)
{
    db->ents = (size_t*)
        realloc(db->ents, sizeof(size_t) * (db->ents_len + 1));
    db->ents[db->ents_len] = where;

    struct slowdb_hashtab_bucket * bucket = &db->hashtab[((size_t ) hash) % db->hashtab_buckets];

    struct slowdb_hashtab_ent htent;
    htent.hash = hash;
    htent.entid = db->ents_len;
    bucket->items = (struct slowdb_hashtab_ent *)
        realloc(bucket->items, sizeof(struct slowdb_hashtab_ent) * (bucket->count + 1));
    bucket->items[bucket->count ++] = htent;

    db->ents_len ++;
}

#include <fcntl.h>
#ifdef _WIN32 
# include <io.h>
#endif

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
    rewind(db->fp);

    db->ents = NULL;
    db->ents_len = 0;

    db->hashtab_buckets = 16;
    db->hashtab = (struct slowdb_hashtab_bucket *)
        malloc(db->hashtab_buckets * sizeof(struct slowdb_hashtab_bucket));
    memset(db->hashtab, 0, db->hashtab_buckets * sizeof(struct slowdb_hashtab_bucket));

    struct slowdb_header header;
	fread(&header, 1, sizeof(header), db->fp);
    if ( feof(db->fp) ) {
        fseek(db->fp, 0, SEEK_SET);
        memcpy(header.magic, slowdb_header_magic, sizeof(slowdb_header_magic));
        fwrite(&header, 1, sizeof(header), db->fp);
    }
    else {
        if ( memcmp(header.magic, slowdb_header_magic, sizeof(slowdb_header_magic)) ){
            fclose(db->fp);
            free(db);
			printf("file magic seq does not match\n");
            return NULL;
        }

        while (1)
        {
            size_t where = ftell(db->fp);
            struct slowdb_ent_header eh;
            if ( fread(&eh, 1, sizeof(eh), db->fp) != sizeof(eh) )
                break;
			if (! (eh.valid == (char) SLOWDB__ENT_MAGIC || eh.valid == (char) (SLOWDB__ENT_MAGIC | 1))) {
				printf("corrupted header in database at %zu; trying to recover\n", where);
				char recovered = 0;
				for (size_t i = 0; i < sizeof(eh); i++) {
					if ((((char *) (void*) &eh)[i] & 0b11111110) == SLOWDB__ENT_MAGIC) {
						printf("header was off by %zu bytes\n", i);
						where += i;
						fseek(db->fp, where, SEEK_SET);
            			fread(&eh, 1, sizeof(eh), db->fp);
						recovered = 1;
						break;
					}
				}
				if (!recovered) {
					printf("could not recover\n");
					return NULL;
				}
			}

            if (eh.valid & 1) {
                unsigned char * k = (unsigned char *) malloc(eh.key_len);
                fread(k, 1, eh.key_len, db->fp);
                int32_t hash = slowdb__hash(k, eh.key_len);
                free(k);

                slowdb__dap(db, where, hash);
            }

            fseek(db->fp, where + sizeof(struct slowdb_ent_header) + eh.data_len + eh.key_len, SEEK_SET);
        }
    }

    return db;
}

// 0 = no next 
#define slowdb__iterPotentialEnt(db, hashh, idVar, fn) \
{ \
    struct slowdb_hashtab_bucket bucket = \
        (db)->hashtab[((size_t) hashh) % (db)->hashtab_buckets]; \
\
    for (size_t __buck = 0; __buck < bucket.count; __buck ++) \
    { \
        if (bucket.items[__buck].hash == (hashh)) \
        { \
            uint16_t idVar = bucket.items[__buck].entid; \
            fn; \
        } \
    } \
}

unsigned char *slowdb_get(slowdb *instance, const unsigned char *key, int keylen, int *vallen)
{
    int32_t keyhs = slowdb__hash(key, keylen);

    int found = 0;
    slowdb__iterPotentialEnt(instance, keyhs, entid, ({
        fseek(instance->fp, instance->ents[entid], SEEK_SET);
        struct slowdb_ent_header header;
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
        struct slowdb_ent_header header;
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
        struct slowdb_ent_header header;
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
		where = sizeof(struct slowdb_header);
	} else {
		size_t where_last = instance->ents[instance->ents_len - 1];

		fseek(instance->fp, where_last, SEEK_SET);
        struct slowdb_ent_header header;
        fread(&header, 1, sizeof(header), instance->fp);

        if (header.valid & 1) {
        	where = where_last + sizeof(struct slowdb_ent_header) + header.key_len + header.data_len;
		} else {
			where = where_last;
		}
	}

	fseek(instance->fp, where, SEEK_SET);

    struct slowdb_ent_header header;
    header.key_len = keylen;
    header.data_len = vallen;
    header.valid = (char) (SLOWDB__ENT_MAGIC | 1);

    fwrite(&header, 1, sizeof(header), instance->fp);
    fwrite(key, 1, header.key_len, instance->fp);
    fwrite(val, 1, header.data_len, instance->fp);

    int32_t hash = slowdb__hash(key, header.key_len);
    slowdb__dap(instance, where, hash);
}

void slowdb_close(slowdb *instance)
{
    fclose(instance->fp);
    free(instance->ents);
    for (size_t i = 0; i < instance->hashtab_buckets; i ++)
        free(instance->hashtab[i].items);
    free(instance->hashtab);
    free(instance);
}

slowdb_iter slowdb_iter_new(slowdb* slowdb)
{
    return (slowdb_iter) { ._db = slowdb, ._ent_id = SIZE_MAX };
}

/** returns 1 if valdi */
static int slowdb__iter_seek_and_get_header(slowdb_iter* iter, struct slowdb_ent_header* header)
{
    fseek(iter->_db->fp, iter->_db->ents[iter->_ent_id], SEEK_SET);
    fread(header, 1, sizeof(*header), iter->_db->fp);
    return (header->valid & 1);
}

unsigned char * slowdb_iter_get_key(slowdb_iter* iter, int* lenout)
{
    struct slowdb_ent_header header;
	if (!slowdb__iter_seek_and_get_header(iter, &header)) return NULL;

    unsigned char * k = (unsigned char *) malloc(header.key_len);
    if (k == NULL) return NULL;
    fread(k, 1, header.key_len, iter->_db->fp);
    if (lenout) *lenout = header.key_len;
    return k;
}

unsigned char * slowdb_iter_get_val(slowdb_iter* iter, int* lenout)
{
    struct slowdb_ent_header header;
	if (!slowdb__iter_seek_and_get_header(iter, &header)) return NULL;

	fseek(iter->_db->fp, header.key_len, SEEK_CUR);

    unsigned char * v = (unsigned char *) malloc(header.data_len);
    if (v == NULL) return NULL;
    fread(v, 1, header.data_len, iter->_db->fp);
    if (lenout) *lenout = header.data_len;
    return v;
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
		struct slowdb_ent_header header;

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
        struct slowdb_hashtab_bucket bucket = db->hashtab[i];

        for (size_t j = 0; j < bucket.count; j ++)
            for (size_t k = 0; k < j; k ++)
                if (bucket.items[k].hash == bucket.items[j].hash)
                    out->num_hash_coll ++;
    }
}

#endif

#endif
