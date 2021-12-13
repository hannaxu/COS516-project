#include "constants.h"
#include <stdlib.h>

typedef struct packet {
    void*    ptr;
    void*    value;
    uint32_t size;
    int16_t  is_write;
    int16_t  type;
    uint64_t sign;
} packet;

typedef struct packet_chunk {
    uint8_t sign[MAX_STAGE_NUM];  // assert (MAX_STAGE_NUM <= 64)
    int8_t  data[PAGE_SIZE];
} packet_chunk;

// per-worker packet pools
static packet** packet_begins;
static packet** packet_ends;
static packet** packet_nexts;

// per-worker buffer for super packets
static packet_chunk** packet_chunk_begins;
static packet_chunk** packet_chunk_ends;
static packet_chunk** packet_chunk_nexts;

// commit-process packet pool
static packet* commit_process_packet_begin;
static packet* commit_process_packet_end;
static packet* commit_process_packet_next;

void init_packets(unsigned n_all_workers) {
    // create a packet pool for each worker (including aux workers)

    int prot = PROT_WRITE | PROT_READ;
    int flags = MAP_SHARED | MAP_ANONYMOUS;

    packet_begins = (packet**)mmap(0, sizeof(packet*)*n_all_workers, prot, flags, -1, 0);
    packet_ends = (packet**)mmap(0, sizeof(packet*)*n_all_workers, prot, flags, -1, 0);
    packet_nexts = (packet**)mmap(0, sizeof(packet*)*n_all_workers, prot, flags, -1, 0);

    // data structure to managing the packet pool

    unsigned i;
    for (i = 0 ; i < n_all_workers ; i++)
    {
        size_t size = sizeof(packet)*PACKET_POOL_SIZE;
        packet_begins[i] = (packet*)mmap(0, size, prot, flags, -1, 0);
        packet_nexts[i] = packet_begins[i];
        packet_ends[i]  = (packet*)( (size_t)packet_begins[i] + size );
    }

    commit_process_packet_begin = (packet*)mmap(0, sizeof(packet)*PACKET_POOL_SIZE, prot, flags, -1, 0);
    commit_process_packet_next = commit_process_packet_begin;
    commit_process_packet_end = (packet*)( (size_t)commit_process_packet_begin + sizeof(packet)*PACKET_POOL_SIZE );

    // allocate space for packet_chunks

    packet_chunk_begins = (packet_chunk**)mmap(0, sizeof(packet_chunk*)*n_all_workers, prot, flags, -1, 0);
    packet_chunk_ends = (packet_chunk**)mmap(0, sizeof(packet_chunk*)*n_all_workers, prot, flags, -1, 0);
    packet_chunk_nexts = (packet_chunk**)mmap(0, sizeof(packet_chunk*)*n_all_workers, prot, flags, -1, 0);

    for (i = 0 ; i < n_all_workers ; i++)
    {
        size_t size = sizeof(packet_chunk)*PACKET_POOL_SIZE;
        packet_chunk_begins[i] = (packet_chunk*)mmap(0, size, prot, flags, -1, 0);
        packet_chunk_nexts[i] = packet_chunk_begins[i];
        packet_chunk_ends[i]  = (packet_chunk*)( (size_t)packet_chunk_begins[i] + size );
    }
}

packet* create_packet(Wid wid, int type) {
    return (packet*)malloc(sizeof(packet));
}

// release packets
void fini_packets(unsigned n_all_workers) {
    unsigned i;
    for (i = 0 ; i < n_all_workers ; i++)
    {
        munmap(packet_begins[i], sizeof(packet)*PACKET_POOL_SIZE);
        munmap(packet_chunk_begins[i], sizeof(packet)*PACKET_POOL_SIZE);
    }

    munmap(packet_begins, sizeof(packet*)*n_all_workers);
    munmap(packet_ends, sizeof(packet*)*n_all_workers);
    munmap(packet_nexts, sizeof(packet*)*n_all_workers);
    munmap(packet_chunk_begins, sizeof(packet*)*n_all_workers);
    munmap(packet_chunk_ends, sizeof(packet*)*n_all_workers);
    munmap(packet_chunk_nexts, sizeof(packet*)*n_all_workers);
}

/*
 * packet management functions
 */

int is_packet_available(packet* p)
{
  return (p->ptr == NULL);
}

packet* get_available_packet(uint32_t wid)
{
  packet* bound = packet_ends[wid];
  packet* next = packet_nexts[wid];

  while ( !is_packet_available(next) )
  {
    next++; 
    if (next >= bound)
      next = packet_begins[wid];
  }

  packet_nexts[wid] = (next+1) >= bound ? packet_begins[wid] : (next+1);

  return next;
}

packet* get_available_commit_process_packet()
{
  packet* bound = commit_process_packet_end;
  packet* next = commit_process_packet_next;

  while ( !is_packet_available(next) )
  {
    next++; 
    if (next >= bound)
      next = commit_process_packet_begin;
  }

  commit_process_packet_next = 
    (next+1) >= bound ? commit_process_packet_begin : (next+1);

  return next;
}