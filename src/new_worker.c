#include <errno.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "new_epoll.h"
#include "new_worker.h"
#include "sockets.h"

// Accepts incoming connections
static void accept_connections_redux(struct WorkerArgs *worker_args,
                                     int incoming_fd) {
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
        // See `man 2 accept`: itt means that the socket is marked nonblocking
        // and no connections are present to be accepted. So, we processed all
        // incoming connection requests and we're done.
        return;
      } else {
        // We had another arbitrary error.
        perror("accept_connections_redux accept");
        return;
      }
    }

    // Allocate memory for the event data that we'll store in the epoll
    // instance.
    struct EventData *event_data = event_data_create();

    // Fill the struct with the relevant data.
    event_data->fd = client_fd;
    event_data->connection_type =
        incoming_fd == worker_args->binary_fd ? BINARY : TEXT;

    // Get the IP address and port of the client and store it in the struct.
    status = getnameinfo(&incoming_addr, incoming_addr_len, event_data->host,
                         NI_MAXHOST, event_data->port, NI_MAXSERV,
                         NI_NUMERICHOST | NI_NUMERICSERV);
    if (status != 0) {
      fprintf(stderr, "accept_connections_redux getnameinfo: %s\n",
              gai_strerror(status));
      return;
    }

    printf("Accepted connection on descriptor %d "
           "(host=%s, port=%s)\n",
           event_data->fd, event_data->host, event_data->port);

    // Make the socket non blocking so that we can use it with edge-triggered
    // epoll.
    status = make_socket_non_blocking(client_fd);
    if (status == -1) {
      fprintf(stderr, "Error marking socket as non blocking\n");
      abort();
    }

    event.data.ptr = (void *)event_data;
    // TODO: might have to change the events here? As a first thought, I think
    // that when accepting a connection we just have to listen for incoming data
    // (EPOLLIN) and always edge-triggered (EPOLLET) so we should be ok. After
    // successfully reading the first content we should re-insert it with
    // EPOLLOUT if we didn't finish writing or in EPOLLIN again to listen for
    // more incoming connections.
    event.events = EPOLLIN | EPOLLET;

    // Finally, add the client file descriptor to the epoll instance.
    status =
        epoll_ctl(worker_args->epoll_fd, EPOLL_CTL_ADD, event_data->fd, &event);
    if (status == -1) {
      perror("accept_connections_redux epoll_ctl");
      abort();
    }
  }
}

// Worker thread function.
void *worker(void *_args) {
  struct WorkerArgs *args = (struct WorkerArgs *)_args;
  struct epoll_event events[MAX_EPOLL_EVENTS];

  while (true) {
    int num_events =
        epoll_wait(args->epoll_fd, &events[0], MAX_EPOLL_EVENTS, -1);
    if (num_events == -1) {
      perror("new_worker_loop epoll_wait");
      abort();
    }

    for (int i = 0; i < num_events; i++) {
      struct EventData *event_data = (struct EventData *)events[i].data.ptr;

      if (events[i].events & EPOLLERR) {
        // Error condition happened on the associated file descriptor.
        fprintf(stderr, "Epoll error\n");
        close_client(event_data);
        // Keep processing fds...
        continue;
      }

      if (events[i].events & EPOLLHUP) {
        fprintf(stderr, "Client hanged up\n");
        close_client(event_data);
        // Keep processing fds...
        continue;
      }

      printf("EPOLLIN? %d\n", events[i].events & EPOLLIN);
      printf("EPOLLOUT? %d\n", events[i].events & EPOLLOUT);
      printf("EPOLLET? %d\n", events[i].events & EPOLLET);
      printf("EPOLLONESHOT? %d\n", events[i].events & EPOLLONESHOT);

      if (event_data->fd == args->text_fd ||
          event_data->fd == args->binary_fd) {
        // Accept the new client and set them up depending on the type of
        // connection.
        printf("Accepting client connections...\n");
        accept_connections_redux(args, event_data->fd);
        // Keep processing ids...
        continue;
      }

      // At this point, we have to respond to ready for reading or ready for
      // writing FOR A CLIENT.
      printf("Received something from a client...\n");
    }
  }
}

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