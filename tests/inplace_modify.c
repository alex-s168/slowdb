#include "util.h"

#define INPLACE_MODIFIES_REOPENS 10
#define INPLACE_MODIFIES_PER_REOPEN 50

int main() {
    slowdb_open_opts opts;
    slowdb_open_opts_default(&opts);

    delete_file(".test.db");

    puts("inplace modifies 1/6 : insert");
    for (size_t i = 0; i < INPLACE_MODIFIES_REOPENS; i ++) {
        slowdb* db = slowdb_openx(".test.db", &opts);
        require(db);
        for (size_t j = 0; j < INPLACE_MODIFIES_PER_REOPEN; j ++) {
            unsigned id = i * INPLACE_MODIFIES_PER_REOPEN + j;
            unsigned val = id * 3;
            require(0 == slowdb_put(db, (unsigned char*) &id, sizeof(id), (unsigned char*) &val, sizeof(val)));
        }
        slowdb_close(db);
    }

    puts("inplace modifies 2/6 : get&check");
    for (size_t i = 0; i < INPLACE_MODIFIES_REOPENS; i ++) {
        slowdb* db = slowdb_openx(".test.db", &opts);
        require(db);
        for (size_t j = 0; j < INPLACE_MODIFIES_PER_REOPEN; j ++) {
            unsigned id = i * INPLACE_MODIFIES_PER_REOPEN + j;
            unsigned expected_val = id * 3;
            int vallen = -1;
            unsigned char* ptr = slowdb_get(db, (unsigned char*) &id, sizeof(id), &vallen);
            require_eq(vallen, sizeof(unsigned));
            require(ptr);
            unsigned actual_val = *(unsigned*)ptr;
            free(ptr);

            require_eq(expected_val, actual_val);
        }
        slowdb_close(db);
    }

    puts("inplace modifies 3/6 : inplace replace");
    for (size_t i = 0; i < INPLACE_MODIFIES_REOPENS; i ++) {
        slowdb* db = slowdb_openx(".test.db", &opts);
        require(db);
        for (size_t j = 0; j < INPLACE_MODIFIES_PER_REOPEN; j ++) {
            unsigned id = i * INPLACE_MODIFIES_PER_REOPEN + j;
            unsigned val = id * 2;
            require(0 == slowdb_replaceOrPut(db, (unsigned char*) &id, sizeof(id), (unsigned char*) &val, sizeof(val)));
        }
        slowdb_close(db);
    }

    puts("inplace modifies 4/6 : get&check");
    for (size_t i = 0; i < INPLACE_MODIFIES_REOPENS; i ++) {
        slowdb* db = slowdb_openx(".test.db", &opts);
        require(db);
        for (size_t j = 0; j < INPLACE_MODIFIES_PER_REOPEN; j ++) {
            unsigned id = i * INPLACE_MODIFIES_PER_REOPEN + j;
            unsigned expected_val = id * 2;
            int vallen = -1;
            unsigned char* ptr = slowdb_get(db, (unsigned char*) &id, sizeof(id), &vallen);
            require_eq(vallen, sizeof(unsigned));
            require(ptr);
            unsigned actual_val = *(unsigned*)ptr;
            free(ptr);

            require_eq(expected_val, actual_val);
        }
        slowdb_close(db);
    }

    puts("inplace modifies 5/6 : delete");
    for (size_t i = 0; i < INPLACE_MODIFIES_REOPENS; i ++) {
        slowdb* db = slowdb_openx(".test.db", &opts);
        require(db);
        for (size_t j = 0; j < INPLACE_MODIFIES_PER_REOPEN; j ++) {
            unsigned id = i * INPLACE_MODIFIES_PER_REOPEN + j;
            require("was inside db" && 0 == slowdb_remove(db, (unsigned char*) &id, sizeof(id)));
        }
        slowdb_close(db);
    }

    puts("inplace modifies 6/6 : get & check deleted");
    for (size_t i = 0; i < INPLACE_MODIFIES_REOPENS; i ++) {
        slowdb* db = slowdb_openx(".test.db", &opts);
        require(db);
        for (size_t j = 0; j < INPLACE_MODIFIES_PER_REOPEN; j ++) {
            unsigned id = i * INPLACE_MODIFIES_PER_REOPEN + j;
            int vallen = -1;
            unsigned char* ptr = slowdb_get(db, (unsigned char*) &id, sizeof(id), &vallen);
            require(ptr == NULL);
            require_eq(vallen, -1);
        }
        slowdb_close(db);
    }
}
