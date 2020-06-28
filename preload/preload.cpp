#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <unordered_map>
#include "lock.h"

typedef void* (*memmove_t)(void*, const void*, size_t);
memmove_t real_memmove = NULL;

typedef struct {
    const void *src;
    size_t n;
} relocation_info_t;

SpinLock relocation_map_lock;
std::unordered_map<void*, relocation_info_t> relocation_map={};

static void setup_real_functions(){ 
  real_memmove = (memmove_t)dlsym(RTLD_NEXT, "memmove");
  if (NULL == real_memmove){
    fprintf(stderr, "Error in dlsym(): %s\n", dlerror());
  }
}

static inline void *call_real_memmove(void *dest, const void *src, size_t n){
  if (real_memmove == NULL){
     setup_real_functions();
  }
  return real_memmove(dest, src, n);
}

void* memmove(void *dest, const void *src, size_t n) {
  void *ptr = call_real_memmove(dest, src, n);

  relocation_map_lock.lock();
  relocation_map[dest].src = src;
  relocation_map[dest].n = n;
  relocation_map_lock.unlock();

  return ptr;
}

