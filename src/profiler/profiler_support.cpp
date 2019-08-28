#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>
#include <fstream>
#include <unordered_map>

#include "profiler_support.h"
#include "x86-misc.h"
#include "thread_data.h"
#include "metrics.h"
#include "debug.h"
#include "lock.h"

#define MAX_BLACK_LIST_ADDRESS (1024)
#define MAX_WP_SLOTS (4)

typedef struct BlackListAddressRange {
    void * startAddr;
    void * endAddr;
} BlackListAddressRange_t;
static BlackListAddressRange_t blackListAddresses[MAX_BLACK_LIST_ADDRESS];
static uint16_t numBlackListAddresses = 0;

static const char *blackListedModules[] = {"libpfm.so", "libxed.so", "libxed-ild.so", "libpreload.so", "libagent.so", "libstdc++.so", "libjvm.so", "libc", "anon_inode:[perf_event]"};
static const int numblackListedModules = 9;

// static const char *blackListedModules[] = {"libpfm.so", "libxed.so", "libxed-ild.so", "libpreload.so", "libagent.so", "anon_inode:[perf_event]"};
// static const int numblackListedModules = 6;

static SpinLock lock;

void GetActiveWatchPoints(WP_TriggerInfo_t wpt[], int *numActiveWPs) {
    WP_GetActiveWatchPoints(wpt, numActiveWPs);
}

void *GetPatchedIP(const void *method_start_addr, const void *method_end_addr, void *ip) {
    void *patchedIP;
    int numExcludes;
    void *excludeList[MAX_WP_SLOTS] = {0};
    WP_GetActiveAddresses(excludeList, &numExcludes);
    patchedIP = get_previous_instruction(method_start_addr, method_end_addr, ip, excludeList, numExcludes);
    
    return patchedIP;
}

bool IsPCSane(void *ip, void *patchedIP) {
    if ((patchedIP == 0) || (patchedIP > ip) || (ip > ((uint8_t *)patchedIP + 15))) {
        return false;
    }
    return true;
}

int GetFloorWPLength(int accessLen) {
    switch (accessLen) {
        default:
        case 8: return 8;
        case 7:case 6: case 5: case 4: return 4;
        case 3:case 2: return 2;
        case 1: return 1;
    }
}


int GetFloorWPLengthAtAddress(void * address, int accessLen) {
    uint8_t alignment = ((size_t) address) & (MAX_WP_LENGTH -1);        
    switch (alignment) {
        case 1: case 3: case 5: case 7: /* 1-byte aligned */ return 1;                                  
        case 2: case 6: /* 2-byte aligned */ return MIN(2, accessLen);
        case 0: /* 8-byte aligned */ return MIN(8, accessLen);                                          
        case 4: /* 8-byte aligned */ return MIN(4, accessLen);                      
        default:
            assert(false && "Should never reach here");
            return 1;
    }
}

/*
double ProportionOfWatchpointAmongOthersSharingTheSameContext(WP_TriggerInfo_t *wpi) {
#if 0
    int share = 0;
    for(int i = 0; i < wpConfig.maxWP; i++) {
        if(tData.watchPointArray[i].isActive && tData.watchPointArray[i].sample.node == wpi->sample.node) {
        share ++;
        }
    }
    assert(share > 0);
    return 1.0/share;
#else
    return 1.0;
#endif
}
*/

int curWatermarkId = 0;
int pebs_metric_id[NUM_WATERMARK_METRICS] = {-1, -1, -1, -1};

// Actually, only one watchpoint client can be active at a time 
void SetupWatermarkMetric(int metricId) {
    if (curWatermarkId == NUM_WATERMARK_METRICS) {
        ERROR("curWatermarkId == NUM_WATERMARK_METRICS = %d", NUM_WATERMARK_METRICS);
        assert(false);
    }
    /* 
    std::unordered_map<Context *, SampleNum> * propAttrTable = reinterpret_cast<std::unordered_map<Context *, SampleNum> *> (TD_GET(prop_attr_state)[curWatermarkId]);
    if (propAttrTable == nullptr) {
        propAttrTable = new(std::nothrow) std::unordered_map<Context *, SampleNum>();
        assert(propAttrTable);
        TD_GET(ctxt_sample_state)[curWatermarkId] = propAttrTable;
    } 
    */
    pebs_metric_id[curWatermarkId] = metricId;
    curWatermarkId++;
}


