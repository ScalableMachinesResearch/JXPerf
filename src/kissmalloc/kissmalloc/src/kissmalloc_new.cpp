/*
 * Copyright (C) 2019 Frank Mertens.
 *
 * Distribution and use is allowed under the terms of the zlib license
 * (see kissmalloc/LICENSE).
 *
 */

#include "kissmalloc.h"

#include <new>

#ifndef KISSMALLOC_VALGRIND
#ifndef NDEBUG
#define KISSMALLOC_VALGRIND
#endif
#endif

#ifdef KISSMALLOC_VALGRIND
#include <valgrind/valgrind.h>
#ifndef KISSMALLOC_REDZONE_SIZE
#ifdef NDEBUG
#define KISSMALLOC_REDZONE_SIZE 0
#else
#define KISSMALLOC_REDZONE_SIZE 16
#endif
#endif
#endif

#if __cplusplus <= 199711L // until C++17
#define KISSMALLOC_NOEXCEPT throw()
#define KISSMALLOC_EXCEPT throw(std::bad_alloc)
#define KISSMALLOC_THROW throw std::bad_alloc()
#else // since C++17
#define KISSMALLOC_NOEXCEPT noexcept
#define KISSMALLOC_EXCEPT
#define KISSMALLOC_THROW throw std::bad_alloc{}
#endif // until/since C++17

void *operator new(std::size_t size) KISSMALLOC_EXCEPT
{
    #ifndef KISSMALLOC_VALGRIND
    void *data = KISSMALLOC_NAME(malloc)(size);
    #else
    #ifdef KISSMALLOC_OVERLOAD_LIBC
    void *data = malloc(size);
    #else
    void *data = (void *)((char *)KISSMALLOC_NAME(malloc)(size + 2 * KISSMALLOC_REDZONE_SIZE) + KISSMALLOC_REDZONE_SIZE);
    VALGRIND_MALLOCLIKE_BLOCK(data, size, KISSMALLOC_REDZONE_SIZE, /*is_zeroed=*/true);
    #endif
    #endif
    if (size && !data) KISSMALLOC_THROW;
    return data;
}

void *operator new[](std::size_t size) KISSMALLOC_EXCEPT
{
    #ifndef KISSMALLOC_VALGRIND
    void *data = KISSMALLOC_NAME(malloc)(size);
    #else
    #ifdef KISSMALLOC_OVERLOAD_LIBC
    void *data = malloc(size);
    #else
    void *data = (void *)((char *)KISSMALLOC_NAME(malloc)(size + 2 * KISSMALLOC_REDZONE_SIZE) + KISSMALLOC_REDZONE_SIZE);
    VALGRIND_MALLOCLIKE_BLOCK(data, size, KISSMALLOC_REDZONE_SIZE, /*is_zeroed=*/true);
    #endif
    #endif
    if (size && !data) KISSMALLOC_THROW;
    return data;
}

void *operator new(std::size_t size, const std::nothrow_t &) KISSMALLOC_NOEXCEPT
{
    #ifndef KISSMALLOC_VALGRIND
    void *data = KISSMALLOC_NAME(malloc)(size);
    #else
    #ifdef KISSMALLOC_OVERLOAD_LIBC
    void *data = malloc(size);
    #else
    void *data = (void *)((char *)KISSMALLOC_NAME(malloc)(size + 2 * KISSMALLOC_REDZONE_SIZE) + KISSMALLOC_REDZONE_SIZE);
    VALGRIND_MALLOCLIKE_BLOCK(data, size, KISSMALLOC_REDZONE_SIZE, /*is_zeroed=*/true);
    #endif
    #endif
    return data;
}

void *operator new[](std::size_t size, const std::nothrow_t &) KISSMALLOC_NOEXCEPT
{
    #ifndef KISSMALLOC_VALGRIND
    void *data = KISSMALLOC_NAME(malloc)(size);
    #else
    #ifdef KISSMALLOC_OVERLOAD_LIBC
    void *data = malloc(size);
    #else
    void *data = (void *)((char *)KISSMALLOC_NAME(malloc)(size + 2 * KISSMALLOC_REDZONE_SIZE) + KISSMALLOC_REDZONE_SIZE);
    VALGRIND_MALLOCLIKE_BLOCK(data, size, KISSMALLOC_REDZONE_SIZE, /*is_zeroed=*/true);
    #endif
    #endif
    return data;
}

#if __cplusplus >= 201703L // since C++17

