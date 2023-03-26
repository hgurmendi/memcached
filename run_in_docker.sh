#/bin/bash

# We want to exit the script if any of the commands fail.
set -e

docker build -t soi-memcached .
# We have to pass --init so that signals are forwarded to the container so we can use
# CTRL+C to stop the process.
docker run --init -p 7666:7666 -p 7667:7667 --rm -it --name memcached soi-memcached
