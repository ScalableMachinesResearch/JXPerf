
//////////////
// #INCLUDE //
//////////////
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <dlfcn.h>

#include "thread_data.h"
#include "agent.h"
#include "profiler.h"
#include "debug.h"
#include "safe-sampling.h"
#include "profiler_support.h"

/* Used to block the interrupt from PERF */
#define BLOCK_SAMPLE (TD_GET(inside_agent) = true) 

/* Used to revert back to the original signal blocking setting */
#define UNBLOCK_SAMPLE (TD_GET(inside_agent) = false)

JavaVM* JVM::_jvm = nullptr;
jvmtiEnv* JVM::_jvmti = nullptr;
Argument* JVM::_argument = nullptr;


void JVM::parseArgs(const char *arg) {
  assert(_argument == nullptr);
  _argument = new(std::nothrow) Argument(arg);
  assert(_argument);
}


// Call GetClassMethods on a given class to force the creation of jmethodIDs of it.
static void createJMethodIDsForClass(jvmtiEnv *jvmti, jclass klass) {
  jint method_count;
  JvmtiScopedPtr<jmethodID> methods;
  jvmtiError err = jvmti->GetClassMethods(klass, &method_count, methods.getRef());
  if (err != JVMTI_ERROR_NONE && err != JVMTI_ERROR_CLASS_NOT_PREPARED) {
    // JVMTI_ERROR_CLASS_NOT_PREPARED is okay because some classes may
    // be loaded but not prepared at this point.
    JvmtiScopedPtr<char> signature;
    err = jvmti->GetClassSignature(klass, signature.getRef(), NULL);
    ERROR("Failed to create method ID in class %s with error %d\n", signature.get(), err);
  }
}


/**
* VM init callback
*/
static void JNICALL callbackVMInit(jvmtiEnv *jvmti, JNIEnv* jni, jthread thread) { 
  // Forces the creation of jmethodIDs of the classes that had already been loaded (eg java.lang.Object, java.lang.ClassLoader) and OnClassPrepare() misses.
  // BLOCK_SAMPLE;
  jint class_count;
  JvmtiScopedPtr<jclass> classes;
  assert(jvmti->GetLoadedClasses(&class_count, classes.getRef()) == JVMTI_ERROR_NONE);
  jclass *classList = classes.get();
  for (int i = 0; i < class_count; ++i) {
    jclass klass = classList[i];
    createJMethodIDsForClass(jvmti, klass);
  }
  std::string client_name = GetClientName();

  if (client_name.compare(DATA_CENTRIC_CLIENT_NAME) == 0 || client_name.compare(OBJECT_LEVEL_CLIENT_NAME) == 0) {
    jclass myClass = NULL;
    jmethodID main = NULL;
    jmethodID main_gc = NULL;
        
    //Call java agent register_callback
    myClass = jni->FindClass("com/google/monitoring/runtime/instrumentation/AllocationInstrumenter");
        
    main = jni->GetStaticMethodID(myClass, "register_callback", "([Ljava/lang/String;)V");
    jni->CallStaticVoidMethod(myClass, main, " ");

    if(client_name.compare(DATA_CENTRIC_CLIENT_NAME) == 0) {
        main_gc = jni->GetStaticMethodID(myClass, "installGCMonitoring", "([Ljava/lang/String;)V");
        jni->CallStaticVoidMethod(myClass, main_gc, " ");
    }
  }
  // UNBLOCK_SAMPLE;
}

/**
* VM Death callback
*/

static void dump_uncompiled_method(jmethodID method, void *data) {
  if (method == 0) return;
  InterpretMethod *m = new(std::nothrow) InterpretMethod(method);
  assert(m);
  xml::XMLObj *obj = m->createXMLObj();
  Profiler::getProfiler().output_method(obj->getXMLStr().c_str());
  delete obj;
  delete m;
  obj = nullptr;
  m = nullptr;
}

