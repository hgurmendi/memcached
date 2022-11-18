#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "epoll.h"
#include "worker.h"

/* Closes the connection to the client.
 */
static void close_client(struct ClientEpollEventData *event_data) {
  printf("Closing client %s (%d)\n", event_data->host, event_data->fd);
  // Closing the descriptor will make epoll automatically remove it from the
  // set of file descriptors which are currently monitored.
  close(event_data->fd);
  // We also have to free the memory allocated for the event data.
  free(event_data);
}

/* Handles incoming data from a client connection. If the client closes the
 * connection, we close the file descriptor and epoll manages it accordingly.
 */
static void handle_client(struct ClientEpollEventData *event_data) {
  int status;
  ssize_t count;
  char read_buf[512];

  // We use a loop because we must read whatever data is available completely,
  // as we're running in edge-triggered mode and won't get a notification again
  // for the same data.
  while (true) {
    count = read(event_data->fd, read_buf, sizeof read_buf);
    if (count == -1) {
      // If an error different than `EAGAIN` happened, log it and close the
      // connection to the client. Otherwise, `EAGAIN` means we've read all the
      // data, so go back to the main loop.
      if (errno != EAGAIN) {
        perror("read");
        close_client(event_data);
      }
      return;
    } else if (count == 0) {
      // Reading 0 bytes means that the client closed the connection, so we
      // close the connection as well.
      close_client(event_data);
      return;
    }

    status = write(STDOUT_FILENO, read_buf, count);
    if (status == -1) {
      perror("write");
      abort();
    }

    // Echo to the client
    bool should_echo_to_client = true;
    if (should_echo_to_client) {
      status = write(event_data->fd, read_buf, count);
      if (status == -1) {
        perror("write echo");
        abort();
      }
    }
  }
}

static void *worker_func(void *worker_args) {
  struct WorkerArgs *args;

  args = (struct WorkerArgs *)worker_args;

  printf("Starting worker \"%s\" (epoll %d)\n", args->name, args->epoll_fd);

  // Allocate enough zeroed memory for all the simultaneous events that we'll
  // be listening to.
  struct epoll_event *events;
  events = calloc(MAX_EVENTS, sizeof(struct epoll_event));
  if (events == NULL) {
    perror("events calloc");
    abort();
  }

  // Continuously poll for events that should be handled by the worker.
  while (true) {
    int num_events;

    num_events = epoll_wait(args->epoll_fd, events, MAX_EVENTS, -1);
    // @TODO? error check `epoll_wait`?
    for (int i = 0; i < num_events; i++) {
      // Interpret the event data.
      struct ClientEpollEventData *event_data = get_event_data(events[i]);

      if (is_epoll_error(events[i])) {
        fprintf(stderr, "%s: epoll error\n", args->name);
        close_client(event_data);
      } else {
        // Handle the data incoming from the client.
        printf("[%s](%d): handling data from %s:%s\n", args->name,
               args->epoll_fd, event_data->host, event_data->port);

        handle_client(event_data);
      }
    }
  }

  free(events);
  free(worker_args);

  return 0;
}

void initialize_workers(int num_workers, pthread_t *worker_threads,
                        int *worker_epoll_fds) {
  for (int i = 0; i < num_workers; i++) {
    printf("Creating worker %d...\n", i);

    // Create an epoll instance for the worker.
    worker_epoll_fds[i] = epoll_create1(0);
    if (worker_epoll_fds[i] == -1) {
      perror("epoll_create1");
      abort();
    }

    // Prepare the arguments for each worker...
    // free responsability is of the caller
    struct WorkerArgs *worker_args = calloc(1, sizeof(struct WorkerArgs));
    if (worker_args == NULL) {
      perror("calloc worker_args");
      abort();
    }
    snprintf(worker_args->name, sizeof(worker_args->name), "Worker %d", i);
    worker_args->epoll_fd = worker_epoll_fds[i];

    int ret = pthread_create(&worker_threads[i], NULL, worker_func,
                             (void *)worker_args);
    if (ret != 0) {
      perror("pthread_create");
      abort();
    }
  }
}