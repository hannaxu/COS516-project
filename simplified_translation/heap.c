#include "constants.h"
#include <stdlib.h>

typedef struct heap {
    // some metadata to mimic
    int type;
    int is_ver;
} heap_t;

typedef struct shadow {
    // some metadata to mimic
    int permissions[NUM_WORKERS];
    int size;
} shadow_t;

// placeholders, no actual allocation
heap_t heaps[NUM_HEAPS];
// heap_t globals[NUM_GLOBALS];
// heap_t stacks[NUM_STACKS];
shadow_t shadow_heaps[NUM_HEAPS];
shadow_t shadow_globals[NUM_GLOBALS];
shadow_t shadow_stacks[NUM_STACKS];

void init_heaps(int global_size, int heap_size, int stack_size) {
    for(int i = 0; i < NUM_HEAPS; i++) {
        shadow_heaps[i].size = heap_size;
    }
    for(int i = 0; i < NUM_GLOBALS; i++) {
        shadow_globals[i].size = global_size;
    }
    for(int i = 0; i < NUM_STACKS; i++) {
        shadow_stacks[i].size = stack_size;
    }
}

// cost, no actual action
void update_ver_malloc() {
    // COST OF: mmap(VER_MALLOC_CHUNKSIZE)
    __cost += 200;
    // VER_MALLOC_CHUNKSIZE defined in constants.h
}

// cost, no actual action
void update_ver_separation_malloc() {
    // COST OF: mmap(SEPARATION_HEAP_CHUNKSIZE)*2
    __cost += 400;
    // SEPARATION_HEAP_CHUNKSIZE defined in constants.h
    // *2 for heap, shadow heap
}

// cost, no actual action
void update_page(int page_size) {
    for (unsigned i = 0 ; i < page_size ; i += 16) {
        // COST OF: 
        // uint64_t m0 = (*((uint64_t*)(&shadow[i]))) << 6;
        // uint64_t m1 = (*((uint64_t*)(&shadow[i+8]))) << 6;
        // uint64_t d0 = (*((uint64_t*)(&data[i])));
        // uint64_t d1 = (*((uint64_t*)(&data[i+8])));
	__cost += 56;

        // __m128i m = _mm_set_epi32((int)(m1 >> 32), (int)m1, (int)(m0 >> 32), (int)m0);
        // __m128i d = _mm_set_epi32((int)(d1 >> 32), (int)d1, (int)(d0 >> 32), (int)d0);
        // _mm_maskmoveu_si128(d, m, (char*)&(addr[i]));
    }
}

void set_shadow_heaps(int num_heaps) {
    for(int i = 0; i < num_heaps; i++) {
        // COST OF:
        // bit shift operations
	__cost += 1;
    }
}
