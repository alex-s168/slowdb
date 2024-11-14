/*

Copyright 2024 Alexander Nutz

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 */ 

#include <stdio.h>
#define SLOWDB_IMPL
#include "slowdb.h"
#include <stdbool.h>

static bool is_c_str(unsigned char * data, int len)
{
    if (len == 0 || data[len-1] != '\0')
        return false;
    for (int i = 0; i < len - 1; i ++)
        if (data[i] == '\0')
            return false;
    return true;
}

static int r_info(slowdb* db, int argc, char** argv)
{
    (void) argv;

    if (argc != 0) {
        fprintf(stderr, "invalid amount of arguments for op\n");
        return 1;
    }

    slowdb_stats stats;
    slowdb_stats_get(db, &stats);

    printf("alive entries:    %zu (%zu B)\n", stats.num_alive_ents, stats.bytes_alive_ents);
    printf("dead entries:     %zu (%zu B)\n", stats.num_dead_ents, stats.bytes_dead_ents);
    printf("hash collissions: %zu\n", stats.num_hash_coll);

    return 0;
}


static int r_list(slowdb* db, int argc, char** argv)
{
    (void) argv;

    if (argc != 0) {
        fprintf(stderr, "invalid amount of arguments for op\n");
        return 1;
    }

    slowdb_iter i = slowdb_iter_new(db);

    while (slowdb_iter_next(&i))
    {
        int keylen;
        unsigned char * key = slowdb_iter_get_key(&i, &keylen);
        if (key == NULL) continue;

        int vallen;
        unsigned char * val = slowdb_iter_get_val(&i, &vallen);
        if (val == NULL) continue;

        if (is_c_str(key, keylen))
            printf("\"%s\": ", (char *) key);
        else printf("<binary>: ");

        if (is_c_str(val, vallen))
            printf("\"%s\"\n", val);
        else printf("<binary>\n");

        free(key);
        free(val);
    }

    return 0;
}

static int r_get(slowdb* db, int argc, char** argv)
{
    if (argc != 1) {
        fprintf(stderr, "invalid amount of arguments for op\n");
        return 1;
    }

    int vallen;
    unsigned char * data = slowdb_get(db,
            (const unsigned char *) argv[0], strlen(argv[0]) + 1,
            &vallen);

    if (data == NULL) {
        fprintf(stderr, "key not found in db\n");
        return 1;
    }

    fwrite(data, 1, vallen, stdout);

    return 0;
}

static int r_rm(slowdb* db, int argc, char** argv)
{
    if (argc != 1) {
        fprintf(stderr, "invalid amount of arguments for op\n");
        return 1;
    }

    slowdb_remove(db,
            (const unsigned char *) argv[0], strlen(argv[0]) + 1);

    return 0;
}

static int r_put(slowdb* db, int argc, char** argv)
{
    if (argc != 2) {
        fprintf(stderr, "invalid amount of arguments for op\n");
        return 1;
    }

    slowdb_remove(db,
            (const unsigned char *) argv[0], strlen(argv[0]) + 1);

    slowdb_put(db,
            (const unsigned char *) argv[0], strlen(argv[0]) + 1,
            (const unsigned char *) argv[1], strlen(argv[1]) + 1);

    return 0;
}

int main(int argc, char** argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: slowdb [operation] [file] args...\n");
        fprintf(stderr, "operations:\n");
        fprintf(stderr, "   create             if the db does not exists, creates it\n");
        fprintf(stderr, "   info               view information about the database\n");
        fprintf(stderr, "   list               list all entries in the database\n");
        fprintf(stderr, "   get  [key]         find a specific entry\n");
        fprintf(stderr, "   rm   [key]         remove a specific entry\n");
        fprintf(stderr, "   put  [key] [value] add or replace a entry\n");
        exit(1);
    }

    const char * op = argv[1];

    slowdb* db = slowdb_open(argv[2]);
    if (db == NULL) {
        fprintf(stderr, "could not open database!\n");
        return 1;
    }

    int status;

    if (!strcmp(op, "list"))
        status = r_list(db, argc - 3, argv + 3);
    else if (!strcmp(op, "info"))
        status = r_info(db, argc - 3, argv + 3);
    else if (!strcmp(op, "get"))
        status = r_get(db, argc - 3, argv + 3);
    else if (!strcmp(op, "rm"))
        status = r_rm(db, argc - 3, argv + 3);
    else if (!strcmp(op, "put"))
        status = r_put(db, argc - 3, argv + 3);
    else if (!strcmp(op, "create"))
        status = 0;
    else {
        fprintf(stderr, "invalid operation\n");
        status = 1;
    }

    slowdb_close(db);
    return status;
}

