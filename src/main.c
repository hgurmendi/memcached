#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "sockets.h"

#define MAX_EVENTS 64
#define MAX_WORKER_EVENTS MAX_EVENTS

#define NUM_WORKERS 5

enum CONNECTION_TYPES { BINARY, TEXT };

// This struct is the argument for the worker thread function.
struct worker_args {
  // an identifier for the worker.
  char name[100];
  // the epoll instance file descriptor for the worker.
  int epoll_fd;
};

// This is a struct whose memory should be requested by the dispatcher
// and it should be assigned to epoll_event.data.ptr (which is a pointer to
// void) and it allows us to store arbitrary data accessible when the event is
// triggered in an epoll wait
struct epoll_event_data {
  // file descriptor of the client
  int fd;
  // connection type of the client
  enum CONNECTION_TYPES connection_type;
  // ip of the client
  char host[NI_MAXHOST];
  // port of the client (wtf?)
  char port[NI_MAXSERV];
};

/* Closes the connection to the client.
 */
void close_client(int client_fd) {
  printf("Closed connection on descriptor %d\n", client_fd);
  // Closing the descriptor will make epoll automatically remove it from the
  // set of file descriptors which are currently monitored.
  close(client_fd);
}

/* Handles incoming data from a client connection. If the client closes the
 * connection, we close the file descriptor and epoll manages it accordingly.
 */
void handle_client(int client_fd) {
  int status;
  ssize_t count;
  char read_buf[512];

  // We use a loop because we must read whatever data is available completely,
  // as we're running in edge-triggered mode and won't get a notification again
  // for the same data.
  while (true) {
    count = read(client_fd, read_buf, sizeof read_buf);
    if (count == -1) {
      // If an error different than `EAGAIN` happened, log it and close the
      // connection to the client. Otherwise, `EAGAIN` means we've read all the
      // data, so go back to the main loop.
      if (errno != EAGAIN) {
        perror("read");
        close_client(client_fd);
      }
      return;
    } else if (count == 0) {
      // Reading 0 bytes means that the client closed the connection, so we
      // close the connection as well.
      close_client(client_fd);
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
      status = write(client_fd, read_buf, count);
      if (status == -1) {
        perror("write echo");
        abort();
      }
    }
  }
}

void *worker_func(void *worker_args) {
  struct worker_args *args;

  args = (struct worker_args *)worker_args;

  printf("Starting worker \"%s\" (epoll %d)\n", args->name, args->epoll_fd);

  // Allocate enough zeroed memory for all the simultaneous events that we'll
  // be listening to.
  struct epoll_event *events;
  events = calloc(MAX_EVENTS, sizeof(struct epoll_event));
  if (events == NULL) {
    perror("events calloc");
    abort();
  }

  while (true) {
    // handle all the events in the worker
    int num_events;

    num_events = epoll_wait(args->epoll_fd, events, MAX_EVENTS, -1);
    for (int i = 0; i < num_events; i++) {
      int is_epoll_error = (events[i].events & EPOLLERR) ||
                           (events[i].events & EPOLLHUP) ||
                           !(events[i].events & EPOLLIN);

      // Interpret the event data.
      struct epoll_event_data *event_data =
          (struct epoll_event_data *)(events[i].data.ptr);

      if (is_epoll_error) {
        fprintf(stderr, "%s: epoll error\n", args->name);

        close(event_data->fd);
      } else {
        // Read data from an arbitrary client socket (i.e. handle incoming
        // interactions).
        printf("[%s](%d): handling data from %s:%s\n", args->name,
               args->epoll_fd, event_data->host, event_data->port);

        handle_client(event_data->fd);
      }
    }
  }

  free(events);
  free(worker_args);

  return 0;
}

/* Accepts all the incoming connections on the server socket and marks them for
 * monitoring by the given epoll instance.
 */
