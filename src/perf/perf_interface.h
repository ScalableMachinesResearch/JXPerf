#ifndef _PERF_EVENTS_H
#define _PERF_EVENTS_H

#include <perfmon/pfmlib_perf_event.h>
#include <signal.h>

#include<vector>
#include<string>

#include "io.h"
#include "config.h"

//TODO: When using map to store thread data, we need to use lock

typedef struct {
  uint64_t ip;
  uint32_t pid, tid;
  uint64_t addr;
  uint32_t cpu;
  // uint64_t nr;
  // uint64_t ips[PERF_MAX_STACK_DEPTH];

  // *********** auxiliary fields *********** //

  bool isPrecise; // whether the obtained ip is precise or not

} perf_sample_data_t;

typedef void (*sample_cb_t)(int,perf_sample_data_t *, void *, int, int, int);


class PerfManager {
public:

  static bool processInit(const std::vector<std::string> &event_list, sample_cb_t sample_cb);
  static void processShutdown();
  
  static bool setupEvents();
  static bool closeEvents();
  
  static bool readCounter(int event_idx, uint64_t *val);
};
#endif
