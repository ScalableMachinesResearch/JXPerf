#define _GNU_SOURCE 1
#include "watchpoint.h"
#include "watchpoint_util.h"
#include "watchpoint_mmap.h"

#include <pthread.h>
#include <sys/syscall.h>
#include <asm/prctl.h>
#include <sys/prctl.h>


//*******************Global Variable*****************************/
static WP_Config_t wpConfig;
static pthread_key_t WP_ThreadData_key;

static void linux_perf_events_pause(){
   WP_ThreadData_t * tData = TD_GET();
   for (int i = 0; i < wpConfig.maxWP; i++) {
        if(tData->watchPointArray[i].isActive) {
            ioctl(tData->watchPointArray[i].fileHandle, PERF_EVENT_IOC_DISABLE, 0);
        }
   }
}


static void linux_perf_events_resume() {
   WP_ThreadData_t *tData = TD_GET();
   for (int i = 0; i < wpConfig.maxWP; i++) {
        if(tData->watchPointArray[i].isActive) {
            ioctl(tData->watchPointArray[i].fileHandle, PERF_EVENT_IOC_ENABLE, 0);
        }
   }
}


/**********     WP setting     **********/
static bool ValidateWPData(void *va, int watchLen) {
    // Check alignment
#if defined(__x86_64__) || defined(__amd64__) || defined(__x86_64) || defined(__amd64)
    switch (watchLen) {
        case 0: EMSG("\nValidateWPData: 0 length WP never allowed"); return false;
        case 1:
        case 2:
        case 4:
        case 8:
            if(IS_ALIGNED(va, watchLen))
                return true; // unaligned
            else
                return false;
            break;
        default:
            EMSG("Unsuppported WP length %d", watchLen);
            return false; // unsupported alignment
    }
#else
#error "unknown architecture"
#endif
}

static bool IsOverlapped(void *va, int watchLen) {
    WP_ThreadData_t * tData = TD_GET();
    // Is a WP with the same/overlapping address active?
    for (int i = 0; i < wpConfig.maxWP; i++) {
        if(tData->watchPointArray[i].isActive) {
            if(ADDRESSES_OVERLAP((uint64_t)tData->watchPointArray[i].va, tData->watchPointArray[i].watchLen, (uint64_t)va, watchLen)){
                return true;
            }
        }
    }
    return false;
}

static inline void CaptureValue(void *va, void *valLoc, int watchLen) {
    switch(watchLen) {
        default: // force 1 length
        case 1: *((uint8_t *)valLoc) = *(uint8_t *)va; break;
        case 2: *((uint16_t *)valLoc) = *(uint16_t *)va; break;
        case 4: *((uint32_t *)valLoc) = *(uint32_t *)va; break;
        case 8: *((uint64_t *)valLoc) = *(uint64_t *)va; break;
    }
}

static inline void EnableWatchpoint(int fd) {
    // Start the event
    CHECK(ioctl(fd, PERF_EVENT_IOC_ENABLE, 0));
}

static inline void DisableWatchpoint(WP_RegisterInfo_t *wpi) {
    // Stop the event
    assert(wpi->fileHandle != -1);
    CHECK(ioctl(wpi->fileHandle, PERF_EVENT_IOC_DISABLE, 0));
    wpi->isActive = false;
}

static void DisArm(WP_RegisterInfo_t * wpi) {
    assert(wpi->fileHandle != -1);

    if(wpi->mmapBuffer) {
        WP_UnmapBuffer(wpi->mmapBuffer, wpConfig.pgsz);
	wpi->mmapBuffer = 0;
    }

    CHECK(close(wpi->fileHandle));
    wpi->fileHandle = -1;
    wpi->isActive = false;
}

void DisableWatchpointWrapper(WP_RegisterInfo_t *wpi) {
    if(wpConfig.isWPModifyEnabled && wpi->isActive) {
        DisableWatchpoint(wpi);
    } else if (!wpConfig.isWPModifyEnabled && wpi->fileHandle != -1) {
        DisArm(wpi);
    }
}

