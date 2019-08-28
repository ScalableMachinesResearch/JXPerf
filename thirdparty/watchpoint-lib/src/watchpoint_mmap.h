#ifndef _WATCHPOINT_MMAP_H
#define _WATCHPOINT_MMAP_H

#include "watchpoint.h"
#include "watchpoint_util.h"

void *WP_MapBuffer(int fd, size_t pgsz);
void WP_UnmapBuffer(void *buf, size_t pgsz);

bool WP_CollectTriggerInfo(WP_RegisterInfo_t *wpi, WP_TriggerInfo_t *wpt, void * context, size_t pgsz);

#endif /* _WATCHPOINT_MMAP_H */
