#define _GNU_SOURCE 1
#include <sys/time.h>
#include <sys/types.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <assert.h>
#include <new>
#include <fstream>
#include <boost/algorithm/string.hpp>

#include "perf_interface.h"
#include <perfmon/pfmlib_perf_event.h>
#include "perf_util.h"
#include "perf_mmap.h"
#include "debug.h"
#include "agent.h"
#include "watchpoint.h"
#include "thread_data.h"
#include "metrics.h"
#include "profiler_support.h"
#include "safe-sampling.h"

#define PERF_SIGNAL (SIGRTMIN+4)
#define MAX_NUM_EVENTS 10 // not the theoretical maximum

typedef struct {
    int id;
    int metric_id1;
    int metric_id2;
    int metric_id3;
    std::string name;
    uint32_t threshold;
    struct perf_event_attr attr;
} perf_event_info_t;


typedef struct {
    int id;
    int fd;
    perf_mmap_t *mmap_buf;
    perf_event_info_t *event;
} perf_event_thread_t;



/******** Global Variables ********************/
static perf_event_info_t event_info[MAX_NUM_EVENTS];
static uint32_t num_events = 0;
static sample_cb_t user_sample_cb = nullptr;
/*********************************************/

//extern __thread bool inside_sig_unsafe_func = false;

namespace {

// Helper class to store and reset errno when in a signal handler.
class ErrnoIR{
public:
    ErrnoIR() { stored_errno_ = errno; }
    ~ErrnoIR() { errno = stored_errno_; }
private:
    int stored_errno_;
};


bool installSignal(void (*sig_handler)(int, siginfo_t *, void *) ){
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, PERF_SIGNAL);

  // Setup the signal handler
  struct sigaction sa;
  // memset(&sa, 0, sizeof(struct sigaction));
  sa.sa_sigaction = sig_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
  // sa.sa_flags = SA_SIGINFO;

  // sigset_t block_mask;
  // sigfillset(&block_mask);
  // sa.sa_mask = set; // only block the perf signal
  // sa.sa_mask = block_mask; // block all signals during the execution of this signal

  if (sigaction(PERF_SIGNAL, &sa, NULL) != 0) {
    ERROR("sigaction() failed\n");
    return false;
  }

  if (pthread_sigmask(SIG_UNBLOCK, &set, NULL)) {
    ERROR("pthread_sigmask(SIG_UNBLOCK)");
    return false;
  }

  return true;
}


bool process_event_list(const std::vector<std::string> &event_list){
    num_events = event_list.size();
    assert(num_events <= MAX_NUM_EVENTS); // We have a limit as to the number of events

    for(uint32_t i=0; i < num_events; i++ ){
        perf_event_info_t &current_event_info = event_info[i];
        const std::string &event = event_list[i];
        //find the client name, event name and the period
        std::size_t pos = event.find("::");
        if(pos == std::string::npos) { // not found
            ERROR("Can't get client name");
            assert(false);
        }
        std::string client_name = event.substr(0, pos);
	boost::to_upper(client_name);

        std::size_t pos2 = event.find("@");
        if(pos2 != std::string::npos) { // found "@"
            current_event_info.name = event.substr(pos+2, pos2-pos-2);
            std::string sample_rate_str = event.substr(pos2+1, std::string::npos);
            current_event_info.threshold = std::stoull(sample_rate_str, nullptr, 10);
            std::string sample_precise_level = event.substr(pos2-1, 1);
            current_event_info.attr.precise_ip = std::stoull(sample_precise_level, nullptr, 10);
        } else {
            current_event_info.name = event;
            current_event_info.attr.precise_ip = 3;
            current_event_info.threshold = 0;
        }
        INFO("client name = %s event name = %s rate = %u\n", client_name.c_str(), current_event_info.name.c_str(),  current_event_info.threshold);

        // encode the event
        if (!perf_encode_event(current_event_info.name, &(current_event_info.attr))){
            ERROR("Can't encode %s", current_event_info.name.c_str());
            assert(false);
        }
        // setup other generic attributes
        if (!perf_attr_init(&(current_event_info.attr), current_event_info.threshold, PERF_SAMPLE_TID)){
            ERROR("Can't init attribute for event %s", current_event_info.name.c_str());
            assert(false);
        }

        //create the corresponding metric
        metrics::metric_info_t metric_info;
        metric_info.client_name = client_name;
        metric_info.name = current_event_info.name;
        metric_info.threshold = current_event_info.threshold;
        metric_info.val_type = metrics::METRIC_VAL_INT;
        current_event_info.metric_id1 = metrics::MetricInfoManager::registerMetric(metric_info);
        current_event_info.metric_id2 = metrics::MetricInfoManager::registerMetric(metric_info);
        current_event_info.metric_id3 = metrics::MetricInfoManager::registerMetric(metric_info);
        
        // extern void SetupWatermarkMetric(int);
        // Watchpoint
        SetupWatermarkMetric(current_event_info.metric_id1);
        SetupWatermarkMetric(current_event_info.metric_id2);
        SetupWatermarkMetric(current_event_info.metric_id3);
    }
    return true;
}

