#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <dlfcn.h>
#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include "preload.h"

__thread bool inside_sig_unsafe_func = false;

#define BLOCK_SAMPLE (inside_sig_unsafe_func = true)
#define UNBLOCK_SAMPLE (inside_sig_unsafe_func = false)

/*malloc*/
typedef void* (*malloc_t)(size_t);
malloc_t real_malloc = NULL;

/*calloc*/
typedef void* (*calloc_t)(size_t,size_t);
calloc_t real_calloc = NULL;

/*realloc*/
typedef void* (*realloc_t)(void *, size_t);
realloc_t real_realloc = NULL;

/*free*/

typedef void (*free_t)(void *);
free_t real_free = NULL;

mallochook_t malloc_hook_fn = NULL;

void setMallocHookFunction(mallochook_t fn){
  malloc_hook_fn = fn;
}

static void setup_real_functions(){
  //set real_malloc
  real_malloc = (malloc_t)dlsym(RTLD_NEXT, "malloc");
  if (NULL == real_malloc){
    fprintf(stderr, "Error in dlsym(): %s\n", dlerror());
  }

  real_calloc = (calloc_t)dlsym(RTLD_NEXT, "calloc");
  if (NULL == real_calloc){
    fprintf(stderr, "Error in dlsym(): %s\n", dlerror());
  }
 
  real_realloc = (realloc_t)dlsym(RTLD_NEXT, "realloc");
  if (NULL == real_realloc){
    fprintf(stderr, "Error in dlsym(): %s\n", dlerror());
  }
  real_free = (free_t)dlsym(RTLD_NEXT, "free");
  if (NULL == real_free){
    fprintf(stderr, "Error in dlsym(): %s\n", dlerror());
  }
}

static inline void *call_real_malloc(size_t size){
  if (real_malloc == NULL){
     setup_real_functions();
  }
  return real_malloc(size);
}

/* calloc() should be handled differently.
   The function dlsym() will invoke calloc() which causes infinite recursion.
   It seems that calloc() can just return NULL even called by dlsym().
*/
static void *call_temp_calloc(size_t nmemb, size_t size){
  return NULL;
}

static inline void *call_real_calloc(size_t nmemb, size_t size){
  if (real_calloc == NULL){
     real_calloc = call_temp_calloc;
     setup_real_functions();
  }
  return real_calloc(nmemb, size);
}

static inline void *call_real_realloc(void *ptr, size_t size){
  if (real_realloc == NULL){
     setup_real_functions();
  }
  return real_realloc(ptr, size);
}

static inline void call_real_free(void *ptr){
  if (real_free == NULL){
     setup_real_functions();
  }
  real_free(ptr);
}


void *malloc(size_t size){
    BLOCK_SAMPLE;
    void *ptr = call_real_malloc(size);
    UNBLOCK_SAMPLE;
    return ptr;
}

void *calloc(size_t nmemb, size_t size){
    BLOCK_SAMPLE;
    void *ptr = call_real_calloc(nmemb,size);
    UNBLOCK_SAMPLE;
    return ptr;
}

void *realloc(void *ptr, size_t size){
    BLOCK_SAMPLE;
    void *ret_ptr = call_real_realloc(ptr, size);
    UNBLOCK_SAMPLE;
    return ret_ptr;
}

void free(void *ptr){
    BLOCK_SAMPLE;
    call_real_free(ptr);
    UNBLOCK_SAMPLE;
}
