# HttpServer

This project implements multithreaded (based on worker threads pool) web server responding to (static) HTTP GET requests.
The protocol version used by the server is HTTP/1.1. In particular, this means that all client connections are considered persistent by default.

The following MIME types are supported:

* text/html
* text/css
* text/javascript
* text/plain
* image/png
* image/gif
* image/jpeg
* image/svg+xml
* application/vnd.ms-fontobject
* font/ttf
* font/woff
* font/woff2

## Compiling, Running and Testing (Linux)

### Compiling
To compile the project, first, create a `build` directory and change to that directory:
```
mkdir build && cd build
```
From within `build` directory, then run `cmake` and `make` as follows:
```
cmake ..
make
```
### Running
Executable called `http_server` will be placed inside `build` directory. From within `build` directory, run project as follows:
```
./http_server -h ip -p port -d dir
```
* `ip` - placeholder for IP address server will bound to
* `port` - placeholder for TCP port server will listen on
* `dir` - placeholder for directory containing web application files (i.e., *.html, *.css and *.js files among others) which need to be served

After this command is executed, server will be running as a background process (i.e., will become a daemon).

### Testing
In order to locally run simple test web site included with the project, from within `build` directory, execute the following command:
```
./http_server -h 127.0.0.1 -p 12345 -d /home/workspace/HttpServer/test/
```
NOTE: `-d` command line argument must be equal to the absolute path to the `test` folder inside project's root directory.
      So, it may differ from `/home/workspace/HttpServer/test/` depending on username and directory project has been built and executed from.

After server is started, open web browser (e.g., Firefox) and enter the following request into the address bar:
```
http://127.0.0.1:12345/index.html
```

## Project Structure

* `CMakeLists.txt` - contains instructions to build project via `CMake` and `Make`

* src/
    * `opts.h`
        * `struct Opts` - structure implementing Singleton pattern for parsing command line arguments using `getopt`.
    * `message_queue.h`
        * `class MessageQueue` - class template implementing thread-safe reusable communication channel between threads as a (bounded) FIFO message queue.
        Upper bound on the number of messages simultaneously waiting in the queue could be specified during construction.
        Given that queue is 'full', no new messages could be added to it until receiver thread pulls out one or more messages from the queue.
        Such behavior is what is expected from the web server: new clients will be discarded if server is currently overwhelmed by requests from clients already connected to it.
    * `worker_pool.h` `worker_pool.cpp`
        * `struct Task` - abstract interface through which tasks are submitted to the worker threads pool.
        * `class WorkerPool` - abstract class representing worker threads pool.
        Desired number of worker threads must be specified during construction.
        Declares pure virtual function `SubmitTask` to be overriden by subclasses with custom scheduling logic.
        Tasks are sent to the workers through corresponding message queues.
        * `class RoundRobinWorkerPool` - class derived from abstract class `WorkerPool` using public inheritance.
        Pure virtual function `SubmitTask` is overriden with implementation of simple round-robin scheduling algorithm.
    * `io.h` `io.cpp`
        * `class Socket` - class implementing thread-safe (by using `std::atomic` type) reference-counting RAII object
        acquiring socket file descriptor on construction and releasing it automatically when last instance referring to it goes out of scope.
        It is worth noting that `Socket` is very much like `std::shared_ptr` with the only major difference being
        that the managed resource in case of `Socket` is the open file descriptor instead of dynamically allocated memory.  
        * `class BufReader` - class allowing to wrap `Socket` objects in order to encapsulate logic of buffered read operations.
        This way parsing of HTTP requests is made much more efficient (because of significantly reduced frequency of `read` system call invocations)
        and controlled (because of ability to easily read requests line-by-line).
    * `http_server.h` `http_server.cpp`
        * `struct Error` - type used to report errors (by throwing exceptions) related to server operation (e.g., during construction of instance of class Server).
        * `struct Request` - 
        * `class Server` - 
    * `main.cpp` - instantiates Http::Server object with arguments passed via command line and starts the server. Along the way, process is daemonized.

* test/ - directory containing files implementing simple web site for quick testing

## Major Rubric Points Addressed
