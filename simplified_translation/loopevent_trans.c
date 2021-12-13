
#include <stdint.h>
#include <stdlib.h>
#include "constants.h"
#include "queue.c"
#include "heap.c"
#include "invariant.c"

/// Changeable constants to make program deterministic ////
int num_ro_pages = NUM_HEAPS/4;
///////////////////////////////////////////////////////////
int num_loop_invariant_loads = MAX_LOADS;
int num_contexts = MAX_CONTEXTS;

void __specpriv_begin_invocation() {
    init_queues(NUM_WORKERS, NUM_AUX_WORKERS, QUEUE_SIZE);
    init_buffers(MAX_LOADS, MAX_CONTEXTS);
    init_packets();
    init_heaps(MAX_GLOBAL_SIZE, MAX_HEAP_SIZE, MAX_STACK_SIZE);

    // COST OF: mmap sizeof(pcb_t) [6 integer struct]
}

void __specpriv_begin_iter() {
    for (int i = 0; i < my_stage; i++) {
        // FIX SOURCE
        int source = 0;
        int done = 0;
        while( !done ) {
            int type = queue_consume(&ucvf_queues, source, wid);
            switch(type) {
                case NORMAL: {
                    queue_consume(&ucvf_queues, source, wid);
                    // COST OF: set bit
                    // COST OF: (bit shift) * 8
                }
                case SUPER: {
                    // replacement of p->is_write == CHECK_RO_PAGE
                    if(num_ro_pages > 0) {
                        num_ro_pages--;
                        // COST OF: set bit
                    }
                    else {
                        queue_consume(&ucvf_queues, source, wid);
                        update_page(PAGE_SIZE);
                    }
                    break;
                }
                case DONE: {
                    done = 1;
                    break;
                }
                case ALLOC: {
                    // get rid of size conditional, assume happends
                    for(int k = 0; k < PAGE_SIZE/sizeof(VerMallocInstance); k++) {
                        // FIX ASSIGNED HEAP
                        if(heaps[wid].is_ver) {
                            update_ver_malloc();
                        }
                        else {
                            update_ver_separation_malloc();
                        }
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }

    if(iter && !my_stage) {
        update_loop_invariants(num_loop_invariant_loads, num_contexts);
        update_linear_predicted_values(num_loop_invariant_loads, num_contexts); 
        for (int i = 0; i < num_aux_workers, i++) {
            queue_produce(queues, wid, i);
        }
    }
    if (is_my_iter) {
        set_shadow_heaps(NUM_HEAPS);
    }
}

void busy_wait() {
    while(reverse_commit_queues[wid] > 0) {
        // fix: define this
        int type = queue_consume(reverse_commit_queues, wid);
        // p->is_write == REGULAR
        if(type % 2 == 0) {
            // get rid of if condition
            update_ver_malloc();
        }
        else {
            // get rid of if condition
            update_ver_separation_malloc();
        }
    }
}

void forward_packet(Wid wid, int shadow_size, int my_stage, int is_my_iter) {
    for(unsigned i=my_stage+1; i < NUM_STAGES; i++) {
        // fake create packet
        if (is_my_iter)
            get_available_commit_process_packet();
        else
            get_available_packet(wid);
        queue_produce( &ucvf_queues, wid, i );
    }
    get_available_packet(wid);
    queue_produce( &queues, wid, wid );
}

void forward_pair(int wid, int shadow_size, int my_stage) {
    // actual is heap, shadow
    forward_packet(wid, shadow_size, my_stage);
    forward_packet(wid, shadow_size, my_stage);
    for(unsigned i = 0; i < PAGE_SIZE; i+=8) {
        forward_packet(wid, 8, my_stage);
        forward_packet(wid, 8, my_stage);
    }
}

void __specpreiv_end_iter() {
    // fix, should be if I am correct worker for iter given stage
    if(wid == my_stage) {
        num_loop_invariant_loads = update_shadow_loop_invariants(num_loop_invariant_loads, num_contexts);
        num_contexts = update_shadow_linear_predicted_values(num_loop_invariant_loads, num_contexts); 
    }
    for(size_t i=0; i < NUM_GLOBALS; i++) {
        forward_pair(wid, shadow_globals[i].size, my_stage);
    }
    for(size_t i=0; i < NUM_HEAPS; i++) {
        forward_pair(wid, shadow_heaps[i].size, my_stage);
    }
    for(size_t i=0; i < NUM_STACKS; i++) {
        forward_pair(wid, shadow_stacks[i].size, my_stage);
    }
}

void main() {
    // begin_invocation
    int jump = NUM_STAGES/(NUM_WORKERS/NUM_WORKERS_PER_HEAP);
    __specpriv_begin_invocation();
    for(int w = 0; w < NUM_WORKERS; w++) {
        int my_stage = w%NUM_WORKERS_PER_HEAP;
        int is_my_iter = 0;
        for(int iter = 0; iter < NUM_ITERS; iter++) {
            if (iter && iter % NUM_WORKERS_PER_HEAP == 0)
                my_stage += jump;
            if (w % NUM_WORKERS_PER_HEAP == iter % NUM_WORKERS_PER_HEAP)
                is_my_iter = 1;
            // begin_iter
            __specpriv_begin_iter(w, iter, my_stage, num_loop_invariant_loads, num_contexts, is_my_iter);
            // imitate busy waiting
            if(iter == 0) {
                busy_wait(w, my_stage);
            }
            // end_iter
            __specpriv_end_iter(w, iter, my_stage);
        }
    }
    //end_invocation
    __specpriv_end_invocation();
}