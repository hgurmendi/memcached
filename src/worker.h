#ifndef __WORKER_H__
#define __WORKER_H__

#define NUM_WORKERS 5

// Argument struct for the worker thread function.
struct WorkerArgs {
  // A name for the worker.
  char name[100];
  // The epoll instance file descriptor for the worker.
  int epoll_fd;
};

/* Initializes the workers and stores some data in dispatcher memory.
 */
void initialize_workers(int num_workers, pthread_t *worker_threads,
                        int *worker_epoll_fds);

#endif