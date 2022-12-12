#ifndef __WORKER_H__
#define __WORKER_H__

#include <stdint.h>

#include "hashtable/hashtable.h"

struct WorkerStats {
  uint64_t put_count;
  uint64_t del_count;
  uint64_t get_count;
  uint64_t take_count;
  uint64_t stats_count;
};

/* Initializes an instance of a WorkerStats struct with all its values in
 * zero.
 */
void worker_stats_initialize(struct WorkerStats *worker_stats);

/* Takes an array of WorkerStats structs and the number of structs in the array
 * and merges (i.e. sums) each member field into the destination struct.
 */
void worker_stats_merge(struct WorkerStats *workers_stats, int num_worker_stats,
                        struct WorkerStats *destination_stats);

// Argument struct for the worker thread function.
struct WorkerArgs {
  // A name for the worker.
  char name[100];
  // The epoll instance file descriptor for the worker.
  int epoll_fd;
  // Shared hash table instance.
  struct HashTable *hashtable;
  // Worker stats for the worker.
  struct WorkerStats *stats;
  // Shared array of all the workers' stats.
  struct WorkerStats *workers_stats;
  // Total number of workers (we need to know the boundary of the workers_stats
  // array).
  int num_workers;
};

/* Initializes the workers and stores some data in dispatcher memory.
 */
void initialize_workers(int num_workers, pthread_t *worker_threads,
                        int *worker_epoll_fds, struct HashTable *hashtable,
                        struct WorkerStats *workers_stats);

#endif
