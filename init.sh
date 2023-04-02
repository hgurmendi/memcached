#!/bin/sh

TEXT_PORT=${1}
BINARY_PORT=${2}

./memcached ${TEXT_PORT} ${BINARY_PORT}