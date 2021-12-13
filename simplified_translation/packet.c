#include "constants.h"

// in this packets are just an integer...


void get_available_commit_process_packet(int num_aux_workers) {
    int idx = 0;
    while(idx < num_aux_workers && commit_queues[idx] >= PACKET_POOL_SIZE) {
        idx++;
    }
    if(idx != num_aux_workers)
        commit_queues[idx]+=PAGE_SIZE;
}

void get_available_packet(int id) {
    int idx = 0;
    while(idx < num_aux_workers && commit_queues[idx] >= PACKET_POOL_SIZE) {
        idx++;
    }
    if(idx != num_aux_workers)
        commit_queues[idx]+=PAGE_SIZE;
}