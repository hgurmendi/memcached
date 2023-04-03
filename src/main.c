#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/sysinfo.h>

#include "epoll.h"
#include "hashtable.h"
#include "parameters.h"
#include "sockets.h"
#include "worker_state.h"
#include "worker_thread.h"

void start_server(int text_fd, int binary_fd);

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "USAGE: %s TEXT_SOCKET_FD BINARY_SOCKET_FD\n", argv[0]);
    return EXIT_FAILURE;
  }

  char *text_fd_arg = argv[1];
  char *binary_fd_arg = argv[2];
  int text_fd = atoi(text_fd_arg);
  int binary_fd = atoi(binary_fd_arg);

  start_server(text_fd, binary_fd);

  return EXIT_SUCCESS;
}

void set_memory_limit() {
  struct rlimit memory_limits;

  int rv = getrlimit(RLIMIT_DATA, &memory_limits);
  if (rv != 0) {
    perror("set_memory_limit getrlimit");
    abort();
  }

  memory_limits.rlim_cur = MEMORY_LIMIT;
  rv = setrlimit(RLIMIT_DATA, &memory_limits);
  if (rv != 0) {
    perror("set_memory_limit setrlimit");
    abort();
  }

  printf("Memory limit correctly set to %ld bytes\n", MEMORY_LIMIT);
}

void start_server(int text_fd, int binary_fd) {
  set_memory_limit();

  // We'll use as many workers as processors in the computer.
  int num_workers = get_nprocs();

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