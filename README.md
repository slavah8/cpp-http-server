# C++ HTTP Server

A beginner-friendly portfolio project for learning how a web server works under the hood in modern C++.

This repository starts small on purpose. Part 10 keeps the same sequential request loop and static file serving, but improves `HttpResponse` so response headers are stored and built more cleanly.

## Part 10 Goals

- Open a TCP socket
- Bind to `localhost:8080`
- Listen for one incoming connection
- Accept one client at a time in a loop
- Read the incoming request
- Parse the first request line into method, path, and version
- Parse request headers into a headers map
- Send back a valid HTTP response
- Store response headers in a cleaner response object
- Serve static files from `public/`
- Return a different response for valid, invalid, missing, and unsupported requests
- Keep the listening socket open until the program is stopped
- Close each client socket cleanly after its response is sent

## Folder Structure

```text
cpp-http-server/
├── CMakeLists.txt
├── README.md
├── include/
│   ├── HttpRequest.hpp
│   ├── HttpResponse.hpp
│   ├── Router.hpp
│   ├── Server.hpp
│   └── StaticFileHandler.hpp
├── public/
│   ├── app.js
│   ├── index.html
│   └── style.css
└── src/
    ├── HttpRequest.cpp
    ├── HttpResponse.cpp
    ├── Router.cpp
    ├── Server.cpp
    ├── StaticFileHandler.cpp
    └── main.cpp
```

## What Each File Does

### `CMakeLists.txt`

Defines how the project is built:

- names the project
- sets the C++ standard to C++17
- tells CMake to compile all `.cpp` files
- tells the compiler where to find header files in `include/`
- creates the executable named `cpp_http_server`

### `include/HttpRequest.hpp`

Declares the `HttpRequest` type. It stores:

- `method`
- `path`
- `version`
- `headers`
- `valid`

It also declares:

- a static `parse(...)` function
- `has_header(...)`
- `get_header(...)`

### `src/HttpRequest.cpp`

Implements the request parser. It now:

- parses the request line
- reads header lines until the blank line
- stores headers in a map
- skips malformed header lines without failing the whole request

Example request line:

```text
GET /health HTTP/1.1
```

### `src/main.cpp`

Contains only the program entry point. In Part 7, it:

- prints the startup message
- creates a `Server` object
- calls `server.run()`

### `include/Server.hpp`

Declares the `Server` class. This file tells the compiler what the class looks like:

- which public methods other files can call
- which private helper methods it uses internally
- which member variables it stores

### `src/Server.cpp`

Defines the `Server` class behavior. This file contains the real socket flow:

- create socket
- bind
- listen once
- enter an accept loop
- handle one client at a time
- close only the client socket after each request
- keep the server socket open until the program is stopped

### `include/HttpResponse.hpp`

Declares the `HttpResponse` class, which now stores:

- the HTTP version
- the status code
- the reason phrase
- a response headers map
- the response body

### `src/HttpResponse.cpp`

Builds the final HTTP response string and provides helpers for:

- setting headers
- setting the body
- turning the response object into raw HTTP text

### `include/StaticFileHandler.hpp`

Declares the helper responsible for:

- mapping URL paths to files under `public/`
- checking whether a path is safe
- reading files from disk
- choosing a MIME type

### `src/StaticFileHandler.cpp`

Implements the static file logic:

- `/` maps to `public/index.html`
- `/style.css` maps to `public/style.css`
- `/app.js` maps to `public/app.js`
- missing files return `404 Not Found`
- unsafe paths such as `/../secret.txt` are blocked

### `include/Router.hpp`

Declares the routing helper function.

### `src/Router.cpp`

Contains the simple routing logic:

- invalid request returns `400 Bad Request`
- unsupported method such as `POST` returns `405 Method Not Allowed`
- `GET /health` returns `status: ok`
- other `GET` paths go through the static file handler

### `public/`

This folder now contains starter static files that the server can return to a browser:

- `index.html`
- `style.css`
- `app.js`

## Build Instructions

### macOS

Make sure you have these installed:

- Xcode Command Line Tools
- CMake

Install the Apple command line tools if needed:

```bash
xcode-select --install
```

Install CMake with Homebrew if needed:

```bash
brew install cmake
```

Build the project:

```bash
cd cpp-http-server
cmake -S . -B build
cmake --build build
```

Run the program:

```bash
./build/cpp_http_server
```

When it starts successfully, it keeps running and waits for client connections until you stop it with `Ctrl+C`.

