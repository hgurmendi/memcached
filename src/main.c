#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 8000

/**
 * Returns a file descriptor for a socket that listens on the given port.
 * Modifies the given sockaddr_in structure according to the desired setup.
 */
int listen_on_port(short int port, struct sockaddr_in *address, socklen_t address_len)
{
    int fd_listen;
    int opt = 1;

    // Create the socket for a TCP connection
    if ((fd_listen = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Configure the socket to allow the port to be reused after a crash
    if (setsockopt(fd_listen, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) != 0) {
        perror("Error setting up socket");
        exit(EXIT_FAILURE);
    }

    // Bind the socket to the port
    address->sin_family = AF_INET;
    address->sin_addr.s_addr = INADDR_ANY;
    address->sin_port = htons(port);
    if (bind(fd_listen, (struct sockaddr *) address, address_len) != 0) {
        perror("Error binding socket");
        exit(EXIT_FAILURE);
    }

    // Set up the socket for listening incoming connections
    if (listen(fd_listen, 5) != 0) {
        perror("Error preparing the socket for listening connections");
        exit(EXIT_FAILURE);
    }

    return fd_listen;
}

int main(int argc, char **argv)
{
    int fd_listen;
    int fd_connection;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);

    fd_listen = listen_on_port(PORT, &address, addrlen);

    // Block until a client connects
    if ((fd_connection = accept(fd_listen, (struct sockaddr *) &address, (socklen_t *) &addrlen )) == -1) {
        perror("Error accepting incoming connection");
        exit(EXIT_FAILURE);
    }

    // Read from the socket
    char buffer[1024] = {0};
    ssize_t bytes_read = 0;
    bytes_read = read(fd_connection, buffer, 1024);
    printf("Read the following: <%s>\n", buffer);

    // Echo the response to the client
    send(fd_connection, buffer, strlen(buffer), 0); // write could be used here, too

    printf("Message sent\n");

    // Close both the client and server socket
    close(fd_connection);
    shutdown(fd_listen, SHUT_RDWR);

    return EXIT_SUCCESS;
}