int GetMatchingCtxtSampleTableId(int pebsMetricId) {
    for (int i=0; i<NUM_WATERMARK_METRICS; i++) {
        if(pebs_metric_id[i] == pebsMetricId) return i;
    }
    assert(false);
}


void UpdateNumSamples(Context *ctxt, int pebsMetricId) {
    assert(ctxt);
    int ctxtSampleTableId = GetMatchingCtxtSampleTableId(pebsMetricId);
    std::unordered_map<Context *, SampleNum> *ctxtSampleTable = reinterpret_cast<std::unordered_map<Context *, SampleNum> *>(TD_GET(ctxt_sample_state)[ctxtSampleTableId]);
    if (ctxtSampleTable == nullptr) {
        ctxtSampleTable = new(std::nothrow) std::unordered_map<Context *, SampleNum>();
        assert(ctxtSampleTable);
        TD_GET(ctxt_sample_state)[ctxtSampleTableId] = ctxtSampleTable;
    } 
    
    std::unordered_map<Context *, SampleNum> &myCtxtSampleTable = *ctxtSampleTable;
    if (myCtxtSampleTable.find(ctxt) == myCtxtSampleTable.end()) myCtxtSampleTable[ctxt] = {0, 1};
    else { 
        myCtxtSampleTable[ctxt].cur_num++; 
    } 
}


uint64_t GetNumDiffSamplesAndReset(Context *ctxt, int pebsMetricId, double prop, uint32_t threshold) {
    assert(ctxt);
    int ctxtSampleTableId = GetMatchingCtxtSampleTableId(pebsMetricId);
    std::unordered_map<Context *, SampleNum> *ctxtSampleTable = reinterpret_cast<std::unordered_map<Context *, SampleNum> *>((TD_GET(ctxt_sample_state))[ctxtSampleTableId]);
    assert(ctxtSampleTable);
    /*
    if (ctxtSampleTable == nullptr) {
        ctxtSampleTable = new(std::nothrow) std::unordered_map<Context *, SampleNum>();
        assert(ctxtSampleTable);
        TD_GET(ctxt_sample_state)[ctxtSampleTableId] = ctxtSampleTable;
    } 
    */
    std::unordered_map<Context *, SampleNum> &myCtxtSampleTable = *ctxtSampleTable;
    
    double diff = 0., diffWithPeriod = 0.;
    // assert(myCtxtSampleTable.find(ctxt) != myCtxtSampleTable.end());
    diff = (myCtxtSampleTable[ctxt].cur_num - myCtxtSampleTable[ctxt].catchup_num) * prop;
    diffWithPeriod = diff * threshold;
    myCtxtSampleTable[ctxt].catchup_num = myCtxtSampleTable[ctxt].cur_num;

    return (uint64_t)diffWithPeriod;
}


std::string GetClientName() {
    int metricId = -1;
    for (int i = 0; i < NUM_WATERMARK_METRICS; i++) {
        if(pebs_metric_id[i] != -1) {
            metricId = pebs_metric_id[i];
            break;
        }
    }
    assert(metricId != -1);
    metrics::metric_info_t * metric_info = metrics::MetricInfoManager::getMetricInfo(metricId);
    assert(metric_info);
    return metric_info->client_name;
}


