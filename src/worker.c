#define _GNU_SOURCE

#include <errno.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "bounded_data.h"
#include "epoll.h"
#include "sockets.h"
#include "worker.h"

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

    // REMARK: we added EPOLLONESHOT because if the reader submits data in
    // bursts (i.e. in netcat first some characters then CTRL+D then more
    // characters then CTRL+D then the last characters and enter, that would be
    // read from 3 read calls).
    event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;

    // Finally, add the client file descriptor to the epoll instance.
    status =
        epoll_ctl(worker_args->epoll_fd, EPOLL_CTL_ADD, event_data->fd, &event);
    if (status == -1) {
      perror("accept_connections_redux epoll_ctl");
      abort();
    }
  }
}

#define CLIENT_READ_ERROR -1
#define CLIENT_READ_CLOSED 0
#define CLIENT_READ_SUCCESS 1
#define CLIENT_READ_INCOMPLETE 2

// Reads from the current client until a newline is found or until the client's
// read buffer is full. If a newline character is found then it's replaced by a
// null character and CLIENT_READ_SUCCESS is returned. If the client closes the
// connection then CLIENT_READ_CLOSED is returned. If the client's read buffer
// has some space in it and the client is not ready for reading then
// CLIENT_READ_INCOMPLETE is returned. If an error happens then
// CLIENT_READ_ERROR is returned.
int read_until_newline(struct WorkerArgs *args, struct EventData *event_data,
                       struct epoll_event *event) {
  ssize_t nread = 0;

  while (event_data->total_bytes_read < event_data->read_buffer->size) {
    // Remaining amount of bytes to read into the buffer.
    size_t remaining_bytes =
        event_data->read_buffer->size - event_data->total_bytes_read;
    // Pointer to the start of the "empty" read buffer.
    void *remaining_buffer =
        event_data->read_buffer->data + event_data->total_bytes_read;

    nread = read(event_data->fd, remaining_buffer, remaining_bytes);
    if (nread == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // The file descriptor is not ready to keep reading. Since we're using
        // edge-triggered mode with one-shot we should add it again to the epoll
        // interest list via EPOLL_CTL_MOD.
        event->events = EPOLLIN | EPOLLET | EPOLLONESHOT;
        int rv =
            epoll_ctl(args->epoll_fd, EPOLL_CTL_MOD, event_data->fd, event);
        if (rv == -1) {
          perror("read_until_newline epoll_ctl");
          return CLIENT_READ_ERROR;
        }

        // If adding it to the epoll interest list was successful, we have to
        // let the handler know that reading is not done yet.
        return CLIENT_READ_INCOMPLETE;
      }

      // Some other error happened.
      perror("read_until_newline read");
      return CLIENT_READ_ERROR;
    } else if (nread == 0) {
      // Client disconnected gracefully.
      return CLIENT_READ_CLOSED;
    }

    // Find a newline in the bytes we just read.
    char *newline = memchr(remaining_buffer, '\n', nread);
    if (newline != NULL) {
      *newline = '\0';
      return CLIENT_READ_SUCCESS;
    }

    // We didn't a newline in the bytes we just read, update the total bytes
    // read and keep reading if possible.
    event_data->total_bytes_read += nread;
  }

  // We read all the bytes we are allowed for the buffer and didn't find the
  // newline, just return an error.
  return CLIENT_READ_ERROR;
}