static void OnWatchPoint(int signum, siginfo_t *info, void *uCtxt) {
    WP_ThreadData_t *tData = TD_GET();
    if (!tData) return;
    // tData->insideSigHandler = true;
    
    linux_perf_events_pause();
    if (wpConfig.userPerfPause){
        wpConfig.userPerfPause();
    }

#ifdef SANITIZATION_CHECK
    bool false_positive = true;
    for(int i = 0 ; i < wpConfig.maxWP; i++) {
        if(info->si_fd == tData->watchPointArray[i].fileHandle) {
            false_positive = false;
            break;
        }
    }
    if (false_positive) EMSG("\n A false-positive WP trigger in thread %d\n", gettid()); 
#endif   

    //find which watchpoint fired
    int location = -1;
    for(int i = 0 ; i < wpConfig.maxWP; i++) {
        if((tData->watchPointArray[i].isActive) && (info->si_fd == tData->watchPointArray[i].fileHandle)) {
            location = i;
            break;
        }
    }

    if(location == -1) {
	// if no existing WP is matched, just ignore and return
        // EMSG("\n thread %d WP trigger did not match any known active WP\n", gettid());
        // tData->insideSigHandler = false;
        return;
    }

    WP_TriggerAction_t retVal;
    WP_RegisterInfo_t *wpi = &tData->watchPointArray[location];
    DisableWatchpoint(wpi);

    WP_TriggerInfo_t wpt = {
        .va = wpi->va,
        .watchLen = wpi->watchLen,
        .watchCtxt = wpi->watchCtxt,
        .uCtxt = uCtxt,
        .pc = 0,
        .pcPrecise = 0,
        .sampleAccessLen = wpi->sampleAccessLen,
        .sampleValue = &(wpi->sampleValue[0]),
        .metric_id1 = wpi->metric_id1
    };
   
    if (wpConfig.isLBREnabled == false) {
    	retVal = tData->fptr(&wpt); 
    } else { 
    	if (WP_CollectTriggerInfo(wpi, &wpt, uCtxt, wpConfig.pgsz)){
    	    retVal = tData->fptr(&wpt); 
    	} else {
	    // drop this sample
	    retVal = WP_DISABLE;
    	}
    }
    
    switch (retVal) {
        case WP_DISABLE: {
	    DisableWatchpointWrapper(wpi);
            // Reset per WP probability
            wpi->samplePostFull = SAMPLES_POST_FULL_RESET_VAL;
        }
        break;
        case WP_DISABLE_ALL: {
            for(int i = 0; i < wpConfig.maxWP; i++) {
                DisableWatchpointWrapper(&(tData->watchPointArray[i]));
                // Reset per WP probability
                tData->watchPointArray[i].samplePostFull = SAMPLES_POST_FULL_RESET_VAL;
            }
        }
        break;
        case WP_ALREADY_DISABLED: { // Already disabled, perhaps in pre-WP action
            assert(wpi->isActive == false);
            // Reset per WP probability
            wpi->samplePostFull = SAMPLES_POST_FULL_RESET_VAL;
        }
        break;
        case WP_RETAIN: { // resurrect this wp
            if(!wpi->isActive) {
                EnableWatchpoint(wpi->fileHandle);
                wpi->isActive = true;
            }
        }
        break;
        default: // Retain the state
            break;
    }

    if (wpConfig.userPerfResume) {
        wpConfig.userPerfResume();
    }
    linux_perf_events_resume();
    // tData->insideSigHandler = false;
    return;
}

