#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "sockets.h"

int main(int argc, char *argv[]) {

  if (argc != 6) {
    fprintf(stderr,
            "USAGE: %s MEMCACHED_BINARY TEXT_PORT BINARY_PORT GID UID\n",
            argv[0]);
    return 1;
  }

  int rv;
  char *memcached_binary = argv[1];
  char *text_port = argv[2];
  char *binary_port = argv[3];
  char *gid_str = argv[4];
  char *uid_str = argv[5];
  gid_t current_gid = getgid();
  uid_t current_uid = getuid();
  gid_t gid = atoi(gid_str);
  uid_t uid = atoi(uid_str);

  printf("Running the binder with uid=%d gid=%d\n", current_uid, current_gid);

  if (current_uid != 0) {
    fprintf(stderr, "ERROR: should run as root.\n");
    return 1;
  }

  // Bind sockets while we have root privileges.
  int text_fd = create_listen_socket(text_port);
  int binary_fd = create_listen_socket(binary_port);

  // Drop root privileges now that we binded the sockets.
  if (setgid(gid) == -1) {
    fprintf(stderr, "ERROR: couldn't drop group privileges.\n");
    close(text_fd);
    close(binary_fd);
    return 1;
  }
  if (setuid(uid) == -1) {
    fprintf(stderr, "ERROR: couldn't drop user privileges.\n");
    close(text_fd);
    close(binary_fd);
    return 1;
  }

  // Run the cache by passing the file descriptors to the cache executable.
  char text_fd_arg[32];
  rv = snprintf(text_fd_arg, 32, "%d", text_fd);
  if (rv < 0) {
    fprintf(
        stderr,
        "ERROR: failed to create text protocol file descriptor argument.\n");
    return 1;
  }
  char binary_fd_arg[32];
  rv = snprintf(binary_fd_arg, 32, "%d", binary_fd);
  if (rv < 0) {
    fprintf(
        stderr,
        "ERROR: failed to create binary protocol file descriptor argument.\n");
    return 1;
  }

  char *args[] = {memcached_binary, text_fd_arg, binary_fd_arg, NULL};

  execv(memcached_binary, args);

  perror("ERROR during execv for cache executable.\n");
  close(text_fd);
  close(binary_fd);
  return 1;
}
