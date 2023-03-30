#include <stdarg.h>
#include <stdio.h>

#include "worker_state.h"

// Initializes the given WorkerStats struct.
void worker_stats_initialize(struct WorkerStats *worker_stats) {
  worker_stats->put_count = 0;
  worker_stats->del_count = 0;
  worker_stats->get_count = 0;
  worker_stats->take_count = 0;
  worker_stats->stats_count = 0;
}

// Reduces the given array of WorkerStats structs into a single one, adding the
// corresponding fields. Writes the result in the given destination struct.
void worker_stats_reduce(struct WorkerStats *workers_stats,
                         int num_worker_stats,
                         struct WorkerStats *destination) {
  worker_stats_initialize(destination);
  for (int i = 0; i < num_worker_stats; i++) {
    destination->put_count += workers_stats[i].put_count;
    destination->del_count += workers_stats[i].del_count;
    destination->get_count += workers_stats[i].get_count;
    destination->take_count += workers_stats[i].take_count;
    destination->stats_count += workers_stats[i].stats_count;
  }
}

// Logs a message from a worker. No newline character needed.
void worker_log(struct WorkerArgs *args, char *fmt, ...) {
  va_list v;

  va_start(v, fmt);
  printf("[WORKER%02d] ", args->worker_id);
  vprintf(fmt, v);
  printf("\n");
  va_end(v);
}