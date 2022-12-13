#include <stdio.h>
#include <stdlib.h>
#include <sys/sysinfo.h>

#include "dispatcher.h"
#include "parameters.h"

int main(int argc, char *argv[]) {
  char *text_port = NULL, *binary_port = NULL;

  if (argc != 3) {
    fprintf(stderr, "Usage: %s text_port binary_port\n", argv[0]);
    return EXIT_FAILURE;
  }

  text_port = argv[1];
  binary_port = argv[2];

  struct DispatcherState dispatcher_state;

  int num_processors = get_nprocs();
  int num_workers = num_processors > 1 ? num_processors - 1 : 1;

  dispatcher_initialize(&dispatcher_state, num_workers, text_port, binary_port);

  dispatcher_loop(&dispatcher_state);

  dispatcher_destroy(&dispatcher_state);

  return EXIT_SUCCESS;
}