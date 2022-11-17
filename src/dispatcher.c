#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "dispatcher.h"
#include "epoll.h"
#include "sockets.h"
#include "worker.h"

/* Accepts all the incoming connections on the server socket and marks them for
 * monitoring by the given epoll instance.
 * The scheduling policy is implemented here.
 */
static void
accept_incoming_connections(struct DispatcherState *dispatcher_state) {
  struct sockaddr incoming_addr;
  socklen_t incoming_addr_len = sizeof(incoming_addr);
  int client_fd;
  int status;
  char host_buf[NI_MAXHOST];
  char port_buf[NI_MAXSERV];
  struct epoll_event event;

  while (true) {
    client_fd =
        accept(dispatcher_state->server_fd, &incoming_addr, &incoming_addr_len);
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

    struct ClientEpollEventData *event_data =
        malloc(sizeof(struct ClientEpollEventData));
    if (event_data == NULL) {
      perror("malloc event_data");
      abort();
    }

    event_data->fd = client_fd;
    event_data->connection_type =
        TEXT; // For now all the clients are using the text protocol
    strncpy(event_data->host, host_buf, NI_MAXHOST);
    strncpy(event_data->port, port_buf, NI_MAXSERV);

    event.data.ptr = (void *)event_data;
    event.events = EPOLLIN | EPOLLET;

    // The scheduling policy is implemented here. We just cycle through all the
    // workers in a round robin fashion.
    int next_epoll_fd =
        dispatcher_state->worker_epoll_fds[dispatcher_state->next_worker];
    dispatcher_state->next_worker =
        (dispatcher_state->next_worker + 1) % dispatcher_state->num_workers;
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
void dispatcher_loop(struct DispatcherState *dispatcher_state) {
  struct epoll_event *events;
  // Allocate enough zeroed memory for all the simultaneous events that we'll
  // be listening to.
  events = calloc(MAX_EVENTS, sizeof(struct epoll_event));
  if (events == NULL) {
    perror("calloc events");
    abort();
  }

  while (true) {
    int num_events;

    num_events = epoll_wait(dispatcher_state->epoll_fd, events, MAX_EVENTS, -1);
    for (int i = 0; i < num_events; i++) {
      if (is_epoll_error(events[i])) {
        fprintf(stderr, "dispatcher epoll error\n");
        close(events[i].data.fd);
      } else {
        printf("Accepting client connection\n");
        // Read data from the server socket (i.e. accept incoming
        // connections).
        accept_incoming_connections(dispatcher_state);
      }
    }
  }

  free(events);
}

/* Initializes the epoll instance for the dispatcher, the one that listens for
 * incoming connections and accepts them.
 */
static void
initialize_dispatcher_epoll(struct DispatcherState *dispatcher_state) {
  int status;
  struct epoll_event event;

  // Create an epoll instance.
  dispatcher_state->epoll_fd = epoll_create1(0);
  if (dispatcher_state->epoll_fd == -1) {
    perror("epoll_create1");
    abort();
  }

  // Start by just monitoring the incoming connections file descriptor
  // `server_fd`.
  event.data.fd = dispatcher_state->server_fd;
  event.events = EPOLLIN | EPOLLET;
  status = epoll_ctl(dispatcher_state->epoll_fd, EPOLL_CTL_ADD,
                     dispatcher_state->server_fd, &event);
  if (status == -1) {
    perror("epoll_ctl");
    abort();
  }
}

/* Initializes the all the resources needed by the Dispatcher.
 */
void initialize_dispatcher(struct DispatcherState *dispatcher_state,
                           int num_workers, char *port) {
  dispatcher_state->num_workers = num_workers;
  dispatcher_state->next_worker = 0;
  dispatcher_state->worker_threads =
      calloc(dispatcher_state->num_workers, sizeof(pthread_t));
  if (dispatcher_state->worker_threads == NULL) {
    perror("callock worker_threads");
    abort();
  }
  dispatcher_state->worker_epoll_fds =
      calloc(dispatcher_state->num_workers, sizeof(int));
  if (dispatcher_state->worker_epoll_fds == NULL) {
    perror("calloc worker_epoll_fds");
    abort();
  }

  initialize_workers(num_workers, dispatcher_state->worker_threads,
                     dispatcher_state->worker_epoll_fds);

  dispatcher_state->server_fd = create_listen_socket(port);

  initialize_dispatcher_epoll(dispatcher_state);
}

/* Destroys the dispatcher state and its allocated resources.
 * NOTE: the memory allocated for the arguments of the workers should be freed
 * by the workers themselves.
 */
void destroy_dispatcher(struct DispatcherState *dispatcher_state) {
  free(dispatcher_state->worker_threads);
  free(dispatcher_state->worker_epoll_fds);
  free(dispatcher_state);
}