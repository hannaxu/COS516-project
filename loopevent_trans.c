
#include <stdint.h>
#include <stdlib.h>
// constants
#define NUM_WORKERS 12
#define BITS_SIZE 1024 / (8*sizeof(unsigned long))

// types
typedef uint32_t Exit;
typedef uint32_t Wid;
typedef uint32_t Iteration;

// based off of s_parallel_control_block (pcb.h)
typedef struct pcb {
    Exit exit_taken;

    // misspeculation tracking
    // Bool
    uint8_t misspeculation_happened;
    // Wid
    Wid misspeculated_worker;
    Iteration misspeculated_iteration;
    const char * misspeculation_reason;
    Iteration last_committed_iteration;
} pcb_t;

// struct to define CPU mask
typedef struct cpu_set {
    // type __cpu_mask
    unsigned long __bits[BITS_SIZE];
} cpu_set_t;

// globals
// only one pcb?
pcb_t* pcb = 0;
char* stack_bound;
  // main process' iteration
  // how to imitate worker process' iterations?
Iteration current_iteration;
cpu_set_t old_affinity;

unsigned __specpriv_begin_invocation() {
    // stack_bound = (char*)get_ebp();
    unsigned long *r;
    __asm__ volatile ("mov %%rbp, %[r]" : /* output */ [r] "=r" (r));
    stack_bound = (char *)r;

    // reset_current_iter();
    current_iteration = 0;

    // sched_getaffinity(0, sizeof(cpu_set_t), &old_affinity);
    // cpu_set_t affinity;
    // CPU_ZERO( &affinity );
    // CPU_SET( CORE(0), &affinity );
    // sched_setaffinity(0, sizeof(cpu_set_t), &affinity );
    sched_getaffinity(0, sizeof(cpu_set_t), &old_affinity);
    cpu_set_t affinity;
    do __builtin_memset (&affinity, '\0', sizeof (cpu_set_t)); while (0);
    (__extension__ ({ size_t __cpu = (( 0 )); __cpu / 8 < (sizeof (cpu_set_t)) ? (((__cpu_mask *) ((&affinity)->__bits))[((__cpu) / (8 * sizeof (__cpu_mask)))] |= ((__cpu_mask) 1 << ((__cpu) % (8 * sizeof (__cpu_mask))))) : 0; }));
    sched_setaffinity(0, sizeof(cpu_set_t), &affinity );

    // initialize pcb
    // calculate cost (PCB*)mmap(0, sizeof(PCB));
    pcb->exit_taken = 0;
    pcb->misspeculation_happened = 0;
    pcb->misspeculated_worker = 0;
    pcb->misspeculated_iteration = -1;
    pcb->misspeculation_reason = 0;
    pcb->last_committed_iteration = -1;

    return (unsigned) NUM_WORKERS;
}
// remove busy waiting
void __specpriv_begin_iter(Wid wid, Iteration iter) {
    // Wid       wid = __specpriv_my_worker_id();
    // Iteration iter = __specpriv_current_iter();

    //  busy waiting is called separately

    // process_incoming_packets( wid, iter );
    unsigned my_stage = GET_MY_STAGE(wid);
    unsigned i;
    for (i = 0 ; i < my_stage ; i++)
    {
        Wid wid_offset = (Wid)iter % GET_REPLICATION_FACTOR(i);
        Wid source_wid = GET_FIRST_WID_OF_STAGE(i) + wid_offset;
        bool done = false;

        while( !done ) {
            packet* p = (packet*)__sw_queue_consume( ucvf_queues[source_wid][wid] );
            DUMPPACKET("\t", p);
            switch(p->type) {
                case NORMAL: {
                    // packet comes as a pair
                    packet* shadow_p = (packet*)__sw_queue_consume( ucvf_queues[source_wid][wid] );
                    update_normal_packet(p, shadow_p);
                    break;
                }
                case SUPER: {
                    if (p->is_write == CHECK_RO_PAGE) {
                        // basically do nothing
                        packet_chunk* mem_chunk = (packet_chunk*)(p->value);
                        mark_packet_chunk_read(my_stage, mem_chunk);
                    }
                    else {
                        // packet comes as a pair
                        packet* shadow_p = (packet*)__sw_queue_consume( ucvf_queues[source_wid][wid] );
                        update_super_packet(my_stage, p, shadow_p);
                    }
                    break;
                }
                case EOI:
                case MISSPEC:
                case EOW:
                    done = true;
                    break;
                case ALLOC: {
                    if (p->size == 0) {
                        unsigned valids = 0;
                        packet_chunk*      chunk = (packet_chunk*)(p->value);
                        VerMallocInstance* vmi = (VerMallocInstance*)(chunk->data);
                        for (unsigned k = 0 ; k < PAGE_SIZE / sizeof(VerMallocInstance) ; k++) {
                            if (!vmi->ptr) break;

                            if (vmi->heap == -1) {
                                update_ver_malloc( source_wid, vmi->size, (void*)vmi->ptr );
                            }
                            else {
                                update_ver_separation_malloc( vmi->size, source_wid, (unsigned)(uint64_t)(vmi->heap), (void*)vmi->ptr );
                            }
                            valids++;
                            vmi++;
                        }
                        mark_packet_chunk_read(my_stage, chunk);
                    }
                    break;
                }
                case FREE:
                    update_ver_free( source_wid, p->ptr );
                    break;
                case BOI:
                    // nothing to do
                    break;
                default:
                    assert( false && "Unsupported type of packet" );
            }
            // mark packet consumed
            p->ptr = NULL;
        }
    }

    // check_misspec()
    if(!pcb) {
        // calculate cost (PCB*)mmap(0, sizeof(PCB));
    }

    unsigned stage = GET_MY_STAGE( wid ) ;
    if (iter && !stage) {
        update_loop_invariants();
        update_linear_predicted_values();
        to_try_commit( wid, (int8_t*)0xDEADBEEF, 0, 0, WRITE, BOI );
    }
    if (run_iteration(wid, iter)) {
        reset_protection(wid);
    }
}

void busy_wait() {
    // if (iter) {
    //  while (!(*good_to_go)) {
    //      process_reverse_commit_queue(wid);
    //  }
    // }
}

void __specpriv_end_iter() {

}

Exit __specpriv_end_invocation() {

}

// imitate main process and spawned worker processes
void main(int num_workers, int num_iters) {
    // begin_invocation
    __specpriv_begin_invocation();
    for(int w = 0; w < num_workers; w++) {
        for(int iter = 0; iter < num_iters; iter++) {
            // begin_iter
            __specpriv_begin_iter();
            // imitate busy waiting
            if(iter == 0) {
                busy_wait();
            }
            // end_iter
            __specpriv_end_iter();
        }
    }
    //end_invocation
    Exit exit = __specpriv_end_invocation();
}