#ifndef _PERF_UTIL_H
#define _PERF_UTIL_H

#include <sys/types.h>
#include <inttypes.h>
#include <err.h>
#include <perfmon/pfmlib_perf_event.h>
#include "config.h"
#include "perf_interface.h"
#include <vector>
#include <string>

#define THREAD_SELF     0
#define CPU_ANY        -1
#define GROUP_FD       -1
#define PERF_FLAGS      0
#define PERF_REQUEST_0_SKID      2
#define PERF_WAKEUP_EACH_SAMPLE  1

#define EXCLUDE    1
#define INCLUDE    0

#define EXCLUDE_CALLCHAIN EXCLUDE
#define INCLUDE_CALLCHAIN INCLUDE


bool perf_encode_event(const std::string &event, struct perf_event_attr *event_attr);
bool perf_attr_init(struct perf_event_attr *attr, uint64_t threshold, uint64_t more_sample_type);
bool perf_read_event_counter(int fd, uint64_t *val);


/*
 * values[0] = raw count
 * values[1] = TIME_ENABLED
 * values[2] = TIME_RUNNING
 */
static inline uint64_t
perf_scale(uint64_t *values)
{
	uint64_t res = 0;

	if (!values[2] && !values[1] && values[0])
		warnx("WARNING: time_running = 0 = time_enabled, raw count not zero\n");

	if (values[2] > values[1])
		warnx("WARNING: time_running > time_enabled\n");

	if (values[2])
		res = (uint64_t)((double)values[0] * values[1]/values[2]);
	return res;
}

static inline uint64_t
perf_scale_delta(uint64_t *values, uint64_t *prev_values)
{
	double pval[3], val[3];
	uint64_t res = 0;

	if (!values[2] && !values[1] && values[0])
		warnx("WARNING: time_running = 0 = time_enabled, raw count not zero\n");

	if (values[2] > values[1])
		warnx("WARNING: time_running > time_enabled\n");

	if (values[2] - prev_values[2]) {
		/* covnert everything to double to avoid overflows! */
		pval[0] = prev_values[0];
		pval[1] = prev_values[1];
		pval[2] = prev_values[2];
		val[0] = values[0];
		val[1] = values[1];
		val[2] = values[2];
		res = (uint64_t)(((val[0] - pval[0]) * (val[1] - pval[1])/ (val[2] - pval[2])));
	}
	return res;
}


/*
 * TIME_RUNNING/TIME_ENABLED
 */
static inline double
perf_scale_ratio(uint64_t *values)
{
	if (!values[1])
		return 0.0;

	return values[2]*1.0/values[1];
}

#endif /* _PERF_UTIL_H */
