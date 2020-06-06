#include <assert.h>
#include <errno.h>
#include <dlfcn.h>
#include <algorithm>
#include <iomanip> 
#include <stack>
#include <numa.h>
 
#include "config.h"
#include "context.h"
#include "profiler.h"
#include "thread_data.h"
#include "perf_interface.h"
#include "stacktraces.h"
#include "agent.h"
#include "metrics.h"
#include "debug.h"
#include "x86-misc.h"
#include "context-pc.h" 
#include "safe-sampling.h" 
//#include "Memory.h" 

#define APPROX_RATE (0.01)
#define MAX_FRAME_NUM (128)
#define MAX_IP_DIFF (100000000)

Profiler Profiler::_instance;
ASGCT_FN Profiler::_asgct = nullptr;
std::string clientName;

static SpinLock lock_map;
SpinLock tree_lock;
interval_tree_node *splay_tree_root = NULL;
static std::unordered_map<Context*, Context*> map = {};

uint64_t GCCounter = 0;
thread_local uint64_t localGCCounter = 0;

uint64_t grandTotWrittenBytes = 0;
uint64_t grandTotLoadedBytes = 0;
uint64_t grandTotUsedBytes = 0;
uint64_t grandTotDeadBytes = 0;
uint64_t grandTotNewBytes = 0;
uint64_t grandTotOldBytes = 0;
uint64_t grandTotOldAppxBytes = 0;
uint64_t grandTotAllocTimes = 0;
uint64_t grandTotSameNUMA = 0;
uint64_t grandTotDiffNUMA = 0;
uint64_t grandTotL1Cachemiss = 0;


thread_local uint64_t totalWrittenBytes = 0;
thread_local uint64_t totalLoadedBytes = 0;
thread_local uint64_t totalUsedBytes = 0;
thread_local uint64_t totalDeadBytes = 0;
thread_local uint64_t totalNewBytes = 0;
thread_local uint64_t totalOldBytes = 0;
thread_local uint64_t totalOldAppxBytes = 0;
thread_local uint64_t totalAllocTimes = 0;
thread_local uint64_t totalSameNUMA = 0;
thread_local uint64_t totalDiffNUMA = 0;
thread_local uint64_t totalL1Cachemiss = 0;


thread_local void *prevIP = (void *)0;

// thread_local uint64_t sampleCnt = 0;
// uint64_t totalSampleCnt = 0;

namespace {

Context *constructContext(ASGCT_FN asgct, void *uCtxt, uint64_t ip, Context *ctxt, jmethodID method_id, uint32_t method_version, uint32_t object_numa_node) {
    ContextTree *ctxt_tree = reinterpret_cast<ContextTree *> (TD_GET(context_state));
    Context *last_ctxt = ctxt;

    ASGCT_CallFrame frames[MAX_FRAME_NUM];
    // ASGCT_CallTrace trace = {JVM::jni(), 0, frames};
    ASGCT_CallTrace trace;
    trace.frames = frames;
    trace.env_id = JVM::jni();
    asgct(&trace, MAX_FRAME_NUM, uCtxt); 
    
    for (int i = trace.num_frames - 1 ; i >= 0; i--) {
        // TODO: We need to consider how to include the native method.
        ContextFrame ctxt_frame;
        if (i == 0) {
            //ctxt_frame.binary_addr = ip;
            ctxt_frame.numa_node = object_numa_node;
        }
        ctxt_frame = frames[i]; //set method_id and bci
        if (last_ctxt == nullptr) last_ctxt = ctxt_tree->addContext((uint32_t)CONTEXT_TREE_ROOT_ID, ctxt_frame);
        else last_ctxt = ctxt_tree->addContext(last_ctxt, ctxt_frame);
    }

    // leaf node 
    ContextFrame ctxt_frame;
    ctxt_frame.binary_addr = ip;
    ctxt_frame.method_id = method_id;
    ctxt_frame.method_version = method_version;
    // It's sort of tricky. Use bci to split a context pair.
    if (ctxt == nullptr) ctxt_frame.bci = -65536;
    if (last_ctxt != nullptr) last_ctxt = ctxt_tree->addContext(last_ctxt, ctxt_frame);
    else last_ctxt = ctxt_tree->addContext((uint32_t)CONTEXT_TREE_ROOT_ID, ctxt_frame);
    
    return last_ctxt;
}

}


