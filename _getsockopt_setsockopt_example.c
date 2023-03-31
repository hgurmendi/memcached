// getsockopt and setsockopt
// Might need the following includes:
// #include <sys/types.h>
// #include <sys/socket.h>
// Arguments
// int socket;           // socket
// int level;            // should be SOL_SOCKET
// int option_name;      // option name, SO_SNDBUF in this case
// void *option_value;   // pointer to the memory where the value will be
//                       // stored. int is the most common, apparently.
// socklen_t option_len; // size of the memory pointed at by option_value.

int elsocket;         // socket fd
int option_value = 0; // where the value will be stored
socklen_t option_len = sizeof(option_value);

// CODE
int rv =
    getsockopt(elsocket, SOL_SOCKET, SO_SNDBUF, &option_value, &option_len);
if (rv != 0) {
  printf("SOMETHING WRONG WITH getsockopt\n");
  abort();
}
printf("SO_SNDBUF = %d\n", option_value);

printf("Setting it low\n");
option_value = 256;
rv = setsockopt(elsocket, SOL_SOCKET, SO_SNDBUF, &option_value, option_len);
if (rv != 0) {
  printf("SOMETHING WRONG WITH setsockopt\n");
  abort();
}

printf("Value set now\n");