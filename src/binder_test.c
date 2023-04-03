#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "USAGE: %s TEXT_SOCKET_FD BINARY_SOCKET_FD\n", argv[0]);
    return 1;
  }

  char *text_fd_arg = argv[1];
  char *binary_fd_arg = argv[2];
  int text_fd = atoi(text_fd_arg);
  int binary_fd = atoi(binary_fd_arg);

  printf("Received text_fd=%d binary_fd=%d\n", text_fd, binary_fd);

  gid_t gid = getgid();
  uid_t uid = getuid();

  printf("gid=%d uid=%d\n", gid, uid);

  close(text_fd);
  close(binary_fd);

  return 0;
}