static void CreateWatchPoint(WP_RegisterInfo_t *wpi, int watchLen, WP_Access_t watchType, void *va, int accessLen, void *watchCtxt, int metric_id1, bool modify) {
    // Perf event settings
    struct perf_event_attr pe;
    memset(&pe, 0, sizeof(struct perf_event_attr));
    pe.type                   = PERF_TYPE_BREAKPOINT;
    pe.size                   = sizeof(struct perf_event_attr);
    // pe.bp_type               = HW_BREAKPOINT_W,
    // pe.bp_len                = HW_BREAKPOINT_LEN_4,
    pe.sample_period          = 1;
    pe.sample_type            = (PERF_SAMPLE_IP );
    // pe.branch_sample_type = (PERF_SAMPLE_BRANCH_ANY);
    pe.disabled               = 0; /* enabled */
    pe.exclude_user           = 0;
    pe.exclude_kernel         = 1;
    pe.exclude_hv             = 1;
    pe.precise_ip             = wpConfig.isLBREnabled? 2 /*precise_ip 0 skid*/ : 0 /* arbitraty skid */;

    switch (watchLen) {
        case 1: pe.bp_len = HW_BREAKPOINT_LEN_1; break;
        case 2: pe.bp_len = HW_BREAKPOINT_LEN_2; break;
        case 4: pe.bp_len = HW_BREAKPOINT_LEN_4; break;
        case 8: pe.bp_len = HW_BREAKPOINT_LEN_8; break;
        default:
            // EMSG("Unsupported .bp_len %d: %s\n", watch_length,strerror(errno));
            exit(0);
    }
    pe.bp_addr = (uintptr_t)va;
    switch (watchType) {
        case WP_READ: pe.bp_type = HW_BREAKPOINT_R; break;
        case WP_WRITE: pe.bp_type = HW_BREAKPOINT_W; break;
        default: pe.bp_type = HW_BREAKPOINT_W | HW_BREAKPOINT_R;
    }

#if defined(FAST_BP_IOC_FLAG)
    if(modify) {
        // modification
        assert(wpi->fileHandle != -1);
        assert(wpi->mmapBuffer != 0);
        // DisableWatchpoint(wpi);
        CHECK(ioctl(wpi->fileHandle, FAST_BP_IOC_FLAG, (unsigned long) (&pe)));
        if (!wpi->isActive) {
	    EnableWatchpoint(wpi->fileHandle);
	}
    } else
#endif
    {
        // fresh creation
        // Create the perf_event for this thread on all CPUs with no event group
        int perf_fd = perf_event_open(&pe, 0, -1, -1 /*group*/, 0);
        if (perf_fd == -1) {
            EMSG("Failed to open perf event file: %s\n",strerror(errno));
            exit(0);
        }
        // Set the perf_event file to async mode
        CHECK(fcntl(perf_fd, F_SETFL, fcntl(perf_fd, F_GETFL, 0) | O_ASYNC));
        // Tell the file to send a signal when an event occurs
        CHECK(fcntl(perf_fd, F_SETSIG, wpConfig.signalDelivered));
        // Deliver the signal to this thread
        struct f_owner_ex fown_ex;
        fown_ex.type = F_OWNER_TID;
        fown_ex.pid  = gettid();
        int ret = fcntl(perf_fd, F_SETOWN_EX, &fown_ex);
        if (ret == -1) {
            EMSG("Failed to set the owner of the perf event file: %s\n", strerror(errno));
            return;
        }

        // CHECK(fcntl(perf_fd, F_SETOWN, gettid()));

        wpi->fileHandle = perf_fd;
        // mmap the file if lbr is enabled
        if(wpConfig.isLBREnabled) {
            wpi->mmapBuffer = WP_MapBuffer(perf_fd, wpConfig.pgsz);
        } else {
	    wpi->mmapBuffer = 0;
	}
    } 

    wpi->isActive = true;
    wpi->va = (void *)pe.bp_addr;
    wpi->watchLen = watchLen;
    wpi->sampleAccessLen = accessLen;
    wpi->watchCtxt = watchCtxt;
    wpi->metric_id1 = metric_id1;
    wpi->startTime = rdtsc();
}


