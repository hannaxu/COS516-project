#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <stdint.h>
//// given constants ////
#define NUM_WORKERS 12
#define BITS_SIZE 1024 / (8*sizeof(unsigned long))
#define QUEUE_SIZE 8192*2
// maximum number of stages
#define MAX_STAGE_NUM (64)
// maximum number of workers
#define MAX_WORKERS (32-1)
#define PAGE_SIZE  (4*1024)
// size of packet pools
#define PACKET_POOL_SIZE (8192*2*4*8)

//// changeable constants ////


// packet types
enum {NORMAL, SUPER, DONE, ALLOC};

// types
typedef uint32_t Exit;
typedef uint32_t Wid;
typedef uint32_t Iteration;

// protection
#define PROT_READ	0x1		/* Page can be read.  */
#define PROT_WRITE	0x2		/* Page can be written.  */
#define MAP_SHARED	0x01		/* Share changes.  */
#define MAP_PRIVATE	0x02		/* Changes are private.  */
#define MAP_ANONYMOUS	0x20		/* Don't use a file.  */

#endif