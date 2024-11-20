#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>

#define max(a, b) ((a > b) ? a : b)

#include "unishox2/unishox2.h"
#define STRPACK_IMPLEMENT
#include "strpack/strpack.h"

#define NUM_ITER (50000)

typedef struct { int a, b; } percent;

static percent to_percent(float f) {
    f = roundf(f * 10000);
    int a = ((int) f) / 100;
    int b = ((int) f) % 100;
    return (percent) { .a = a, .b = b };
}

static void test(const char * str)
{
    clock_t start, end;

    char out[strlen(str) + 128];

    start = clock();
    int unish;
    for (size_t i = 0; i < NUM_ITER; i ++)
        unish = unishox2_compress_simple(str, strlen(str), out);
    end = clock();
    float unish_ms = ((float)(end - start) / CLOCKS_PER_SEC) * 1000 * 1000000 / NUM_ITER;
    percent unishp = to_percent(1.0f - ((float) unish) / ((float) strlen(str)));

    start = clock();
    int strpk;
    for (size_t i = 0; i < NUM_ITER; i ++)
        strpk = strpack_compress(str, out, sizeof(out), NULL);
    end = clock();
    float strpk_ms = ((float)(end - start) / CLOCKS_PER_SEC) * 1000 * 1000000 / NUM_ITER;
    percent strpkp = to_percent(1.0f - ((float) strpk) / ((float) strlen(str)));

    start = clock();
    for (size_t i = 0; i < NUM_ITER; i ++)
        memcpy(out, str, strlen(str));
    end = clock();
    float memcpy_ms = ((float)(end - start) / CLOCKS_PER_SEC) * 1000 * 1000000 / NUM_ITER;

    printf("\n\"%s\"\n", str);
    printf("  memcpy  : %zu -> %zu (%i.%i%%) in %ins\n", strlen(str), strlen(str), 0, 0, (int) memcpy_ms);
    printf("  strpack : %zu -> %zu (%i.%i%%) in %ins\n", strlen(str), (size_t) strpk, strpkp.a, strpkp.b, (int) strpk_ms);
    printf("  unishox2: %zu -> %zu (%i.%i%%) in %ins\n", strlen(str), (size_t) unish, unishp.a, unishp.b, (int) unish_ms);
}

int main()
{
    test("This is a Cool Example Str,, Ing");
    test("~/some/path/to/some-file.c");
    test("hello");
    test("/usr/include/someeee/reaaAlylA/loNNG/path.h");
    test("Example Name");
    test("Some Stret 20 Aprt.4, 1234 Some City, Austria");
}
