#define _GNU_SOURCE

#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "binary_protocol.h"
#include "epoll.h"
#include "protocol.h"
#include "sockets.h"
#include "text_protocol.h"
#include "worker_state.h"
#include "worker_thread.h"

#define SOCKET_SEND_BUFFER_SIZE 0

// Accepts incoming connections
static void accept_connections(struct WorkerArgs *args, int incoming_fd) {
  struct sockaddr incoming_addr;
  socklen_t incoming_addr_len = sizeof(incoming_addr);
  int client_fd;
  int status;
  struct epoll_event event;

  // We might have to accept an arbitrary amount of incoming connections, so we
  // run an infinite loop until all connections are accepted.
  while (true) {
    client_fd = accept(incoming_fd, &incoming_addr, &incoming_addr_len);
    if (client_fd == -1) {
      if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
        // See `man 2 accept`: it means that the socket is marked nonblocking
        // and no connections are present to be accepted. So, we processed all
        // incoming connection requests and we're done.
        return;
      } else {
        // We had another arbitrary error.
        perror("accept_connections accept");
        return;
      }
    }

    // Allocate memory for the event data that we'll store in the epoll
    // instance and initialize it.
    struct EventData *event_data =
        hashtable_malloc_evict(args->hashtable, sizeof(struct EventData));
    if (event_data == NULL) {
      printf("Couldn't accept incoming connection because we ran out of "
             "memory...\n");
      return;
    }
    event_data_initialize(event_data, client_fd,
                          incoming_fd == args->binary_fd ? BINARY : TEXT);

    // Get the IP address and port of the client and store it in the struct.
    status = getnameinfo(&incoming_addr, incoming_addr_len, event_data->host,
                         NI_MAXHOST, event_data->port, NI_MAXSERV,
                         NI_NUMERICHOST | NI_NUMERICSERV);
    if (status != 0) {
      fprintf(stderr, "accept_connections getnameinfo: %s\n",
              gai_strerror(status));
      return;
    }

    worker_log(args,
               "Accepted connection on descriptor %d "
               "(host=%s, port=%s)",
               event_data->fd, event_data->host, event_data->port);

    // Make the socket non blocking so that we can use it with edge-triggered
    // epoll.
    status = make_socket_non_blocking(client_fd);
    if (status == -1) {
      fprintf(stderr, "Error marking socket as non blocking\n");
      abort();
    }

    event.data.ptr = (void *)event_data;
    event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;

#if SOCKET_SEND_BUFFER_SIZE
    // Make sure the write buffer is small
    int option_value;
    socklen_t option_len;
    int rv = getsockopt(client_fd, SOL_SOCKET, SO_SNDBUF, &option_value,
                        &option_len);
    worker_log(args, "Setting write buffer size to 1024");
    if (rv != 0) {
      perror("getsockopt error");
      abort();
    }
    option_value = 256;
    rv = setsockopt(client_fd, SOL_SOCKET, SO_SNDBUF, &option_value,
                    sizeof(option_value));
    if (rv != 0) {
      perror("setsockopt error");
      abort();
    }
#endif

    // Finally, add the client file descriptor to the epoll instance.
    status = epoll_ctl(args->epoll_fd, EPOLL_CTL_ADD, event_data->fd, &event);
    if (status == -1) {
      perror("accept_connections epoll_ctl");
      abort();
    }
  }
}

// Handles the outcome of a client's request or response handling.
static void handle_client_outcome(int rv, struct WorkerArgs *args,
                                  struct epoll_event *event) {
  struct EventData *event_data = event->data.ptr;

  switch (rv) {
  case CLIENT_READ_INCOMPLETE:
    worker_log(args, "Read incomplete while in state <%s>, waiting for more.",
               client_state_str(event_data->client_state));
    epoll_mod_client(args->epoll_fd, event, EPOLLIN);
    break;
  case CLIENT_WRITE_INCOMPLETE:
    worker_log(args, "Write incomplete while in state <%s>, waiting for more.",
               client_state_str(event_data->client_state));
    epoll_mod_client(args->epoll_fd, event, EPOLLOUT);
    break;
  case CLIENT_READ_ERROR:
    worker_log(args, "Read error while in state <%s>, killing connection.",
               client_state_str(event_data->client_state));
    event_data_close_client(event_data);
    break;
  case CLIENT_WRITE_ERROR:
    worker_log(args, "Write error while in state <%s>, killing connection.",
               client_state_str(event_data->client_state));
    event_data_close_client(event_data);
    break;
  case CLIENT_READ_CLOSED:
    worker_log(args, "Client terminated the connection while in state <%s>.",
               client_state_str(event_data->client_state));
    event_data_close_client(event_data);
    break;
  case CLIENT_WRITE_SUCCESS:
    worker_log(args, "Request successfully handled");
    // Read next request.
    event_data_reset(event_data);
    epoll_mod_client(args->epoll_fd, event, EPOLLIN);
    break;
  case CLIENT_READ_SUCCESS:
  // Fall-through!
  default:
    worker_log(args, "Invalid outcome received, probably an error");
    event_data_close_client(event_data);
    break;
  }
}

static void handle_client(struct WorkerArgs *args, struct epoll_event *event) {
  struct EventData *event_data = event->data.ptr;
  int rv;

  if (event->events & EPOLLIN) {
    worker_log(args, "fd %d (%s) is ready to read (state %s)...",
               event_data->fd, connection_type_str(event_data->connection_type),
               client_state_str(event_data->client_state));

    if (event_data->connection_type == TEXT) {
      rv = handle_text_client_request(args, event);
    } else {
      rv = handle_binary_client_request(args, event);
    }

    handle_client_outcome(rv, args, event);
    return;
  } else if (event->events & EPOLLOUT) {
    worker_log(args, "fd %d (%s) is ready to write (state %s)...",
               event_data->fd, connection_type_str(event_data->connection_type),
               client_state_str(event_data->client_state));

    if (event_data->connection_type == TEXT) {
      rv = handle_text_client_response(args, event);
    } else {
      rv = handle_binary_client_response(args, event);
    }

    handle_client_outcome(rv, args, event);
    return;
  }

  worker_log(args, "Never should reach here...");
}

// Worker thread function.
void *worker(void *_args) {
  struct WorkerArgs *args = (struct WorkerArgs *)_args;
  struct epoll_event events[MAX_EPOLL_EVENTS];

  while (true) {
    int num_events =
        epoll_wait(args->epoll_fd, &events[0], MAX_EPOLL_EVENTS, -1);
    if (num_events == -1) {
      perror("worker epoll_wait");
      abort();
    }

    for (int i = 0; i < num_events; i++) {
      struct EventData *event_data = (struct EventData *)events[i].data.ptr;

      if (events[i].events & EPOLLERR) {
        // Error condition happened on the associated file descriptor.
        worker_log(args, "Epoll error");
        event_data_close_client(event_data);
        // Keep processing fds...
        continue;
      }

      if (events[i].events & EPOLLHUP) {
        worker_log(args, "Client hanged up");
        event_data_close_client(event_data);
        // Keep processing fds...
        continue;
      }

      if (event_data->fd == args->text_fd ||
          event_data->fd == args->binary_fd) {
        // Accept the new client and set them up depending on the type of
        // connection.
        worker_log(args, "Accepting client connections...");
        accept_connections(args, event_data->fd);
        // Keep processing ids...
        continue;
      }

      // At this point, we have to respond to ready for reading or ready for
      // writing FOR A CLIENT.
      worker_log(args, "Received something from a client...");
      handle_client(args, &events[i]);
    }
  }
}