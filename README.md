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

The `memcached` executable will be built for you, assuming you're in a Linux environment. Otherwise,
you should check the [Docker instructions](#docker-instructions).

There are a couple of compilation time parameters for the cache which can be changed in the
`src/parameters.h` file:

- `HASH_TABLE_BUCKETS_SIZE`: determines the number of buckets in the hash table.
- `MEMORY_LIMIT`: (soft) limit for the memory of the process.
- `MAX_EVICITIONS_PER_OPERATION`: Maximum number of evictions that are made before giving up on a
  malloc.

# Run instructions

Just run:

```bash
$ ./memcached <TEXT_PORT> <BINARY_PORT>
```

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
python siege.py --help
```

If you want to benchmark the performance of the server, you can combine the script with GNU
Parallel in the following way (sample run):

```bash
seq 1 ${NUM_CLIENTS} | parallel --jobs ${NUM_CLIENTS} --ungroup python siege.py --id {} --log-every 2 --value 5000
```

The script above will run ${NUM_CLIENTS} scripts in parallel that will continuously insert key and
value pairs in the cache with a value of size 5000 bytes each.
