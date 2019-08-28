#ifndef _STACKTRACES_H
#define _STACKTRACES_H
#include <jvmti.h>

//  Fields:
//    1) For Java frame (interpreted and compiled),
//       lineno    - bci of the method being executed or -1 if bci is not available
//       method_id - jmethodID of the method being executed
//    2) For native method
//       lineno    - (-3)
//       method_id - jmethodID of the method being executed


typedef struct {
    jint lineno;                      // line number in the source file
    jmethodID method_id;              // method executed in this frame
} ASGCT_CallFrame;

// call trace copied from old .h file and renamed
typedef struct {
    JNIEnv *env_id;                   // Env where trace was recorded
    jint num_frames;                  // number of frames in this trace
    ASGCT_CallFrame *frames;          // frames
} ASGCT_CallTrace;

// These name match the names reported by the forte quality kit
enum {
  ticks_no_Java_frame         =  0,
  ticks_no_class_load         = -1,
  ticks_GC_active             = -2,
  ticks_unknown_not_Java      = -3,
  ticks_not_walkable_not_Java = -4,
  ticks_unknown_Java          = -5,
  ticks_not_walkable_Java     = -6,
  ticks_unknown_state         = -7,
  ticks_thread_exit           = -8,
  ticks_deopt                 = -9,
  ticks_safepoint             = -10
};

typedef void (*ASGCT_FN)(ASGCT_CallTrace *, jint , void *);

#endif