void *operator new(std::size_t size, std::align_val_t alignment)
{
    void *data = nullptr;
    #ifndef KISSMALLOC_VALGRIND
    if (KISSMALLOC_NAME(posix_memalign)(&data, alignment, size) != 0) KISSMALLOC_THROW;
    #else
    #ifdef KISSMALLOC_OVERLOAD_LIBC
    if (posix_memalign(&data, alignment, size) != 0) KISSMALLOC_THROW;
    #else
    if (KISSMALLOC_NAME(posix_memalign)(&data, alignment, size + 2 * KISSMALLOC_REDZONE_SIZE) != 0) KISSMALLOC_THROW;
    if (data) {
        data = (void *)((char *)data + KISSMALLOC_REDZONE_SIZE);
        VALGRIND_MALLOCLIKE_BLOCK(data, size, KISSMALLOC_REDZONE_SIZE, /*is_zeroed=*/true);
    }
    #endif
    #endif
}

void *operator new[](std::size_t size, std::align_val_t alignment)
{
    void *data = nullptr;
    #ifndef KISSMALLOC_VALGRIND
    if (KISSMALLOC_NAME(posix_memalign)(&data, alignment, size) != 0) KISSMALLOC_THROW;
    #else
    #ifdef KISSMALLOC_OVERLOAD_LIBC
    if (posix_memalign(&data, alignment, size) != 0) KISSMALLOC_THROW;
    #else
    if (KISSMALLOC_NAME(posix_memalign)(&data, alignment, size + 2 * KISSMALLOC_REDZONE_SIZE) != 0) KISSMALLOC_THROW;
    if (data) {
        data = (void *)((char *)data + KISSMALLOC_REDZONE_SIZE);
        VALGRIND_MALLOCLIKE_BLOCK(data, size, KISSMALLOC_REDZONE_SIZE, /*is_zeroed=*/true);
    }
    #endif
    #endif
}

void *operator new(std::size_t size, std::align_val_t alignment, const std::nothrow_t &)
{
    void *data = nullptr;
    #ifndef KISSMALLOC_VALGRIND
    KISSMALLOC_NAME(posix_memalign)(&data, alignment, size);
    #else
    #ifdef KISSMALLOC_OVERLOAD_LIBC
    posix_memalign(&data, alignment, size);
    #else
    KISSMALLOC_NAME(posix_memalign)(&data, alignment, size + 2 * KISSMALLOC_REDZONE_SIZE);
    if (data) {
        data = (void *)((char *)data + KISSMALLOC_REDZONE_SIZE);
        VALGRIND_MALLOCLIKE_BLOCK(data, size, KISSMALLOC_REDZONE_SIZE, /*is_zeroed=*/true);
    }
    #endif
    #endif
    return data;
}

void *operator new[](std::size_t size, std::align_val_t alignment, const std::nothrow_t &)
{
    void *data = nullptr;
    #ifndef KISSMALLOC_VALGRIND
    KISSMALLOC_NAME(posix_memalign)(&data, alignment, size);
    #else
    #ifdef KISSMALLOC_OVERLOAD_LIBC
    posix_memalign(&data, alignment, size);
    #else
    KISSMALLOC_NAME(posix_memalign)(&data, alignment, size + 2 * KISSMALLOC_REDZONE_SIZE);
    if (data) {
        data = (void *)((char *)data + KISSMALLOC_REDZONE_SIZE);
        VALGRIND_MALLOCLIKE_BLOCK(data, size, KISSMALLOC_REDZONE_SIZE, /*is_zeroed=*/true);
    }
    #endif
    #endif
    return data;
}

#endif // since C++17

void operator delete(void *data) KISSMALLOC_NOEXCEPT
{
    #ifndef KISSMALLOC_VALGRIND
    KISSMALLOC_NAME(free)(data);
    #else
    #ifdef KISSMALLOC_OVERLOAD_LIBC
    free(data);
    #else
    KISSMALLOC_NAME(free)((void *)((char *)data - KISSMALLOC_REDZONE_SIZE));
    VALGRIND_FREELIKE_BLOCK(data, KISSMALLOC_REDZONE_SIZE);
    #endif
    #endif
}

void operator delete[](void *data) KISSMALLOC_NOEXCEPT
{
    #ifndef KISSMALLOC_VALGRIND
    KISSMALLOC_NAME(free)(data);
    #else
    #ifdef KISSMALLOC_OVERLOAD_LIBC
    free(data);
    #else
    KISSMALLOC_NAME(free)((void *)((char *)data - KISSMALLOC_REDZONE_SIZE));
    VALGRIND_FREELIKE_BLOCK(data, KISSMALLOC_REDZONE_SIZE);
    #endif
    #endif
}

