# C++ HTTP Server

A lightweight educational HTTP server built from scratch in C++17.

This project demonstrates backend and systems fundamentals: POSIX sockets, HTTP request parsing, response formatting, static file serving, routing, concurrency, graceful shutdown, and test coverage. It is production-inspired, but intentionally small and readable for learning.

## Features

- TCP server built with `socket`, `bind`, `listen`, `accept`, `recv`, and `send`
- HTTP request parsing for method, path, version, headers, and body
- HTTP response builder with status line, headers, `Content-Length`, `Connection: close`, and body
- Static file serving from `public/`
- MIME type detection for common file extensions
- JSON response helper for API endpoints
- Exact routes and simple route parameters
- Fixed-size configurable thread pool
- Bounded client queue with `503 Service Unavailable` backpressure
- Per-client receive timeout for slow or stuck clients
- Graceful `Ctrl+C` shutdown
- RAII socket wrapper for accepted client sockets
- Thread-safe terminal logging
- Unit tests and integration tests over real TCP

## Tech Stack

- C++17
- CMake
- POSIX sockets
- `std::thread`, `std::mutex`, `std::condition_variable`
- Framework-free C++ test runner

Note: the networking code currently targets macOS/Linux-style POSIX sockets. Windows support is listed as a future improvement.

## Routes

| Method | Path | Description |
| --- | --- | --- |
| `GET` | `/health` | Plain text health check |
| `GET` | `/api/status` | JSON server status |
| `POST` | `/api/echo` | Echoes the request body as plain text |
| `GET` | `/api/users/:id` | Demonstrates route parameters |
| `GET` | `/*` | Serves static files from `public/` |

## CLI Options

| Option | Default | Description |
| --- | ---: | --- |
| `--port PORT` | `8080` | Port to listen on |
| `--public DIRECTORY` | `public` | Static file directory |
| `--workers COUNT` | `4` | Number of worker threads |
| `--max-queue COUNT` | `64` | Maximum queued client connections |
| `--recv-timeout SECONDS` | `5` | Per-client socket receive timeout |

Invalid values print usage and exit. Ports must be `1` to `65535`; worker count, queue size, and receive timeout must be at least `1`.

## Project Structure

```text
cpp-http-server/
├── CMakeLists.txt
├── LICENSE
├── README.md
├── include/
│   ├── CommandLineOptions.hpp
│   ├── ErrorResponse.hpp
│   ├── HttpRequest.hpp
│   ├── HttpResponse.hpp
│   ├── Logger.hpp
│   ├── Router.hpp
│   ├── Server.hpp
│   ├── ServerConfig.hpp
│   ├── SocketHandle.hpp
│   └── StaticFileHandler.hpp
├── public/
│   ├── app.js
│   ├── index.html
│   └── style.css
├── src/
│   ├── CommandLineOptions.cpp
│   ├── ErrorResponse.cpp
│   ├── HttpRequest.cpp
│   ├── HttpResponse.cpp
│   ├── Logger.cpp
│   ├── Router.cpp
│   ├── Server.cpp
│   ├── StaticFileHandler.cpp
│   └── main.cpp
└── tests/
    └── test_main.cpp
```

## Architecture Overview

`main.cpp` parses command-line options into `ServerConfig`, installs the `SIGINT` handler, creates a `Server`, and calls `run()`.

`Server` owns the socket lifecycle: create the listening socket, bind, listen, accept connections, enqueue clients, and coordinate worker shutdown.

Accepted client sockets are wrapped in `SocketHandle`, a move-only RAII class that closes the file descriptor automatically when ownership ends.

Worker threads pop `ClientConnection` jobs from a bounded queue. Each worker reads a request, parses it into `HttpRequest`, routes it, serializes an `HttpResponse`, sends it, and lets `SocketHandle` close the socket.

Routing lives in `Router.cpp`. Static file handling lives in `StaticFileHandler.cpp`. Logging is centralized in `Logger`.

## Request Lifecycle

1. The main thread accepts a TCP connection.
2. The accepted file descriptor is wrapped in `SocketHandle`.
3. The connection is pushed into the bounded worker queue.
4. A worker thread pops the connection.
5. The worker reads bytes with `recv()` until headers are complete.
6. If `Content-Length` is present, the worker reads the request body.
7. `HttpRequest::parse()` extracts method, path, version, headers, and body.
8. `Router` chooses an application response or delegates to `StaticFileHandler`.
9. `HttpResponse::to_string()` builds raw HTTP text.
10. The worker sends the response with `send()`.
11. `SocketHandle` closes the client socket automatically.

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Run

```bash
./build/cpp_http_server
```

Run with custom settings:

```bash
./build/cpp_http_server --port 9090 --workers 2 --max-queue 16 --recv-timeout 3
```

Stop the server with `Ctrl+C`.

## Test

```bash
./build/cpp_http_server_tests
```

The test executable includes:

- unit tests for request parsing, response formatting, routing, static files, MIME types, CLI parsing, and `SocketHandle`
- integration tests that start the real server on port `18080` and send actual HTTP requests over TCP

## Example Requests

Start the server:

```bash
./build/cpp_http_server
```

Then run:

```bash
curl -i http://127.0.0.1:8080/health
curl -i http://127.0.0.1:8080/api/status
curl -i http://127.0.0.1:8080/api/users/123
curl -i -X POST http://127.0.0.1:8080/api/echo -d "hello"
curl -i http://127.0.0.1:8080/
curl -i http://127.0.0.1:8080/style.css
curl -i http://127.0.0.1:8080/missing.html
```

Expected API response:

```http
HTTP/1.1 200 OK
Content-Type: application/json; charset=utf-8
Content-Length: 43
Connection: close

{"status":"ok","message":"server running"}
```

## What This Project Demonstrates

- How HTTP is represented as plain text over TCP
- How a server accepts connections and reads request bytes
- Why TCP requires careful request reading instead of assuming one `recv()` call is enough
- How headers, `Content-Length`, and response bodies work
- How static file servers map URL paths to filesystem paths safely
- How MIME types affect browser behavior
- How a thread pool and worker queue handle concurrent clients
- How backpressure prevents unbounded queue growth
- How RAII prevents file descriptor leaks
- How integration tests can exercise the real networking stack

## What I Learned

- Built a TCP server from the socket API upward
- Parsed enough HTTP/1.1 to understand request lines, headers, bodies, and responses
- Used C++17 ownership patterns to manage sockets safely
- Added concurrency with a fixed-size thread pool and condition variable
- Added graceful shutdown and timeout handling around blocking socket calls
- Tested both isolated components and the running server over real TCP

## Limitations

This is an educational server, not a production-ready web server.

- No HTTP keep-alive
- No chunked transfer encoding
- No TLS/HTTPS
- No streaming for large static files
- Limited HTTP/1.1 edge case handling
- Basic routing only
- Basic terminal logging only
- POSIX-focused networking code

## Future Improvements

- HTTP keep-alive support
- Chunked transfer encoding
- More complete HTTP/1.1 edge case handling
- More complete routing
- File streaming for large files
- TLS/HTTPS
- Log levels
- Metrics
- Cross-platform Windows support

## License

MIT License. See [LICENSE](LICENSE).
