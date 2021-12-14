#include <stdint.h>
#include <stdlib.h>
#include "constants.h"

// call with NUM_WORKERS, NUM_AUX_WORKERS, QUEUE_SIZE
void init_queues(int num_workers, int num_aux_workers, int queue_size) {
    // initialize ucvf_queues[NUM_WORKERS][NUM_WORKERS]
    // COST OF: mmap sizeof(queue_t**)*num_workers
    for(int i = 0; i < num_workers; i++) {
        // COST OF: mmap sizeof(queue_t*)*num_workers
        for(int j = i+1; j < num_workers; j++) {
            // COST OF: mmap sizeof(queue_t)
            ucvf_queues[i][j] = 0;
            for(int k = 0; k < queue_size; k++) {
                // COST OF: queue->data[i] = 0
            }
        }
    }

    // initialize queues[NUM_WORKERS][NUM_AUX_WORKERS];
    // COST OF: mmap sizeof(queue_t**)*num_workers
    for(int i = 0; i < num_workers; i++) {
        // COST OF: mmap sizeof(queue_t*)*num_aux_workers
        for(int j = 0; j < num_aux_workers; j++) {
            // COST OF: mmap sizeof(queue_t)
            queues[i][j] = 0;
            for(int k = 0; k < queue_size; k++) {
                // COST OF: queue->data[i] = 0
            }
        }
    }
    // commit_queues[NUM_AUX_WORKERS];
    // COST OF: mmap sizeof(queue_t*)*num_aux_workers
    for(int j = 0; j < num_aux_workers; j++) {
        // COST OF: mmap sizeof(queue_t)
        commit_queues[j] = 0;
        for(int k = 0; k < queue_size; k++) {
            // COST OF: queue->data[i] = 0
        }
    }
    // reverse_commit_queues[NUM_WORKERS];
    // COST OF: mmap sizeof(queue_t*)*num_aux_workers
    for(int j = 0; j < num_workers; j++) {
        // COST OF: mmap sizeof(queue_t)
        reverse_commit_queues[j] = 0;
        for(int k = 0; k < queue_size; k++) {
            // COST OF: queue->data[i] = 0
        }
    }
}

// call with NUM_WORKERS, NUM_AUX_WORKERS, QUEUE_SIZE
void uninit_queues(int num_workers, int num_aux_workers, int queue_size) {
    // COST OF: munmap sizeof(queue_t**)*num_workers
    for(int i = 0; i < num_workers; i++) {
        // COST OF: munmap sizeof(queue_t*)*num_workers
        for(int j = i+1; j < num_workers; j++) {
            // COST OF: munmap sizeof(queue_t)
        }
    }

    // COST OF: mmap sizeof(queue_t**)*num_workers
    for(int i = 0; i < num_workers; i++) {
        // COST OF: munmap sizeof(queue_t*)*num_aux_workers
        for(int j = 0; j < num_aux_workers; j++) {
            // COST OF: munmap sizeof(queue_t)
        }
    }
    // commit_queues[NUM_AUX_WORKERS];
    // COST OF: munmap sizeof(queue_t*)*num_aux_workers
    for(int j = 0; j < num_aux_workers; j++) {
        // COST OF: munmap sizeof(queue_t)
    }
    // reverse_commit_queues[NUM_WORKERS];
    // COST OF: munmap sizeof(queue_t*)*num_aux_workers
    for(int j = 0; j < num_workers; j++) {
        // COST OF: munmap sizeof(queue_t)
    }
}

// enum {NORMAL, SUPER, DONE, ALLOC};
int queue_consume(int *packet_queue) {
    // COST OF: set pointer value to another pointer's value *3
    if(*packet_queue == 0) {
        // we're done
        return DONE;
    }
    // can consume 2 packets, NORMAL or SUPER
    if(*packet_queue % 4 == 0) {
        *packet_queue -= 1;
        return SUPER;
    }
    if(*packet_queue % 2 == 0) {
        *packet_queue -= 1;
        return NORMAL;
    }
    *packet_queue -= 1;
    return ALLOC;
}

void queue_produce(int *packet_queue) {
    // COST OF: set pointer value to another pointer's value *3
    *packet_queue += 1;
}