#include "inc/slowdb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <assert.h>

long rand_range(long min, long max) {
	return rand() % (max + 1 - min) + min; 
}

uint8_t* rand_arr(size_t num, uint8_t min, uint8_t max) {
	uint8_t* ptr = malloc(num + 1);
	ptr[num] = '\0';
	for (size_t i = 0; i < num; i ++)
		ptr[i] = rand_range(min, max);
	return ptr;
}

#define require(v) { assert(v); if (!(v)) exit(1); }

void rand_putrem_test(slowdb* db, uint8_t key_min, uint8_t key_max, uint8_t val_min, uint8_t val_max) {
	size_t keylen = rand_range(10, 200);
	size_t vallen = rand_range(0, 800);
	uint8_t* key = rand_arr(keylen, key_min, key_max);
	uint8_t* val = rand_arr(vallen, val_min, val_max);
	keylen++;
	vallen++;

	slowdb_put(db, key, keylen, val, vallen);

	int vallen2;
	uint8_t* val2 = slowdb_get(db, key, keylen, &vallen2);
	require(val2);
	require(vallen == vallen2);
	require(!memcmp(val2, val, vallen));

	slowdb_remove(db, key, keylen);

	require(slowdb_get(db, key, keylen, NULL) == NULL);

	free(key);
	free(val);
}

#define R_ASCII 1, 127
#define R_BIN   0, 255

int main() {
	srand(time(NULL));

	for (size_t iii = 0; iii < 4; iii ++) {
		slowdb* db = slowdb_open(".test.db");

		puts("ascii - ascii");
		for (size_t i = 0; i < 100; i ++)
			rand_putrem_test(db, R_ASCII, R_ASCII);

		puts("ascii - binary");
		for (size_t i = 0; i < 100; i ++)
			rand_putrem_test(db, R_ASCII, R_BIN);

		puts("binary - ascii");
		for (size_t i = 0; i < 100; i ++)
			rand_putrem_test(db, R_BIN, R_ASCII);

		puts("binary - binary");
		for (size_t i = 0; i < 100; i ++)
			rand_putrem_test(db, R_BIN, R_BIN);

		slowdb_close(db);
	}
}