void Profiler::OnSample(int eventID, perf_sample_data_t *sampleData, void *uCtxt, int metric_id1, int metric_id2, int metric_id3) {
    if (!sampleData->isPrecise || !sampleData->addr) return;

    void *sampleIP = (void *)(sampleData->ip);
    void *sampleAddr = (void *)(sampleData->addr); 

    if (clientName.compare(DATA_CENTRIC_CLIENT_NAME) != 0 && clientName.compare(NUMANODE_CLIENT_NAME) != 0) {
        if (!IsValidAddress(sampleIP, sampleAddr)) return;
    }

    jmethodID method_id = 0;
    uint32_t method_version = 0;
    CodeCacheManager &code_cache_manager = Profiler::getProfiler().getCodeCacheManager();
    
    CompiledMethod *method = code_cache_manager.getMethod(sampleData->ip, method_id, method_version);
    if (method == nullptr) return;
    
    uint32_t threshold = (metrics::MetricInfoManager::getMetricInfo(metric_id1))->threshold;

    //numa analysis
    if (clientName.compare(NUMANODE_CLIENT_NAME) == 0) {
        NumaAnalysis(sampleData, uCtxt, method_id, method_version, threshold, metric_id2, metric_id3);
        return;
    }

    // data-centric analysis
    if (clientName.compare(DATA_CENTRIC_CLIENT_NAME) == 0) {
        DataCentricAnalysis(sampleData, uCtxt, method_id, method_version, threshold, metric_id2);
        return;
    }

    int accessLen;
    AccessType accessType;
    if (false == get_mem_access_length_and_type(sampleIP, (uint32_t *)(&accessLen), &accessType)) return;
    if (accessType == UNKNOWN || accessLen == 0) return;
    int watchLen = GetFloorWPLength(accessLen);

    Context *watchCtxt = constructContext(_asgct, uCtxt, sampleData->ip, nullptr, method_id, method_version, 10);
    if (watchCtxt == nullptr) return;
    UpdateNumSamples(watchCtxt, metric_id1); 

#ifdef PRINT_PMU_INS
    std::ofstream *pmu_ins_output_stream = reinterpret_cast<std::ofstream *>(TD_GET(pmu_ins_output_stream));
    assert(pmu_ins_output_stream != nullptr); 
    print_single_instruction(pmu_ins_output_stream, (const void *)sampleIP);
#endif
    
#ifdef ACCURACY_CHECK    
    bool isSamplePointAccurate = true; 
    void *contextIP = getContextPC(uCtxt);
    if (contextIP != sampleIP) { 
        void *startaddr = nullptr, *endAddr = nullptr; 
        method->getMethodBoundary(&start_addr, &endAddr);
        if (contextIP < startAddr || contextIP > endAddr) {
            isSamplePointAccurate = false;
            INFO("InAccurate sample: sampleData->ip = %p contextPC = %p\n", sampleIP, contextIP);
        }
    }
#endif
    
#ifdef COUNT_UP_SAMPLE_NUM
    metrics::ContextMetrics *metrics = watchCtxt->getMetrics();
    if (metrics == nullptr) {
        metrics = new metrics::ContextMetrics();
        watchCtxt->setMetrics(metrics); 
    }
    metrics::metric_val_t metric_val;
    metric_val.i = threshold;
    assert(metrics->increment(metric_id1, metric_val));
#endif
    if (localGCCounter < GCCounter) {
        WP_DisableAllWatchPoints();
        localGCCounter++;
    }

    if (clientName.compare(DEADSTORE_CLIENT_NAME) == 0 && accessType != LOAD) {
        totalWrittenBytes += accessLen * threshold;
        WP_Subscribe(sampleAddr, watchLen, WP_RW, accessLen, watchCtxt, metric_id1, false);
    } else if (clientName.compare(SILENTSTORE_CLIENT_NAME) == 0 && accessType != LOAD) {
        totalWrittenBytes += accessLen * threshold;
        WP_Subscribe(sampleAddr, watchLen, WP_WRITE, accessLen, watchCtxt, metric_id1, true);
    } else if (clientName.compare(SILENTLOAD_CLIENT_NAME) == 0 && accessType != STORE) { 
        totalLoadedBytes += accessLen * threshold;
        WP_Subscribe(sampleAddr, watchLen, WP_RW, accessLen, watchCtxt, metric_id1, true);
    } else {
        ERROR("Unknown client name: %s or mismatch between the client name: %s and the sampled instruction: %p", clientName.c_str(), clientName.c_str(), sampleIP);
    }
}