bool restart_perf_event(perf_event_thread_t &event_thread) {
    if( event_thread.fd < 0){
        ERROR("Unable to start the event %s: fd is not valid.", event_thread.event->name.c_str());
        return false;
    }
    if (ioctl(event_thread.fd, PERF_EVENT_IOC_RESET, 0) < 0){
        ERROR("Error fd %d in PERF_EVENT_IOC_RESET: %s", event_thread.fd, strerror(errno));
        return false;
    }
    if (ioctl(event_thread.fd, PERF_EVENT_IOC_REFRESH, 1) < 0){
        ERROR("Error fd %d in PERF_EVENT_IOC_REFRESH: %s", event_thread.fd, strerror(errno));
        return false;
    }
    return true;
}

bool restart_perf_event(int fd){
    if (fd < 0){
        ERROR("Unable to start the event, fd is not valid.");
        return false;
    }
    if (ioctl(fd, PERF_EVENT_IOC_RESET, 0) < 0){
        ERROR("Error fd %d in PERF_EVENT_IOC_RESET: %s", fd, strerror(errno));
        return false;
    }
    if (ioctl(fd, PERF_EVENT_IOC_REFRESH , 1) < 0){
        ERROR("Error fd %d in PERF_EVENT_IOC_REFRESH: %s", fd, strerror(errno));
        return false;
    }
    return true;
}

bool perf_start_all(std::vector<perf_event_thread_t> &event_thread_list ){
    for(auto &event_thread: event_thread_list) {
        int fd = event_thread.fd;
        if (fd < 0) continue;
        if (ioctl(fd, PERF_EVENT_IOC_ENABLE, 0) < 0) {
            ERROR("Enable the event %s failed: %s", event_thread.event->name.c_str(), strerror(errno));
            return false;
        }
    }
    return true;
}

bool perf_stop_all(std::vector<perf_event_thread_t> &event_thread_list){
    for(auto &event_thread: event_thread_list) {
        int fd = event_thread.fd;
        if (fd < 0) continue;
        if (ioctl(fd, PERF_EVENT_IOC_DISABLE, 0) < 0) {
            ERROR("Disable the event %s failed: %s", event_thread.event->name.c_str(), strerror(errno));
            return false;
        }
    }
    return true;
}

