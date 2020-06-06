#ifndef __PROFILER_SUPPORT__
#define __PROFILER_SUPPORT__

#include "context.h"
#include "watchpoint.h"

#define DEADSTORE_CLIENT_NAME "DEADSTORE"
#define SILENTSTORE_CLIENT_NAME "SILENTSTORE"
#define SILENTLOAD_CLIENT_NAME "SILENTLOAD"
#define DATA_CENTRIC_CLIENT_NAME "DATACENTRIC"
#define NUMANODE_CLIENT_NAME "NUMA"

#define MAX_WP_LENGTH (8L)
#define NUM_WATERMARK_METRICS (4)
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define GET_OVERLAP_BYTES(a, a_len, b, b_len) ((a) >= (b)? MIN(a_len, (int64_t)(b_len) - ((int64_t)(a) - (int64_t)(b))) : MIN(b_len, (int64_t)(a_len) - ((int64_t)(b) - (int64_t)(a))))
#define ADDRESSES_OVERLAP(addr1, len1, addr2, len2) (((int64_t)(addr1)+(len1) > (int64_t)(addr2)) && ((int64_t)(addr2) + (len2) > (int64_t)(addr1)))
#define FIRST_OVERLAPPED_BYTE_OFFSET_IN_FIRST(a, a_len, b, b_len) ((int64_t)(a) >= (int64_t)(b)? (0) : ((int64_t)b - (int64_t)a))
#define FIRST_OVERLAPPED_BYTE_OFFSET_IN_SECOND(a, a_len, b, b_len) ((int64_t)(b) >= (int64_t)(a)? (0) : ((int64_t)a - (int64_t)b))
#define IS_4_BYTE_ALIGNED(addr) (!((size_t)(addr) & (3)))
#define IS_8_BYTE_ALIGNED(addr) (!((size_t)(addr) & (7)))

typedef enum AccessType {LOAD, STORE, LOAD_AND_STORE, UNKNOWN} AccessType;
typedef enum FloatType {ELEM_TYPE_FLOAT16, ELEM_TYPE_SINGLE, ELEM_TYPE_DOUBLE, ELEM_TYPE_LONGDOUBLE, ELEM_TYPE_LONGBCD, ELEM_TYPE_UNKNOWN} FloatType;

typedef struct SampleNum {
    uint64_t catchup_num;
    uint64_t cur_num;
} SampleNum_t;


void GetActiveWatchPoints(WP_TriggerInfo_t wpt[], int *numActiveWPs);
void *GetPatchedIP(const void *method_start_addr, const void *method_end_addr, void *ip);
bool IsPCSane(void *ip, void *patchedIP);
int GetFloorWPLength(int accessLen);
int GetFloorWPLengthAtAddress(void *address, int accessLen);
// double ProportionOfWatchpointAmongOthersSharingTheSameContext(WP_TriggerInfo_t *wpi);
extern void SetupWatermarkMetric(int metricId);
int GetMatchingCtxtSampleTableId(int pebsMetricId);
void UpdateNumSamples(Context *ctxt, int pebsMetricId);
uint64_t GetNumDiffSamplesAndReset(Context *ctxt, int pebsMetricId, double prop, uint32_t threshold);
std::string GetClientName();
bool IsValidAddress(void *pc, void *addr);
bool IsValidPC(void *pc);
bool IsAddressReadable(void *wpiStartByte);
void PopulateBlackListAddresses();

#endif