static bool ArmWatchPoint(WP_RegisterInfo_t *wpi, int watchLen, WP_Access_t watchType, void *va, int accessLen, void *watchCtxt, int metric_id1) {
    // if WP modification is suppoted use it
    if(wpConfig.isWPModifyEnabled) {
        // Does not matter whether it was active or not.
        // If it was not active, enable it.
        if(wpi->fileHandle != -1) {
            CreateWatchPoint(wpi, watchLen, watchType, va, accessLen, watchCtxt, metric_id1, true);
            return true;
        }
    }

    // disable the old WP if active
    if(wpi->isActive) {
        DisArm(wpi);
    }
    CreateWatchPoint(wpi, watchLen, watchType, va, accessLen, watchCtxt, metric_id1, false);
    return true;
}


// Finds a victim slot to set a new WP
static WP_Victim_t GetVictim(int *location, WP_ReplacementPolicy_t policy) {
    WP_ThreadData_t *tData = TD_GET();
    // If any WP slot is inactive, return it;
    for(int i = 0; i < wpConfig.maxWP; i++) {
        if(!tData->watchPointArray[i].isActive) {
            *location = i;
            // Increase samplePostFull for those who survived.
            for(int rest = 0; rest < wpConfig.maxWP; rest++) {
                 if (tData->watchPointArray[rest].isActive) {
                     tData->watchPointArray[rest].samplePostFull++;
                 }
            }
            return WP_VICTIM_EMPTY_SLOT;
        }
    }
    switch (policy) {
        case WP_REPLACEMENT_AUTO:{
            // Shuffle the visit order
            int slots[MAX_WP_SLOTS];
            for(int i = 0; i < wpConfig.maxWP; i++)
                slots[i] = i;
            // Shuffle
            for(int i = 0; i < wpConfig.maxWP; i++){
                long int randVal;
                lrand48_r(&(tData->randBuffer), &randVal);
                randVal = randVal % wpConfig.maxWP;
                int tmp = slots[i];
                slots[i] = slots[randVal];
                slots[randVal] = tmp;
            }

            // attempt to replace each WP with its own probability
            for(int i = 0; i < wpConfig.maxWP; i++) {
                int loc = slots[i];
                double probabilityToReplace =  1.0/(1.0 + (double)tData->watchPointArray[loc].samplePostFull);
                double randValue;
                drand48_r(&(tData->randBuffer), &randValue);

                // update tData.samplePostFull
                tData->watchPointArray[loc].samplePostFull++;

                if(randValue <= probabilityToReplace) {
                    *location = loc;
                    for(int rest = i+1; rest < wpConfig.maxWP; rest++){
                        tData->watchPointArray[slots[rest]].samplePostFull++;
                    }
                    return WP_VICTIM_NON_EMPTY_SLOT;
                }
                // TODO: Milind: Not sure whether I should increment samplePostFull of the remainiing slots.
            }
            // this is an indication not to replace, but if the client chooses to force, they can
            *location = slots[0] /*random value*/;
            return WP_VICTIM_NONE_AVAILABLE;
        }
            break;

        case WP_REPLACEMENT_NEWEST:{
            // Always replace the newest

            int64_t newestTime = 0;
            for(int i = 0; i < wpConfig.maxWP; i++){
                if(newestTime < tData->watchPointArray[i].startTime) {
                    *location = i;
                    newestTime = tData->watchPointArray[i].startTime;
                }
            }
            return WP_VICTIM_NON_EMPTY_SLOT;
        }
            break;
        case WP_REPLACEMENT_OLDEST:{
            // Always replace the oldest

            int64_t oldestTime = INT64_MAX;
            for(int i = 0; i < wpConfig.maxWP; i++){
                if(oldestTime > tData->watchPointArray[i].startTime) {
                    *location = i;
                    oldestTime = tData->watchPointArray[i].startTime;
                }
            }
            return WP_VICTIM_NON_EMPTY_SLOT;
        }
            break;

        case WP_REPLACEMENT_EMPTY_SLOT:{
            return WP_VICTIM_NONE_AVAILABLE;
        }
            break;
        default:
            return WP_VICTIM_NONE_AVAILABLE;
    }
    // No unarmed WP slot found.
}