void::Profiler::NumaAnalysis(perf_sample_data_t *sampleData, void *uCtxt, jmethodID method_id, uint32_t method_version, uint32_t threshold, int metric_id2, int metric_id3) { 
    void *sampleAddr = (void *)(sampleData->addr);
    int status[1];
    int ret_code;
    status[0] = -1; 
    ret_code = numa_move_pages(0, 1, &sampleAddr, NULL, status, 0);
    if (ret_code == -1)
        return;
    
    uint32_t object_numa_node = (uint32_t)status[0];
    uint32_t sampling_numa_node = sampleData->cpu % 4;
     
    void* startaddress;
	tree_lock.lock();
	interval_tree_node *p = SplayTree::interval_tree_lookup(&splay_tree_root, (void *)(sampleData->addr), &startaddress);
	tree_lock.unlock();

#if 0
    if (object_numa_node == sampling_numa_node)
        printf("\nEqual! sampling at cpu#: %lu (Reside numa node: %lu); Object Memory at %p is at numa node %lu\n", sampleData->cpu, sampling_numa_node, sampleAddr, object_numa_node);
    else
        printf("\nNot equal! sampling at cpu#: %lu (Reside numa node: %lu); Object Memory at %p is at numa node %lu\n", sampleData->cpu, sampling_numa_node, sampleAddr, object_numa_node);
#endif

	if (p != NULL) {
		// assert(p->node_ctxt != nullptr);
        Context *ctxt = p->node_ctxt; 
		std::stack<Context *> ctxt_stack;
		
        while (ctxt != nullptr) {
			ctxt_stack.push(ctxt);
            ctxt = ctxt->getParent();
            //ctxt = nullptr;
		}
        
        #if 1
		if (!ctxt_stack.empty()) ctxt_stack.pop(); // pop out the root
		
		ctxt = nullptr;
		ContextTree *ctxt_tree = reinterpret_cast<ContextTree *> (TD_GET(context_state));
		while (!ctxt_stack.empty()) {
			ContextFrame ctxt_frame = ctxt_stack.top()->getFrame();
			if (ctxt == nullptr) ctxt = ctxt_tree->addContext((uint32_t)CONTEXT_TREE_ROOT_ID, ctxt_frame);
			else ctxt = ctxt_tree->addContext(ctxt, ctxt_frame);
			ctxt_stack.pop();
		}

		Context *ctxt_allocate = constructContext(_asgct, uCtxt, sampleData->ip, ctxt, method_id, method_version, object_numa_node);
		Context *ctxt_access = constructContext(_asgct, uCtxt, sampleData->ip, nullptr, method_id, method_version, object_numa_node);

		lock_map.lock();
		map[ctxt_access] = ctxt_allocate;
		lock_map.unlock();
		if (ctxt_allocate != nullptr && sampleData->ip != 0) {
			metrics::ContextMetrics *metrics = ctxt_allocate->getMetrics();
			if (metrics == nullptr) {
				//metrics = new metrics::ContextMetrics();
                metrics = new metrics::ContextMetrics();

				ctxt_allocate->setMetrics(metrics);
			}
			metrics::metric_val_t metric_val;
			metric_val.i = threshold;
            
            if (object_numa_node == sampling_numa_node) {
			    assert(metrics->increment(metric_id3, metric_val));
                totalSameNUMA += threshold;
            } else {
                assert(metrics->increment(metric_id2, metric_val));
                totalDiffNUMA += threshold;
            }

		}
        #endif
	} else {
		Context *ctxt_access = constructContext(_asgct, uCtxt, sampleData->ip, nullptr, method_id, method_version, object_numa_node);
		lock_map.lock();
		std::unordered_map<Context*, Context*>::iterator it = map.find(ctxt_access);
		lock_map.unlock();
		if (it != map.end()) {
			Context *ctxt_allocate = it->second;
			if (ctxt_allocate != nullptr && sampleData->ip != 0) {
				metrics::ContextMetrics *metrics = ctxt_allocate->getMetrics();
				if (metrics == nullptr) {
					metrics = new metrics::ContextMetrics();
					ctxt_allocate->setMetrics(metrics);
				}
				metrics::metric_val_t metric_val;
				metric_val.i = threshold;

                if (object_numa_node == sampling_numa_node) {
			        assert(metrics->increment(metric_id3, metric_val));
                    totalSameNUMA += threshold;
                } else {
                    assert(metrics->increment(metric_id2, metric_val));
                    totalDiffNUMA += threshold;
                }

			}
		}
	}
}

