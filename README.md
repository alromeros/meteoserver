# meteoserver

**meteoserver** is a simple server designed to process TCP requests, storing their content as a MD5 hash in a LRU cache.

The server makes use of a configurable thread-pool to handle client-server connection and processing, while the main thread is in charge of accepting new connections and adding them to a queue.

## How it works:

### Requirements

Ideally, **meteoserver** should only need GNU's **GCC** and **Make** for compilation.

The program has been developed using these versions of the tools:

- GNU Make 4.2.1
- gcc (Ubuntu 9.3.0-17ubuntu1~20.04) 9.3.0

Support for *Windows* is not available: **meteoserver** has been developed using *Linux*-like systems as a reference.

### Compilation:

To build **meteoserver**, run `make` inside the **meteoserver** directory, as shown in the next snippet:

```bash
$ make
```

**Make** will build the **meteoserver** binary accordingly.

Apart from the standard compilation, the `Makefile` presents some additional options:

```bash
$ make DEBUG=1  # Enables debug flags in compilation.
$ make test     # Automatically runs a test case after building the program.
$ make clean 	# Clears the object files and temporary logs associated with the program.
$ make fclean 	# Same as above but also deletes the built binary.
```

### Usage

Running `$ ./meteoserver -h` prompts a help message with information about the program's usage:

```
Usage: ./meteoserver [-p port] [-C amount] [-t amount]
    -p  <port>          Port.
    -C  <amount>        Cache size.
    -t  <amount>        Number of threads for the thread pool (8 by default).
    -h                  Show this help message.
```

Both the `-p` and `-C` arguments are mandatory, while the `-t` is completely optional.

Some usage examples (server side):

```bash
# Runs the serve listening on port 100 and with a LRU cache of 10 elements
$ ./meteoserver -p 100 -C 10
```
```bash
# Same as above, but with a thread pool of 20 threads
$ ./meteoserver -p 100 -C 10 -t 20
```
```bash
# After receiving an USR1 signal
$ kill -USR1 $(pidof meteoserver)
Done!
```

```bash
# After receiving a TERM signal
$ kill -TERM $(pidof meteoserver)
#prints the cached elements
Bye!
```

Some usage examples (client side):

```bash
$ echo "get test1 3000" | nc localhost 100
5a105e8b9d40e1329780d62ea2265d8a
```

```bash
$ echo "get test2 5000" | nc localhost 100
ad0234829205b9033196ba818f7a872b
```

```bash
$ echo "" | nc localhost 100
Request is invalid.
```

```bash
$ nc localhost 100
Timeout
```

```bash
$ echo "(request with +4096 characters)" | nc localhost 100
Request is too long.
```

## Project structure

```bash
meteoserver             # Main directory
├── inc                 # Directory with '.h' files
│   └── meteoserver.h
├── LICENSE
├── Makefile
├── README.md
├── src                 # Source code
│   ├── dataStructures  # Data structures
│   │   ├── lruCache.c
│   │   └── requestQueue.c
│   ├── main.c
│   ├── requestMonitor  # Code in charge of processing server-client communication (thread pool)
│   │   └── requestMonitor.c
│   └── utils           # Additional functions and algorithms
│       └── crypto.c
└── test                # Simple test
    └── stress_test.sh
```

## License

Meteoserver is licensed under the [General Public License, Version 2.0](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html).