### Windows

Recommended tools:

- Visual Studio with the "Desktop development with C++" workload
- CMake

Build the project from a Developer Command Prompt or PowerShell:

```powershell
cd cpp-http-server
cmake -S . -B build
cmake --build build
```

Run the program:

```powershell
.\build\Debug\cpp_http_server.exe
```

Note: On Windows, the output path may vary depending on the generator you use.

## Testing The Server

### Option 1: Test from a browser

1. Start the server:

```bash
./build/cpp_http_server
```

2. Open the home route in your browser:

```text
http://127.0.0.1:8080
```

You should see the contents of `public/index.html`.

3. Keep the server running, then open the health route:

```text
http://127.0.0.1:8080/health
```

You should see a plain text response like `status: ok`.

4. You can also open these static assets directly:

```text
http://127.0.0.1:8080/style.css
http://127.0.0.1:8080/app.js
```

### Option 2: Test from the terminal with `curl`

Start the server in one terminal:

```bash
./build/cpp_http_server
```

Then open a second terminal and run:

```bash
curl -i http://127.0.0.1:8080
```

To test the health route:

```bash
curl -i http://127.0.0.1:8080/health
```

To test a custom header:

```bash
curl -i -H "X-Test: hello" http://127.0.0.1:8080/health
```

To test a custom user agent:

```bash
curl -i -A "MyTestClient/1.0" http://127.0.0.1:8080
```

To test CSS:

```bash
curl -i http://127.0.0.1:8080/style.css
```

To test JavaScript:

```bash
curl -i http://127.0.0.1:8080/app.js
```

To test a missing route:

```bash
curl -i http://127.0.0.1:8080/missing.html
```

To test an unsupported method:

```bash
curl -i -X POST http://127.0.0.1:8080
```

To test the path-safety check:

```bash
curl -i "http://127.0.0.1:8080/../secret.txt"
```

## Expected Output

```text
Starting C++ HTTP Server...
Listening on http://127.0.0.1:8080
Server is running. Press Ctrl+C to stop it.
Waiting for the next client connection...
```

After a client connects, you should also see a line like:

```text
Client connected from 127.0.0.1:54321
Received 78 bytes:
GET / HTTP/1.1
Host: 127.0.0.1:8080
...
Parsed request line: method=GET, path=/, version=HTTP/1.1
Parsed headers count: 3
Host header: 127.0.0.1:8080
User-Agent header: curl/...
Sent HTTP response to client.
Closed client connection.
```

## Core Networking Concepts

### Socket

A socket is an operating system object that lets a program communicate over a network. For this project, you can think of it as the server's communication endpoint.

### Bind

`bind(...)` attaches the socket to a specific IP address and port. In this project, that means:

- IP address: `127.0.0.1`
- Port: `8080`

That combination tells the OS exactly where your server should listen.

### Listen

`listen(...)` switches the bound socket into passive server mode. After this call, the socket is ready to wait for incoming connection attempts from clients.

### Accept

`accept(...)` waits until a client connects. When a connection arrives, it returns a new socket just for that client. The original server socket keeps its job as the listening socket.

## Accept Loop

Part 7 adds a simple server loop:

1. accept one client
2. receive and parse that client request
3. build and send the response
4. close that client socket
5. go back and wait for the next client

This means the setup work (`socket`, `bind`, and `listen`) happens once, while the request-handling work repeats for every new connection.

## Why The Server Socket Stays Open

The listening socket exists so the server can keep accepting new clients on port `8080`. If that socket were closed after each request, the whole server would stop listening and no new clients could connect.

The client socket is different. It belongs to one specific connected client, so it should be closed after that one request/response cycle finishes.

## Why This Is Sequential

This server is still single-threaded. It handles one client from start to finish before moving on to the next one.

That means:

- one client connects
- the server processes that request
- the client socket closes
- only then does the server accept another client

So it can handle multiple clients over time, but not at the same exact moment.

## What `Ctrl+C` Does

When you press `Ctrl+C` in the terminal, the operating system interrupts the running program and stops it. For now, that is the normal way to shut down the server manually.

### recv

`recv(...)` reads bytes from a connected socket. In this project, it tries to copy incoming data from the client into a local character buffer in memory.

The return value matters:

- `-1` means an error happened
- `0` means the client closed the connection without sending more data
- a positive number means that many bytes were received

### Buffer

A buffer is just a block of memory used to temporarily store incoming data. Here, the buffer has room for 1024 characters.