void Profiler::DataCentricAnalysis(perf_sample_data_t *sampleData, void *uCtxt, jmethodID method_id, uint32_t method_version, uint32_t threshold, int metric_id2) {
	void* startaddress;
	tree_lock.lock();
	interval_tree_node *p = SplayTree::interval_tree_lookup(&splay_tree_root, (void *)(sampleData->addr), &startaddress);
	tree_lock.unlock();

	if (p != NULL) {
		// assert(p->node_ctxt != nullptr);
		Context *ctxt = p->node_ctxt; 
		std::stack<Context *> ctxt_stack;
		while (ctxt != nullptr) {
			ctxt_stack.push(ctxt);
		ctxt = ctxt->getParent();
		}

		if (!ctxt_stack.empty()) ctxt_stack.pop(); // pop out the root
		
		ctxt = nullptr;
		ContextTree *ctxt_tree = reinterpret_cast<ContextTree *> (TD_GET(context_state));
		while (!ctxt_stack.empty()) {
			ContextFrame ctxt_frame = ctxt_stack.top()->getFrame();
			if (ctxt == nullptr) ctxt = ctxt_tree->addContext((uint32_t)CONTEXT_TREE_ROOT_ID, ctxt_frame);
			else ctxt = ctxt_tree->addContext(ctxt, ctxt_frame);
			ctxt_stack.pop();
		}
			
		Context *ctxt_allocate = constructContext(_asgct, uCtxt, sampleData->ip, ctxt, method_id, method_version, 10);
		Context *ctxt_access = constructContext(_asgct, uCtxt, sampleData->ip, nullptr, method_id, method_version, 10);
			
		lock_map.lock();
		map[ctxt_access] = ctxt_allocate;
		lock_map.unlock();
		if (ctxt_allocate != nullptr && sampleData->ip != 0) {
			metrics::ContextMetrics *metrics = ctxt_allocate->getMetrics();
			if (metrics == nullptr) {
				metrics = new metrics::ContextMetrics();
				ctxt_allocate->setMetrics(metrics);
			}
			metrics::metric_val_t metric_val;
			metric_val.i = threshold;
			assert(metrics->increment(metric_id2, metric_val));
			totalL1Cachemiss += threshold;
		}
	} else {
		Context *ctxt_access = constructContext(_asgct, uCtxt, sampleData->ip, nullptr, method_id, method_version, 10);
		lock_map.lock();
		std::unordered_map<Context*, Context*>::iterator it = map.find(ctxt_access);
		lock_map.unlock();
		if (it != map.end()) {
			Context *ctxt_allocate = it->second;
			if (ctxt_allocate != nullptr && sampleData->ip != 0) {
				metrics::ContextMetrics *metrics = ctxt_allocate->getMetrics();
				if (metrics == nullptr) {
					metrics = new metrics::ContextMetrics();
					ctxt_allocate->setMetrics(metrics);
				}
				metrics::metric_val_t metric_val;
				metric_val.i = threshold;
				assert(metrics->increment(metric_id2, metric_val));
				totalL1Cachemiss += threshold;
			}
		}
	}
}

WP_TriggerAction_t Profiler::OnDeadStoreWatchPoint(WP_TriggerInfo_t *wpt) {
    if (!profiler_safe_enter()) return WP_DISABLE;

    if (wpt->pc == 0) wpt->pc = getContextPC(wpt->uCtxt);
    if (wpt->pc == 0) {
        profiler_safe_exit();
        return WP_DISABLE; 
    }
    
    jmethodID method_id = 0;
    uint32_t method_version = 0;
    CodeCacheManager &code_cache_manager = Profiler::getProfiler().getCodeCacheManager();
    
    CompiledMethod *method = code_cache_manager.getMethod((uint64_t)(wpt->pc), method_id, method_version);
    if(method == nullptr) {
        profiler_safe_exit();
        return WP_DISABLE;
    }
    
    // fix the imprecise IP 
    void *patchedIP = wpt->pc;
    if (!wpt->pcPrecise) {
        void *startAddr = nullptr, *endAddr = nullptr; 
        method->getMethodBoundary(&startAddr, &endAddr);
        if (prevIP > startAddr && prevIP < patchedIP)
            patchedIP = GetPatchedIP(prevIP, endAddr, wpt->pc);
        else
            patchedIP = GetPatchedIP(startAddr, endAddr, wpt->pc);
        if (!IsPCSane(wpt->pc, patchedIP)) {
            profiler_safe_exit();
            return WP_DISABLE; 
        }
        wpt->pc = patchedIP;
        prevIP = patchedIP;
    }
    
    int accessLen; 
    AccessType accessType;
    FloatType *floatType = 0;
    void *addr = (void *)-1;
    
    if (false == get_mem_access_length_and_type_address(patchedIP, (uint32_t *)(&accessLen), &accessType, floatType, wpt->uCtxt, &addr)) {
        profiler_safe_exit();
        return WP_DISABLE;
    }
    if (accessType == UNKNOWN || accessLen == 0) { 
        profiler_safe_exit();
        return WP_DISABLE;
    }
    
    Context *watchCtxt =(Context *)(wpt->watchCtxt);
    
    double myProp = 1.0;
    uint32_t threshold = (metrics::MetricInfoManager::getMetricInfo(wpt->metric_id1))->threshold;
    uint64_t numDiffSamples = GetNumDiffSamplesAndReset(watchCtxt, wpt->metric_id1, myProp, threshold);
    uint64_t inc = numDiffSamples * wpt->sampleAccessLen;
    if (inc == 0) {
        profiler_safe_exit();
        return WP_DISABLE;
    }
    
    // Only if the addresses do NOT overlap, do we use the sample address!
    void *sampleAddr = wpt->va;
    if(true == ADDRESSES_OVERLAP(addr, accessLen, sampleAddr, wpt->watchLen)) wpt->va = addr;
    int overlapBytes = GET_OVERLAP_BYTES(sampleAddr, wpt->watchLen, wpt->va, accessLen);
    if (overlapBytes <= 0) {
        ERROR("No overlaps\n!");
        profiler_safe_exit();
        return WP_DISABLE;
    }
    
    if (accessType == LOAD || accessType == LOAD_AND_STORE) {
        totalUsedBytes += inc;
    } else {
        totalDeadBytes += inc;
        Context *triggerCtxt = constructContext(_asgct, wpt->uCtxt, (uint64_t)wpt->pc, watchCtxt, method_id, method_version, 10);
        
        assert(triggerCtxt != nullptr);
        /* if (triggerCtxt == nullptr) {
            profiler_safe_exit();
            return WP_DISABLE;
        }*/
        metrics::ContextMetrics *metrics = triggerCtxt->getMetrics();
        if (metrics == nullptr) {
            metrics = new metrics::ContextMetrics();
            triggerCtxt->setMetrics(metrics);
        }
        metrics::metric_val_t metric_val;
        metric_val.i = inc;
        assert(metrics->increment(wpt->metric_id1, metric_val));
    }
    
    profiler_safe_exit();
    return WP_DISABLE;
}