void perf_event_handler(int sig, siginfo_t* siginfo, void* context){
    // ----------------------------------------------------------------------------
    // check #0:
    // if the interrupt came while inside our code, then drop the sample
    // and return and avoid the potential for deadlock.
    // ----------------------------------------------------------------------------
    if (!profiler_safe_enter()) return;

    /* if (WP_IsInsideSigHandler() || inside_sig_unsafe_func) {
        profiler_safe_exit();
        return; 
    }*/
    
    ErrnoIR err_storage;
    std::vector<perf_event_thread_t> *event_thread_list = reinterpret_cast<std::vector<perf_event_thread_t> *> (TD_GET(perf_state));
    assert(event_thread_list);

    //------------------------------------------------------------
    // disable all counters
    //------------------------------------------------------------
    assert(perf_stop_all(*event_thread_list));

    //------------------------------------------------------------
    // check #1: check if signal generated by kernel for profiling
    //------------------------------------------------------------
    if (siginfo->si_code < 0){
        WARNING("signal si_code %d < 0 indicates not from kernel.", siginfo->si_code);
        assert(perf_start_all(*event_thread_list));
        profiler_safe_exit();
        return;  // the signal has not been handled
    }


    //------------------------------------------------------------
    // check #2: we expect POLL_HUP, not POLL_IN; or maybe not true???
    //------------------------------------------------------------
#if 0
    if (siginfo->si_code != POLL_HUP){
        assert(restart_perf_event(siginfo->si_fd));
        assert(perf_start_all(*event_thread_list));
        profiler_safe_exit();
        return; // the signal has not been handled
    }
#endif

    //------------------------------------------------------------
    // check #3: check whether the file descripter is on the list we have
    //------------------------------------------------------------
    perf_event_thread_t *current = nullptr;
    for( auto &event_thread : (*event_thread_list)){
        if(event_thread.fd == siginfo->si_fd) {
            current = &(event_thread);
            break;
        }
    }
    if (current == nullptr) {
        WARNING("signal si_code %d with fd %d: unknown perf event", siginfo->si_code, siginfo->si_fd);
        assert(restart_perf_event(siginfo->si_fd));
        assert(perf_start_all(*event_thread_list));
        profiler_safe_exit();
        return;
    }
    
    //------------------------------------------------------------
    // parse the buffer
    //------------------------------------------------------------
    assert(user_sample_cb != nullptr);
    
    int remaining_data_size;
    while ((remaining_data_size = perf_num_of_remaining_data(current->mmap_buf)) > 0) {
        struct perf_event_header ehdr;
        if (remaining_data_size < (int)sizeof(ehdr)) {
            perf_skip_all(current->mmap_buf);
            break;
        }
        assert(perf_read_header(current->mmap_buf, &ehdr));
	if (ehdr.type == PERF_RECORD_SAMPLE) {
            perf_sample_data_t sample_data;
            memset(&sample_data, 0, sizeof(perf_sample_data_t));
	        sample_data.isPrecise = (ehdr.misc & PERF_RECORD_MISC_EXACT_IP) ? true : false;
            perf_read_record_sample(current->mmap_buf, current->event->attr.sample_type, &sample_data);
            //if (!inside_sig_unsafe_func)
                user_sample_cb(current->id, &sample_data, context, current->event->metric_id1, current->event->metric_id2, current->event->metric_id3);
        }
        else {
            if (ehdr.size == 0) {
                perf_skip_all(current->mmap_buf);
            }
            else {
                perf_skip_record(current->mmap_buf, &ehdr);
            }
        }
    }
    
    //------------------------------------------------------------
    // Finished processing this sample.
    //------------------------------------------------------------
    assert(restart_perf_event(*current));
    assert(perf_start_all(*event_thread_list));
    profiler_safe_exit();
    return;
}

}


bool PerfManager::processInit(const std::vector<std::string> &event_list, sample_cb_t sample_cb) {
    if (pfm_initialize() != PFM_SUCCESS){
        ERROR("pfm_initialize() failed");
        return false;
    }
    user_sample_cb = sample_cb;
    assert(process_event_list(event_list));
    perf_mmap_init();
    // if (!installSegvSignal(segv_handler)) return false;
    if (!installSignal(perf_event_handler)) return false;
    return true;
}

