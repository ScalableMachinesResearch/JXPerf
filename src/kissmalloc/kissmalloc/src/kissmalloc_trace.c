/*
 * Copyright (C) 2019 Frank Mertens.
 *
 * Distribution and use is allowed under the terms of the zlib license
 * (see kissmalloc/LICENSE).
 *
 */

#define KISSMALLOC_TRACE_BUF_SIZE 64

inline static char *trace_text(const char *text, char *eoi)
{
    while (*text) {
        *eoi = *text;
        ++eoi;
        ++text;
    }
    return eoi;
}

inline static char *trace_size(size_t size, char *eoi)
{
    int skip_leading = 1;
    for (int i = 15; i >= 0; --i) {
        char digit = (char)((size >> (i << 2)) & 0xF);
        if (skip_leading && digit == 0) continue;
        skip_leading = 0;
        if (digit < 10) digit += '0';
        else digit += -10 + 'a';
        *eoi = digit;
        ++eoi;
    }
    if (skip_leading)
        eoi = trace_text("0", eoi);
    return eoi;
}

inline static char *trace_size_dec(size_t size, char *eoi)
{
    char buf[KISSMALLOC_TRACE_BUF_SIZE];
    int fill = 0;
    while (size > 0) {
        buf[fill] = '0' + (char)(size % 10);
        size /= 10;
        ++fill;
    }
    for (int i = fill - 1; i >= 0; --i) {
        *eoi = buf[i];
        ++eoi;
    }
    return eoi;
}

inline static char *trace_ptr(void *ptr, char *eoi)
{
    return trace_size((uint8_t *)ptr - (uint8_t *)NULL, eoi);
}

inline static void trace_write(const char *message, char *eoi)
{
    write(2, message, eoi - message);
}

inline static void trace_malloc(size_t size)
{
    char message[KISSMALLOC_TRACE_BUF_SIZE];
    char *eoi = message;
    eoi = trace_text("malloc(size=0x", eoi);
    eoi = trace_size(size, eoi);
    eoi = trace_text(")\n", eoi);
    trace_write(message, eoi);
}

inline static void trace_free(void *ptr)
{
    char message[KISSMALLOC_TRACE_BUF_SIZE];
    char *eoi = message;
    eoi = trace_text("free(ptr=0x", eoi);
    eoi = trace_ptr(ptr, eoi);
    eoi = trace_text(")\n", eoi);
    trace_write(message, eoi);
}

inline static void trace_calloc(size_t number, size_t size)
{
    char message[KISSMALLOC_TRACE_BUF_SIZE];
    char *eoi = message;
    eoi = trace_text("calloc(number=0x", eoi);
    eoi = trace_size(number, eoi);
    eoi = trace_text(", size=0x", eoi);
    eoi = trace_size(size, eoi);
    eoi = trace_text(")\n", eoi);
    trace_write(message, eoi);
}

inline static void trace_realloc(void *ptr, size_t size)
{
    char message[KISSMALLOC_TRACE_BUF_SIZE];
    char *eoi = message;
    eoi = trace_text("realloc(ptr=0x", eoi);
    eoi = trace_ptr(ptr, eoi);
    eoi = trace_text(", size=0x", eoi);
    eoi = trace_size(size, eoi);
    eoi = trace_text(")\n", eoi);
    trace_write(message, eoi);
}

inline static void trace_posix_memalign(void **ptr, size_t alignment, size_t size)
{
    char message[KISSMALLOC_TRACE_BUF_SIZE];
    char *eoi = message;
    eoi = trace_text("posix_memalign(ptr=0x", eoi);
    eoi = trace_ptr(ptr, eoi);
    eoi = trace_text(", alignment=0x", eoi);
    eoi = trace_size(alignment, eoi);
    eoi = trace_text(", size=0x", eoi);
    eoi = trace_size(size, eoi);
    eoi = trace_text(")\n", eoi);
    trace_write(message, eoi);
}

inline static void trace_aligned_alloc(size_t alignment, size_t size)
{
    char message[KISSMALLOC_TRACE_BUF_SIZE];
    char *eoi = message;
    eoi = trace_text("aligned_alloc(alignment=0x", eoi);
    eoi = trace_size(alignment, eoi);
    eoi = trace_text(", size=0x", eoi);
    eoi = trace_size(size, eoi);
    eoi = trace_text(")\n", eoi);
    trace_write(message, eoi);
}

inline static void trace_memalign(size_t alignment, size_t size)
{
    char message[KISSMALLOC_TRACE_BUF_SIZE];
    char *eoi = message;
    eoi = trace_text("memalign(alignment=0x", eoi);
    eoi = trace_size(alignment, eoi);
    eoi = trace_text(", size=0x", eoi);
    eoi = trace_size(size, eoi);
    eoi = trace_text(")\n", eoi);
    trace_write(message, eoi);
}

inline static void trace_valloc(size_t size)
{
    char message[KISSMALLOC_TRACE_BUF_SIZE];
    char *eoi = message;
    eoi = trace_text("valloc(size=0x", eoi);
    eoi = trace_size(size, eoi);
    eoi = trace_text(")\n", eoi);
    trace_write(message, eoi);
}

inline static void trace_pvalloc(size_t size)
{
    char message[KISSMALLOC_TRACE_BUF_SIZE];
    char *eoi = message;
    eoi = trace_text("pvalloc(size=0x", eoi);
    eoi = trace_size(size, eoi);
    eoi = trace_text(")\n", eoi);
    trace_write(message, eoi);
}

inline static void trace_inspect_size(const char *source, int line, const char *name, size_t size)
{
    char message[2 * KISSMALLOC_TRACE_BUF_SIZE];
    char *eoi = message;
    eoi = trace_text(source, eoi);
    eoi = trace_text(":", eoi);
    eoi = trace_size_dec(line, eoi);
    eoi = trace_text(": ", eoi);
    eoi = trace_text(name, eoi);
    eoi = trace_text("=0x", eoi);
    eoi = trace_size(size, eoi);
    eoi = trace_text("\n", eoi);
    trace_write(message, eoi);
}

inline static void *trace_inspect_ptr(const char *source, int line, const char *name, void *ptr)
{
    char message[2 * KISSMALLOC_TRACE_BUF_SIZE];
    char *eoi = message;
    eoi = trace_text(source, eoi);
    eoi = trace_text(":", eoi);
    eoi = trace_size_dec(line, eoi);
    eoi = trace_text(": ", eoi);
    eoi = trace_text(name, eoi);
    eoi = trace_text("=0x", eoi);
    eoi = trace_ptr(ptr, eoi);
    eoi = trace_text("\n", eoi);
    trace_write(message, eoi);
    return ptr;
}

inline static void trace_debug(const char *source, int line, const char *text)
{
    char message[2 * KISSMALLOC_TRACE_BUF_SIZE];
    char *eoi = message;
    eoi = trace_text(source, eoi);
    eoi = trace_text(":", eoi);
    eoi = trace_size_dec(line, eoi);
    eoi = trace_text(": ", eoi);
    eoi = trace_text(text, eoi);
    eoi = trace_text("\n", eoi);
    trace_write(message, eoi);
}

#define KISSMALLOC_INSPECT_SIZE(size) \
    trace_inspect_size(__FILE__, __LINE__, #size, size)

#define KISSMALLOC_INSPECT_PTR(ptr) \
    trace_inspect_ptr(__FILE__, __LINE__, #ptr, ptr)

#define KISSMALLOC_DEBUG(text) \
    trace_debug(__FILE__, __LINE__, text)
