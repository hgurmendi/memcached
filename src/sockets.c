#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "sockets.h"

/* Creates and binds a socket to listen on the given port.
 * This is a portable way of getting an IPv4 or IPv6 socket. `getaddrinfo`
 * returns a bunch of `addrinfo` structures in the last argument which are
 * compatible with the hints passed in the first argument.
 * Returns the socket file descriptor if successful, -1 otherwise.
 */
static int create_and_bind(char *port) {
  struct addrinfo hints;
  struct addrinfo *results, *rp;
  int status;
  int server_fd;

  // Set the desired socket configuration to the `hints` structure. This
  // structure is passed to `getaddrinfo`, which will then mutate `results` to
  // point to an array of possible `addrinfo` structures that satisfy the
  // desired configuration.
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;     // Return IPv4 and IPv6 choices
  hints.ai_socktype = SOCK_STREAM; // TCP socket
  hints.ai_flags = AI_PASSIVE;     // All interfaces

  status = getaddrinfo(NULL, port, &hints, &results);
  if (status != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
    return -1;
  }

  // Iterate the `results` array until we find a file descriptor that we can
  // bind to.
  for (rp = results; rp != NULL; rp = rp->ai_next) {
    server_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (server_fd == -1) {
      continue;
    }

    status = bind(server_fd, rp->ai_addr, rp->ai_addrlen);
    if (status == 0) {
      // Bind successful
      break;
    }

    close(server_fd);
  }

  if (rp == NULL) {
    fprintf(stderr, "Could not bind\n");
    return -1;
  }

  freeaddrinfo(results);

  return server_fd;
}

/* Makes the given file descriptor non-blocking.
 * Specifically, it sets the `O_NONBLOCK` flag on the descriptor passed in the
 * argument.
 */
int make_socket_non_blocking(int socket_fd) {
  int status;
  int flags;

  // Get the flags of the file descriptor by using the `F_GETFL` command.
  flags = fcntl(socket_fd, F_GETFL, 0);
  if (flags == -1) {
    perror("make_socket_non_blocking fcntl 1");
    return -1;
  }

  // Then set the flags of the file descriptor by adding the new flag to the old
  // ones and using the `F_SETFL` command.
  flags |= O_NONBLOCK;
  status = fcntl(socket_fd, F_SETFL, flags);
  if (status == -1) {
    perror("make_socket_non_blocking fcntl 2");
    return -1;
  }

  return 0;
}

/* Creates a listening non-blocking socket on the given port and returns its
 * file descriptor. Aborts execution if anything bad happens.
 */
int create_listen_socket(char *port) {
  int fd;
  int status;

  fd = create_and_bind(port);
  if (fd == -1) {
    abort();
  }

  status = make_socket_non_blocking(fd);
  if (status == -1) {
    abort();
  }

  status = listen(fd, SOMAXCONN);
  if (status == -1) {
    perror("create_listen_socket listen");
    abort();
  }

  return fd;
}
