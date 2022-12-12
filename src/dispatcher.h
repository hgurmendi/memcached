#ifndef __DISPATCHER_H__
#define __DISPATCHER_H__

#include <pthread.h>

#include "hashtable/hashtable.h"

struct DispatcherState {
  // Bound file descriptor used for the text protocol.
  int text_fd;
  // Bound file descriptor used for the binary protocol.
  int binary_fd;
  // Epoll instance file descriptor.
  int epoll_fd;
  // Total number of workers.
  int num_workers;
  // Array of file descriptors for each epoll instance from worker.
  int *worker_epoll_fds;
  // Array of worker thread identifiers.
  pthread_t *worker_threads;
  // Next worker index to handle the next connection (used for the Round Robin
  // schedule policy).
  int next_worker;
  // Shared hash table instance.
  struct HashTable *hashtable;
};

// void dispatcher_loop(struct DispatcherState *dispatcher_state);

void dispatcher_initialize(struct DispatcherState *dispatcher_state,
                           int num_workers, char *text_port, char *binary_port);

void dispatcher_destroy(struct DispatcherState *dispatcher_state);

void dispatcher_loop(struct DispatcherState *dispatcher_state);

#endif