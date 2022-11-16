#include <errno.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "sockets.h"

#define MAXEVENTS 64

/* Accepts all the incoming connections on the server socket and marks them for
 * monitoring by epoll.
 */
void accept_incoming_connections(int server_fd, int epoll_fd) {
  struct sockaddr incoming_addr;
  socklen_t incoming_addr_len = sizeof incoming_addr;
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

    event.data.fd = client_fd;
    event.events = EPOLLIN | EPOLLET;
    status = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event);
    if (status == -1) {
      perror("epoll_ctl");
      abort();
    }
  }
}

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

/* Main server loop. Accepts incoming connections and serves them accordingly.
 */
void main_server_loop(int server_fd) {
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
  events = calloc(MAXEVENTS, sizeof event);

  while (true) {
    int num_events;

    num_events = epoll_wait(epoll_fd, events, MAXEVENTS, -1);
    for (int i = 0; i < num_events; i++) {
      int is_epoll_error = (events[i].events & EPOLLERR) ||
                           (events[i].events & EPOLLHUP) ||
                           !(events[i].events & EPOLLIN);

      if (is_epoll_error) {
        fprintf(stderr, "epoll error\n");
        close(events[i].data.fd);
      } else if (server_fd == events[i].data.fd) {
        // Read data from the server socket (i.e. accept incoming connections).
        accept_incoming_connections(server_fd, epoll_fd);
      } else {
        // Read data from an arbitrary client socket (i.e. handle incoming
        // interactions).
        handle_client(events[i].data.fd);
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

  server_fd = create_listen_socket(argv[1]);

  main_server_loop(server_fd);

  close(server_fd);
  return EXIT_SUCCESS;
}