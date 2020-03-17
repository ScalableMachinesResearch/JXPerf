#ifndef _PROFILER_H
#define _PROFILER_H

#include <signal.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include "perf_interface.h"
#include "stacktraces.h"
#include "code_cache.h"
#include "watchpoint.h"
#include "profiler_support.h"
#include "splay.h"
#include "lock.h"

class Profiler {
public:

  Profiler();
 
  void init();
  void shutdown();

  void threadStart();
  void threadEnd();
  void IncrementGCCouter();

  int output_method(const char *buf); 

  inline CodeCacheManager &getCodeCacheManager() {return _code_cache_manager;}

  inline MethodCache &getUnCompiledMethodCache() {return _uncompiled_method_cache;}

  static inline Profiler &getProfiler(){return _instance;}

  static ASGCT_FN _asgct;

private:
  static void OnSample(int event_idx, perf_sample_data_t *sample_data, void *context, int metric_id1, int metric_id2, int metric_id3);
  static WP_TriggerAction_t OnDeadStoreWatchPoint(WP_TriggerInfo_t *wpi);
  static WP_TriggerAction_t OnRedStoreWatchPoint(WP_TriggerInfo_t *wpi);
  static WP_TriggerAction_t OnRedLoadWatchPoint(WP_TriggerInfo_t *wpi);
  static WP_TriggerAction_t DetectRedundancy(WP_TriggerInfo_t *wpi, jmethodID method_id, uint32_t method_version, std::string client_name);
  static void NumaAnalysis(perf_sample_data_t *sampleData, void *uCtxt, jmethodID method_id, uint32_t method_version, uint32_t threshold, int metric_id2, int metric_id3);
  static void DataCentricAnalysis(perf_sample_data_t *sampleData, void *uCtxt, jmethodID method_id, uint32_t method_version, uint32_t threshold, int metric_id2);
  
  inline void output_statistics(); 

  std::ofstream _method_file;
  std::ofstream _statistics_file;

  CodeCacheManager _code_cache_manager;
  MethodCache _uncompiled_method_cache; // used to store the methods which are never added but shown in context;

  static Profiler _instance;
};

#endif