void operator delete(void *data, const std::nothrow_t &) KISSMALLOC_NOEXCEPT
{
    #ifndef KISSMALLOC_VALGRIND
    KISSMALLOC_NAME(free)(data);
    #else
    #ifdef KISSMALLOC_OVERLOAD_LIBC
    free(data);
    #else
    KISSMALLOC_NAME(free)((void *)((char *)data - KISSMALLOC_REDZONE_SIZE));
    VALGRIND_FREELIKE_BLOCK(data, KISSMALLOC_REDZONE_SIZE);
    #endif
    #endif
}

void operator delete[](void *data, const std::nothrow_t &) KISSMALLOC_NOEXCEPT
{
    #ifndef KISSMALLOC_VALGRIND
    KISSMALLOC_NAME(free)(data);
    #else
    #ifdef KISSMALLOC_OVERLOAD_LIBC
    free(data);
    #else
    KISSMALLOC_NAME(free)((void *)((char *)data - KISSMALLOC_REDZONE_SIZE));
    VALGRIND_FREELIKE_BLOCK(data, KISSMALLOC_REDZONE_SIZE);
    #endif
    #endif
}

#if __cplusplus >= 201402L // since C++14

void operator delete(void *data, std::size_t size) noexcept
{
    #ifndef KISSMALLOC_VALGRIND
    KISSMALLOC_NAME(free)(data);
    #else
    #ifdef KISSMALLOC_OVERLOAD_LIBC
    free(data);
    #else
    KISSMALLOC_NAME(free)((void *)((char *)data - KISSMALLOC_REDZONE_SIZE));
    VALGRIND_FREELIKE_BLOCK(data, KISSMALLOC_REDZONE_SIZE);
    #endif
    #endif
}

void operator delete[](void *data, std::size_t size) noexcept
{
    #ifndef KISSMALLOC_VALGRIND
    KISSMALLOC_NAME(free)(data);
    #else
    #ifdef KISSMALLOC_OVERLOAD_LIBC
    free(data);
    #else
    KISSMALLOC_NAME(free)((void *)((char *)data - KISSMALLOC_REDZONE_SIZE));
    VALGRIND_FREELIKE_BLOCK(data, KISSMALLOC_REDZONE_SIZE);
    #endif
    #endif
}

#endif // since C++14

#if __cplusplus >= 201703L // since C++17

void operator delete(void *data, std::align_val_t alignment) noexcept
{
    #ifndef KISSMALLOC_VALGRIND
    KISSMALLOC_NAME(free)(data);
    #else
    #ifdef KISSMALLOC_OVERLOAD_LIBC
    free(data);
    #else
    KISSMALLOC_NAME(free)((void *)((char *)data - KISSMALLOC_REDZONE_SIZE));
    VALGRIND_FREELIKE_BLOCK(data, KISSMALLOC_REDZONE_SIZE);
    #endif
    #endif
}

void operator delete[](void *data, std::align_val_t alignment) noexcept
{
    #ifndef KISSMALLOC_VALGRIND
    KISSMALLOC_NAME(free)(data);
    #else
    #ifdef KISSMALLOC_OVERLOAD_LIBC
    free(data);
    #else
    KISSMALLOC_NAME(free)((void *)((char *)data - KISSMALLOC_REDZONE_SIZE));
    VALGRIND_FREELIKE_BLOCK(data, KISSMALLOC_REDZONE_SIZE);
    #endif
    #endif
}

void operator delete(void *data, std::size_t size, std::align_val_t alignment) noexcept
{
    #ifndef KISSMALLOC_VALGRIND
    KISSMALLOC_NAME(free)(data);
    #else
    #ifdef KISSMALLOC_OVERLOAD_LIBC
    free(data);
    #else
    KISSMALLOC_NAME(free)((void *)((char *)data - KISSMALLOC_REDZONE_SIZE));
    VALGRIND_FREELIKE_BLOCK(data, KISSMALLOC_REDZONE_SIZE);
    #endif
    #endif
}

void operator delete[](void *data, std::size_t size, std::align_val_t alignment) noexcept
{
    #ifndef KISSMALLOC_VALGRIND
    KISSMALLOC_NAME(free)(data);
    #else
    #ifdef KISSMALLOC_OVERLOAD_LIBC
    free(data);
    #else
    KISSMALLOC_NAME(free)((void *)((char *)data - KISSMALLOC_REDZONE_SIZE));
    VALGRIND_FREELIKE_BLOCK(data, KISSMALLOC_REDZONE_SIZE);
    #endif
    #endif
}

#endif // since C++17
