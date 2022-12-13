#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/sysinfo.h>

#include "dispatcher.h"
#include "parameters.h"

void set_memory_limit() {
  int ret;
  struct rlimit rlim;

  ret = getrlimit(RLIMIT_DATA, &rlim);
  if (ret != 0) {
    perror("getrlimit");
    abort();
  }

  rlim.rlim_cur = MEMORY_LIMIT_IN_BYTES;
  ret = setrlimit(RLIMIT_DATA, &rlim);
  if (ret != 0) {
    perror("setrlimit");
    abort();
  }
}

int main(int argc, char *argv[]) {
  char *text_port = NULL, *binary_port = NULL;

  if (argc != 3) {
    fprintf(stderr, "Usage: %s text_port binary_port\n", argv[0]);
    return EXIT_FAILURE;
  }

  text_port = argv[1];
  binary_port = argv[2];

  set_memory_limit();

  struct DispatcherState dispatcher_state;

  int num_processors = get_nprocs();
  int num_workers = num_processors > 1 ? num_processors - 1 : 1;

  dispatcher_initialize(&dispatcher_state, num_workers, text_port, binary_port);

  dispatcher_loop(&dispatcher_state);

  dispatcher_destroy(&dispatcher_state);

  return EXIT_SUCCESS;
}