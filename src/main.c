#include <stdio.h>
#include <stdlib.h>

#include "dispatcher.h"
#include "worker.h"

int main(int argc, char *argv[]) {
  char *text_port = NULL, *binary_port = NULL;

  if (argc != 3) {
    fprintf(stderr, "Usage: %s text_port binary_port\n", argv[0]);
    return EXIT_FAILURE;
  }

  text_port = argv[1];
  binary_port = argv[2];

  struct DispatcherState *dispatcher_state;
  dispatcher_state = calloc(1, sizeof(struct DispatcherState));
  if (dispatcher_state == NULL) {
    perror("calloc dispatcher_state");
    abort();
  }

  initialize_dispatcher(dispatcher_state, NUM_WORKERS, text_port, binary_port);

  dispatcher_loop(dispatcher_state);

  destroy_dispatcher(dispatcher_state);

  return EXIT_SUCCESS;
}