static void JNICALL callbackVMDeath(jvmtiEnv *jvmti_env, JNIEnv* jni_env) {
  BLOCK_SAMPLE;
  INFO("VMDeath\n");
#ifndef COUNT_OVERHEAD
  Profiler::getProfiler().getUnCompiledMethodCache().performActionAll(dump_uncompiled_method, nullptr);
#endif
  INFO("VMDeath end\n");
  UNBLOCK_SAMPLE;
}

/**
* Thread start
*/
void JNICALL callbackThreadStart(jvmtiEnv *jvmti, JNIEnv* jni_env, jthread thread) {
  BLOCK_SAMPLE;
  INFO("callbackThreadStart %d\n", TD_GET(tid));
  Profiler::getProfiler().threadStart();
  UNBLOCK_SAMPLE;
}
/**
* Thread end
*/
static void JNICALL callbackThreadEnd(jvmtiEnv *jvmti, JNIEnv* jni_env, jthread thread) {
  BLOCK_SAMPLE;
  INFO("callbackThreadEnd %d\n", TD_GET(tid));
  Profiler::getProfiler().threadEnd();
  INFO("callbackThreadEnd %d END\n", TD_GET(tid));
  UNBLOCK_SAMPLE;
}

void JNICALL callbackGCEnd(jvmtiEnv *jvmti) {
  BLOCK_SAMPLE;
  // printf("test\n");
  INFO("callbackGCEnd %d\n", TD_GET(tid));
  Profiler::getProfiler().IncrementGCCouter();
  INFO("callbackGCEnd %d END\n", TD_GET(tid));
  UNBLOCK_SAMPLE;
}

static void JNICALL callbackMethodEntry(jvmtiEnv *jvmti, JNIEnv* jni_env, jthread thread, jmethodID method) {
}

static void JNICALL callbackException(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread, jmethodID method, jlocation location, jobject exception, jmethodID catch_method, jlocation catch_location) {
}

static void JNICALL callbackNativeMethodBind(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread, jmethodID method, void* address, void** new_address_ptr) {
  BLOCK_SAMPLE;
  JvmtiScopedPtr<char> methodName;
  jvmtiError err = (JVM::jvmti())->GetMethodName( method, methodName.getRef(), NULL, NULL);
  INFO("callbackNativeMethodBind %s\n", methodName.get());
  UNBLOCK_SAMPLE;
}

static void JNICALL callbackCompiledMethodLoad(jvmtiEnv *jvmti_env, jmethodID method, jint code_size, const void* code_addr, jint map_length, const jvmtiAddrLocationMap* map, const void* compile_info) { 
  BLOCK_SAMPLE;
  //The Loaded method can be native
  // JvmtiScopedPtr<char> methodName;
  // jvmtiError err = (JVM::jvmti())->GetMethodName( method, methodName.getRef(), NULL, NULL);
  // INFO("CompiledMethodLoad %d MethodID %lx, MethodName %s, start %lx\n", TD_GET(tid),(uint64_t)method, methodName.get(), (uint64_t) code_addr);

#if 0 // examine inline information
  const jvmtiCompiledMethodLoadRecordHeader *header;
  uint64_t first_pc = UINT64_MAX;
  for (header = (jvmtiCompiledMethodLoadRecordHeader *) compile_info; header != NULL; header = header->next) {
    if (header->kind == JVMTI_CMLR_INLINE_INFO){
        first_pc = map_length > 0 ? (uint64_t)map[0].start_address : 0;
        decode_method(code_addr, (void *)((uint64_t)code_addr + code_size -1), (const void *)first_pc); 
        for (int i = 0; i < map_length; ++i) {
            fflush(stdout);
        }
        const jvmtiCompiledMethodLoadInlineRecord *record = (jvmtiCompiledMethodLoadInlineRecord *) header;
        for(int i=0; i < record->numpcs; i++){
	        PCStackInfo* pcinfo = &(record->pcinfo[i]);
	        if (first_pc > (uint64_t)pcinfo->pc)
                first_pc = (uint64_t)pcinfo->pc;
            // for(int j=0; j < pcinfo->numstackframes;j++){
            // }
        }
        */
    }
  }
#endif

  CompiledMethod *m = Profiler::getProfiler().getCodeCacheManager().addMethodAndRemoveFromUncompiledSet(method, code_size, code_addr, map_length, map);
  // CompiledMethod *m = Profiler::getProfiler().getCodeCacheManager().addMethod(method, code_size, code_addr, map_length, map);
  // Profiler::getProfiler().getUnCompiledMethodCache().removeMethod(method);
  xml::XMLObj *obj = xml::createXMLObj(m);
#ifndef COUNT_OVERHEAD  
  Profiler::getProfiler().output_method(obj->getXMLStr().c_str());
#endif
  delete obj;
  obj = nullptr;
  INFO("CompiledMethodLoad finish\n");
  UNBLOCK_SAMPLE;
}

