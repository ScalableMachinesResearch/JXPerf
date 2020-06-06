#include <sys/types.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

#include <assert.h>
#include <perfmon/pfmlib_perf_event.h>
#include "perf_util.h"
#include "debug.h"
#include <linux/version.h>

bool perf_encode_event(const std::string &event, struct perf_event_attr *event_attr){
    pfm_perf_encode_arg_t arg;
    char *fqstr = NULL;

    event_attr->size = sizeof(struct perf_event_attr);
    memset(&arg, 0, sizeof(arg));
    arg.fstr = &fqstr; /* out/in: fully qualified event string */
    arg.size = sizeof(pfm_perf_encode_arg_t);
    arg.attr = event_attr;
    int ret = pfm_get_os_event_encoding(event.c_str(), PFM_PLM0|PFM_PLM3, PFM_OS_PERF_EVENT_EXT, &arg);
    if (ret == PFM_SUCCESS) {
        free(fqstr);
        return true;
    }
    return false;
}

bool perf_attr_init(struct perf_event_attr *attr, uint64_t threshold, uint64_t more_sample_type){
    uint64_t sample_type = more_sample_type;
    sample_type |= PERF_SAMPLE_IP;
    sample_type |= PERF_SAMPLE_CALLCHAIN;
    sample_type |= PERF_SAMPLE_ADDR;
    sample_type |= PERF_SAMPLE_CPU;

    attr->sample_type = sample_type;
    attr->size   = sizeof(struct perf_event_attr); /* Size of attribute structure */
    // attr->sample_max_stack = PERF_MAX_STACK_DEPTH; // libpfm 4.8 does not support it
    attr->exclude_callchain_kernel = 1;
    attr->freq   = 0;
    attr->sample_period = threshold;
    if (threshold == 0){
        attr->read_format = PERF_FORMAT_TOTAL_TIME_ENABLED|PERF_FORMAT_TOTAL_TIME_RUNNING;
    }
    else {
        attr->read_format = 0;
    }
    attr->disabled = 1; /* the counter will be enabled later  */
    attr->wakeup_events = 1; /* overflow notifications happen after wakeup_events samples */ 

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)
    attr->exclude_callchain_user   = EXCLUDE_CALLCHAIN;
    attr->exclude_callchain_kernel = EXCLUDE_CALLCHAIN;
#endif
    
    attr->exclude_kernel = 1;
    attr->exclude_hv     = 1;
    attr->exclude_idle   = 1;

    return true;
}


bool perf_read_event_counter(int fd, uint64_t *val){
    if (fd < 0){
        ERROR("Unable to open the event %d file descriptor", fd);
        return false;
    }
    int ret = read(fd, val, sizeof(uint64_t) * 3 );
    if (ret < sizeof(uint64_t)*3) {
        ERROR("Unable to read the event %d file descriptor", fd);
        return false;
    }
    return true;
}
