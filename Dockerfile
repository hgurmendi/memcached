FROM alpine:3.17

# Set a directory for the server
WORKDIR soi-memcached/

# Install the dependencies
RUN apk update
RUN apk add --no-cache --update gcc libc-dev make

# Copy the source contents into the container
COPY ./src .

# Build the project
RUN make

# Expose the ports 7666 and 7667 for the text and binary protocols, respectively.
EXPOSE 7666 7667

# Run the server
ENTRYPOINT ["./memcached", "7666", "7667"]