### Bytes Received

`bytes_received` tells you how much of the buffer is actually filled with real data. The buffer might be larger than the message, so you should only trust the part that `recv(...)` says was written.

### Null Termination

C-style text uses a special ending character called the null terminator, written as `'\0'`. Since `recv(...)` gives you raw bytes and does not automatically add that terminator, this project reserves one extra byte and places `'\0'` manually after the received data so the buffer can be printed safely as text.

### send

`send(...)` writes bytes from your program out to the connected client socket. In Part 5, the program uses it to send a complete HTTP response string back to the browser or `curl`.

Like `recv(...)`, the return value matters:

- `-1` means an error happened
- a positive number means that many bytes were sent

One important detail is that one call to `send(...)` does not always send the entire response. That is why this project uses a small helper that keeps calling `send(...)` until all response bytes are written.

## `.hpp` vs `.cpp`

In this project:

- `.hpp` files contain declarations
- `.cpp` files contain definitions

A declaration tells C++ that something exists. A definition contains the real implementation.

For example:

- `include/Server.hpp` says that `Server` has a constructor and a `run()` method
- `src/Server.cpp` contains the code that actually makes those methods work

## Class Declarations vs Definitions

A class declaration describes the shape of a class:

- method names
- method visibility
- data members

A class definition provides the code for the methods.

This split helps keep projects organized because other files can include the header without needing to see every implementation detail.

## Public Methods, Private Helpers, and Constructors

### Constructor

A constructor runs when an object is created. In this project, `Server server(8080);` calls the `Server` constructor and stores the port number inside the object.

### Public Methods

Public methods are the part of the class other files are allowed to call. Here, `run()` is public because `main.cpp` needs to start the server.

### Private Helpers

Private helpers are internal tools used only inside the class. In `Server`, methods like `create_socket()`, `bind_socket()`, and `receive_request()` break the work into readable pieces without exposing those details to `main.cpp`.

## HTTP Response Structure

A valid HTTP response has these parts:

1. Status line
2. Headers
3. Blank line
4. Body

Example:

```text
HTTP/1.1 200 OK
Content-Type: text/plain; charset=utf-8
Content-Length: 11

status: ok
```

### Content-Length

`Content-Length` tells the client exactly how many bytes are in the response body. This matters because the browser or `curl` needs to know where the body ends.

For example, the body:

```text
status: ok
```

contains 11 bytes, so the header must say:

```text
Content-Length: 11
```

## Request Line Parsing

In HTTP, the first request line usually looks like this:

```text
GET /health HTTP/1.1
```

This project now splits that line into three parts:

- method: `GET`
- path: `/health`
- version: `HTTP/1.1`

If the first line is missing, or if it does not contain exactly those three parts, the request is marked invalid and the server returns `400 Bad Request`.

### `std::istringstream`

`std::istringstream` lets you treat a string like an input stream. That means you can extract words from a string using `>>`, just like reading from `std::cin`.

In this project it is used twice:

- once to read the first line from the full request text
- once to split that first line into `method`, `path`, and `version`

That keeps the parsing small and readable without writing manual loops over characters.

## HTTP Headers

HTTP headers are the extra lines after the request line and before the blank line. They carry metadata about the request.

Examples:

```text
Host: 127.0.0.1:8080
User-Agent: curl/8.12.0
Accept: */*
```

Real servers use headers for many things, such as host routing, content negotiation, authentication, cookies, caching, and more.

## Request Headers vs Response Headers

Request headers come from the client and describe the request. Examples are:

- `Host`
- `User-Agent`
- `Accept`

Response headers go back from the server to the client and describe the response. Examples are:

- `Content-Type`
- `Content-Length`
- `Connection`

So request headers help the server understand what the client wants, while response headers help the client understand what the server sent back.

## `std::unordered_map`

`std::unordered_map<std::string, std::string>` stores key-value pairs. In this project:

- the key is the header name, such as `Host`
- the value is the header value, such as `127.0.0.1:8080`

It is a convenient way to look up headers by name without scanning a full list every time. In Part 10, the same idea is also used for response headers.

## Splitting At `:`

Each header line is split at the first colon. For example:

```text
User-Agent: curl/8.12.0
```

becomes:

- header name: `User-Agent`
- header value: `curl/8.12.0`

Using the first colon matters because the value part may sometimes contain additional colons later.

## Trimming Whitespace

After splitting, the parser trims spaces and tabs around both the header name and value. That way:

```text
Host:   127.0.0.1:8080
```

