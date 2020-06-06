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
#include <stdarg.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

static void thread_printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    const int buf_size = 128;
    char buf[128];
    const int buf_fill = vsnprintf(buf, buf_size, format, args);
    if (buf_fill > 0) write(0, buf, buf_fill);
}

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

typedef struct {
    int thread_id;
    int object_count;
    int *object_size;
    void **object;
} thread_state_t;

static void *thread_run_malloc(void *arg)
{
    thread_state_t *state = (thread_state_t *)arg;

    for (int k = 0; k < state->object_count; ++k) {
        state->object[k] = malloc(state->object_size[k]);
        // memset(state->object[k], k, state->object_size[k]);
    }

    return NULL;
}

static void *thread_run_free(void *arg)
{
    thread_state_t *state = (thread_state_t *)arg;

    for (int k = 0; k < state->object_count; ++k)
        free(state->object[k]);

    return NULL;
}

int main(int argc, char **argv)
{
    const int thread_count = 8;
    const int object_count = 10000000;
    const int size_min = 12;
    const int size_max = 130;

    printf(
        "kissmalloc threads malloc()/free() benchmark\n"
        "--------------------------------------------\n"
        "\n"
        "n = %d (number of objects)\n"
        "m = %d (number of threads)\n"
        "\n",
        object_count,
        thread_count
    );

    thread_state_t thread_state[thread_count];

    for (int i = 0; i < thread_count; ++i)
    {
        thread_state_t *state = &thread_state[i];

        state->thread_id = i;
        state->object_count = object_count;

        state->object_size = malloc(object_count * sizeof(int));
        state->object = malloc(object_count * sizeof(void *));

        for (int k = 0; k < object_count; ++k)
            state->object_size[k] = random_get(size_min, size_max);
    }

    {
        pthread_t thread[thread_count];

        double t = time_get();

        for (int i = 0; i < thread_count; ++i) {
            if (pthread_create(&thread[i], NULL,  &thread_run_malloc, &thread_state[i]) != 0)
                thread_printf("failed to create thread %d\n", 1);
        }

        for (int i = 0; i < thread_count; ++i) {
            if (pthread_join(thread[i], NULL) != 0)
                thread_printf("failed to wait for thread %d\n", i);
        }

        t = time_get() - t;

        printf("malloc() burst speed:\n");
        printf("  t = %f s (test duration)\n", t);
        printf("  n/t = %f MHz (average number of allocations per second, all threads combined)\n", thread_count * object_count / t / 1e6);
        printf("  n/t = %f MHz (average number of allocations per second)\n", object_count / t / 1e6);
        printf("  t/n = %f ns (average latency of an allocation)\n", t / object_count * 1e9);
        printf("\n");
    }

    {
        pthread_t thread[thread_count];

        double t = time_get();

        for (int i = 0; i < thread_count; ++i) {
            if (pthread_create(&thread[i], NULL, &thread_run_free, &thread_state[i]) != 0)
                thread_printf("failed to create thread %d\n", 1);
        }

        for (int i = 0; i < thread_count; ++i) {
            if (pthread_join(thread[i], NULL) != 0)
                thread_printf("failed to wait for thread %d\n", i);
        }

        t = time_get() - t;

        printf("free() burst speed:\n");
        printf("  t = %f s (test duration)\n", t);
        printf("  n/t = %f MHz (average number of deallocations per second, all threads combined)\n", thread_count * object_count / t / 1e6);
        printf("  n/t = %f MHz (average number of deallocations per second)\n", object_count / t / 1e6);
        printf("  t/n = %f ns (average latency of an deallocation)\n", t / object_count * 1e9);
        printf("\n");
    }

    for (int i = 0; i < thread_count; ++i)
    {
        thread_state_t *state = &thread_state[i];

        free(state->object);
        free(state->object_size);
    }

    return 0;
}
