#include "util.h"

static size_t next_keylen;
static size_t next_vallen;

#define KEYLEN_ITERS 270
#define KEYLEN_STEP 1

#define VALLEN_ITERS 270
#define VALLEN_STEP 1

#define NUM_REOPEN_PER_VALLEN 4

void rand_putrem_test(slowdb* db, uint8_t key_min, uint8_t key_max, uint8_t val_min, uint8_t val_max) {
    assert(db);
	size_t keylen = next_keylen;
	size_t vallen = next_vallen;

	uint8_t* key = rand_arr(keylen, key_min, key_max);
	uint8_t* val = rand_arr(vallen, val_min, val_max);

	require(!slowdb_put(db, key, keylen, val, vallen));

	int vallen2 = -1;
	uint8_t* val2 = slowdb_get(db, key, keylen, &vallen2);
	require(vallen == 0 || val2);
    require(vallen2 != -1);
	require_eq(vallen, (size_t)vallen2);
	require(!memcmp(val2, val, vallen));

	require(!slowdb_remove(db, key, keylen));
	require(slowdb_get(db, key, keylen, NULL) == NULL);

	free(key);
	free(val);

    next_keylen += KEYLEN_STEP;
}

#define R_ASCII 1, 127
#define R_BIN   0, 255

int main() {
    // even though that is bad practice in a test, it allows us to potentially catch more issues than fixing the seed.
	srand(time(NULL));

    slowdb_open_opts opts;
    slowdb_open_opts_default(&opts);

    delete_file(".test.db");

    puts("inserts and deletes");
    next_vallen = 0;
	for (size_t iii = 0; iii < VALLEN_ITERS; iii ++) 
    {
        for (size_t i = 0; i < NUM_REOPEN_PER_VALLEN; i ++)
        {
            //puts("reopen");
            slowdb* db = slowdb_openx(".test.db", &opts);
            require(db);

            next_keylen = 1;
            //puts(" ascii  - ascii");
            for (size_t i = 0; i < KEYLEN_ITERS; i ++)
                rand_putrem_test(db, R_ASCII, R_ASCII);

            next_keylen = 1;
            //puts(" ascii  - binary");
            for (size_t i = 0; i < KEYLEN_ITERS; i ++)
                rand_putrem_test(db, R_ASCII, R_BIN);

            next_keylen = 1;
            //puts(" binary - ascii");
            for (size_t i = 0; i < KEYLEN_ITERS; i ++)
                rand_putrem_test(db, R_BIN, R_ASCII);

            next_keylen = 1;
            //puts(" binary - binary");
            for (size_t i = 0; i < KEYLEN_ITERS; i ++)
                rand_putrem_test(db, R_BIN, R_BIN);

            slowdb_close(db);
        }

        next_vallen += VALLEN_STEP;
	}
}
