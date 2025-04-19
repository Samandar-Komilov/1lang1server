# C-Server

This is a part of [1lang1server](https://github.com/Samandar-Komilov/1lang1server/) repository which is aimed to practice pretty much every language by building simple web servers, step by step. The project is divided into chapters, each of which focuses on a specific feature or technique. The chapters are ordered by difficulty level, starting with the basics and gradually increasing in complexity.
> Along the way, we'll explore the related concepts and used function definitions. So, it's not simply writing code or copying-pasting, but analyzing what is happening at each moment.

---

The overall plan will be as follows:


### 0️⃣ Basic TCP Server
**Goal**  
Create a foundation that listens for and accepts TCP connections, setting up the socket infrastructure for later HTTP functionality. 

**Implementation**  
Use a `Server` struct to store socket configuration. Implement `server_constructor` to initialize the struct, create a socket with `socket()`, bind it to a port using `bind()` and start listening with `listen()`. Don't forget to set the `S0_REUSEADDR` option to avoid "address already in use" errors.
Create a `launch()` function that runs an infinite loop, calling `accept()` to handle incoming connections and logging each connection.


### 1️⃣ Basic HTTP Server
**Goal**  
Extend the TCP server to parse HTTP requests and respond with a static HTTP response, introducing HTTP protocol handling.

**Implementation**  
Define an `HTTPRequest` struct to store parsed request data:
- `method`, `path` and `version`
- `Headers`
- `header_count`
- `Body`
- `body_length`

Implement `http_request_parser` to read the request from a client socket, tokenize the request line, and parse headers until an empty line (`\r\n`).
Create an `HTTPResponse` struct for responses.

**Best Practices**  
- Allocate memory dynamically for `HTTPRequest` fields and free them with `http_request_free` to prevent leaks. 
- Test with curl http://localhost:8080 to verify the response. 
- Use Valgrind to check for memory leaks in `http_request_parse`.


### 2️⃣ Static File Serving
**Goal**  
Serve static files (e.g., HTML, CSS, images) from a directory, adding a key Nginx feature.

**Implementation**  
Introduce an `HTTPServer` struct to manage HTTP-specific state: a reference to the `Server` struct, a `static_dir` path, and a `handler` function pointer for processing requests. The constructor should also initialize `static_dir`.

**Best Practices**  
- Allocate a buffer for file contents dynamically based on file size.
- Test with a browser to verify files (e.g., index.html, images) are served correctly.
- Log file access errors (e.g., “File not found”) to debug issues.
- Hardcode common `Content-Type` values (e.g., `text/html` for .html, `image/png` for .png) in a lookup table, deferring MIME type detection libraries.
- Check for path traversal attacks (e.g., `../../etc/passwd`) by validating the resolved path stays within `static_dir`.

### 3️⃣ Reverse Proxy
**Goal**  
Forward HTTP requests to a backend server, let's say FastAPI app on Uvicorn, enabling the server to act as a reverse proxy. Relay the backend’s response back to the client.

**Implementation**  
Extend `HTTPServer` to include a `proxy_backends` array, which includes strings like `“localhost:8000”` and `backend_count`. Implement `proxy_request` to:
- Create a new socket to connect to the backend using `socket()` and `connect()`.
- Reconstruct the client’s HTTP request (method, path, headers) and send it to the backend.
- Read the backend’s response and forward it to the client.

Update the handler function in `HTTPServer` to decide whether to serve a static file or proxy based on the request path (e.g., proxy `/api/*` to the backend, serve `/static/*` from disk).

**Best Practices**  
- Handle connection failures (e.g., backend down) by returning a `502 Bad Gateway` response.
- Test with a Python FastAPI app running on `localhost:8000` and `curl` to verify proxying.
- Use `strace` to debug socket issues between the server and backend.

### 4️⃣ Load Balancing
**Goal**  
Distribute requests across multiple backends to improve scalability, adding load balancing to the reverse proxy. Support multiple backend servers (e.g., `localhost:8000`, `localhost:8001`). Implement a **round-robin algorithm** to distribute requests evenly.

**Implementation**  
Define a `Backend` struct with host (e.g., `“localhost”`) and port (e.g., `8000`). Store backends in `HTTPServer`’s `proxy_backends` as an array of strings, parsed into `Backend` structs at runtime. Implement `choose_backend` to select the next backend in a round-robin fashion, tracking the current index with a static variable. Modify `proxy_request` to use the selected backend’s host and port. Update `handler` to call `choose_backend` for proxied requests.

**Best Practices**  
- Log which backend handles each request to verify distribution.
- Test with multiple FastAPI instances (e.g., on ports `8000` and `8001`) and `wrk` to simulate load.
- Assume backends are always available, adding health checks in a future iteration if needed.


### 5️⃣ Config File (.ini)
**Goal**  
Add a configuration file to make the server flexible, allowing users to specify the port, static directory, and backends without recompiling. Parse a simple `.ini` configuration file to set server parameters. Use the parsed values to initialize the `Server` and `HTTPServer` structs.

**Implementation**  
Define a config file format like:
```ini
port=8080
static_dir=./static
backend=localhost:8000
backend=localhost:8001
```
Implement `parse_config` to read the file line by line, extracting key-value pairs (e.g., `port`, `static_dir`, `backend`). Then store parsed backends in a dynamic array for `HTTPServer`. Update `main` to call `parse_config` and pass the results to `server_constructor` and `http_server_constructor`.

**Best Practices**  
- Validate config values (e.g., port range, valid directory) to prevent crashes.
- Provide default values (e.g., port `8080`) if the config file is missing.
- Test with different config files to ensure flexibility.


### 6️⃣ Concurrency (Event-based)
**Goal**  
Enable the server to handle multiple clients concurrently using an event-based model, improving performance and scalability.
- Replace the single-threaded `accept()` loop with an event-driven model to handle multiple connections simultaneously. That way, process client requests (parsing, static files, proxying) without blocking other clients.

**Implementation**  
Use `select()` (then `epoll()` later on) to monitor multiple sockets for events.
Modify `launch()` to:
- Initialize a `fd_set` for `select`.
- Accept new connections and add client sockets to the set.
- Read and process requests from ready sockets, calling handler for HTTP logic.
- Close sockets after processing to free resources.

Maintain a dynamic array or list of active client sockets in HTTPServer to track state.

**Best Practices**  
- Limit the number of concurrent connections (e.g., `1024`) to avoid `select`’s file descriptor limit.
- Test with `wrk` to measure throughput under load (e.g., 100 concurrent clients).
- Use `Valgrind` to ensure no memory leaks in the event loop.
- Stick to a single thread to avoid threading complexity, mimicking Nginx’s event-driven model.


---

### 🧩 Data Structures
- **Dynamic Arrays** for simplicity
- Maybe I'll try **LinkedLists** with **Binary Search Trees** later on if needed


### 🧰 Toolkit

- [Valgrind](https://valgrind.org/docs/manual/quick-start.html) for memory leak checks
- **GDB** debugger to debug runtime issues
- [Check](https://libcheck.github.io/check/) framework for Unit Testing
- [wrk](https://github.com/wg/wrk) for modern benchmarking
- Custom logging and tracing
