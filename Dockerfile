FROM alpine:3.17

# Set the working directory for the container.
WORKDIR memcached/

# Install the dependencies.
RUN apk update
RUN apk add --no-cache --update gcc libc-dev make clang-extra-tools git

# Copy the source contents into the container.
COPY ./src .

# Build the project.
RUN make

# Copy the server runner into the container and mark it as executable.
COPY ./docker_init.sh .
RUN chmod +x docker_init.sh

# Create a memcached user with uid = 420, gid = 420
RUN adduser -D -u 420 -g 420 memcached
