#ifndef _SAFE_SAMPLING_H_
#define _SAFE_SAMPLING_H_

#include "thread_data.h"

static inline bool profiler_safe_enter() {
    
    bool prev = TD_GET(inside_agent);
    TD_GET(inside_agent) = true;
    return (prev == false);
}

static inline void profiler_safe_exit() {
    TD_GET(inside_agent) = false;
}

#endif 
