#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/sysinfo.h>

#include "epoll.h"
#include "hashtable.h"
#include "parameters.h"
#include "sockets.h"
#include "worker_state.h"
#include "worker_thread.h"

void start_server(char *text_port, char *binary_port);

int main(int argc, char *argv[]) {
  char *text_port = NULL, *binary_port = NULL;

  if (argc != 3) {
    fprintf(stderr, "Usage: %s text_port binary_port\n", argv[0]);
    return EXIT_FAILURE;
  }

  text_port = argv[1];
  binary_port = argv[2];

  start_server(text_port, binary_port);

  return EXIT_SUCCESS;
}

void start_server(char *text_port, char *binary_port) {
  // We'll use as many workers as processors in the computer.
  int num_workers = get_nprocs();

  // Create listen socket for text protocol.
  int text_fd = create_listen_socket(text_port);
  // Create listen socket for binary protocol.
  int binary_fd = create_listen_socket(binary_port);
  // Create epoll instance file descriptor.
  int epoll_fd = epoll_initialize(text_fd, binary_fd);

  // Create and initialize the hash table.
  struct HashTable *hashtable = hashtable_create(HASH_TABLE_BUCKETS_SIZE);

  // Create the array of thread ids.
  pthread_t *thread_ids = malloc(sizeof(pthread_t) * num_workers);
  if (thread_ids == NULL) {
    perror("start_server malloc 1");
    abort();
  }
  thread_ids[0] = pthread_self();

  // Create the array of usage statistic structs for all workers.
  struct WorkerStats *workers_stats =
      malloc(sizeof(struct WorkerStats) * num_workers);
  if (workers_stats == NULL) {
    perror("start_server malloc 2");
    abort();
  }

  // Create the arguments for each worker and start the worker threads.
  struct WorkerArgs *worker_args =
      malloc(sizeof(struct WorkerArgs) * num_workers);
  if (worker_args == NULL) {
    perror("start_server malloc 3");
    abort();
  }
  for (int i = 0; i < num_workers; i++) {
    worker_args[i].text_fd = text_fd;
    worker_args[i].binary_fd = binary_fd;
    worker_args[i].epoll_fd = epoll_fd;
    worker_args[i].worker_id = i;
    worker_args[i].num_workers = num_workers;
    worker_args[i].thread_ids = thread_ids;
    worker_args[i].hashtable = hashtable;
    worker_args[i].workers_stats = workers_stats;
    worker_stats_initialize(&workers_stats[i]);

    if (i == 0) {
      // The first worker id belongs to the main thread.
      continue;
    }

    // Other worker ids belong to their own threads, so create them.
    int ret = pthread_create(&worker_args[i].thread_ids[i], NULL, worker,
                             (void *)&worker_args[i]);
    if (ret != 0) {
      perror("start_server pthread_create");
      abort();
    }
  }

  // Finally, run the worker in the main thread.
  worker(&worker_args[0]);
}