void accept_incoming_connections(int server_fd, int worker_epoll_fds[],
                                 int *worker_index) {
  struct sockaddr incoming_addr;
  socklen_t incoming_addr_len = sizeof(incoming_addr);
  int client_fd;
  int status;
  char host_buf[NI_MAXHOST];
  char port_buf[NI_MAXSERV];
  struct epoll_event event;

  while (true) {
    client_fd = accept(server_fd, &incoming_addr, &incoming_addr_len);
    if (client_fd == -1) {
      if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
        // We already processed all incoming connections, just return.
        return;
      } else {
        // An error happened, just log it and continue.
        perror("accept");
        return;
      }
    }

    status = getnameinfo(&incoming_addr, incoming_addr_len, host_buf,
                         sizeof host_buf, port_buf, sizeof port_buf,
                         NI_NUMERICHOST | NI_NUMERICSERV);

    if (status == 0) {
      printf("Accepted connection on descriptor %d "
             "(host=%s, port=%s)\n",
             client_fd, host_buf, port_buf);
    }

    status = make_socket_non_blocking(client_fd);
    if (status == -1) {
      abort();
    }

    // We used to just store the file descriptor in the data member of the epoll
    // event:
    // event.data.fd = client_fd;
    // But no we just have our own type of event data!!
    struct epoll_event_data *event_data;

    event_data = malloc(sizeof(struct epoll_event_data));
    if (event_data == NULL) {
      perror("malloc event_data");
      abort();
    }

    event_data->fd = client_fd;
    event_data->connection_type = TEXT;
    strncpy(event_data->host, host_buf, NI_MAXHOST);
    strncpy(event_data->port, port_buf, NI_MAXSERV);
    event.data.ptr = (void *)event_data;

    event.events = EPOLLIN | EPOLLET;

    // Scheduling policy (sorta): just round robin through all the available
    // epoll instances.
    int next_epoll_fd = worker_epoll_fds[*worker_index];
    // Update the next worker index.
    *worker_index = (*worker_index + 1) % NUM_WORKERS;
    status = epoll_ctl(next_epoll_fd, EPOLL_CTL_ADD, event_data->fd, &event);
    if (status == -1) {
      perror("epoll_ctl");
      abort();
    }
  }
}

/* Main server loop. Accepts incoming connections according to a policy and
 * dispatches them to a worker's epoll.
 */
void main_server_loop(int server_fd, int worker_epoll_fds[]) {
  int epoll_fd;
  int status;
  struct epoll_event event;
  struct epoll_event *events;

  // Create an epoll instance.
  epoll_fd = epoll_create1(0);
  if (epoll_fd == -1) {
    perror("epoll_create1");
    abort();
  }

  // Start by just monitoring the incoming connections file descriptor
  // `server_fd`.
  event.data.fd = server_fd;
  event.events = EPOLLIN | EPOLLET;
  status = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event);
  if (status == -1) {
    perror("epoll_ctl");
    abort();
  }

  // Allocate enough zeroed memory for all the simultaneous events that we'll
  // be listening to.
  events = calloc(MAX_EVENTS, sizeof event);

  // We store the index of the worker that we dispatched the last connection to
  // in `worker_index`.
  int worker_index = 0;

  while (true) {
    int num_events;

    num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
    for (int i = 0; i < num_events; i++) {
      int is_epoll_error = (events[i].events & EPOLLERR) ||
                           (events[i].events & EPOLLHUP) ||
                           !(events[i].events & EPOLLIN);

      if (is_epoll_error) {
        fprintf(stderr, "epoll error\n");
        close(events[i].data.fd);
      } else {
        printf("Accepting client connection\n");
        // Read data from the server socket (i.e. accept incoming
        // connections).
        accept_incoming_connections(server_fd, worker_epoll_fds, &worker_index);
      }
    }
  }

  free(events);
}

int main(int argc, char *argv[]) {
  int status;
  int server_fd;
  int event_fd;
  struct epoll_event event;
  struct epoll_event *events;

  if (argc != 2) {
    fprintf(stderr, "Usage: %s [port]\n", argv[0]);
    return EXIT_FAILURE;
  }

  // Workers info
  pthread_t workers[NUM_WORKERS];
  int worker_epoll_fds[NUM_WORKERS];

  for (int i = 0; i < NUM_WORKERS; i++) {
    printf("Creating thread %d...\n", i);

    // Create an epoll instance for the worker.
    worker_epoll_fds[i] = epoll_create1(0);
    if (worker_epoll_fds[i] == -1) {
      perror("epoll_create1");
      abort();
    }

    // Prepare the arguments for each worker...
    // free responsability is of the caller
    struct worker_args *args = malloc(sizeof(struct worker_args));
    if (args == NULL) {
      perror("malloc thread");
      abort();
    }
    snprintf(args->name, 100, "Worker %d", i);
    args->epoll_fd = worker_epoll_fds[i];

    int ret = pthread_create(&workers[i], NULL, worker_func, (void *)args);
    if (ret != 0) {
      perror("pthread_create");
      abort();
    }
  }

  printf("All workers done creating!\n");

  printf("Creating server socket...\n");

  server_fd = create_listen_socket(argv[1]);

  printf("Going to the main loop...\n");

  main_server_loop(server_fd, worker_epoll_fds);

  close(server_fd);
  return EXIT_SUCCESS;
}