void PerfManager::processShutdown(){
  pfm_terminate();
}


bool PerfManager::setupEvents(){

    std::vector<perf_event_thread_t> *event_thread_list = reinterpret_cast<std::vector<perf_event_thread_t> *> (TD_GET(perf_state));
    if (event_thread_list == nullptr){
        event_thread_list = new(std::nothrow) std::vector<perf_event_thread_t>();
        assert(event_thread_list);
        TD_GET(perf_state) = event_thread_list;
    }
    pid_t tid = TD_GET(tid);

    for(uint32_t i = 0; i < num_events; i++) {
        perf_event_thread_t event_thread;
        event_thread.id = i;
        event_thread.fd = perf_event_open(&(event_info[i].attr), tid, -1, -1, 0);
        if (event_thread.fd < -1){
            ERROR("perf_event_open() failed, cannot open event %s: %s", event_thread.event->name.c_str(), strerror(errno));
            return false;
        }
        ioctl(event_thread.fd,PERF_EVENT_IOC_RESET,0);

        int flags = fcntl( event_thread.fd , F_GETFL, 0);
        if (flags < 0) {
            ERROR("fcntl(%d, F_GETFL) failed: %s", event_thread.fd, strerror(errno));
            return false;
        }
        if (fcntl(event_thread.fd, F_SETFL, flags| O_ASYNC) < 0) {
            ERROR("fcntl(%d, F_SETFL, %d | PERF_SIGNAL| O_ASYNC) failed: %s", event_thread.fd, flags, strerror(errno));
            return false;
        }

        // set the file descriptor owner to this specific thread
        struct f_owner_ex fown_ex;
        fown_ex.type = F_OWNER_TID;
        fown_ex.pid  = tid;
        if ( fcntl(event_thread.fd, F_SETOWN_EX, (unsigned long)&fown_ex) < 0) {
            ERROR("fcntl(%d, F_SETOWN_EX, &fown_ex) failed: %s", event_thread.fd, strerror(errno));
            return false;
        }

        // need to set PERF_SIGNAL to this file descriptor
        // to avoid POLL_HUP in the signal handler
        if (fcntl(event_thread.fd, F_SETSIG, PERF_SIGNAL) < 0) {
            ERROR("fcntl(%d, F_SETSIG, PERF_SIGNAL) failed: %s", event_thread.fd, strerror(errno));
            return false;
        }


        event_thread.mmap_buf = perf_set_mmap(event_thread.fd);
        assert(event_thread.mmap_buf);

        event_thread.event = &(event_info[i]);
        event_thread_list->push_back(event_thread);
    }

    perf_start_all(*event_thread_list);
    return true;
}

bool PerfManager::closeEvents(){
    std::vector<perf_event_thread_t> *event_thread_list = reinterpret_cast<std::vector<perf_event_thread_t> *> (TD_GET(perf_state));
    if (event_thread_list == nullptr) {
        return true;
    }
    perf_stop_all(*event_thread_list);
    for (uint32_t i = 0; i < num_events; i++) {
        perf_event_thread_t &event_thread = (*event_thread_list)[i];
        if (event_thread.fd >= 0){
            close(event_thread.fd);
        }
        perf_unmmap(event_thread.mmap_buf);
    }
    delete event_thread_list;
    TD_GET(perf_state) = nullptr;
    return true;
}

bool PerfManager::readCounter(int event_idx, uint64_t *val){
    std::vector<perf_event_thread_t> *event_thread_list = reinterpret_cast<std::vector<perf_event_thread_t> *> (TD_GET(perf_state));
    assert(event_thread_list);
    if( event_idx < 0 or event_idx >= static_cast<int> (event_thread_list->size())){
        return false;
    }
    perf_event_thread_t &current = (*event_thread_list)[event_idx];
    return perf_read_event_counter(current.fd, val);
}
