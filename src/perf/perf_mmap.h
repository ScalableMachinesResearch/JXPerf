#ifndef _PERF_MMAP_H
#define _PERF_MMAP_H
#include <perfmon/pfmlib_perf_event.h>
#include "perf_interface.h"


typedef struct perf_event_header perf_event_header_t;
typedef struct perf_event_mmap_page perf_mmap_t;


bool perf_mmap_init();
bool perf_mmap_shutdown();

perf_mmap_t *perf_set_mmap(int fd);
void perf_unmmap(perf_mmap_t *mmap_buf);

bool perf_read_header(perf_mmap_t *mmap_buf, perf_event_header_t *ehdr);
bool perf_read_record_sample(perf_mmap_t *mmap_buf, uint64_t sample_type, perf_sample_data_t *sample_data);

int perf_num_of_remaining_data(perf_mmap_t *mmap_buf);
bool perf_skip_record(perf_mmap_t *mmap_buf, perf_event_header_t *ehdr);
bool perf_skip_all(perf_mmap_t *mmap_buf);

#endif /*  _PERF_MMAP_H */
