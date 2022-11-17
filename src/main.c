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

#include "dispatcher.h"
#include "worker.h"

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s [port]\n", argv[0]);
    return EXIT_FAILURE;
  }

  struct DispatcherState *dispatcher_state;
  dispatcher_state = calloc(1, sizeof(struct DispatcherState));
  if (dispatcher_state == NULL) {
    perror("calloc dispatcher_state");
    abort();
  }

  initialize_dispatcher(dispatcher_state, NUM_WORKERS, argv[1]);

  dispatcher_loop(dispatcher_state);

  destroy_dispatcher(dispatcher_state);

  return EXIT_SUCCESS;
}