//*************************************Interface Implementation*****************************//

bool WP_Init(){
    if ( pthread_key_create(&WP_ThreadData_key, NULL) != 0){
        EMSG("Failed to pthread_key_create: %s\n", strerror(errno));
        return false;
    }

    volatile int dummyWP[MAX_WP_SLOTS];

#if defined(FAST_BP_IOC_FLAG)
    struct perf_event_attr peLBR;
    memset(&peLBR, 0, sizeof(struct perf_event_attr));
    peLBR.type                   = PERF_TYPE_BREAKPOINT;
    peLBR.size                   = sizeof(struct perf_event_attr);
    peLBR.bp_type                = HW_BREAKPOINT_W;
    peLBR.bp_len                 = HW_BREAKPOINT_LEN_1;
    peLBR.bp_addr                = (uintptr_t)&dummyWP[0];
    peLBR.sample_period          = 1;
    peLBR.precise_ip             = 0; /* arbitraty skid */
    peLBR.sample_type            = 0;
    peLBR.exclude_user           = 0;
    peLBR.exclude_kernel         = 1;
    peLBR.exclude_hv             = 1;
    peLBR.disabled               = 0; /* enabled */

    int fd =  perf_event_open(&peLBR, 0, -1, -1 /*group*/, 0);
    if (fd != -1) {
        wpConfig.isLBREnabled = true;
    } else {
        wpConfig.isLBREnabled = false;
    }
    CHECK(close(fd));

    wpConfig.isWPModifyEnabled = true;
#else
    wpConfig.isLBREnabled = false;
    wpConfig.isWPModifyEnabled = false;
#endif
    wpConfig.signalDelivered = SIGTRAP;
    // wpConfig.signalDelivered = SIGIO;
    // wpConfig.signalDelivered = SIGUSR1;
    // wpConfig.signalDelivered = SIGRTMAX;
    // wpConfig.signalDelivered = SIGRTMIN + 3;
    // EMSG("SIGRTMIN=%d SIGRTMAX=%d\n", SIGRTMIN, SIGRTMAX);
    
    // Setup the signal handler
    sigset_t block_mask;
    sigfillset(&block_mask);
    struct sigaction sa1;
    memset(&sa1, 0, sizeof(struct sigaction));
    sa1.sa_sigaction = OnWatchPoint;
    sa1.sa_mask = block_mask;
    sa1.sa_flags = SA_SIGINFO | SA_RESTART | SA_NODEFER | SA_ONSTACK;

    if(sigaction(wpConfig.signalDelivered, &sa1, NULL) == -1) {
        EMSG("Failed to set WHICH_SIG handler: %s\n", strerror(errno));
        exit(0);
    }

    wpConfig.pgsz = sysconf(_SC_PAGESIZE);

    // identify max WP supported by the architecture
    volatile int wpHandles[MAX_WP_SLOTS];
    int i = 0;
    for(; i < MAX_WP_SLOTS; i++){
        struct perf_event_attr pe;
        memset(&pe, 0, sizeof(struct perf_event_attr));
        pe.type                   = PERF_TYPE_BREAKPOINT;
        pe.size                   = sizeof(struct perf_event_attr);
        pe.bp_type                = HW_BREAKPOINT_W;
        pe.bp_len                 = HW_BREAKPOINT_LEN_1;
        pe.bp_addr                = (uintptr_t)&dummyWP[i];
        pe.sample_period          = 1;
        pe.precise_ip             = 0; /* arbitraty skid */
        pe.sample_type            = 0;
        pe.exclude_user           = 0;
        pe.exclude_kernel         = 1;
        pe.exclude_hv             = 1;
        pe.disabled               = 0; /* enabled */

        wpHandles[i] =  perf_event_open(&pe, 0, -1, -1 /*group*/, 0);
        if (wpHandles[i] == -1) {
            break;
        }
    }
    if(i == 0) {
        EMSG("Cannot create a single watch point\n");
        exit(0);
    }
    for (int j = 0 ; j < i; j ++) {
        CHECK(close(wpHandles[j]));
    }
    wpConfig.maxWP = i;
    wpConfig.replacementPolicy = WP_REPLACEMENT_AUTO;
    wpConfig.userPerfPause = NULL;
    wpConfig.userPerfResume = NULL;

    return true;
}

