# memcached

A programming assignment for the Operating Systems I subject for the Computer Science degree in
Universidad Nacional de Rosario: a memcached implementation in C.

[Memcached](https://memcached.org/) is an in-memory key-value store for small chunks of arbitrary
data (strings, objects) from results of database calls, API calls, or page rendering.

# References

- Used the following tutorial for `epoll`: https://web.archive.org/web/20120504033548/https://banu.com/blog/2/how-to-use-epoll-a-complete-example-in-c/
- Used the following tutorial for POSIX threads: https://www.cs.cmu.edu/afs/cs/academic/class/15492-f07/www/pthreads.html

# Build instructions

Just run the following inside the `src` directory:

```bash
$ make
```

The `binder` and `memcached` executables will be built for you, assuming you're in a Linux
environment (the server uses epoll to handle the sockets). Otherwise, you can check the [Docker instructions](#docker-instructions) to run it in a somewhat portable way.

The `binder` executable implements the binding of the listen sockets (for both the text and binary
protocols), then drops the privileges and finally runs the `memcached` executable passing the file
descriptors as arguments.

There are a couple of compilation time parameters for the cache which can be changed in the
`src/parameters.h` file:

- `HASH_TABLE_BUCKETS_SIZE`: determines the number of buckets in the hash table.
- `MEMORY_LIMIT`: (soft) limit for the memory of the process, in bytes.
- `MAX_EVICITIONS_PER_OPERATION`: Maximum number of evictions that are made before giving up on a
  malloc.

# Run instructions

Just run the following command as root (so that it can bind privileged ports):

```bash
$ ./binder $MEMCACHED_EXECUTABLE $TEXT_PORT $BINARY_PORT $TARGET_GID $TARGET_UID
```

Make sure to provide a valid value for `$MEMCACHED_EXECUTABLE` (should be `memcached`), the default
value for `$TEXT_PORT` should be `888`, the default value for `$BINARY_PORT` should be `889` and
finally the values for `$TARGET_GID` and `$TARGET_UID` should be the gid and uid of the group and
user that the binder will drop privileges to, which in most cases it's both `1000` (usually the
first user created in a Linux system).

# Docker instructions

There is a `Dockerfile` for running the project inside a Docker container in case you're using
an operating system that doesn't support epoll, which is only available in Linux. The container is
based on an Alpine Linux image.

In order to run the Docker container you should run the followin scripts that deals with creating
a container image for the project and appropriately running it:

```bash
$ ./run_in_docker.sh
```

Bear in mind that you'll have to stop the container, build and run it again after changes.

There is also a `.devcontainer` hidden directory with a `devcontainer.json` file for setting up
a development container in VSCode (through the DevContainers addon), which helps with IntelliSense
support when not working in a Linux environment.

# Benchmarking

There's a Python script included in the `resources` directory that allows benchmarking the memcached
server. It's very rudimentary and it was mostly used to check that eviction was working correctly
and that the server can run for a long time without having degraded performance or increased
eviction failure (which might indicate memory leaks). The Python script supports multiple
parameters, which can be found by running:

```bash
$ python siege.py --help
```

If you want to benchmark the performance of the server, you can combine the script with GNU
Parallel in the following way (sample run):

```bash
$ seq 1 ${NUM_CLIENTS} | parallel --jobs ${NUM_CLIENTS} --ungroup python siege.py --id {} --log-every 2 --value 5000
```

The script above will run ${NUM_CLIENTS} scripts in parallel that will continuously insert key and
value pairs in the cache with a value of size 5000 bytes each.

# Erlang bindings

Erlang bindings for the cache are implemented in `resources/memcached.erl`. The following functions
are exported from the module:

- `start/0`
- `start/1`
- `put/3`
- `del/2`
- `get/2`
- `take/2`
- `stats/1`

`start/0` initiates a TCP connection with the cache using `localhost` as the host and the default
port (889). `start/1` behaves in the same way except that it receives an address as a parameter.
Both return `{ok, Connection}` if successful or `{error, Reason}` if unsuccessful. The `Connection`
should be considered opaque by the client but it's actually the socket used for the client.

The rest of the functions first receive a connection identifier and then the arguments (if any). If
a connection error happens while handling any of the operation, a pair `{error, Reason}` will be
returned. If a cache error happens, a pair `{cacheerror, ErrorCode}` will be returned. Otherwise, if
the interaction with the cache was successful, a pair `{ok, Result}` will be returned when the
operation includes a content (like `get/2` or `take/2`), or just an `ok` when the successful
response doesn't include a content.