void PopulateBlackListAddresses() {
    LockScope<SpinLock> lock_scope(&lock);
    if (numBlackListAddresses == 0) {
        std::ifstream loadmap;
        loadmap.open("/proc/self/maps");
        if (!loadmap.is_open()) {
            ERROR("Could not open /proc/self/maps");
            return;
        }

#ifdef DUMP_MAP_FILE
char map_file[256];
if (!getcwd(map_file, sizeof(map_file))) return;
strcat(map_file, "/self-map.txt");
std::ofstream dumpmap(map_file);
#endif        
        
        char tmpname[PATH_MAX];
        char* addr = NULL;
        while (!loadmap.eof()) {
            std::string tmp;
            std::getline(loadmap, tmp);
#ifdef DUMP_MAP_FILE
            dumpmap << tmp << std::endl;
#endif        
            char *line = strdup(tmp.c_str());
            char *save = NULL;
            const char delim[] = " \n";
            addr = strtok_r(line, delim, &save);
            strtok_r(NULL, delim, &save);
            // skip 3 tokens
            for (int i=0; i < 3; i++) {(void)strtok_r(NULL, delim, &save);}
            char *name = strtok_r(NULL, delim, &save);
            realpath(name, tmpname);
            for (int i = 0; i < numblackListedModules; i++) {
                if (strstr(tmpname, blackListedModules[i])) {
                    save = NULL;
                    const char dash[] = "-";
                    char* start_str = strtok_r(addr, dash, &save);
                    char* end_str   = strtok_r(NULL, dash, &save);
                    void *start = (void*)(uintptr_t)strtol(start_str, NULL, 16);
                    void *end   = (void*)(uintptr_t)strtol(end_str, NULL, 16);
                    blackListAddresses[numBlackListAddresses].startAddr = start;
                    blackListAddresses[numBlackListAddresses].endAddr = end;
                    numBlackListAddresses++;
                    INFO("%s %p %p\n", tmpname, start, end);
                }
            }

        }
        loadmap.close();
        
        // No TLS
        // extern void * __tls_get_addr (void *);
        // blackListAddresses[numBlackListAddresses].startAddr = ((void *)__tls_get_addr) - 1000 ;
        // blackListAddresses[numBlackListAddresses].endAddr = ((void *)__tls_get_addr) + 1000;
        // numBlackListAddresses++;

        // No first page
        blackListAddresses[numBlackListAddresses].startAddr = 0 ;
        blackListAddresses[numBlackListAddresses].endAddr = (void*) sysconf(_SC_PAGESIZE);
        numBlackListAddresses++;
    }
}


static inline bool IsBlackListedAddress(void *addr) {
    for(int i = 0; i < numBlackListAddresses; i++){
        if (addr >= blackListAddresses[i].startAddr && addr < blackListAddresses[i].endAddr) return true;
    }
    return false;
}


static inline bool IsTdataAddress(void *addr) {
    bool inside_agent = TD_GET(inside_agent); 
    void *tdata = &inside_agent;
    if (((uint8_t *)addr > (uint8_t *)tdata - 100) && ((uint8_t *)addr < (uint8_t *)tdata + 100)) return true;
    return false;
}


// Avoids Kernel address and zeros
bool IsValidAddress(void *pc, void *addr) {
    if (addr == 0) return false;
    if (pc == 0) return false;
    
    ThreadData::thread_data_t *td = ThreadData::thread_data_get(); 
    if(((void*)(td-1) <= addr) && (addr < (void*)(td+2))) return false;
    
    if (WP_IsAltStackAddress(addr)) return false;
    if (WP_IsFSorGS(addr)) return false;
    if (IsBlackListedAddress(pc) || IsBlackListedAddress(addr)) return false;
    if (IsTdataAddress(addr)) return false;
    if ((!((uint64_t)addr & 0xF0000000000000)) && (!((uint64_t)pc & 0xF0000000000000))) return true;
    return false;
}


bool IsValidPC(void *pc) {
    if (pc == 0) return false;
    return true;
    /* 
    ThreadData::thread_data_t *td = ThreadData::thread_data_get(); 
    if (IsBlackListedAddress(pc)) return false;
    if (pc && !((uint64_t)pc & 0xF0000000000000)) return true;
    return false;
    */
}

#if 0
bool IsAddressReadable(void *addr) {
    bool retVal = true;
    sigjmp_buf &jbuf = TD_GET(jbuf);
    if (sigsetjmp(jbuf, 1) == 0) { 
        volatile char i = *(char*)(addr);
    } else { 
        retVal = false;
    }
    return retVal;
}
#endif