void WP_Shutdown(){

}


// Per thread initialization
bool WP_ThreadInit( WP_TrapCallback_t cb_func){
    // setup the thread local data
    WP_ThreadData_t *tData = (WP_ThreadData_t *)malloc(sizeof(WP_ThreadData_t));
    if (tData == NULL){
        EMSG("Failed to malloc thread local data\n");
        return false;
    }

    if (pthread_setspecific(WP_ThreadData_key, (void *) tData) != 0){
        EMSG("Failed to pthread_setspecific: %s\n", strerror(errno));
        return false;
    }

    tData->ss.ss_sp = malloc(ALT_STACK_SZ);
    if (tData->ss.ss_sp == NULL) {
        EMSG("Failed to malloc ALT_STACK_SZ\n");
        return false;
    }
    tData->ss.ss_size = ALT_STACK_SZ;
    tData->ss.ss_flags = 0;
    if (sigaltstack(&tData->ss, NULL) == -1) {
        EMSG("Failed sigaltstack\n");
        return false;
    }
    tData->lbrDummyFD = -1;
    tData->fptr = cb_func;
    tData->fs_reg_val = (void*)-1;
    tData->gs_reg_val = (void*)-1;
    srand48_r(time(NULL), &tData->randBuffer);
    tData->numWatchpointTriggers = 0;
    tData->numWatchpointImpreciseIP = 0;
    tData->numWatchpointImpreciseAddressArbitraryLength = 0;
    tData->numWatchpointImpreciseAddress8ByteLength = 0;
    tData->numWatchpointDropped = 0;
    tData->numSampleTriggeringWatchpoints = 0;
    tData->numInsaneIP = 0;

    for (int i=0; i<wpConfig.maxWP; i++) {
        tData->watchPointArray[i].isActive = false;
        tData->watchPointArray[i].fileHandle = -1;
        tData->watchPointArray[i].startTime = 0;
        tData->watchPointArray[i].samplePostFull = SAMPLES_POST_FULL_RESET_VAL;
    }

    //if LBR is supported create a dummy PERF_TYPE_HARDWARE for Linux workaround
    /*if(wpConfig.isLBREnabled) {
        CreateDummyHardwareEvent();
    }*/
    return true;
}

void WP_ThreadTerminate(){
    WP_ThreadData_t *tData = (WP_ThreadData_t *)pthread_getspecific(WP_ThreadData_key);
    pthread_setspecific(WP_ThreadData_key,NULL);

    for (int i = 0; i < wpConfig.maxWP; i++) {
        if(tData->watchPointArray[i].fileHandle != -1) {
            DisArm(&tData->watchPointArray[i]);
        }
    }
    /*
    if(tData.lbrDummyFD != -1) {
        CloseDummyHardwareEvent(tData.lbrDummyFD);
        tData.lbrDummyFD = -1;
    }*/
    tData->fs_reg_val = (void*)-1;
    tData->gs_reg_val = (void*)-1;
    
    tData->ss.ss_flags = SS_DISABLE;
    if (sigaltstack(&tData->ss, NULL) == -1) {
        EMSG("Failed sigaltstack\n");
        // no need to abort, just leak the memory
    } else {
        free(tData->ss.ss_sp);
    }
    free(tData);
}

