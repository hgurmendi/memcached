#/bin/bash

# We want to exit the script if any of the commands fail.
set -e

# Build the container image and tag it "soi-memcached".
echo "Building image..."
docker build -t soi-memcached .

# Remove the container with name "memcached" if it exists.
echo "Removing existing container, if any..."
docker rm -f memcached

# We have to pass --init so that signals are forwarded to the container so we can use
# CTRL+C to stop the process.
echo "Running server..."
docker run --init -p 7666:7666 -p 7667:7667 --rm -it --name memcached soi-memcached
