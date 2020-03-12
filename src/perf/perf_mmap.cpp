#include <unistd.h>
#include <stdint.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>

#include "debug.h"
#include "perf_mmap.h"

#define PERF_BUFFER_PAGES 1

static uint32_t page_size = 0;
static uint32_t page_mask = 0;


//-------------private operationis----------------------//
static void perf_skip_buffer_nbytes(perf_mmap_t *mmap_buf, size_t n){
    uint64_t data_head = mmap_buf->data_head;
    __sync_synchronize();
    if ((mmap_buf->data_tail + n) > data_head)
        n = data_head - mmap_buf->data_tail;
    mmap_buf->data_tail += n;
}

/* return val: 0, success; -1, error; 1, not enough data */
static int perf_read_buffer_nbytes(perf_mmap_t *mmap_buf, void *buf /*output*/, size_t n){ 
    if (mmap_buf == nullptr)
        return -1;

    // front of the circular data buffer
    char *data = (char *) mmap_buf + page_size;

    // compute bytes available in the circular buffer
    uint64_t data_head = mmap_buf->data_head;
    __sync_synchronize(); // required by the man page to issue a barrier for SMP-capable platforms

    size_t bytes_available = data_head - mmap_buf->data_tail;

    if (n > bytes_available)
        return 1;

    // compute offset of tail in the circular buffer
    uint64_t tail = mmap_buf->data_tail & page_mask;

    size_t bytes_at_right = (page_mask+1) - tail;

    // bytes to copy to the right of tail
    size_t right = bytes_at_right < n ? bytes_at_right : n;

    // copy bytes from tail position
    memcpy(buf, data + tail, right);

    // if necessary, wrap and continue copy from left edge of buffer
    if (n > right) {
        size_t left = n - right;
        memcpy((char *)buf + right, data, left);
    }

    // update tail after consuming n bytes
    mmap_buf->data_tail += n;

    return 0;

}

//------------------------------------------------------//




bool perf_mmap_init(){
    page_size = sysconf(_SC_PAGESIZE);
    page_mask = (PERF_BUFFER_PAGES * page_size) - 1;
    return true;
}
bool perf_mmap_shutdown(){
    // do nothing
   return true;
}

