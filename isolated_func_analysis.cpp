#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "emmintrin.h"
#include "smmintrin.h"

#include "api.h"
#include "internals/affinity.h"
#include "internals/debug.h"
#include "internals/pcb.h"
#include "internals/private.h"
#include "internals/profile.h"
#include "internals/strategy.h"
#include "internals/utils.h"
#include "internals/smtx/communicate.h"
#include "internals/smtx/smtx.h"
#include "internals/smtx/malloc.h"
#include "internals/smtx/prediction.h"
#include "internals/smtx/protection.h"
#include "internals/smtx/separation.h"
#include "internals/smtx/units.h"

// cost = 3*o(1)
static bool run_iteration(Wid wid, Iteration iter)
{
  // O(1) - look up corresponding stage to worker id
  unsigned stage = GET_MY_STAGE(wid);
  // O(1) - look up number of worker threads in stage
  Wid      wid_offset = (Wid)iter % GET_REPLICATION_FACTOR(stage);
  // O(1) - look up first worker id for given stage
  Wid      target_wid = GET_FIRST_WID_OF_STAGE(stage) + wid_offset;

  return target_wid == wid;
}


static void check_misspec(void)
{
  // IF the_pcb is NULL: O(pcb_size), create pcb memory
  // ELSE O(1), return the_pcb
  PCB*      pcb = get_pcb();
  // prefix func
  Wid       wid = __specpriv_my_worker_id();
  // O(1)
  Iteration iter = __specpriv_current_iter();

}

uintptr_t *get_ebp(void)
{
  uintptr_t *r;
  __asm__ volatile ("mov %%rbp, %[r]" : /* output */ [r] "=r" (r));
  return r;
}

// cost = PAGE_SIZE
static unsigned clear_read(uint8_t* shadow)
{
  unsigned nonzero = 0;
  bool     readonly = true;

  for (unsigned i = 0 ; i < PAGE_SIZE ; i += 16)
  {
    uint64_t s0 = (*((uint64_t*)(&shadow[i])));
    uint64_t s1 = (*((uint64_t*)(&shadow[i+8])));

    if (s0 == 0 && s1 == 0) continue;
    if (s0) nonzero++;
    if (s1) nonzero++;

    if ((s0 & 0xfbfbfbfbfbfbfbfbL) || (s1 & 0xfbfbfbfbfbfbfbfbL))
      readonly = false;

    // As try_commit unit checks the validity of load with bit 0 only,
    // for read-only bytes whose metadata is b'0100, make it to b'0001,
    // so the commit process verifies its validity
    //
    // For the bytes that written after read, write operation sets the bit 0,
    // thus no need to worry

    uint64_t mask0 = (s0 >> 2) & (~(s0 >> 1)) & 0x0101010101010101L;
    uint64_t mask1 = (s1 >> 2) & (~(s1 >> 1)) & 0x0101010101010101L;

    *((uint64_t*)(&shadow[i])) |= (s0 & mask0);
    *((uint64_t*)(&shadow[i+8])) |= (s1 & mask1);
  }

  if (readonly) nonzero = 0;

  return nonzero;
}

static inline void forward_ro_page(Wid wid, Iteration iter)
{
  if (ro_page_buffer.index != 0)
  {
    forward_page( wid, iter, (void*)(ro_page_buffer.data), CHECK_RO_PAGE );
    memset((void*)(ro_page_buffer.data), 0, PAGE_SIZE);
  }
}

static inline void forward_pair(unsigned nonzero, Wid wid, Iteration iter, void* mem, void* shadow, int16_t check)
{
  if ( nonzero > 8 )
  {
    forward_page( wid, iter, (void*)mem, check);
    forward_page( wid, iter, (void*)shadow, check);

    set_zero_page( (uint8_t*)shadow );
  }
  else if ( nonzero )
  {
    int8_t* m = (int8_t*)mem;
    int8_t* s = (int8_t*)shadow;

    for (unsigned i = 0 ; i < PAGE_SIZE ; i += 8)
    {
      uint64_t* s8 = (uint64_t*)&(s[i]);
      uint64_t* m8 = (uint64_t*)&(m[i]);
      uint64_t  sv = *s8;

      if (sv)
      {
        forward_packet( wid, iter, (void*)m8, (void*)*m8, 8, check);
        forward_packet( wid, iter, (void*)NULL, (void*)sv, 8, check);
      }
    }

    set_zero_page( (uint8_t*)shadow );
  }
  else
  {
    if (ro_page_buffer.index == RO_ENTRY_BUFFER_SIZE)
    {
      forward_ro_page(wid, iter);
      ro_page_buffer.index = 0;
    }

    (ro_page_buffer.data)[ro_page_buffer.index] = mem;
    ro_page_buffer.index += 1;

    set_zero_page( (uint8_t*)shadow );
  }
}