#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define PORT 8000

/**
 * Returns a file descriptor for a socket that listens on the given port.
 * Modifies the given sockaddr_in structure according to the desired setup.
 */
int listen_on_port(short int port) {
  int server_fd;
  int opt = 1;
  struct sockaddr_in server_address;

  // Create the socket for a TCP connection
  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("Error creating socket");
    exit(EXIT_FAILURE);
  }

  // Configure the socket to allow the port to be reused after a crash
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
                 sizeof(opt)) != 0) {
    perror("Error setting up socket");
    exit(EXIT_FAILURE);
  }

  // Bind the socket to the port
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = INADDR_ANY;
  server_address.sin_port = htons(port);
  if (bind(server_fd, (struct sockaddr *)&server_address,
           sizeof(server_address)) != 0) {
    perror("Error binding socket");
    exit(EXIT_FAILURE);
  }

  // Set up the socket for listening incoming connections
  if (listen(server_fd, 5) != 0) {
    perror("Error preparing the socket for listening connections");
    exit(EXIT_FAILURE);
  }

  return server_fd;
}

/**
 * Blocks until a client connects to the given server_fd socket file descriptor.
 * Prints the IP address of the client and returns the client's socket file
 * descriptor.
 */
int accept_connection(int server_fd) {
  int client_fd;
  struct sockaddr_in client_address;
  socklen_t client_address_len = sizeof(client_address);

  if ((client_fd = accept(server_fd, (struct sockaddr *)&client_address,
                          (socklen_t *)&client_address_len)) == -1) {
    perror("Error accepting incoming connection");
    exit(EXIT_FAILURE);
  }

  char client_ip_address[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &client_address.sin_addr, client_ip_address,
            sizeof(client_address));
  printf("Client connected from ip: %s\n", client_ip_address);

  return client_fd;
}

/**
 * Continuously reads from the given client socket file descriptor and responds
 * the read data.
 */
void handle_connection(int client_fd) {
  char read_buffer[1024] = {0};
  ssize_t bytes_read = 0;

  /**
   * read returns 0 for EOF (i.e. connection terminated), -1 for errors, and a
   * positive number when one or more characters were read.
   */
  while ((bytes_read = read(client_fd, read_buffer, 1024)) > 0) {
    read_buffer[bytes_read] = '\0';
    printf("Read the following from the client: <%s>\n", read_buffer);
    send(client_fd, read_buffer, strlen(read_buffer),
         0); // write could have been used here
    printf("Message sent!\n");
  }

  close(client_fd);
}

int main(int argc, char **argv) {
  int server_fd;
  int client_fd;

  server_fd = listen_on_port(PORT);

  printf("Listening for connections in port %d...\n", PORT);

  while (1) {
    client_fd = accept_connection(server_fd);

    handle_connection(client_fd);
  }

  shutdown(server_fd, SHUT_RDWR);

  return EXIT_SUCCESS;
}