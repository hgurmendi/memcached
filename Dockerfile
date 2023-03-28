FROM alpine:3.17

# Set the working directory for the container.
WORKDIR soi-memcached/

# Install the dependencies.
RUN apk update
RUN apk add --no-cache --update gcc libc-dev make

# Copy the source contents into the container.
COPY ./src .

# Build the project.
RUN make

# Expose the ports 7666 and 7667 for the text and binary protocols, respectively.
EXPOSE 7666 7667

# Copy the server runner into the container and mark it as executable.
COPY ./init.sh .
RUN chmod +x init.sh

# Commented out the entrypoint in order to try running a VSCode dev container.
# # Run the server
# ENTRYPOINT ["./init.sh"]
