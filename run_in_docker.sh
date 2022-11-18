#/bin/bash

docker build -t soi-memcached .
docker run -p 8000:8000 --rm -it --name memcached soi-memcached
