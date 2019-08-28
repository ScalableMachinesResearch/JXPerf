#include "watchpoint_mmap.h"


void *WP_MapBuffer(int fd, size_t pgsz) {
    void * buf = mmap(0, 2 * pgsz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (buf == MAP_FAILED) {
        EMSG("Failed to mmap : %s\n", strerror(errno));
        buf = NULL;
    }
    return buf;
}

void WP_UnmapBuffer(void *buf, size_t pgsz) {
    if( buf != NULL){
        CHECK(munmap(buf, 2 * pgsz));
    }
}


static int ReadMmapBuffer(void  *mbuf, void *buf, size_t sz, size_t pgsz) {
    struct perf_event_mmap_page *hdr = (struct perf_event_mmap_page *)mbuf;
    void *data;
    unsigned long tail;
    size_t avail_sz, m, c;
    size_t pgmsk = pgsz - 1;
    /*
     * data points to beginning of buffer payload
     */
    data = ((void *)hdr) + pgsz;

    /*
     * position of tail within the buffer payload
     */
    tail = hdr->data_tail & pgmsk;

    /*
     * size of what is available
     *
     * data_head, data_tail never wrap around
     */
    avail_sz = hdr->data_head - hdr->data_tail;
    if (sz > avail_sz) {
        EMSG("\n sz > avail_sz: sz = %lu, avail_sz = %lu\n", sz, avail_sz);
        __sync_synchronize();
        return -1;
    }

    /* From perf_event_open() manpage */
    __sync_synchronize();

    /*
     * sz <= avail_sz, we can satisfy the request
     */

    /*
     * c = size till end of buffer
     *
     * buffer payload size is necessarily
     * a power of two, so we can do:
     */
    c = pgmsk + 1 -  tail;

    /*
     * min with requested size
     */
    m = c < sz ? c : sz;

    /* copy beginning */
    memcpy(buf, data + tail, m);

    /*
     * copy wrapped around leftover
     */
    if (sz > m)
        memcpy(buf + m, data, sz - m);

    hdr->data_tail += sz;

    return 0;
}


static void ConsumeAllRingBufferData(void  *mbuf, size_t pgsz) {
    struct perf_event_mmap_page *hdr = (struct perf_event_mmap_page *)mbuf;
    unsigned long tail;
    size_t avail_sz;
    size_t pgmsk = pgsz - 1;
    /*
     * data points to beginning of buffer payload
     */
    void * data = ((void *)hdr) + pgsz;

    /*
     * position of tail within the buffer payload
     */
    tail = hdr->data_tail & pgmsk;

    /*
     * size of what is available
     *
     * data_head, data_tail never wrap around
     */
    avail_sz = hdr->data_head - hdr->data_tail;
    __sync_synchronize();

    hdr->data_tail = hdr->data_head;
}


bool WP_CollectTriggerInfo(WP_RegisterInfo_t  * wpi, WP_TriggerInfo_t *wpt, void *context, size_t pgsz) {
        //struct perf_event_mmap_page * b = wpi->mmapBuffer;
    struct perf_event_header hdr;

    if (ReadMmapBuffer(wpi->mmapBuffer, &hdr, sizeof(struct perf_event_header), pgsz) < 0) {
        EMSG("Failed to ReadMmapBuffer: %s\n", strerror(errno));
        return false;
    }
    switch(hdr.type) {
        case PERF_RECORD_SAMPLE:
		{
            assert (hdr.type & PERF_SAMPLE_IP);
            void *preciseIP = (void *) 0;
            if (hdr.type & PERF_SAMPLE_IP) {
                if (ReadMmapBuffer(wpi->mmapBuffer, &preciseIP, sizeof(uint64_t), pgsz) < 0) {
                    EMSG("Failed to ReadMmapBuffer: %s\n", strerror(errno));
                    return false;
                }
                if(hdr.misc & PERF_RECORD_MISC_EXACT_IP) { //precise IP
                    wpt->pc = preciseIP;
                    wpt->pcPrecise = 1;
                } else {//imprecise IP
                    wpt->pc = preciseIP;
                    wpt->pcPrecise = 0;
                }
#if 0
		//read branch
		uint64_t bnr;
		struct perf_branch_entry lbr[32];

		if (ReadMmapBuffer(wpi->mmapBuffer, &bnr, sizeof(uint64_t), pgsz) < 0) {
			EMSG("Failed to ReadMmapBuffer: %s\n", strerror(errno));
			return false;
		}
		printf("bnr = %lu\n", bnr);
		assert(bnr <= 32);
		if (ReadMmapBuffer(wpi->mmapBuffer, lbr, sizeof(struct perf_branch_entry)*bnr, pgsz) < 0){
			EMSG("Failed to ReadMmapBuffer: %s\n", strerror(errno));
			return false;
		}
		printf("lbr[0] %lx --> %lx \n", lbr[0].from, lbr[0].to);
#endif
	    } else {
                assert(false);
            }

            // We must cleanup the mmap buffer if there is any data left
            ConsumeAllRingBufferData(wpi->mmapBuffer, pgsz);
            return true;
		}
        case PERF_RECORD_EXIT:
            EMSG("PERF_RECORD_EXIT sample type %d sz=%d\n", hdr.type, hdr.size);
            goto ErrExit;
        case PERF_RECORD_LOST:
            EMSG("PERF_RECORD_LOST sample type %d sz=%d\n", hdr.type, hdr.size);
            goto ErrExit;
        case PERF_RECORD_THROTTLE:
            EMSG("PERF_RECORD_THROTTLE sample type %d sz=%d\n", hdr.type, hdr.size);
            goto ErrExit;
        case PERF_RECORD_UNTHROTTLE:
            EMSG("PERF_RECORD_UNTHROTTLE sample type %d sz=%d\n", hdr.type, hdr.size);
            goto ErrExit;
        default:
            EMSG("unknown sample type %d sz=%d\n", hdr.type, hdr.size);
            goto ErrExit;
    }

ErrExit:
    // We must cleanup the mmap buffer if there is any data left
    ConsumeAllRingBufferData(wpi->mmapBuffer, pgsz);
    return false;
}
