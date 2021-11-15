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

namespace specpriv_smtx
{

// read-only page buffer

// this macro assumes that the size of the pointer is 8 bytes
#define RO_ENTRY_BUFFER_SIZE (PAGE_SIZE / 8)

struct ReadOnlyPageBuffer
{
  void*    data[RO_ENTRY_BUFFER_SIZE];
  unsigned index;
};

ReadOnlyPageBuffer ro_page_buffer;

static cpu_set_t old_affinity;

static bool run_iteration(Wid wid, Iteration iter)
{
  unsigned stage = GET_MY_STAGE(wid);
  Wid      wid_offset = (Wid)iter % GET_REPLICATION_FACTOR(stage);
  Wid      target_wid = GET_FIRST_WID_OF_STAGE(stage) + wid_offset;

  return target_wid == wid;
}

static void check_misspec(void)
{
  // O(pcb_size)
  PCB*      pcb = get_pcb();
  // O(prefix+id)
  Wid       wid = PREFIX(my_worker_id)();
  // constant
  Iteration iter = __specpriv_current_iter();
}

uintptr_t *get_ebp(void)
{
  uintptr_t *r;
  __asm__ volatile ("mov %%rbp, %[r]" : /* output */ [r] "=r" (r));
  return r;
}

unsigned PREFIX(begin_invocation)()
{
  stack_bound = (char*)get_ebp();


  reset_current_iter();

  //
  // set affinity for the main process
  //

  sched_getaffinity(0, sizeof(cpu_set_t), &old_affinity);

  cpu_set_t affinity;
  CPU_ZERO( &affinity );
  CPU_SET( CORE(0), &affinity );
  sched_setaffinity(0, sizeof(cpu_set_t), &affinity );

  //
  // initialize pcb
  //

  // O(sizeof pcb)
  PCB *pcb = get_pcb();
  pcb->exit_taken = 0;
  pcb->misspeculation_happened = 0;
  pcb->misspeculated_worker = 0;
  pcb->misspeculated_iteration = -1;
  pcb->misspeculation_reason = 0;
  pcb->last_committed_iteration = -1;

  //
  // returns number of threads to be used
  // TODO: it is possible to have a discrepancy between this number and the
  // number used in __specpriv_spawn_workers_callback. might be good to make
  // them consistent for all cases
  //

  return (unsigned)PREFIX(num_available_workers)();
}

Exit PREFIX(end_invocation)()
{
  // O(sizeof pcb)
  Exit exit = get_pcb()->exit_taken;
  // O(sizeof pcb)
  destroy_pcb();

  //
  // reset affiinity
  //

  sched_setaffinity(0, sizeof(cpu_set_t), &old_affinity);

  return exit;
}

/*
 * Description:
 *
 *  What worker proceesses do at the beginning of every iteration
 */

void PREFIX(begin_iter)()
{
  Wid       wid = PREFIX(my_worker_id)();
  Iteration iter = PREFIX(current_iter)();


  //
  // First iteration is special, because it updates loop-invariants values for following iterations.
  // Thus, wait here until the first iteration commits and updates all required iteration
  //

  if (iter)
  {
    // busy waiting

    while (!(*good_to_go)) {
      // At the end of the first iteration, commit prcoess sends some ALLOC packets back to the worker
      // processes to inform all the memory allocation happened during the execution of the first
      // iteration.
      //
      // This is required, because some of these are accessed throughout the loop execution. SLAMP
      // dependence profiling and loop invariant profiling indicates that handling first iteration in
      // special way should be enough. (Needs better explanation...)

      process_reverse_commit_queue(wid);
    }
  }

  //
  // update uncomiited values come from previous stages.
  // This should be happened before misspeculation checking, following stages are executed only when
  // there is no speculation in previous stages.
  //

  process_incoming_packets( wid, iter );

  //
  // check if misspeculation happened from other processes
  //

  check_misspec();

  //
  // From the second iteration, update loop invariant values and linear predictable values for the
  // worker process at the first stage. Extected values are logged to the buffer when the commit
  // process commits the first iteration. (This is what good_to_go variable for)
  //

  unsigned stage = GET_MY_STAGE( wid ) ;

  if (iter && !stage)
  {
    // !!! THIS WILL NEED LOOP ANALYSIS !!!
    update_loop_invariants();
    // !!! THIS WILL NEED LOOP ANALYSIS !!!
    update_linear_predicted_values();


    // sends BOI packet to the try-commit stage here, which makes them to check that the predicted
    // value in the buffer is matched to the actual value

    // O(num_aux_workers) [single for loop]
    to_try_commit( wid, (int8_t*)0xDEADBEEF, 0, 0, WRITE, BOI );

  }

  //
  // Set protection bit of all pre-allocated pages to PROT_NONE so signal handler can know when the
  // pages are touched
  //

  if (run_iteration(wid, iter))
  {
    reset_protection(wid);
  }

}

/*
 * Utilities for end_iter function
 */

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

/*
 * Description:
 *
 *   What worker processes does at the end of every iteration
 */

void PREFIX(end_iter)(void)
{
  Wid       wid = PREFIX(my_worker_id)();
  Iteration iter = PREFIX(current_iter)();
  unsigned  stage = GET_MY_STAGE( wid ) ;

  if ( run_iteration(wid, iter) )
  {
    //
    // First, update metadata for values that predicted by loop-invariant/linear prediction. Clear the
    // 'Read Before Write' bit for these values, so they don't trigger misspeculation from the
    // try-commit stage. There are separate mechanism for verify those values
    //

    update_shadow_for_loop_invariants();
    update_shadow_for_linear_predicted_values();

    // If there are ver_mallocs that not broadcasted yet, handle them first

    VerMallocBuffer* buf = &(ver_malloc_buffer[wid]);
    broadcast_malloc_chunk(wid, (int8_t*)(buf->elem), buf->index * sizeof(VerMallocInstance));
    memset(buf->elem, 0, sizeof(VerMallocInstance) * PAGE_SIZE);
    buf->index = 0;

    //
    // Forward pages for global varialabes that have ever touched during the execution
    // (thus have metadata set)
    //

    for (size_t i=0,e=shadow_globals->size() ; i<e ; i++)
    {

      // sot: optimize globals. Do similar opts to heaps pages. If less than 64 bytes then do not send the whole page.

      uint8_t* shadow = (*shadow_globals)[i];

      // sot: not sure if touched can be used here
      // maybe the mapping of globals is different from heaps
      // TODO: need to figure out if I can do 0x80 and &0x7f
      bool     touched = (*shadow) & 0x80;

      if (touched)
      {
        // sot: TODO: maybe this is not needed. Used for heaps, not for globals
        *shadow = *shadow & 0x7f;

        if (is_zero_page(shadow)) continue;

        unsigned nonzero = clear_read(shadow);

        forward_pair(nonzero, wid, iter, (void*)GET_ORIGINAL_OF(shadow), (void*)shadow, CHECK_REQUIRED);
      }

    }

    //
    // Forward pages for stack that have ever touched during the execution
    // (thus have metadata set)
    // TODO: still unclear why we should send stack pages
    //

    for (size_t i=0,e=shadow_stacks->size() ; i<e ; i++)
    {

      // sot: optimize stacks. Do similar opts to heaps pages. If less than 64 bytes then do not send the whole page.

      uint8_t* shadow = (*shadow_stacks)[i];

      // sot: not sure if touched can be used here
      // maybe the mapping of stacks is different from heaps
      // TODO: need to figure out if I can do 0x80 and &0x7f
      bool     touched = (*shadow) & 0x80;

      if (touched)
      {
        // sot: TODO: maybe this is not needed. Used for heaps, not for stacks
        *shadow = *shadow & 0x7f;

        if (is_zero_page(shadow)) continue;

        unsigned nonzero = clear_read(shadow);

        forward_pair(nonzero, wid, iter, (void*)GET_ORIGINAL_OF(shadow), (void*)shadow, CHECK_REQUIRED);
      }

    }

    //
    // Forward pages for dynamically allocated memories that have ever thouched during the execution.
    // (thus have it's shadow bit set)
    // Those pages might be allocated during the execution of the parallel region.
    //

    for (std::set<uint8_t*>::iterator i=shadow_heaps->begin(), e=shadow_heaps->end() ; i != e ; i++)
    {

      uint8_t* shadow = *i;
      bool     touched = (*shadow) & 0x80;

      if (touched)
      {
        *shadow = *shadow & 0x7f;

        if (is_zero_page(shadow)) continue;

        unsigned nonzero = clear_read(shadow);

        forward_pair(nonzero, wid, iter, (void*)GET_ORIGINAL_OF(shadow), (void*)shadow, CHECK_REQUIRED);
      }
    }
    forward_ro_page(wid, iter);

    //
    // Forward 'unclassified' heaps
    //

    //
    // Check NRBW(No-Read-Before-Write) heaps. If the violate the property, set misspec. Otherwise,
    // forward with CHECK_FREE flag.
    //

  }

  //
  // If I'm the worker process who is responsible for current iteration,
  // send EOI packet to following stages. 'wid_offset' computation is to find the worker process
  // responsible for the iteration 'iter'. Compare it with my wid to check if I'm the one.
  //

  Wid wid_offset = GET_WID_OFFSET_IN_STAGE( (Wid)iter, stage );

  if ( ( wid - GET_FIRST_WID_OF_STAGE( stage ) ) == wid_offset )
  {
    broadcast_event( wid, (int8_t*)0xDEADBEEF, 0, NULL, WRITE, EOI );
  }

  // increase iteration count

  advance_iter();
}

}
