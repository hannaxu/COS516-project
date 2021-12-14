#ifndef CONSTANTS_H
#define CONSTANTS_H

/// global cost ///
int __cost = 0;
//// changeable constants ////
#define MAX_LOADS 12
#define MAX_CONTEXTS 12
#define NUM_AUX_WORKERS 2
#define NUM_HEAPS 20
#define NUM_GLOBALS 20
#define NUM_STACKS 20
#define MAX_GLOBAL_SIZE (0x1000)
#define MAX_STACK_SIZE (0x1000)
#define MAX_HEAP_SIZE (0x1000)
#define NUM_WORKERS_PER_HEAP 2
//////////////////////////////

//// given constants ////
#define NUM_STAGES NUM_HEAPS
#define NUM_WORKERS 12-NUM_AUX_WORKERS
#define NUM_ITERS (NUM_HEAPS / (NUM_WORKERS/NUM_WORKERS_PER_HEAP))*NUM_WORKERS_PER_HEAP
#define BITS_SIZE 1024 / (8*sizeof(unsigned long))
#define QUEUE_SIZE 8192*2
// maximum number of stages
#define MAX_STAGE_NUM (64)
// maximum number of workers
#define MAX_WORKERS (32-1)
#define PAGE_SIZE  (4*1024)
// size of packet pools
#define PACKET_POOL_SIZE (8192*2*4*8)
// code for chunk checks
#define CHECK_FREE     (0)
#define CHECK_REQUIRED (1)
#define CHECK_RO_PAGE  (2)
// code for ALLOC broadcast
// write 
#define REGULAR    (0)
#define SEPARATION (1)


// protection
#define PROT_READ	0x1		/* Page can be read.  */
#define PROT_WRITE	0x2		/* Page can be written.  */
#define MAP_SHARED	0x01		/* Share changes.  */
#define MAP_PRIVATE	0x02		/* Changes are private.  */
#define MAP_ANONYMOUS	0x20		/* Don't use a file.  */
// packet types
enum {NORMAL, SUPER, DONE, ALLOC};

// heap sizes
#define VER_MALLOC_CHUNKSIZE   (0x1000) // 4K
#define SEPARATION_HEAP_CHUNKSIZE (0x1000) // 4K
typedef struct VerMallocInstance
{
  uint64_t ptr;
  uint32_t size;
  int32_t  heap;
}VerMallocInstance;

/// simplified structures ///
// queue imitation
// represent actual queue as tally (# of elements, max = QUEUE_SIZE)
int ucvf_queues[NUM_WORKERS][NUM_WORKERS];
int queues[NUM_WORKERS][NUM_AUX_WORKERS];
int commit_queues[NUM_AUX_WORKERS];
int reverse_commit_queues[NUM_WORKERS];



//////////////////////////////

#endif