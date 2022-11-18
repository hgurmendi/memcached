FROM alpine:3.17

# Set a directory for the server
WORKDIR soi-memcached/

RUN apk update
RUN apk add --no-cache --update gcc libc-dev make

# Copy the source contents into the container
COPY ./src .

# Build the project
RUN make

# Run the server
CMD ["./memcached", "8000"]

# Expose the port 8000
EXPOSE 8000