#include <stdint.h>
#include <stdlib.h>
#include "constants.h"
#include "queue.c"
#include "packet.c"
#include "heap.c"

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
pcb_t* pcb;
char* stack_bound;
  // main process' iteration
  // how to imitate worker process' iterations?
Iteration current_iteration;
cpu_set_t old_affinity;

// begin_iter stuff
  // [from_worker][to_worker] = queue of uncommitted pages
  // correspond to data heaps, protected vs none, per worker
// initialized by main, changed by workers
int num_loop_invariant_loads = MAX_LOADS;
int num_contexts = MAX_CONTEXTS;
  // [num_loop_invar_max][num_context_max]
int64_t loop_invar_buff[MAX_LOADS][MAX_CONTEXTS];
int64_t linear_predict_buff[MAX_LOADS][MAX_CONTEXTS];


unsigned __specpriv_begin_invocation() {
    // stack_bound = (char*)get_ebp();
    unsigned long *r;
    __asm__ volatile ("mov %%rbp, %[r]" : /* output */ [r] "=r" (r));
    stack_bound = (char *)r;

    // reset_current_iter();
    current_iteration = 0;

    // **parallelization setting cores stuff is this relevant??**
    // sched_getaffinity(0, sizeof(cpu_set_t), &old_affinity);
    // cpu_set_t affinity;
    // do __builtin_memset (&affinity, '\0', sizeof (cpu_set_t)); while (0);
    // (__extension__ ({ size_t __cpu = (( 0 )); __cpu / 8 < (sizeof (cpu_set_t)) ? (((__cpu_mask *) ((&affinity)->__bits))[((__cpu) / (8 * sizeof (__cpu_mask)))] |= ((__cpu_mask) 1 << ((__cpu) % (8 * sizeof (__cpu_mask))))) : 0; }));
    // sched_setaffinity(0, sizeof(cpu_set_t), &affinity );

    // initialize pcb
    pcb = (pcb_t*)mmap(0, sizeof(pcb_t));
    pcb->exit_taken = 0;
    pcb->misspeculation_happened = 0;
    pcb->misspeculated_worker = 0;
    pcb->misspeculated_iteration = -1;
    pcb->misspeculation_reason = 0;
    pcb->last_committed_iteration = -1;

    return (unsigned) NUM_WORKERS;
}
// remove busy waiting
void __specpriv_begin_iter(Wid wid, Iteration iter, unsigned my_stage, int num_loop_invariant_loads, int num_contexts) {
    // Wid       wid = __specpriv_my_worker_id();
    // Iteration iter = __specpriv_current_iter();

    //  busy waiting is called separately

    // process_incoming_packets( wid, iter );
    unsigned i;
    for (i = 0 ; i < my_stage ; i++)
    {
        Wid source_wid = GET_FIRST_WID_OF_STAGE(i);
        int done = 0;
        while( !done ) {
            packet* p = (packet*)__sw_queue_consume( ucvf_queues[source_wid][wid] );
            switch(p->type) {
                case NORMAL: {
                    // packet comes as a pair
                    packet* shadow_p = (packet*)__sw_queue_consume( ucvf_queues[source_wid][wid] );
                    // set shadow bit of p to true, irrelevant cost
                    // update_normal_packet(p, shadow_p);
                    break;
                }
                case SUPER: {
                    if (p->is_write == CHECK_RO_PAGE) {
                        // basically do nothing, irrelevant cost
                        // packet_chunk* mem_chunk = (packet_chunk*)(p->value);
                        // mark_packet_chunk_read(my_stage, mem_chunk);
                    }
                    else {
                        // packet comes as a pair
                        packet* shadow_p = (packet*)__sw_queue_consume( ucvf_queues[source_wid][wid] );
                        // update_super_packet(my_stage, p, shadow_p);
                        update_page(PAGE_SIZE);
                    }
                    break;
                }
                case DONE:
                    done = 1;
                    break;
                case ALLOC: {
                    if (p->size == 0) {
                        unsigned valids = 0;
                        packet_chunk* chunk = (packet_chunk*)(p->value);
                        for (unsigned k = 0 ; k < PAGE_SIZE / sizeof(VerMallocInstance) ; k++)
                        {
                            if (heaps[GET_ASSIGNED_HEAP(wid, my_stage)].is_ver)
                            {
                                update_ver_malloc();
                            }
                            else
                            {
                                update_ver_separation_malloc();
                            }
                        }
                    }
                    break;
                }
                default:
                    break;
            }
            // mark packet consumed
            p->ptr = NULL;
        }
    }

    // check_misspec()
    // assume no misspec
    unsigned stage = GET_MY_STAGE( wid ) ;
    if (iter && !stage) {
        for (unsigned i = 0 ; i < num_loop_invariant_loads ; i++) {
            for (unsigned j = 0 ; j < num_contexts; j++) {   
                // update_loop_invariants();
                // update_linear_predicted_values();
                // buffer access, bit shift cost
                // buffer access, bit shift cost
            }
        }
        // to_try_commit
        // send "packet" to queues[wid][i] for num_aux_workers (non first)
        for (unsigned i = 0 ; i < num_aux_workers ; i++) {
            packet* p = create_packet(wid, -1);
            __sw_queue_produce( queues[wid][i], (void*)p );
        }
    }
    if (wid == GET_FIRST_WID_OF_STAGE(stage)) {
        set_shadow_heaps(NUM_HEAPS);
    }
}
// cost of ONE busy wait operation (one iteration)
void busy_wait(Wid wid) {
    //  process_reverse_commit_queue(wid);
    queue_t* queue = reverse_commit_queues[wid];
    while ( !__sw_queue_empty( queue ) ) {
        packet* p = (packet*)__sw_queue_consume( queue );
    if (p->is_write == REGULAR)
    {
      // For REGULAR type ALLOC packet, p->value is the source id.

      Wid source_wid = (Wid)((uint64_t)p->value);

      unsigned src_wid_stage = GET_MY_STAGE(source_wid);
      unsigned my_stage = GET_MY_STAGE(wid);

      if ( (src_wid_stage >= my_stage) && (source_wid != wid) )
      {
        update_ver_malloc( source_wid, p->size, p->ptr );
      }
    }
    else
    {
      assert( p->is_write == SEPARATION );

      // For SEPARATION type ALLOC packet, p->value is a pack of heapid and source wid

      Wid source_wid = (Wid)( (uint64_t)(p->value) >> 32 );
      unsigned heapid = (unsigned)((uint64_t)(p->value));

      unsigned src_wid_stage = GET_MY_STAGE(source_wid);
      unsigned my_stage = GET_MY_STAGE(wid);

      if ( (src_wid_stage >= my_stage) && (source_wid != wid) )
      {
        update_ver_separation_malloc( p->size, source_wid, heapid, p->ptr );
      }
    }
}

void forward_packet(Wid wid, int shadow_size, int my_stage) {
    for(unsigned i=my_stage+1; i < NUM_STAGES; i++) {
        if (wid == GET_FIRST_WID_OF_STAGE(my_stage))
            get_available_commit_process_packet();
        else
            get_available_packet();
        packet* p = create_packet(wid, addr, (void*)chunk, sizeof(packet_chunk), check, SUPER);
        __sw_queue_produce( ucvf_queues[wid][i], (void*)p );
    }
    packet* p = create_packet(wid, addr, (void*)chunk, sizeof(packet_chunk), check, SUPER);
    __sw_queue_produce( queues[wid][target_try_commit_id], (void*)p );
}

void forward_pair(Wid wid, int shadow_size, int size, int my_stage) {
    // actual is heap, shadow
    forward_packet(wid, shadow_size, my_stage);
    forward_packet(wid, shadow_size, my_stage);
    for(unsigned i = 0; i < PAGE_SIZE; i+=8) {
        forward_packet(wid, 8, my_stage);
        forward_packet(wid, 8, my_stage);
    }
}

void __specpriv_end_iter(Wid wid, Iteration iter, int my_stage) {

    if (wid == my_stage) {
        update_shadow_for_loop_invariants();
        update_shadow_for_linear_predicted_values();  

            // If there are ver_mallocs that not broadcasted yet, handle them first
        broadcast_malloc_chunk(wid, (int8_t*)(buf->elem), buf->index * sizeof(VerMallocInstance));
        memset(buf->elem, 0, sizeof(VerMallocInstance) * PAGE_SIZE);
        buf->index = 0;

        for(size_t i=0; i < NUM_GLOBALS; i++) {
            forward_pair(shadow_globals[i].size);
        }
        for(size_t i=0; i < NUM_HEAPS; i++) {
            forward_pair(shadow_heaps[i].size);
        }
        for(size_t i=0; i < NUM_STACKS; i++) {
            forward_pair(shadow_stacks[i].size);
        }
    }
}

Exit __specpriv_end_invocation() {
    Exit exit = pcb->exit_taken;
    munmap(pcb, sizeof(pcb_t));
    pcb = 0;
    return exit;
}

// imitate main process and spawned worker processes
void main(void) {
    // begin_invocation
    __specpriv_begin_invocation();
    for(int w = 0; w < NUM_WORKERS; w++) {
        for(int iter = 0; iter < NUM_ITERS; iter++) {
            // begin_iter
            __specpriv_begin_iter(w, iter, my_stage, num_loop_invariant_loads, num_contexts);
            // imitate busy waiting
            if(iter == 0) {
                busy_wait(w);
            }
            // end_iter
            __specpriv_end_iter(w, iter);
        }
    }
    //end_invocation
    Exit exit = __specpriv_end_invocation();
}