WP_TriggerAction_t Profiler::DetectRedundancy(WP_TriggerInfo_t *wpt, jmethodID method_id, uint32_t method_version, std::string clientName) {
    int accessLen; 
    AccessType accessType;
    FloatType floatType = ELEM_TYPE_FLOAT16;
    void *addr = (void *)-1;

    if (false == get_mem_access_length_and_type_address(wpt->pc, (uint32_t *)(&accessLen), &accessType, &floatType, wpt->uCtxt, &addr)) return WP_DISABLE;
    if (accessLen == 0) return WP_DISABLE;
    
    if (clientName.compare(SILENTSTORE_CLIENT_NAME) == 0) {
        if (accessType == UNKNOWN) return WP_DISABLE;
    } else if (clientName.compare(SILENTLOAD_CLIENT_NAME) == 0) {
        if (accessType == STORE) return WP_RETAIN;
        if (accessType == UNKNOWN) return WP_DISABLE;
    } else {
        assert(false);
    }
    
    Context *watchCtxt =(Context *)(wpt->watchCtxt);
    
    void *sampleAddr = wpt->va; 
    if (true == ADDRESSES_OVERLAP(addr, accessLen, sampleAddr, wpt->watchLen)) wpt->va = addr;
    int overlapBytes = GET_OVERLAP_BYTES(sampleAddr, wpt->watchLen, wpt->va, accessLen);
    if (overlapBytes <= 0) {
        ERROR("No overlaps!\n");
        return WP_DISABLE;
    }

    int firstOffset = FIRST_OVERLAPPED_BYTE_OFFSET_IN_FIRST(wpt->va, accessLen, sampleAddr, wpt->watchLen);
    int secondOffset = FIRST_OVERLAPPED_BYTE_OFFSET_IN_SECOND(wpt->va, accessLen, sampleAddr, wpt->watchLen);
    void *sampleStartByte = sampleAddr + secondOffset;
    void *wptStartByte = wpt->va + firstOffset;
    
    double myProp = 1.0;
    uint32_t threshold = metrics::MetricInfoManager::getMetricInfo(wpt->metric_id1)->threshold;
    uint64_t numDiffSamples = GetNumDiffSamplesAndReset(watchCtxt, wpt->metric_id1, myProp, threshold);
    uint64_t inc = numDiffSamples * wpt->sampleAccessLen;
    if (inc == 0) return WP_DISABLE;
    
    uint8_t redBytes = 0;
    bool isFloatOperation = (floatType == ELEM_TYPE_UNKNOWN) ? false : true;
    if(isFloatOperation) {
        switch (floatType) {
            case ELEM_TYPE_SINGLE: {
                if (overlapBytes < (int)sizeof(float)) {
                    goto TreatLikeInteger;
                }
                if (!IS_4_BYTE_ALIGNED(sampleStartByte)) { 
                    goto TreatLikeInteger;  
                } 
                if (!IS_4_BYTE_ALIGNED(wptStartByte)) {
                    goto TreatLikeInteger;  
                }
               float oldValue = *(float *)(wpt->sampleValue + secondOffset); 
               float newValue = *(float *)(wptStartByte);
                if (oldValue != newValue) {
                    float rate = (oldValue - newValue) / oldValue;
                    if (rate > APPROX_RATE || rate < -APPROX_RATE) redBytes = 0;
                    else redBytes = sizeof(float);
                } else {
                    redBytes = sizeof(float);
                }
            }
                break;
            case ELEM_TYPE_DOUBLE: {
                if (overlapBytes < (int)sizeof(double)) {
                    goto TreatLikeInteger;
                }
                if (!IS_8_BYTE_ALIGNED(sampleStartByte)) { 
                    goto TreatLikeInteger;  
                } 
                if (!IS_8_BYTE_ALIGNED(wptStartByte)) {
                    goto TreatLikeInteger;  
                }
                double oldValue = *(double *)(wpt->sampleValue + secondOffset); 
                double newValue = *(double *)(wptStartByte);
                if (oldValue != newValue) {
                    double rate = (oldValue - newValue) / oldValue;
                    if (rate > APPROX_RATE || rate < -APPROX_RATE) redBytes = 0;
                    else redBytes = sizeof(double);
                } else {
                    redBytes = sizeof(double);
                }
            }
                break;
            default: // unhandled!!
                goto TreatLikeInteger;
                break;
        }
        if (redBytes != 0) {
            totalOldAppxBytes += inc;
            Context *triggerCtxt = constructContext(_asgct, wpt->uCtxt, (uint64_t)wpt->pc, watchCtxt, method_id, method_version, 10);
            assert(triggerCtxt != nullptr);
            // if (triggerCtxt == nullptr) return WP_DISABLE;
            metrics::ContextMetrics *metrics = triggerCtxt->getMetrics();
            if (metrics == nullptr) {
                metrics = new metrics::ContextMetrics();
                triggerCtxt->setMetrics(metrics);
            }
            metrics::metric_val_t metric_val;
            metric_val.r = inc;
            assert(metrics->increment(wpt->metric_id1, metric_val));
        } else {
            totalNewBytes += inc;
        }
    } else {
    TreatLikeInteger:
        for(int i = firstOffset, k = secondOffset ; i < firstOffset + overlapBytes; i++, k++) {
            if(((uint8_t *)(wpt->va))[i] == ((uint8_t *)(wpt->sampleValue))[k]) {
                redBytes++;
            } else {
                redBytes = 0;
                break;
            }
        }        
        if (redBytes != 0) {
            totalOldBytes += inc;
            Context *triggerCtxt = constructContext(_asgct, wpt->uCtxt, (uint64_t)wpt->pc, watchCtxt, method_id, method_version, 10);
            assert(triggerCtxt != nullptr);
            // if (triggerCtxt == nullptr) return WP_DISABLE;
            metrics::ContextMetrics *metrics = triggerCtxt->getMetrics();
            if (metrics == nullptr) {
                metrics = new metrics::ContextMetrics();
                triggerCtxt->setMetrics(metrics);
            }
            metrics::metric_val_t metric_val;
            metric_val.i = inc;
            assert(metrics->increment(wpt->metric_id1, metric_val));
        } else {
            totalNewBytes += inc;
        }
    }
    return WP_DISABLE;
}


