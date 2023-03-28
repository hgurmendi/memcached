#ifndef __NEW_WORKER_H__
#define __NEW_WORKER_H__

#include <pthread.h>

#include "bounded_data_hashtable.h"

struct WorkerStats {
  uint64_t put_count;   // Number of PUT requests.
  uint64_t del_count;   // Number of DEL requests.
  uint64_t get_count;   // Number of GET requests.
  uint64_t take_count;  // Number of TAKE requests.
  uint64_t stats_count; // Number of STATS requests.
};

struct WorkerArgs {
  int text_fd;                 // File descriptor of the text protocol socket.
  int binary_fd;               // File descriptor of the binary protocol socket.
  int epoll_fd;                // File descriptor of the shared epoll instance.
  unsigned worker_id;          // Worker id.
  unsigned num_workers;        // Number of workers.
  pthread_t *thread_ids;       // Pthread ids of the workers.
  struct HashTable *hashtable; // Shared hash table instance.
  struct WorkerStats *workers_stats; // Usage statistics of the workers.
};

// Worker thread function.
void *worker(void *_args);

// Initializes the given WorkerStats struct.
void worker_stats_initialize(struct WorkerStats *worker_stats);

// Reduces the given array of WorkerStats structs into a single one, adding the
// corresponding fields. Writes the result in the given destination struct.
void worker_stats_reduce(struct WorkerStats *workers_stats,
                         int num_worker_stats, struct WorkerStats *destination);

#endif