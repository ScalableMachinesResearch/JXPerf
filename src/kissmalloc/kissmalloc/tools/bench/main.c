/*
 * Copyright (C) 2019 Frank Mertens.
 *
 * Distribution and use is allowed under the terms of the zlib license
 * (see kissmalloc/LICENSE).
 *
 */

#include <kissmalloc.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <time.h>

static int random_get(const int a, const int b)
{
    const unsigned m = (1u << 31) - 1;
    static unsigned x = 7;
    x = (16807 * x) % m;
    return ((uint64_t)x * (b - a)) / (m - 1) + a;
}

static double time_get()
{
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

int main(int argc, char **argv)
{
    const int object_count = 50000000;

    const int size_min = 12;
    const int size_max = 130;

    void **object = malloc(object_count * sizeof(void *));
    int *object_size = malloc(object_count * sizeof(int));

    for (int i = 0; i < object_count; ++i)
        object_size[i] = random_get(size_min, size_max);

    printf(
        "kissmalloc malloc()/free() benchmark\n"
        "------------------------------------\n"
        "\n"
        "n = %d (number of objects)\n"
        "\n",
        object_count
    );

    {
        double t = time_get();

        for (int i = 0; i < object_count; ++i)
            object[i] = malloc(object_size[i]);

        t = time_get() - t;

        printf("malloc() burst speed:\n");
        printf("  t = %f s (test duration)\n", t);
        printf("  n/t = %f MHz (average number of allocations per second)\n", object_count / t / 1e6);
        printf("  t/n = %f ns (average latency of an allocation)\n", t / object_count * 1e9);
        printf("\n");
    }

    {
        double t = time_get();

        for (int i = object_count - 1; i >= 0; --i)
            free(object[i]);

        t = time_get() - t;

        printf("free() burst speed:\n");
        printf("  t = %f s (test duration)\n", t);
        printf("  n/t = %f MHz (average number of deallocations per second)\n", object_count / t / 1e6);
        printf("  t/n = %f ns (average latency of a deallocation)\n", t / object_count * 1e9);
        printf("\n");
    }

    free(object);
    free(object_size);

    return 0;
}
