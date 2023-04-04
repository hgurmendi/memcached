FROM ubuntu:22.04

# Set the working directory for the container.
WORKDIR memcached/

# Install the dependencies.
# RUN apk update
# RUN apk add --no-cache --update gcc libc-dev make clang-extra-tools git
RUN apt-get update
RUN apt-get install -y build-essential

# Copy the source contents into the container.
COPY ./src .

# Build the project.
RUN make drop_privileges_test

# # Expose the ports 7666 and 7667 for the text and binary protocols, respectively.
# EXPOSE 7666 7667

# Copy the server runner into the container and mark it as executable.
COPY ./docker_init.sh .
RUN chmod +x docker_init.sh

RUN groupadd 
RUN adduser --disabled-password --uid 1420 --gid 1420 memcached
USER memcached

# Commented out the entrypoint in order to try running a VSCode dev container.
# # Run the server
# ENTRYPOINT ["./init.sh"]