WP_TriggerAction_t Profiler::OnRedStoreWatchPoint(WP_TriggerInfo_t *wpt) {
    if (!profiler_safe_enter()) return WP_DISABLE;
    
    if (wpt->pc == 0) wpt->pc = getContextPC(wpt->uCtxt);
    if (wpt->pc == 0) {
        profiler_safe_exit();
        return WP_DISABLE; 
    }
    
    jmethodID method_id = 0;
    uint32_t method_version = 0;
    CodeCacheManager &code_cache_manager = Profiler::getProfiler().getCodeCacheManager();
    
    CompiledMethod *method = code_cache_manager.getMethod((uint64_t)(wpt->pc), method_id, method_version);
    if (method == nullptr) {
        profiler_safe_exit();
        return WP_DISABLE;
    }
    
    void *patchedIP = wpt->pc;
    if (!wpt->pcPrecise) {
        void *startAddr = nullptr, *endAddr = nullptr; 
        method->getMethodBoundary(&startAddr, &endAddr);
        if (prevIP > startAddr && prevIP < patchedIP)
            patchedIP = GetPatchedIP(prevIP, endAddr, wpt->pc);
        else
            patchedIP = GetPatchedIP(startAddr, endAddr, wpt->pc);
        if (!IsPCSane(wpt->pc, patchedIP)) {
            profiler_safe_exit();
            return WP_DISABLE;
        }
        wpt->pc = patchedIP;
        prevIP = patchedIP;
    }
    
    WP_TriggerAction_t ret = DetectRedundancy(wpt, method_id, method_version, SILENTSTORE_CLIENT_NAME);
    profiler_safe_exit();
    return ret;
}


WP_TriggerAction_t Profiler::OnRedLoadWatchPoint(WP_TriggerInfo_t *wpt) {
    if (!profiler_safe_enter()) return WP_DISABLE;
    
    if (wpt->pc == 0) wpt->pc = getContextPC(wpt->uCtxt);
    if (wpt->pc == 0) {
        profiler_safe_exit();
        return WP_DISABLE; 
    }
    
    jmethodID method_id = 0;
    uint32_t method_version = 0;
    CodeCacheManager &code_cache_manager = Profiler::getProfiler().getCodeCacheManager();
    
    CompiledMethod *method = code_cache_manager.getMethod((uint64_t)(wpt->pc), method_id, method_version);
    if (method == nullptr) {
        profiler_safe_exit();
        return WP_DISABLE;
    }
    
    void *patchedIP = wpt->pc;
    if (!wpt->pcPrecise) {
        void *startAddr = nullptr, *endAddr = nullptr; 
        method->getMethodBoundary(&startAddr, &endAddr);
        if (prevIP > startAddr && prevIP < patchedIP)
            patchedIP = GetPatchedIP(prevIP, endAddr, wpt->pc);
        else
            patchedIP = GetPatchedIP(startAddr, endAddr, wpt->pc);
        if (!IsPCSane(wpt->pc, patchedIP)) {
            profiler_safe_exit();
            return WP_DISABLE; 
        }
        wpt->pc = patchedIP;
        prevIP = patchedIP;
    }

    WP_TriggerAction_t ret = DetectRedundancy(wpt, method_id, method_version, SILENTLOAD_CLIENT_NAME);
    profiler_safe_exit();
    return ret;
}


