#include "constants.h"

#define SW_QUEUE_OCCUPIED 1   // b'01
#define SW_QUEUE_NULL 0

#define SYNC_FINISHED 1
#define SYNC_NOT_FINISHED 0

// queue
typedef struct {
    void **head;
    void **prealloc;
    void **tail;
    void **prealloc_tail;
    int finished[1];
    void *data[QUEUE_SIZE];
} queue_t;

// global queues to be shared
queue_t*** ucvf_queues; 
queue_t*** queues;
// for try-commit to commit
queue_t** commit_queues;
// from commit to the workers. 
queue_t** reverse_commit_queues;

queue_t* __sw_queue_create(void)
{
  queue_t* queue = (queue_t*)mmap(0, sizeof(queue_t), 
      PROT_WRITE | PROT_READ, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

  __sw_queue_reset(queue);

  return queue;
}

void __sw_queue_produce(queue_t* queue, void* value)
{
  while (*(queue->head) != (void*) SW_QUEUE_NULL);

  *(queue->head+1) = (void*)value;

  *queue->head = (void *) SW_QUEUE_OCCUPIED;

  queue->head+=2;
  if ( queue->head >= &(queue->data[QUEUE_SIZE]) )
    queue->head = &queue->data[0];    
}

void* __sw_queue_consume(queue_t* queue) 
{
  void *value;
  while ((*queue->tail) == (void*) SW_QUEUE_NULL);

  value = *( (queue->tail)+1 );
  *( (queue->tail)+1 ) = (void*) SW_QUEUE_NULL;
  *(queue->tail) = (void*) *( (queue->tail)+1 );
  queue->tail+=2;
  if ( queue->tail >= &(queue->data[QUEUE_SIZE]) )
    queue->tail = &(queue->data[0]);

  return value;
}

void __sw_queue_clear(queue_t* queue)
{
  while((*queue->tail) != (void*) SW_QUEUE_NULL)
  {
    (*queue->tail) = (void*) SW_QUEUE_NULL;
    queue->tail+=2;
    if ( queue->tail >= &(queue->data[QUEUE_SIZE]) )
      queue->tail = &queue->data[0];
  }
}

void __sw_queue_reset(queue_t* queue)
{
  int i;
  queue->head = &(queue->data[0]);
  queue->tail = &(queue->data[0]);
  *(queue->finished) = SYNC_NOT_FINISHED;
  for(i=0 ; i<QUEUE_SIZE ; i++) 
    queue->data[i] = (void*) SW_QUEUE_NULL;   
}

void __sw_queue_free(queue_t* queue)
{
  munmap(queue, sizeof(queue_t));
}

uint8_t __sw_queue_empty(queue_t* queue)
{
  return ( (*queue->tail) == SW_QUEUE_NULL ) ? 1 : 0;
}

void init_queues(unsigned n_workers, unsigned n_aux_workers)
{
  unsigned i, j;
  int prot = PROT_WRITE | PROT_READ;
  int flags = MAP_SHARED | MAP_ANONYMOUS;

  ucvf_queues = (queue_t***)mmap(0, sizeof(queue_t**)*n_workers, prot, flags, -1, 0);

  for (i = 0 ; i < n_workers ; i++)
  {
    ucvf_queues[i] = (queue_t**)mmap(0, sizeof(queue_t*)*n_workers, prot, flags, -1, 0);

    for (j = i+1 ; j < n_workers ; j++)
      if ( GET_MY_STAGE(i) == GET_MY_STAGE(j) )
        continue;
      else
        ucvf_queues[i][j] = __sw_queue_create();
  }

  // queues for misspec detection and committing
  // - each non-aux worker try_commit proess
  // - (n_workers) number of queues

  queues = (queue_t***)mmap(0, sizeof(queue_t**)*n_workers, prot, flags, -1, 0);

  for (i = 0 ; i < n_workers ; i++)
  {
    queues[i] = (queue_t**)mmap(0, sizeof(queue_t*)*n_aux_workers, prot, flags, -1, 0);

    for (j = 0 ; j < n_aux_workers ; j++)
      queues[i][j] = __sw_queue_create();
  }

  // queue for the try-commit process to the commit process 

  commit_queues = (queue_t**)mmap(0, sizeof(queue_t*)*n_aux_workers, prot, flags, -1, 0);

  for (i = 0 ; i < n_aux_workers ; i++)
    commit_queues[i] = __sw_queue_create();
  
  // queue for the commit process to the worker process

  reverse_commit_queues = (queue_t**)mmap(0, sizeof(queue_t*)*n_workers, prot, flags, -1, 0);

  for (i = 0 ; i < n_workers ; i++)
    reverse_commit_queues[i] = __sw_queue_create();
}

void fini_queues(unsigned n_workers, unsigned n_aux_workers)
{
  // wrap up for ucvf_queue

  unsigned i, j;

  for (i = 0 ; i < n_workers ; i++)
  {
    for (j = 0 ; j < n_workers ; j++)
    {
      queue_t* queue = ucvf_queues[i][j];
      if (queue)
        __sw_queue_free(queue); 
    }
    munmap(ucvf_queues[i], sizeof(queue_t*)*n_workers);
  }
  munmap(ucvf_queues, sizeof(queue_t**)*n_workers);

  // wrap up for queue

  for (i = 0 ; i < n_workers ; i++)
  {
    for (j = 0 ; j < n_aux_workers ; j++)
    {
      queue_t* queue = queues[i][j];
      if (queue)
        __sw_queue_free(queue);
    }
    munmap(queues[i], sizeof(queue_t*)*n_aux_workers);
  }
  munmap(queues, sizeof(queue_t**)*n_workers);

  // wrap up for commit queue

  for (i = 0 ; i < n_aux_workers ; i++)
    __sw_queue_free(commit_queues[i]);
  munmap(commit_queues, sizeof(queue_t*)*n_aux_workers);

  // wrap up reverse commit queue
  for (i = 0 ; i < n_workers ; i++)
    __sw_queue_free(reverse_commit_queues[i]);
  munmap(reverse_commit_queues, sizeof(queue_t*)*n_workers);
}