#ifndef _SLOWDB_H
#define _SLOWDB_H

#include <stddef.h>

typedef struct slowdb slowdb;

// TODO: use size_t everywhere

/** add a new record to the database. does NOT overwrite old records */
void slowdb_put(slowdb *instance, const unsigned char *key, size_t keylen, const unsigned char *val, size_t vallen);

/**
 * Get a HEAP ALLOCATED pointer to an existing record. 
 * *vallen contains the size of the returned value in bytes and can be NULL if you are not interested in it.
 */ 
unsigned char *slowdb_get(slowdb *instance, const unsigned char *key, int keylen, int *vallen);

/**
 * try to remove an entry from the database 
 */
void slowdb_remove(slowdb *instance, const unsigned char *key, int keylen);

/**
 * replace (or if it does not exist, put) the value of an entry in the database with new data of SAME LENGTH 
 */
void slowdb_replaceOrPut(slowdb *instance, const unsigned char *key, int keylen, const unsigned char *val, int vallen);

/** Open existing or create new database */
slowdb *slowdb_open(const char *filename);

/** 
 * close the database
 * since all changes get instantly written to the database, 
 * this only needs to be called to close the file handle (to make the OS happy) 
 */
void slowdb_close(slowdb *instance);

typedef struct slowdb_iter slowdb_iter;

/** 
 * creates a new iterator over data in the database.
 * there can be multiple iterators at the same time, as long as they do NOT run in parallel.
 * iter has to be closed
 */
slowdb_iter* slowdb_iter_new(slowdb* slowdb);

unsigned char * slowdb_iter_get_key(slowdb_iter* iter, int* lenout);
unsigned char * slowdb_iter_get_val(slowdb_iter* iter, int* lenout);

/** returns 1 if there was a next; ALSO HAS TO BE CALLED BEFORE THE FIRST ELEM */
int slowdb_iter_next(slowdb_iter* iter);

void slowdb_iter_delete(slowdb_iter* iter);

typedef struct {
    size_t num_alive_ents;
    size_t bytes_alive_ents;

    size_t num_dead_ents;
    size_t bytes_dead_ents;

    size_t num_hash_coll;
} slowdb_stats;
void slowdb_stats_get(slowdb* db, slowdb_stats* out);

#endif