bool WP_Subscribe(void *va, int watchLen, WP_Access_t watchType, int accessLen,  void *watchCtxt, int metric_id1, bool isCaptureValue) {
    if(ValidateWPData(va, watchLen) == false) {
        return false;
    }
    if(IsOverlapped(va, watchLen)) {
        return false; // drop the sample if it overlaps an existing address
    }
    WP_ThreadData_t *tData = TD_GET();
    // No overlap, look for a victim slot
    int victimLocation = -1;
    // Find a slot to install WP
    WP_Victim_t r = GetVictim(&victimLocation, wpConfig.replacementPolicy);
    if(r != WP_VICTIM_NONE_AVAILABLE) {
        if (isCaptureValue) {
            CaptureValue(va, &(tData->watchPointArray[victimLocation].sampleValue[0]), watchLen);
        }
        if(ArmWatchPoint(&(tData->watchPointArray[victimLocation]), watchLen, watchType, va, accessLen, watchCtxt, metric_id1) == false) {
            EMSG("ArmWatchPoint failed for address %p", va);
            return false;
        }
        return true;
    }
    return false;
}


void WP_SetPerfPauseAndResumeFunctions(WP_PerfCallback_t pause_fn,  WP_PerfCallback_t resume_fn){
    wpConfig.userPerfPause = pause_fn;
    wpConfig.userPerfResume = resume_fn;
}

void WP_GetActiveAddresses(void *addrs[/*MAX_WP_SLOT*/], int *numAddr) {
    WP_ThreadData_t *tData = TD_GET();
    *numAddr = 0;
    for (int idx = 0; idx < wpConfig.maxWP; idx++) { 
        if (tData->watchPointArray[idx].isActive) {
           addrs[(*numAddr)++] = tData->watchPointArray[idx].va;  
        } 
    } 
}

void WP_GetActiveWatchPoints(WP_TriggerInfo_t wpt[], int *numActiveWP) {
    WP_ThreadData_t *tData = TD_GET();
    *numActiveWP = 0;
    for (int idx = 0; idx < wpConfig.maxWP; idx++) { 
        WP_RegisterInfo_t *wpi = &tData->watchPointArray[idx];
        if (wpi->isActive) {
            wpt[*numActiveWP].watchCtxt = wpi->watchCtxt;
            wpt[*numActiveWP].metric_id1 = wpi->metric_id1;
            wpt[(*numActiveWP)++].sampleAccessLen = wpi->sampleAccessLen;
        } 
    } 
}

bool WP_IsAltStackAddress(void *addr) {
    WP_ThreadData_t *tData = TD_GET();
    if((addr >= tData->ss.ss_sp) && (addr < tData->ss.ss_sp + tData->ss.ss_size)) return true;
    return false;
}

bool WP_IsFSorGS(void * addr) {
    WP_ThreadData_t *tData = TD_GET();
    if (tData->fs_reg_val == (void *) -1) {
        syscall(SYS_arch_prctl, ARCH_GET_FS, &(tData->fs_reg_val));
        syscall(SYS_arch_prctl, ARCH_GET_GS, &(tData->gs_reg_val));
    }
    // 4096 smallest one page size
    if ((tData->fs_reg_val <= addr) && (addr < tData->fs_reg_val + 4096)) return true;
    if ((tData->gs_reg_val <= addr) && (addr < tData->gs_reg_val + 4096)) return true;
    return false;
}

void WP_DisableAllWatchPoints() {
    WP_ThreadData_t *tData = TD_GET();
    if (!tData) return;
    
    for(int i = 0; i < wpConfig.maxWP; i++) {
        if(tData->watchPointArray[i].isActive){
            DisableWatchpointWrapper(&(tData->watchPointArray[i]));
        }
        // Reset per WP probability
        tData->watchPointArray[i].samplePostFull = SAMPLES_POST_FULL_RESET_VAL;
    }
}

/*
bool WP_IsInsideSigHandler() {
    WP_ThreadData_t *tData = TD_GET();
    return tData->insideSigHandler; 
}
*/