Profiler::Profiler() { 
    _asgct = (ASGCT_FN)dlsym(RTLD_DEFAULT, "AsyncGetCallTrace"); 
}


void Profiler::init() {
#ifndef COUNT_OVERHEAD
    _method_file.open("agent-trace-method.run");
    _method_file << XML_FILE_HEADER << std::endl;
#endif

    _statistics_file.open("agent-statistics.run");
    ThreadData::thread_data_init();
    assert(PerfManager::processInit(JVM::getArgument()->getPerfEventList(), Profiler::OnSample));
    assert(WP_Init());
    std::string client_name = GetClientName();
    std::transform(client_name.begin(), client_name.end(), std::back_inserter(clientName), ::toupper);
}


void Profiler::shutdown() {
    WP_Shutdown();
    PerfManager::processShutdown();
    ThreadData::thread_data_shutdown();
    output_statistics(); 
    _statistics_file.close();

#ifndef COUNT_OVERHEAD
    _method_file.close();
#endif
}

void Profiler::IncrementGCCouter() {
    __sync_fetch_and_add(&GCCounter, 1);    
}

void Profiler::threadStart() {
    totalWrittenBytes = 0;
    totalLoadedBytes = 0;
    totalUsedBytes = 0;
    totalDeadBytes = 0;
    totalNewBytes = 0;
    totalOldBytes = 0;
    totalOldAppxBytes = 0;
    totalAllocTimes = 0;
    totalSameNUMA = 0;
    totalDiffNUMA = 0;
    totalL1Cachemiss = 0;

    ThreadData::thread_data_alloc();
    ContextTree *ct_tree = new(std::nothrow) ContextTree();
    assert(ct_tree);
    TD_GET(context_state) = reinterpret_cast<void *>(ct_tree);
  
    // settup the output
    {
#ifndef COUNT_OVERHEAD
        char name_buffer[128];
        snprintf(name_buffer, 128, "agent-trace-%u.run", TD_GET(tid));
        OUTPUT *output_stream = new(std::nothrow) OUTPUT();
        assert(output_stream);
        output_stream->setFileName(name_buffer);
        output_stream->writef("%s\n", XML_FILE_HEADER);
        TD_GET(output_state) = reinterpret_cast<void *> (output_stream);
#endif
#ifdef PRINT_PMU_INS
        std::ofstream *pmu_ins_output_stream = new(std::nothrow) std::ofstream();
        char file_name[128];
        snprintf(file_name, 128, "pmu-instruction-%u", TD_GET(tid));
        pmu_ins_output_stream->open(file_name, std::ios::app); 
        TD_GET(pmu_ins_output_stream) = reinterpret_cast<void *>(pmu_ins_output_stream);
#endif
    }
    if (clientName.compare(DEADSTORE_CLIENT_NAME) == 0) assert(WP_ThreadInit(Profiler::OnDeadStoreWatchPoint));
    else if (clientName.compare(SILENTSTORE_CLIENT_NAME) == 0) assert(WP_ThreadInit(Profiler::OnRedStoreWatchPoint));
    else if (clientName.compare(SILENTLOAD_CLIENT_NAME) == 0) assert(WP_ThreadInit(Profiler::OnRedLoadWatchPoint));
    else if (clientName.compare(DATA_CENTRIC_CLIENT_NAME) != 0 && clientName.compare(NUMANODE_CLIENT_NAME) != 0) { 
        ERROR("Can't decode client %s", clientName.c_str());
        assert(false);
    }
    PopulateBlackListAddresses();
    PerfManager::setupEvents();
}


