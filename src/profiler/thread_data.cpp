#define _GNU_SOURCE 1 
#include <unistd.h>
#include <sys/syscall.h>
#include <pthread.h>
#include <string.h>
#include <new>
#include "thread_data.h"
#include "debug.h"

static pthread_key_t key;

namespace ThreadData {

void thread_data_init() {
    if(pthread_key_create(&key, NULL) != 0) {
        ERROR("pthread_key_create() failed\n");
        assert(false);
    }
}

thread_data_t *thread_data_alloc() {
    thread_data_t *td_ptr = new(std::nothrow) thread_data_t();
    assert(td_ptr);
    memset(td_ptr, 0, sizeof(thread_data_t));
    // set some fields
    td_ptr->tid = syscall(__NR_gettid);
    td_ptr->inside_agent = false;
    pthread_setspecific(key, (void *) td_ptr);
    return td_ptr;
}

thread_data_t *thread_data_get() {
    thread_data_t *ret = (thread_data_t*) pthread_getspecific(key);
    if (ret == nullptr) {
       ret = thread_data_alloc();
    }
    return ret;
} 

void thread_data_dealloc() {
    thread_data_t *td_ptr = (thread_data_t*) pthread_getspecific(key);
    assert (td_ptr != nullptr);
    // make sure each field is properly freed
    assert(td_ptr->perf_state == nullptr);
    assert(td_ptr->context_state == nullptr);
#ifndef COUNT_OVERHEAD 
    assert(td_ptr->output_state == nullptr);
#endif
#ifdef PRINT_PMU_INS
    assert(td_ptr->pmu_ins_output_stream == nullptr);
#endif
    for (int i =0; i < 4; i++) assert(td_ptr->ctxt_sample_state[i] == nullptr);
    delete td_ptr;
    td_ptr = nullptr;
    pthread_setspecific(key, nullptr);
}

void thread_data_shutdown(){
    pthread_key_delete(key);
}

}
