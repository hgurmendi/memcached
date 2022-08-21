#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 8000

int main(int argc, char **argv)
{
    int fd_listen;
    int fd_client;
    int opt = 1;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

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
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind the socket to the port
    if (bind(fd_listen, (struct sockaddr *) &address, sizeof(address)) != 0) {
        perror("Error binding socket");
        exit(EXIT_FAILURE);
    }

    // Set up the socket for listening incoming connections
    if (listen(fd_listen, 5) != 0) {
        perror("Error marking the socket for listening");
        exit(EXIT_FAILURE);
    }

    // Block until a client connects
    if ((fd_client = accept(fd_listen, (struct sockaddr *) &address, (socklen_t *) &addrlen )) == -1) {
        perror("Error accepting incoming connection");
        exit(EXIT_FAILURE);
    }

    // Read from the socket
    char buffer[1024] = {0};
    ssize_t bytes_read = 0;
    bytes_read = read(fd_client, buffer, 1024);
    printf("Read the following: %s\n", buffer);

    // Write a response to the client
    char *response = "Hello from server!\n";
    send(fd_client, response, strlen(response), 0); // write could be used too

    printf("Message sent\n");

    // Close both the client and server socket
    close(fd_client);
    shutdown(fd_listen, SHUT_RDWR);

    return EXIT_SUCCESS;
}