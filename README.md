# HttpServer

This project implements multithreaded (based on worker threads pool) web server responding to HTTP requests for static content.
The protocol version used by the server is HTTP/1.1.

### Supported HTTP/1.1 Features

* persistent connections (with timeout)
* HTTP requests pipelining

### Supported Methods

* GET
* HEAD

### Supported Response Status Codes

* 200 OK
* 400 Bad Request
* 404 Not Found
* 501 Not Implemented

### Supported MIME Types

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
To compile the project, first, create a `build` directory (inside project's root folder) and change to it:
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

**Note:** it could happen that server won't start because of specified port is currently unavailabe (probably temporary).
To verify that server is actually started, please, use `top` Linux command and check whether `http_server` is listed among running processes.
If it isn't, then either try to run the server in a minute or try to use different port number in `-p` command line option.

### Testing
In order to locally run test web site included with the project, from within `build` directory, execute the following command:
```
./http_server -h 127.0.0.1 -p 12345 -d /home/workspace/HttpServer/test/
```
**Note:** `-d` command line argument must be equal to the absolute path to the `test` folder inside project's root directory.
      So, it may differ from `/home/workspace/HttpServer/test/` depending on username and directory project has been built and executed from.

After server is started, open web browser (e.g., Firefox) and enter the following request into the address bar:
```
http://127.0.0.1:12345/index.html
```

**Note:** project could be tested on any other supported web site (e.g., consisting from `.html`, `.css`, `.js`, `.png`, `.gif`, `.jpg` files).
Just place your web site files inside some directory and provide path to that directory via command line `-d` option.

## Main Event Loop

The main event loop is implemented using `epoll` IO multiplexing mechanism of Linux operating system. It is responsible for:
* accepting new connections
* dispatching arrived (possibly pipelined) requests from already existing connections to worker threads
* closing idle persistent connections (to prevent server resources from being wasted or even exhausted)

## Worker Pool

Each persistent connection is associated with some worker thread in order to serialize responses to pipelined requests through worker's message queue.
Worker threads are responsible for:
* file IO operations
* interpreting assigned HTTP requests and sending responses to them

## Project Structure

* `CMakeLists.txt` - contains instructions to build project via `CMake` and `Make`

* `src/`
    * `opts.h`
        * `struct Opts` - structure implementing Singleton pattern for parsing command line arguments using `getopt`.
    * `message_queue.h`
        * `class MessageQueue` - class template implementing thread-safe reusable communication channel between threads as a (bounded) FIFO message queue.
        Upper bound on the number of messages simultaneously waiting in the queue could be specified during construction.
        Given that queue is 'full', no new messages could be added to it until receiver thread pulls out one or more messages from the queue.
        Such behavior is what is expected from the web server: new clients will be discarded if server is currently overwhelmed by requests from clients already connected to it.
    * `worker_pool.h` `worker_pool.cpp`
        * `struct ITask` - abstract interface representing task which could be performed by a worker thread.
        Declares pure virtual function `Perform` to be overriden by subclasses.
        * `class IWorker` - abstract interface representing worker tasks could be assigned to.
        Declares pure virtual function `AssignTask` to be overriden by subclasses.
        * `class WorkerPool` - abstract class representing worker threads pool.
        Desired number of worker threads must be specified during construction.
        Declares pure virtual function `SubmitTask` to be overriden by subclasses with custom scheduling logic.
        Return value of `SubmitTask` could be used to associate worker with specific persistent connection to assign all subsequent requests (from that connection)
        directly to that worker in order to ensure responses (to pipelined requests) are properly serialized (i.e., sent in order of received requests) via corresponding message queue.
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
        * `class Server` - class encapsulating entire web server functionality.
        This class is implemented using the well-known **pimpl idiom** in C++,
        i.e., all helper structs and functions are implemented in corresponding .cpp file so that all implementation details are hidden from the user.
        * `struct Request` - structure implementing abstract interface `ITask`.
        Pure virtual function `Perform` is overriden with logic needed to interpret HTTP request, read necessary content from file and send it to the client as HTTP response.
        * `struct Error` - type used to report errors (by throwing exceptions) related to server operation (e.g., during construction of instance of class Server).
    * `main.cpp` - instantiates Http::Server object with arguments passed via command line and starts the server. Along the way, process is daemonized.

* `test/` - directory containing files implementing relatively complex web site for quick testing of this HttpServer functionality. The code of this web site was written by author of the [online course](https://www.coursera.org/learn/html-css-javascript-for-web-developers) as a case study during the course and was made available among study materials. This web site uses html, css and javascript files along with images (in particular, of different resolutions for different screen sizes) which need to be properly and timely downloaded from the server. Also, this web site employs AJAX technology in order to dynamically load (from the server) parts of the webpage's content.

## Rubric Points Addressed

* The project uses multithreading.
    * file: worker_pool.h, line: 34
    * file: worker_pool.cpp, line: 11
* A mutex or lock is used in the project.
    * file: message_queue.h, line: 30
* A condition variable is used in the project.
    * file: message_queue.h, line: 29
* The project uses scope / Resource Acquisition Is Initialization (RAII) where appropriate.
    * file: message_queue.h, lines: 47, 62, 77
    * file: io.h, lines: 8-30 (class Socket follows RAII idiom)
    * file: http_server.cpp, line: 271
* The project follows the Rule of 5.
    * file: io.h, lines: 13-19
* The project uses move semantics to move data, instead of copying it, where possible.
    * file: worker_pool.h, lines: 18, 27
    * file: message_queue.h, line: 24
    * file: http_server.cpp, lines: 108, 202, 234, 424, 426
    * file: io.cpp, lines: 112, 124
* The project makes use of references in function declarations.
    * file: http_server.h, line: 16
    * file: http_server.cpp, lines: 49, 66, 94
* Class constructors utilize member initialization lists.
    * file: io.cpp, lines: 31, 112
    * file: message_queue.h, line: 40
    * file: http_server.cpp, lines: 234-237, 370-373, 444
* Classes encapsulate behavior.
    * file: message_queue.h, lines: 11-35
    * file: io.h, lines: 32-48
* Classes follow an appropriate inheritance hierarchy.
    * file: worker_pool.h, line: 37
    * file: worker_pool.cpp, line: 8
    * file: http_server.cpp, line: 80
* Derived class functions override virtual base class functions.
    * file: worker_pool.h, line: 42
    * file: worker_pool.cpp, line: 17
    * file: http_server.cpp, line: 89
* The project reads data from a file and process the data, or the program writes data to a file.
    * file: http_server.cpp, lines: 271-279
