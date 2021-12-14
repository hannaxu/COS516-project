#include "constants.h"

// in this packets are just an integer...
// below imitates packet creation

void init_packets(int n_all_workers) {
    // create a packet pool for each worker (including aux workers)

    // data structure to managing the packet pool
    for (int i = 0 ; i < n_all_workers ; i++){
        // COST OF: mmap (2 void*, int 32,32,64) * PACKET_POOL_SIZE
	__cost += 200;
    }
    // allocate space for packet_chunks

    for (int i = 0 ; i < n_all_workers ; i++){
        // COST OF: mmap (NUM_STAGES+PAGE_SIZE)*PACKET_POOL_SIZE
	__cost += 200;
    }
}

void uninit_packets(int n_all_workers) {
    // create a packet pool for each worker (including aux workers)

    // data structure to managing the packet pool
    for (int i = 0 ; i < n_all_workers ; i++){
        // COST OF: munmmap (2 void*, int 32,32,64) * PACKET_POOL_SIZE
	__cost += 500;
    }
    // allocate space for packet_chunks

    for (int i = 0 ; i < n_all_workers ; i++){
        // COST OF: munmmap (NUM_STAGES+PAGE_SIZE)*PACKET_POOL_SIZE
	__cost += 500;
    }
}

int get_available_commit_process_packet(int num_aux_workers) {
    // COST OF: mmap 2 void pointers+int32,16,16,64
    __cost += 1200;
    int idx = 0;
    while(idx < num_aux_workers && commit_queues[idx] >= QUEUE_SIZE/(PAGE_SIZE/4)) {
        idx++;
        // COST OF: pointer addition
	__cost += 3;
    }
    // reset
    if(idx == num_aux_workers) {
        idx = 0;
        commit_queues[idx]-=2;
    }
    return idx;
}

int get_available_ucvf_packet(int id, int size) {
    // COST OF: mmap 2 void pointers+int32,16,16,64
    __cost += 1200;
    int idx = 0;
    while(idx < size && ucvf_queues[id][idx] >= QUEUE_SIZE/(PAGE_SIZE/4)) {
        idx++;
        // COST OF: pointer addition
	__cost += 3;
    }
    if(idx == size) {
        idx = 0;
        ucvf_queues[id][idx] -=2;
    }
    return idx;
}

int get_available_packet(int id, int size) {
    // COST OF: mmap 2 void pointers+int32,16,16,64 (packet)
    __cost += 1200;
    int idx = 0;
    while(idx < size && queues[idx] >= QUEUE_SIZE/(PAGE_SIZE/4)) {
        idx++;
        // COST OF: pointer addition
	__cost += 3;
    }
    if(idx == size) {
        idx = 0;
        queues[id][idx] -=2;
    }
    return idx;
}
