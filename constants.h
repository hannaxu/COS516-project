#include <stdint.h>
// constants
#define NUM_WORKERS 12
#define BITS_SIZE 1024 / (8*sizeof(unsigned long))
#define QUEUE_SIZE 8192*2

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
