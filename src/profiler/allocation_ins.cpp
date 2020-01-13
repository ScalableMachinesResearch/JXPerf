#include "allocation_ins.h"

#define MAX_FRAME_NUM (128)
extern SpinLock tree_lock;
extern interval_tree_node *splay_tree_root;
extern thread_local uint64_t totalAllocTimes;
extern thread_local std::unordered_map<Context*, per_context_info_t> allocation_callback_ctxt;

namespace {
	Context *allocation_constructContext(ASGCT_FN asgct, void *context, std::string client_name){
    ASGCT_CallTrace trace;
    ASGCT_CallFrame frames[MAX_FRAME_NUM];
    
    trace.frames = frames;
    trace.env_id = JVM::jni();
    
    asgct(&trace, MAX_FRAME_NUM, context);
    
    ContextTree *ctxt_tree = reinterpret_cast<ContextTree *> (TD_GET(context_state));
    if(ctxt_tree == nullptr) return nullptr;
    
    Context *last_ctxt = nullptr;
        
    for(int i=trace.num_frames - 1 ; i >= 0; i--) {
        ContextFrame ctxt_frame;
        ctxt_frame = frames[i]; // set method_id and bci
        
        if (last_ctxt == nullptr) last_ctxt = ctxt_tree->addContext((uint32_t)CONTEXT_TREE_ROOT_ID, ctxt_frame);
        else last_ctxt = ctxt_tree->addContext(last_ctxt, ctxt_frame);
    }
    	
    ContextFrame ctxt_frame;
    ctxt_frame.bci = -65536;
    if (last_ctxt == nullptr)
        last_ctxt = ctxt_tree->addContext((uint32_t)CONTEXT_TREE_ROOT_ID, ctxt_frame);
    else
        last_ctxt = ctxt_tree->addContext(last_ctxt, ctxt_frame);       
    
    if (client_name.compare(DATA_CENTRIC_CLIENT_NAME) == 0) {
        metrics::ContextMetrics *metrics = last_ctxt->getMetrics();
        if (metrics == nullptr) {
            metrics = new metrics::ContextMetrics();
            last_ctxt->setMetrics(metrics);
        }
        metrics::metric_val_t metric_val;
        metric_val.i = 1;
        assert(metrics->increment(0, metric_val)); // id = 0: allocation times
        totalAllocTimes += 1;
    }
    
    return last_ctxt;
}
}

void empty_splay_tree(interval_tree_node *root) {
    if(root == NULL)
        return;
    empty_splay_tree(LEFT(root));
    empty_splay_tree(RIGHT(root));
    free(root);
}

JNIEXPORT void JNICALL
Java_com_google_monitoring_runtime_instrumentation_AllocationInstrumenter_clearTree(JNIEnv *env, jobject obj) {
    profiler_safe_enter();
    tree_lock.lock();
    empty_splay_tree(splay_tree_root);
    splay_tree_root = NULL; //splay_tree_root also has been removed in empty_splay tree, we couldn't insert anything if we don't reinitialize it here
    tree_lock.unlock();
    profiler_safe_exit();
}

JNIEXPORT void JNICALL
Java_com_google_monitoring_runtime_instrumentation_AllocationInstrumenter_dataCentric(JNIEnv *env, jobject obj, jstring addr, jlong size) { 
    profiler_safe_enter();
    
    const char *tmp = env->GetStringUTFChars(addr, 0);
    char *pend;
    uint64_t startingAddr = strtol(tmp, &pend, 16);
    uint64_t obj_size = size;
    uint64_t endingAddr = startingAddr + obj_size;
    
    std::string client_name = GetClientName();
    ucontext_t context, *cp = &context;
    getcontext(cp);
    Context *ctxt = allocation_constructContext(Profiler::_asgct, (void*)cp, client_name);

	interval_tree_node *node = SplayTree::node_make((void*)startingAddr, (void*)endingAddr, ctxt);

	tree_lock.lock();
    SplayTree::interval_tree_insert(&splay_tree_root, node);
	tree_lock.unlock();

    if (client_name.compare(OBJECT_LEVEL_CLIENT_NAME) == 0) {
        if(allocation_callback_ctxt.find(ctxt) == allocation_callback_ctxt.end()) {
            allocation_callback_ctxt[ctxt].first_process = true;
            allocation_callback_ctxt[ctxt].sample_count = 0;
            allocation_callback_ctxt[ctxt].avail_wp = 0;
        }
        else {
            allocation_callback_ctxt[ctxt].sample_count = 0;
            allocation_callback_ctxt[ctxt].first_process = false;
        }
    }

	env->ReleaseStringUTFChars(addr, tmp);

    profiler_safe_exit();
}
