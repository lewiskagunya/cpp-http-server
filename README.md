# cpp-http-server 🚀

A lightweight, high-performance, asynchronous **HTTP Web Server** built entirely from scratch in C++ using core Linux system programming primitives.

This project implements a non-blocking network architecture capable of routing incoming requests, running middleware pipelines, and prepping for multi-threaded request dispatching.

---

## 🛠 Features

*   ⚡ **Asynchronous I/O:** Uses Linux `O_NONBLOCK` sockets to eliminate thread-blocking delays during network transmission.
*   🛤 **Dynamic Routing Engine:** Maps combination paths (e.g., `GET /`) to custom isolated handling functions using an efficient hash-map registry.
*   🛡 **Pluggable Middleware Pipeline:** Sequentially processes requests before routing. Includes built-in `LoggingMiddleware` and an `AuthMiddleware` bouncer.
*   🧵 **Thread-Safe Task Queue:** Equipped with a synchronized `TaskQueue` using mutexes and condition variables, laying the foundation for a thread-pool architecture.
*   ⚙ **Manual HTTP Parsing:** Zero external dependencies; parses raw TCP stream bytes into structured HTTP requests and compliant response strings.

---

## 🏗 Architecture Overview

When a client transmits data to the server, the connection traverses the following modular system:

```text
 [Client Browser] 
        │
        ▼
 [Non-blocking Socket (recv)] ──► (Buffers raw incoming stream data)
        │
        ▼
 [Middleware Pipeline] ───────► 1. LoggingMiddleware (Prints action to terminal)
        │                      2. AuthMiddleware    (Verifies security headers)
        ▼
 [Global Router Mapping] ─────► Dispatches request to target route handler
        │
        ▼
 [Client Response (send)] ────► Returns HTTP response (e.g., 200 OK, 401 Unauthorized)
```

---

## 🚀 Getting Started

### Prerequisites
*   A Linux environment (or WSL/Windows Subsystem for Linux)
*   A modern C++ compiler supporting **C++17** or higher (`g++` or `clang++`)

### Compilation
Compile the source code using the following shell command:
```bash
g++ -std=c++17 main.cpp -o http_server -lpthread
```

### Execution
Run the compiled web server binary:
```bash
./http_server
```

---

## 📝 Code Examples & Usage

### Adding Custom Endpoints
You can map specific web endpoints directly inside your main initialization wrapper:

```cpp
int main() {
    // 1. Add request checkpoints to the global pipeline
    global_pipeline.add(std::make_unique<LoggingMiddleware>());

    // 2. Define standard endpoint routes 
    global_router.add("GET", "/", [](const HTTPRequest& r) { 
        return HTTPResponse{200, "Hello World"}; 
    });

    global_router.add("GET", "/about", [](const HTTPRequest& r) { 
        return HTTPResponse{200, "About This Server Engine"}; 
    });

    return 0;
}
```

---

## 📈 Roadmap / Future Enhancements
*   [ ] Complete the `epoll_wait` asynchronous event loop integration in `main()`.
*   [ ] Spawn dedicated worker threads to pull payloads from the `TaskQueue`.
*   [ ] Implement a `std::mutex` around the global `client_buffers` to ensure full multi-threaded safety.

---

## 📜 License
This project is open-source and available under the [MIT License](LICENSE).