static void JNICALL callbackCompiledMethodUnload(jvmtiEnv *jvmti_env, jmethodID method, const void* code_addr) {
    BLOCK_SAMPLE;
    Profiler::getProfiler().getCodeCacheManager().removeMethod(method, code_addr);
    UNBLOCK_SAMPLE;
}
static void JNICALL callbackDynamicCodeGenerated(jvmtiEnv *jvmti_env, const char* name, const void* address, jint length) {
}

// This has to be here, or the VM turns off class loading events.
// And AsyncGetCallTrace needs class loading events to be turned on!
static void JNICALL callbackClassLoad(jvmtiEnv *jvmti, JNIEnv* jni_env, jthread thread, jclass klass) {
    // BLOCK_SAMPLE;
    IMPLICITLY_USE(jvmti);
    IMPLICITLY_USE(jni_env);
    IMPLICITLY_USE(thread);
    IMPLICITLY_USE(klass);
    // UNBLOCK_SAMPLE;
}

// Call GetClassMethods on a given class to force the creation of jmethodIDs of it.
// Otherwise, some method IDs are missing.
static void JNICALL callbackClassPrepare(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread, jclass klass) {
    // BLOCK_SAMPLE;
    IMPLICITLY_USE(jni);
    IMPLICITLY_USE(thread);
    createJMethodIDsForClass(jvmti, klass);
    // UNBLOCK_SAMPLE;
}



