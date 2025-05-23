#ifndef _SLOWDB_H
#define _SLOWDB_H

#include <stddef.h>

typedef struct slowdb slowdb;

/**
 * add a new record to the database. does NOT overwrite old records 
 *
 * returns 0 on success
 */
int slowdb_put(slowdb *instance, const unsigned char *key, size_t keylen, const unsigned char *val, size_t vallen);

/**
 * Get a HEAP ALLOCATED pointer to an existing record. 
 * *vallen contains the size of the returned value in bytes and can be NULL if you are not interested in it.
 *
 * note that the result will be NULL either when an error occured, or when the val len is 0!
 * if the val len is 0, or the entry is not found, it won't overwrite the [vallen] arg
 */ 
unsigned char *slowdb_get(slowdb *instance, const unsigned char *key, int keylen, int *vallen);

/**
 * try to remove an entry from the database 
 *
 * returns 0 if entry found
 */
int slowdb_remove(slowdb *instance, const unsigned char *key, int keylen);

/**
 * replace (or if it does not exist, put) the value of an entry in the database with new data of SAME LENGTH 
 *
 * returns 0 on success.
 */
int slowdb_replaceOrPut(slowdb *instance, const unsigned char *key, int keylen, const unsigned char *val, int vallen);


typedef enum {
    SLOWDB_LOG_LEVEL__VERBOSE = 0,
    SLOWDB_LOG_LEVEL__WARNING,
    SLOWDB_LOG_LEVEL__ERROR,
} slowdb_log_level;

typedef void (*slowdb_logfn)(
        slowdb_log_level level,
        /** null terminated */
        char const* cstr,
        /** identical to strlen(cstr) */
        size_t cstrln,
        void* userdata);

/** future versions might add more fields at the end */
typedef struct {
    size_t offset;
    int valid;
    union { size_t valid; } compressed_keylen;
    union { size_t valid; } compressed_vallen;
} slowdb_open_entcb_info;

typedef void (*slowdb_open_entcb)(
        slowdb *instance,
        slowdb_open_entcb_info const* info,
        void* userdata);


/** you have to create this using [slowdb_open_opts_default()] !!! */
typedef struct {
    int __internal__magic;

    /**
     * default: prints all warnings and errors to stderr, and ignores verbose
     *
     * optional
     */
    slowdb_logfn logfn;
    void* logfn_userdata;

    /**
     * default: 32
     *
     * note that a future implementation may ignore this
     */
    size_t index_num_buckets;

    /**
     * default: true
     */
    int create_if_not_exist;

    /**
     * should try to recover header offsets by searching for magic number?
     * incorrect header offsets could be caused by bugs in the writer.
     * there are currently no known cases in which that might happen.
     *
     * enabling this when your keys or values could contain a high byte value (currently 0b10101010, but might change in the future),
     * is not a good idea, as it could recover incorrect offsets.
     *
     * default: false
     */
    int try_recover_header_offsets;

    struct {
        /** optional */
        slowdb_open_entcb valid_ent;
        void*             valid_ent_userdata;

        /** optional */
        slowdb_open_entcb invalid_ent;
        void*             invalid_ent_userdata;
    } on_read;

    int __internal__magic2;
} slowdb_open_opts;

void slowdb_open_opts_default(slowdb_open_opts* opts);

/**
 * Open existing or create new database.
 * You can get default options by calling [slowdb_open_opts_default()]
 *
 * the options only need to live for as long as this function runs.
 * all values inside options need to live longer tho.
 */
slowdb *slowdb_openx(const char *filename, slowdb_open_opts const* opts);

/** Open existing or create new database */
__attribute__((deprecated("use slowdb_openx() instead")))
slowdb *slowdb_open(const char *filename);

/** 
 * close the database
 * since all changes get instantly written to the database, 
 * this only needs to be called to close the file handle (to make the OS happy) 
 */
void slowdb_close(slowdb *instance);

/**
 * this is completely safe
 * if the reopen failed, it will return a non-zero value, and the instance gets closed.
 */
int slowdb_reopen(slowdb *instance, const char *filename, slowdb_open_opts const* opts);

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
    size_t num_lookup_entries;
} slowdb_stats;

/**
 * this is really slow!
 *
 * the returned instance is identical to doing [slowdb_openx] with the given path and options.
 * because of this, you need to [slowdb_close] the result.
 *
 * on error, returns NULL
 */
slowdb* slowdb_stats_get(char const* path, slowdb_open_opts const* opts, slowdb_stats* out);

#endif
