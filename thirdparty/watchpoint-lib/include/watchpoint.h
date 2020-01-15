// debug register interface

#ifndef _WATCHPOINT_H
#define _WATCHPOINT_H

#include <stdint.h>
#include <stdbool.h>

/* ================================================================== */
// Debug register data types and structures
/* ================================================================== */
typedef enum WP_Access_t {WP_READ, WP_WRITE, WP_RW, WP_INVALID} WP_Access_t;
typedef enum WP_ReplacementPolicy_t {WP_REPLACEMENT_AUTO, WP_REPLACEMENT_EMPTY_SLOT, WP_REPLACEMENT_OLDEST, WP_REPLACEMENT_NEWEST} WP_ReplacementPolicy_t;
typedef enum WP_TriggerAction_t {WP_DISABLE, WP_ALREADY_DISABLED, WP_DISABLE_ALL, WP_RETAIN} WP_TriggerAction_t;

// Data structure that is captured when a WP triggers
typedef struct WP_TriggerInfo_t{
    void *va;
    int watchLen;
    void *watchCtxt;
    void *uCtxt;
    void *pc;
    int pcPrecise; // 1: precise; 0: not precise
    int sampleAccessLen;
    void *sampleValue; 
    int metric_id1;
    // WP_Access_t trappedAccessType;
} WP_TriggerInfo_t;

typedef WP_TriggerAction_t (*WP_TrapCallback_t)( WP_TriggerInfo_t *);
typedef void (*WP_PerfCallback_t) ();

#ifdef __cplusplus
extern "C"
{
#endif

//*****************************Basics*****************************//
extern bool WP_Init();
extern void WP_Shutdown();
extern bool WP_ThreadInit(WP_TrapCallback_t cb_func);
extern void WP_ThreadTerminate();
bool WP_Subscribe(void *va, int watchLen, WP_Access_t watchType, int accessLen,  void *watchCtxt, int metric_id1, bool isCaptureValue);
extern void WP_GetActiveAddresses(void *addrs[], int *numAddr);
extern void WP_GetActiveWatchPoints(WP_TriggerInfo_t wpt[], int *numActiveWP);
extern bool WP_IsAltStackAddress(void *addr);
extern bool WP_IsFSorGS(void * addr);
extern bool WP_IsInsideSigHandler();

//*****************************Advanced Configuration********************//
//If you are using perf events in your own system, you may want to pause all your perf events just after a watchpoint is captured and resume all your perf events just before finishing handling a trap.
extern void WP_SetPerfPauseAndResumeFunctions(WP_PerfCallback_t pause_fn,  WP_PerfCallback_t resume_fn);

extern void WP_DisableAllWatchPoints();

#ifdef __cplusplus
}
#endif

#endif /* _WATCHPOINT_H */