void handle_text_client_request(struct WorkerArgs *args,
                                struct EventData *event_data,
                                struct epoll_event *event) {
  worker_log(args, "Reading from fd %d (%s)", event_data->fd,
             connection_type_str(event_data->connection_type));

  if (event_data->client_state == READ_READY) {
    struct BoundedData *read_buffer =
        bounded_data_create(TEXT_REQUEST_BUFFER_SIZE);
    event_data->read_buffer = read_buffer;
    event_data->total_bytes_read = 0;
    event_data->client_state = READING;
  } else if (event_data->client_state == READING) {
    // do nothing, calling the read_until_newline function should pick up from
    // last time.
  }

  int rv = read_until_newline(args, event_data, event);
  if (rv == CLIENT_READ_ERROR) {
    worker_log(args, "ERROR READING FROM TEXT CLIENT! TERMINATING CONNECTION");
    close_client(event_data);
  } else if (rv == CLIENT_READ_CLOSED) {
    worker_log(args, "CLIENT CLOSED CONNECTION");
    close_client(event_data);
  } else if (rv == CLIENT_READ_SUCCESS) {
    worker_log(args, "SUCCESS! HERES WHAT WE GOT: %s",
               event_data->read_buffer->data);
    // for now keep reading.
    // TODO: transition to WRITE_READY and start writing...
    event->events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    int rv = epoll_ctl(args->epoll_fd, EPOLL_CTL_MOD, event_data->fd, event);
    if (rv == -1) {
      perror("handle_text_client_request epoll_ctl");
      abort();
    }
  } else if (rv == CLIENT_READ_INCOMPLETE) {
    worker_log(args, "READ INCOMPLETE, WAITING FOR MORE DATA");
  } else {
    worker_log(args, "UNKNOWN READ RETURN VALUE, WTF HAPPENED");
  }
}

void handle_client(struct WorkerArgs *args, struct epoll_event *event) {
  struct EventData *event_data = event->data.ptr;

  if (event->events & EPOLLIN) {
    worker_log(args, "fd %d (%s) is ready to read", event_data->fd,
               connection_type_str(event_data->connection_type));
    if (event_data->connection_type == TEXT) {
      handle_text_client_request(args, event_data, event);
    }
  }

  if (event->events & EPOLLOUT) {
    worker_log(args, "fd %d (%s) is ready to write", event_data->fd,
               connection_type_str(event_data->connection_type));
    worker_log(args, "not ready to handle epoll for writing yet!");
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
      perror("worker epoll_wait");
      abort();
    }

    for (int i = 0; i < num_events; i++) {
      struct EventData *event_data = (struct EventData *)events[i].data.ptr;

      if (events[i].events & EPOLLERR) {
        // Error condition happened on the associated file descriptor.
        // fprintf(stderr, "Epoll error\n");
        worker_log(args, "Epoll error");
        close_client(event_data);
        // Keep processing fds...
        continue;
      }

      if (events[i].events & EPOLLHUP) {
        // fprintf(stderr, "Client hanged up\n");
        worker_log(args, "Client hanged up");
        close_client(event_data);
        // Keep processing fds...
        continue;
      }

      printf("EPOLLERR? %d\n", events[i].events & EPOLLERR);
      printf("EPOLLIN? %d\n", events[i].events & EPOLLIN);
      printf("EPOLLOUT? %d\n", events[i].events & EPOLLOUT);
      printf("EPOLLET? %d\n", events[i].events & EPOLLET);
      printf("EPOLLONESHOT? %d\n", events[i].events & EPOLLONESHOT);

      if (event_data->fd == args->text_fd ||
          event_data->fd == args->binary_fd) {
        // Accept the new client and set them up depending on the type of
        // connection.
        // printf("Accepting client connections...\n");
        worker_log(args, "Accepting client connections...");
        accept_connections_redux(args, event_data->fd);
        // Keep processing ids...
        continue;
      }

      // At this point, we have to respond to ready for reading or ready for
      // writing FOR A CLIENT.
      // printf("Received something from a client...\n");
      worker_log(args, "Received something from a client...");
      handle_client(args, &events[i]);
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

// Logs a message from a worker. No newline character needed.
void worker_log(struct WorkerArgs *args, char *fmt, ...) {
  va_list v;

  va_start(v, fmt);
  printf("[WORKER%02d] ", args->worker_id);
  vprintf(fmt, v);
  printf("\n");
  va_end(v);
}