is stored cleanly as:

- `Host`
- `127.0.0.1:8080`

### Why `HttpRequest` Is Cleaner

Using a parsed `HttpRequest` is cleaner than raw `starts_with(...)` checks because:

- parsing happens once in one place
- the router receives named fields instead of raw text
- adding new rules later becomes easier
- the code more closely matches how HTTP is actually structured
- headers now live alongside method, path, and version in one request object

## URL Paths To Files

The static file handler maps URL paths to files inside `public/`:

- `/` -> `public/index.html`
- `/style.css` -> `public/style.css`
- `/app.js` -> `public/app.js`

For normal file requests, the server adds the URL path after `public`. The only special case is `/`, which becomes `public/index.html`.

## `std::ifstream`

`std::ifstream` is the standard C++ type for reading from files. In this project, it opens a file from disk and streams its contents into a string so the server can send that file back in the response body.

If the file cannot be opened, the static file handler returns `404 Not Found`.

## MIME Types

A MIME type tells the browser what kind of content it received. This project now returns:

- `.html` -> `text/html; charset=utf-8`
- `.css` -> `text/css; charset=utf-8`
- `.js` -> `application/javascript; charset=utf-8`
- `.txt` -> `text/plain; charset=utf-8`
- unknown -> `application/octet-stream`

These headers help the browser decide whether to render a page, apply styles, run JavaScript, or treat the content as a generic file.

## Why A Headers Map Is Cleaner

Storing response headers in a map is cleaner than hard-coding them directly into one constructor because:

- headers and body are separate concepts
- different routes can add different headers
- `HttpResponse` becomes easier to extend later
- the code matches real HTTP structure more closely

For example, the router can now create a response and then set:

- `Content-Type`
- any future custom header

without changing the constructor every time.

## How `to_string()` Builds Raw HTTP Text

`HttpResponse::to_string()` turns the response object into the exact text sent over the socket.

It builds these parts in order:

1. the status line, such as `HTTP/1.1 200 OK`
2. each header line from the headers map
3. automatic headers for `Content-Length` and `Connection: close`
4. a blank line
5. the response body

That final string is what `send(...)` writes to the client socket.

## Path-Safety Check

The static file handler blocks paths containing `..`, such as `/../secret.txt`. That matters because `..` means "go up one folder", and a web server should not let a URL escape the `public/` directory.

Right now the safety check is intentionally small:

- the path must start with `/`
- the path cannot contain `..`

If a path fails that check, the server returns `404 Not Found`.

### Simple Routing Logic

This project now routes based on parsed request fields:

- invalid request -> `400 Bad Request`
- non-`GET` method -> `405 Method Not Allowed`
- `GET /health` -> health response
- normal `GET` file paths -> static file handler
- missing or unsafe files -> `404 Not Found`

## Why A Browser Sends `GET / HTTP/1.1`

When you enter `http://127.0.0.1:8080` in a browser, the browser opens a TCP connection first. After that, it speaks the HTTP protocol by sending text like:

```text
GET / HTTP/1.1
Host: 127.0.0.1:8080
```

This is the browser asking your server for the `/` path using HTTP version `1.1`. The `Host` line tells the server which host and port the browser is trying to reach. The project now parses those header lines and stores them in `HttpRequest::headers`.

## Important C++ Refactor Syntax

### `explicit Server(int port);`

`explicit` prevents C++ from doing unwanted automatic conversions when creating a `Server`. It is a good habit for single-argument constructors.

### `ClassName::methodName(...)`

In `.cpp` files, methods are defined using the class name and scope operator, like:

```cpp
bool Server::run() {
    // ...
}
```

This tells C++ that `run()` belongs to the `Server` class.

### Member Initializer List

The `Server` constructor uses syntax like this:

```cpp
Server::Server(int port)
    : port_(port),
      backlog_(1),
      server_socket_(-1),
      client_socket_(-1),
      client_port_(0) {
}
```

This is called an initializer list. It initializes member variables before the constructor body runs.

### Private Member Naming

Names like `port_` and `server_socket_` end with `_` to make it clear they are member variables owned by the class.

## What To Study Before Part 11

Before moving on, study these topics:

1. How request logging helps when debugging a server
2. Basic timestamp formatting in C++
3. The difference between application logs and HTTP responses
4. What information is useful to log for each request
5. How to keep terminal output readable as the server grows

In Part 11, the next logical step will be adding request logging with timestamps and cleaner terminal output.
