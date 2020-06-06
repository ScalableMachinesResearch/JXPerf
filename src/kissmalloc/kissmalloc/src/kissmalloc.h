/*
 * Copyright (C) 2019 Frank Mertens.
 *
 * Distribution and use is allowed under the terms of the zlib license
 * (see kissmalloc/LICENSE).
 *
 */

#pragma once

/// Uncomment the following line to enable overloading of the standard libc memory allocation functions
// #define KISSMALLOC_OVERLOAD_LIBC

#ifdef KISSMALLOC_OVERLOAD_LIBC
#define KISSMALLOC_NAME(function) function
#else
#define KISSMALLOC_NAME(function) kiss ## function
#endif

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

void *KISSMALLOC_NAME(malloc)(size_t size);
void KISSMALLOC_NAME(free)(void *ptr);
void *KISSMALLOC_NAME(calloc)(size_t number, size_t size);
void *KISSMALLOC_NAME(realloc)(void *ptr, size_t size);
int KISSMALLOC_NAME(posix_memalign)(void **ptr, size_t alignment, size_t size);

void *KISSMALLOC_NAME(aligned_alloc)(size_t alignment, size_t size);
void *KISSMALLOC_NAME(memalign)(size_t alignment, size_t size);
void *KISSMALLOC_NAME(valloc)(size_t size);
void *KISSMALLOC_NAME(pvalloc)(size_t size);

ssize_t KISSMALLOC_NAME(memsource)();
size_t KISSMALLOC_NAME(memusage)();

#ifdef __cplusplus
} // extern "C"
#endif
