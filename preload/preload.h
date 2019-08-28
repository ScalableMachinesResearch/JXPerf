#ifndef _PRELOAD_H
#define _PRELOAD_H

#include <stdlib.h>
#include <stdbool.h>

typedef void (*mallochook_t)(void *, size_t);

#ifdef __cplusplus
extern "C"
{
#endif

void setMallocHookFunction(mallochook_t fn);

void setAgentFlag();
void unsetAgentFlag();


#ifdef __cplusplus
}
#endif

#endif
