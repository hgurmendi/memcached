#include <stdio.h>
#include <unistd.h>

#include "sockets.h"

int main(int argc, char *argv[]) {

  if (argc != 4) {
    fprintf(stderr, "USAGE: %s MEMCACHED_BINARY TEXT_PORT BINARY_PORT\n",
            argv[0]);
    return 1;
  }

  int rv;
  char *memcached_binary = argv[1];
  char *text_port = argv[2];
  char *binary_port = argv[3];
  gid_t gid = getgid();
  uid_t uid = getuid();

  printf("Running with gid=%d uid=%d\n", gid, uid);

  // if (uid != 0) {
  //   fprintf(stderr, "ERROR: should run as root.\n");
  //   return 1;
  // }

  gid_t egid = getegid();
  uid_t euid = geteuid();

  printf("Running with egid=%d euid=%d\n", egid, euid);

  // Bind sockets while we have root privileges.
  int text_fd = create_listen_socket(text_port);
  int binary_fd = create_listen_socket(binary_port);

  // Drop root privileges now that we binded the sockets.
  if (setgid(gid) == -1) { // TODO: change to an appropriate gid.
    fprintf(stderr, "ERROR: couldn't drop group privileges.\n");
    close(text_fd);
    close(binary_fd);
    return 1;
  }
  if (setuid(uid) == -1) { // TODO: change to an appropriate uid.
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