perf_mmap_t *perf_set_mmap(int fd){
    void *buf = mmap(NULL, (PERF_BUFFER_PAGES + 1)* page_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (buf == MAP_FAILED) {
        ERROR("mmap() failed: %s", strerror(errno));
        return nullptr;
    }
    memset(buf, 0, sizeof(perf_mmap_t));
    return (perf_mmap_t *)buf;
}

void perf_unmmap(perf_mmap_t *mmap_buf){
    if (mmap_buf){
        munmap(mmap_buf, (PERF_BUFFER_PAGES + 1)* page_size);
    }
}

bool perf_read_header(perf_mmap_t *mmap_buf, perf_event_header_t *ehdr){
   return (perf_read_buffer_nbytes(mmap_buf, ehdr, sizeof(perf_event_header_t)) == 0); 
}

bool perf_read_record_sample(perf_mmap_t *mmap_buf, uint64_t sample_type, perf_sample_data_t *sample_data){

#define COPY_SAMPLE_DATA(field, size) { \
        int ret = perf_read_buffer_nbytes(mmap_buf, &(sample_data->field), size); \
        assert(ret == 0); }

    if (sample_type & PERF_SAMPLE_IP)
        COPY_SAMPLE_DATA(ip, 8);

    if (sample_type & PERF_SAMPLE_TID) {
        COPY_SAMPLE_DATA(pid, 4);
        COPY_SAMPLE_DATA(tid, 4);
    }
    if (sample_type & PERF_SAMPLE_ADDR) {
        // to be used by datacentric event
        COPY_SAMPLE_DATA(addr, 8);
    }
    if (sample_type & PERF_SAMPLE_CPU) {
        COPY_SAMPLE_DATA(cpu, 4);
    }

#if 0
    uint32_t data_read = 0; //currently, we don't need this value
    
    if (sample_type & PERF_SAMPLE_TIME) {
        perf_read_buffer_64(mmap_buf, &sample_data->time);
        data_read++;
    }
    if (sample_type & PERF_SAMPLE_ID) {
        perf_read_buffer_64(mmap_buf, &sample_data->id);
        data_read++;
    }
    if (sample_type & PERF_SAMPLE_STREAM_ID) {
        perf_read_buffer_64(mmap_buf, &sample_data->stream_id);
        data_read++;
    }
    if (sample_type & PERF_SAMPLE_CPU) {
        perf_read_buffer_32(mmap_buf, &sample_data->cpu);
        perf_read_buffer_32(mmap_buf, &sample_data->res);
        data_read++;
    }
    if (sample_type & PERF_SAMPLE_PERIOD) {
        perf_read_buffer_64(mmap_buf, &sample_data->period);
        data_read++;
    }
    if (sample_type & PERF_SAMPLE_READ) {
        // to be used by datacentric event
        handle_struct_read_format(mmap_buf,
        current->event->attr.read_format);
        data_read++;
    }
    if (sample_type & PERF_SAMPLE_CALLCHAIN) {
        COPY_SAMPLE_DATA(nr, 8);
        if (sample_data->nr > PERF_MAX_STACK_DEPTH + 1){
            //NOTE: the number of stacks can reach PERF_MAX_STACK_DEPTH + 1 instead of PERF_MAX_STACK_DEPTH. I don't know why.
            fprintf(stderr, "ERROR: sample_data->nr = %lx\n", sample_data->nr);
            assert(-1); //TODO: How to handle this situation?
        }
        COPY_SAMPLE_DATA(ips[0], sample_data->nr * 8);

        // add call chain from the kernel
        //perf_sample_callchain(mmap_buf, sample_data);
        //data_read++;
    }
    if (sample_type & PERF_SAMPLE_RAW) {
        perf_read_buffer_32(mmap_buf, &sample_data->size);
        sample_data->data = alloca( sizeof(char) * sample_data->size );
        perf_read( mmap_buf, sample_data->data, sample_data->size) ;
        data_read++;
    }
    if (sample_type & PERF_SAMPLE_BRANCH_STACK) {
        data_read++;
    }
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)
    if (sample_type & PERF_SAMPLE_REGS_USER) {
        data_read++;
    }
    if (sample_type & PERF_SAMPLE_STACK_USER) {
        data_read++;
    }
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
    if (sample_type & PERF_SAMPLE_WEIGHT) {
        perf_read_buffer_64(mmap_buf, &sample_data->weight);
        data_read++;
    }
    if (sample_type & PERF_SAMPLE_DATA_SRC) {
        perf_read_buffer_64(mmap_buf, &sample_data->data_src);
        data_read++;
    }
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,12,0)
    if (sample_type & PERF_SAMPLE_IDENTIFIER) {
        perf_read_buffer_nbytes(mmap_buf, &sample_data->sample_id);
        data_read++;
    }
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,13,0)
    // only available since kernel 3.19
    if (sample_type & PERF_SAMPLE_TRANSACTION) {
        data_read++;
    }
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,19,0)
    // only available since kernel 3.19
    if (sample_type & PERF_SAMPLE_REGS_INTR) {
        data_read++;
    }
#endif
#endif
    
    return true;
}

int perf_num_of_remaining_data(perf_mmap_t *mmap_buf){
    return mmap_buf->data_head - mmap_buf->data_tail;
}

bool perf_skip_record(perf_mmap_t *mmap_buf, perf_event_header_t *ehdr){
    if (mmap_buf == nullptr) return false;
    perf_skip_buffer_nbytes(mmap_buf, ehdr->size - sizeof(perf_event_header_t));
    return true; 
} 
bool perf_skip_all(perf_mmap_t *mmap_buf){
    if (mmap_buf == nullptr) return false;
    int remains = perf_num_of_remaining_data(mmap_buf);
    if (remains > 0){
        perf_skip_buffer_nbytes(mmap_buf, remains);
    }
    return true;
}
