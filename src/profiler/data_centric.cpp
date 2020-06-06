#include "config.h"

#ifdef DATA_CENTRIC
#include <assert.h>
#include <stdio.h>
#include <ucontext.h>
#include "data_centric.h"
#include "agent.h"
//#include "thread.h"
#include "profiler.h"

RangeSet<uint64_t, Context *> DataCentric::_allocated_range_set;
ASGCT_FN DataCentric::_asgct = nullptr;

void DataCentric::init(ASGCT_FN asgct){
  _asgct = asgct;
  setMallocHookFunction(malloc_hook_function);
}


void DataCentric::malloc_hook_function(void *ptr, size_t size) {
#define FUNCTION_END {unsetAgentFlag();}

  setAgentFlag();

  //***** We need to get the call stack. We can use the native one provided by JVMTI or also the asynchrous one.
  //***** The native one GetStackTrace() can still cause deadlock which is not preferable.
  //***** We just use the asynchorous one since we already have the infastructure.

 /* NOTE: 
   1) The use of GetStackTrace()
      It requires to use GetCurrentThread() in order to get the thread parameter.
      GetCurrentThread() may fail (maybe due to that it can not bind to any Java thread)
      Even we can get the thread information, GetStackTrace() can still fail as well.
      The worst thing is getStackTrace() may cause deadlock.
   2) The use of AsyncGetCallTrace()
      It can work basically fine. However, in simple test program, it shows a lot of unwalkable stacks.
      I am not sure whether it is most due to the initialization of something (class?) inside JVM.
 */

#if 0  //the use of GetStackTrace()
  {
  jvmtiFrameInfo frames[5];
  jint count;
  jvmtiError err;
  jthread cur_thread;

  
  err = JVM::jvmti()->GetCurrentThread(&cur_thread);
  if (err == JVMTI_ERROR_NONE) {
    err = JVM::jvmti()->GetStackTrace(cur_thread, 0, 1024, frames, &count);
    if (err == JVMTI_ERROR_NONE) {
      INFO("normal ");
    }
    else {
      INFO("stack_error ");
    }
  }
  else{
    INFO("thread_error ");
  }
  }
#endif

#if 0 // The use of AsyncGetCallTrace()
  ucontext_t context;
  assert(getcontext(&context) == 0);

  ASGCT_CallTrace trace;
  ASGCT_CallFrame frames[1024];
  trace.frames = frames;
  trace.env_id = JVM::jni();
  
  _asgct(&trace, 1024, (void *)&context);

  if (trace.num_frames <= 0){ // it is unwalkable. We just return
    FUNCTION_END;
    return;
  }


  Thread * thread = Thread::getCurrentThread();
  ContextTree *ctxt_tree = reinterpret_cast<ContextTree *> (thread->getThreadData(Thread::TD_CONTEXT));
  //NOTE: Currently we assume that each thread only allocate memory for its own use. 
  //We do not monitor the free()
  //TODO: Check whether the memory allocated will be transferred to another thread for use (except GC)
  //UPDATE: If we print the malloc thread, it seems that we can assume the allocated memory is not transferable.

  Context *last_ctxt = nullptr;
  CodeMethod *curr_m = nullptr, *prev_m = nullptr;

  CodeCache &codecache = Profiler::getProfiler().getCodeCache();

  //INFO("malloc %u from %d\n", size, thread->getTID());
#if 0
  for(int i=0; i < trace.num_frames; i++){
    JvmtiScopedPtr <char> method_name;
    jvmtiError err = (JVM::jvmti())->GetMethodName( frames[i].method_id, method_name.getRef(), NULL, NULL);
    if (err == JVMTI_ERROR_NONE){
//      ERROR("(%s<--", method_name.get());
    }
    else{
//      ERROR("(ERROR)<--");
    }
  }
#endif
#if 1  
  for(int i=trace.num_frames - 1 ; i >= 0; prev_m = curr_m, i--){
    //TODO: We need to consider how to include the native method.
    ContextFrame ctxtframe;
    ctxtframe = frames[i]; //set method_id and lineno

    curr_m = codecache.getMethod(frames[i].method_id);
    if(curr_m){
      ctxtframe.method_name = curr_m->getMethodName();
      ctxtframe.method_version = curr_m->getMethodVersion();
    }
    if (last_ctxt == nullptr ){
     last_ctxt = ctxt_tree->addContext((uint32_t)CONTEXT_TREE_ROOT_ID, ctxtframe);
    }
    else {
      last_ctxt = ctxt_tree->addContext(last_ctxt, ctxtframe);
    }
    //assert(last_ctxt);
  }
#endif
#if 1
  //add the pseudo node to the leaf
  assert(last_ctxt);
  {
    ContextFrame ctxtframe;
    ctxtframe.method_id = (jmethodID) 1;
    last_ctxt = ctxt_tree->addContext(last_ctxt, ctxtframe);
    assert(last_ctxt);
  }
#endif
#endif


  //Add the allocated range
  uint64_t start, end;
  start = (uint64_t) ptr;
  end = start + size - 1;
 
  //_allocated_range_set.insert(start, end, last_ctxt);

  INFO("SIZE = %lu\n", size);

  FUNCTION_END;
#undef FUNCTION_END
}

#endif