/////////////
// METHODS //
/////////////
bool JVM::init(JavaVM *jvm, const char *arg) {
  jvmtiError error;
  jint res;
  jvmtiEventCallbacks callbacks;

  _jvm = jvm;

  // parse the input arguments
  parseArgs(arg);

  //////////////////////////
  // Init JVMTI environment:
  res = _jvm->GetEnv((void **) &_jvmti, JVMTI_VERSION_1_0);
  if (res != JNI_OK || _jvmti == NULL) {
    ERROR("Unable to access JVMTI Version 1 (0x%x),"
    " is your J2SE a 1.5 or newer version?"
    " JNIEnv's GetEnv() returned %d\n",
    JVMTI_VERSION_1, res);
    return false;
  }
  
  {
    jvmtiJlocationFormat location_format;
    error = _jvmti->GetJLocationFormat(&location_format);
    assert(check_jvmti_error(error, "can't get location format"));
    assert(location_format == JVMTI_JLOCATION_JVMBCI);//currently only support this implementation
  }

  /////////////////////
  // Init capabilities:
  jvmtiCapabilities capa;
  memset(&capa, '\0', sizeof(jvmtiCapabilities));
  //capa.can_signal_thread = 1;
  //capa.can_get_owned_monitor_info = 1;
  //capa.can_suspend = 1;
  //capa.can_generate_exception_events = 1;
  //capa.can_generate_monitor_events = 1;
  capa.can_generate_garbage_collection_events = 1;
  capa.can_get_source_file_name = 1; //GetSourceFileName()
  capa.can_get_line_numbers = 1; // GetLineNumberTable()
  capa.can_generate_method_entry_events = 1; // This one must be enabled in order to get the stack trace
  //capa.can_generate_native_method_bind_events = 1;
  capa.can_generate_compiled_method_load_events = 1;

  error = _jvmti->AddCapabilities(&capa);
  check_jvmti_error(error, "Unable to get necessary JVMTI capabilities.");

  //////////////////
  // Init callbacks:
  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.VMInit = &callbackVMInit;
  callbacks.VMDeath = &callbackVMDeath;
  callbacks.ThreadStart = &callbackThreadStart;
  callbacks.ThreadEnd = &callbackThreadEnd;
  callbacks.GarbageCollectionFinish = &callbackGCEnd;

  ////callbacks.Exception = &callbackException;
  ////callbacks.MethodEntry = &callbackMethodEntry;
  ////callbacks.NativeMethodBind = &callbackNativeMethodBind;
  callbacks.CompiledMethodLoad = &callbackCompiledMethodLoad;
  callbacks.CompiledMethodUnload = &callbackCompiledMethodUnload;
  callbacks.DynamicCodeGenerated = &callbackDynamicCodeGenerated;

  callbacks.ClassLoad = &callbackClassLoad;
  callbacks.ClassPrepare = &callbackClassPrepare;

  error = _jvmti->SetEventCallbacks(&callbacks, (jint)sizeof(callbacks));
  check_jvmti_error(error, "Cannot set jvmti callbacks");
  ///////////////
  // Init events:
  error = _jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_INIT, (jthread)NULL);
  check_jvmti_error(error, "Cannot set event notification");
  error = _jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_DEATH, (jthread)NULL);
  check_jvmti_error(error, "Cannot set event notification");
  error = _jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_THREAD_START, (jthread)NULL);
  check_jvmti_error(error, "Cannot set event notification");
  error = _jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_THREAD_END, (jthread)NULL);
  check_jvmti_error(error, "Cannot set event notification");
  error = _jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_GARBAGE_COLLECTION_FINISH, (jthread)NULL);
  check_jvmti_error(error, "Cannot set event notification");
  //error = jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_EXCEPTION, (jthread)NULL);
  //check_jvmti_error(jvmti, error, "Cannot set event notification");
  //error = _jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_METHOD_ENTRY, (jthread)NULL);
  //check_jvmti_error( error, "Cannot set event notification");

  //error = _jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_NATIVE_METHOD_BIND, (jthread)NULL);
  //check_jvmti_error( error, "Cannot set event notification");
  error = _jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_COMPILED_METHOD_LOAD, (jthread)NULL);
  check_jvmti_error(error, "Cannot set event notification");
  error = _jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_COMPILED_METHOD_UNLOAD, (jthread)NULL);
  check_jvmti_error(error, "Cannot set event notification");

  error = _jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_DYNAMIC_CODE_GENERATED, (jthread)NULL);
  check_jvmti_error(error, "Cannot set event notification");

  error = _jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_CLASS_LOAD, (jthread)NULL);
  check_jvmti_error(error, "Cannot set event notification");
  error = _jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_CLASS_PREPARE, (jthread)NULL);
  check_jvmti_error(error, "Cannot set event notification");

#if 0
  error = _jvmti->GenerateEvents(JVMTI_EVENT_DYNAMIC_CODE_GENERATED);
  check_jvmti_error(error, "Cannot generate DynamicCodeGenerate");
  error = _jvmti->GenerateEvents(JVMTI_EVENT_COMPILED_METHOD_LOAD);
  check_jvmti_error(error, "Cannot generate CompiledMethodLoad");
#endif

  Profiler::getProfiler().init();

  return true;
}

bool JVM::shutdown() {

  Profiler::getProfiler().shutdown();
  if (_argument){
    delete _argument;
    _argument = nullptr;
  }
  return true; 
}


/**
* Agent entry point
*/
JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM *jvm, char *options, void *reserved) {
  // for debugging 
  // bool pause = true; while(pause);

  BLOCK_SAMPLE;
  INFO("Agent argument = %s\n", options);
  if (!JVM::init(jvm, options)) {
    UNBLOCK_SAMPLE;
    return JNI_ERR;
  }
  INFO("Agent_onLoad\n");
  UNBLOCK_SAMPLE;
  return JNI_OK;
}

/**
* Agent exit point. Last code executed.
*/
JNIEXPORT void JNICALL Agent_OnUnload(JavaVM *vm) {
  BLOCK_SAMPLE;
  JVM::shutdown();
  INFO("Agent_OnUnload\n");
  UNBLOCK_SAMPLE;
}
