#include "../inc/slowdb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <assert.h>


static long rand_range(long min, long max) {
	if (min > max) {
		long temp = min;
		min = max;
		max = temp;
	}
	long val = min + (long)((double)(max - min + 1) * rand() / (RAND_MAX + 1.0));
    assert(val >= min);
    assert(val <= max);
    return val;
}

static uint8_t* rand_arr(size_t num, uint8_t min, uint8_t max) {
	uint8_t* ptr = (uint8_t*) malloc(num + 1);
    assert(ptr);
	ptr[num] = '\0';
	for (size_t i = 0; i < num; i ++)
		ptr[i] = rand_range(min, max);
	return ptr;
}

static void require_eq__impl(size_t a, size_t b, char const* aa, char const* bb) {
    if (a != b) {
        fprintf(stderr, "assertion failed: %s == %s\n left side was: %zu\n right side was: %zu\n", aa, bb, a, b);
        exit(1);
    }
}

#define require_eq(a,b) require_eq__impl((size_t) a, (size_t) b, #a, #b)
#define require(v) require_eq(1, !!(v))

static void delete_file(char const* path) {
    remove(path);
}
