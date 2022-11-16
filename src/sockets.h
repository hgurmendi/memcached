#ifndef __SOCKETS_H__
#define __SOCKETS_H__

/* Makes the given socket non-blocking.
 * Returns 0 if successful, -1 otherwise.
 */
int make_socket_non_blocking(int socket_fd);

/* Creates a non-blocking socket to listen on the given port.
 * Returns its file descriptor is successful, aborts the program otherwise.
 */
int create_listen_socket(char *port);

#endif