void Profiler::threadEnd() {
    PerfManager::closeEvents();
    if (clientName.compare(DATA_CENTRIC_CLIENT_NAME) != 0 && clientName.compare(NUMANODE_CLIENT_NAME) != 0) {
        WP_ThreadTerminate();
    }
    ContextTree *ctxt_tree = reinterpret_cast<ContextTree *>(TD_GET(context_state));
        
#ifndef COUNT_OVERHEAD    
    OUTPUT *output_stream = reinterpret_cast<OUTPUT *>(TD_GET(output_state));
    std::unordered_set<Context *> dump_ctxt = {};
    
    for (auto elem : (*ctxt_tree)) {
        Context *ctxt_ptr = elem;

	    jmethodID method_id = ctxt_ptr->getFrame().method_id;
        _code_cache_manager.checkAndMoveMethodToUncompiledSet(method_id);
    
        if (ctxt_ptr->getMetrics() != nullptr && dump_ctxt.find(ctxt_ptr) == dump_ctxt.end()) { // leaf node of the redundancy pair
            dump_ctxt.insert(ctxt_ptr);
            xml::XMLObj *obj;
            obj = xml::createXMLObj(ctxt_ptr);
            if (obj != nullptr) {
                output_stream->writef("%s", obj->getXMLStr().c_str());
                delete obj;
            } else continue;
        
            ctxt_ptr = ctxt_ptr->getParent();
            while (ctxt_ptr != nullptr && dump_ctxt.find(ctxt_ptr) == dump_ctxt.end()) {
                dump_ctxt.insert(ctxt_ptr);
                obj = xml::createXMLObj(ctxt_ptr);
                if (obj != nullptr) {
                    output_stream->writef("%s", obj->getXMLStr().c_str());
                    delete obj;
                }
                ctxt_ptr = ctxt_ptr->getParent();
            }
        }
    }
    
    //clean up the output stream
    delete output_stream;
    TD_GET(output_state) = nullptr;
#endif
    
    //clean up the context state
    delete ctxt_tree;
    TD_GET(context_state) = nullptr;

#ifdef PRINT_PMU_INS
    std::ofstream *pmu_ins_output_stream = reinterpret_cast<std::ofstream *>(TD_GET(pmu_ins_output_stream));
    pmu_ins_output_stream->close();
    delete pmu_ins_output_stream;
    TD_GET(pmu_ins_output_stream) = nullptr;
#endif    

    // clear up context-sample tables 
    for (int i = 0; i < NUM_WATERMARK_METRICS; i++) {
        std::unordered_map<Context *, SampleNum> *ctxtSampleTable = reinterpret_cast<std::unordered_map<Context *, SampleNum> *> (TD_GET(ctxt_sample_state)[i]);
        if (ctxtSampleTable != nullptr) {
            delete ctxtSampleTable;
            TD_GET(ctxt_sample_state)[i] = nullptr;
        }
    }

    ThreadData::thread_data_dealloc();

    __sync_fetch_and_add(&grandTotWrittenBytes, totalWrittenBytes);
    __sync_fetch_and_add(&grandTotLoadedBytes, totalLoadedBytes);
    __sync_fetch_and_add(&grandTotUsedBytes, totalUsedBytes);
    __sync_fetch_and_add(&grandTotDeadBytes, totalDeadBytes);
    __sync_fetch_and_add(&grandTotNewBytes, totalNewBytes);
    __sync_fetch_and_add(&grandTotOldBytes, totalOldBytes);
    __sync_fetch_and_add(&grandTotOldAppxBytes, totalOldAppxBytes);
    __sync_fetch_and_add(&grandTotAllocTimes, totalAllocTimes);
    __sync_fetch_and_add(&grandTotSameNUMA, totalSameNUMA);
    __sync_fetch_and_add(&grandTotDiffNUMA, totalDiffNUMA);
    __sync_fetch_and_add(&grandTotL1Cachemiss, totalL1Cachemiss);
}


int Profiler::output_method(const char *buf) {
  _method_file << buf;
  return 0;
}


void Profiler::output_statistics() {
    
    if (clientName.compare(DEADSTORE_CLIENT_NAME) == 0) {
        _statistics_file << grandTotWrittenBytes << std::endl;
        _statistics_file << grandTotDeadBytes << std::endl; 
        _statistics_file << (double)grandTotDeadBytes / (grandTotDeadBytes + grandTotUsedBytes);
    } else if (clientName.compare(SILENTSTORE_CLIENT_NAME) == 0) {
        _statistics_file << grandTotWrittenBytes << std::endl;
        _statistics_file << grandTotOldBytes + grandTotOldAppxBytes << std::endl;
        _statistics_file << (double)grandTotOldBytes / (grandTotOldBytes + grandTotOldAppxBytes + grandTotNewBytes) << std::endl;
        _statistics_file << (double)grandTotOldAppxBytes / (grandTotOldBytes + grandTotOldAppxBytes + grandTotNewBytes);
    } else if (clientName.compare(SILENTLOAD_CLIENT_NAME) == 0) {
        _statistics_file << grandTotLoadedBytes << std::endl;
        _statistics_file << grandTotOldBytes + grandTotOldAppxBytes << std::endl; 
        _statistics_file << (double)grandTotOldBytes / (grandTotOldBytes + grandTotOldAppxBytes + grandTotNewBytes) << std::endl;
        _statistics_file << (double)grandTotOldAppxBytes / (grandTotOldBytes + grandTotOldAppxBytes + grandTotNewBytes);
    } else if (clientName.compare(DATA_CENTRIC_CLIENT_NAME) == 0) {
        _statistics_file << clientName << std::endl;
        _statistics_file << grandTotAllocTimes << std::endl;
        _statistics_file << grandTotL1Cachemiss << std::endl;
    } else if (clientName.compare(NUMANODE_CLIENT_NAME) == 0) {
        _statistics_file << clientName << std::endl;
        _statistics_file << grandTotSameNUMA << std::endl;
        _statistics_file << grandTotDiffNUMA << std::endl;
    }
}
