#include <iostream>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <unordered_map>
#include <sstream>
#include <string>
#include <cerrno>
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <memory>
struct HTTPRequest { 
    std::string method, path, version; 
    std::unordered_map<std::string, std::string> headers;
};
struct HTTPResponse { int status_code; std::string body; };
class Middleware {
public:
    virtual bool handle(HTTPRequest& req, HTTPResponse& res) = 0;
    virtual ~Middleware() = default;
};

class LoggingMiddleware : public Middleware {
public:
    bool handle(HTTPRequest& req, HTTPResponse& res) override {
        std::cout << "[LOG] " << req.method << " " << req.path << std::endl;
        return true;
    }
};

class AuthMiddleware : public Middleware {
public:
    bool handle(HTTPRequest& req, HTTPResponse& res) override {
        if (req.headers.find("Authorization") == req.headers.end()) {
            res.status_code = 401;
            res.body = "Unauthorized";
            return false;
        }
        return true;
    }
};

class MiddlewarePipeline {
    std::vector<std::unique_ptr<Middleware>> middlewares;
public:
    void add(std::unique_ptr<Middleware> m) { middlewares.push_back(std::move(m)); }
    bool execute(HTTPRequest& req, HTTPResponse& res) {
        for (auto& m : middlewares) if (!m->handle(req, res)) return false;
        return true;
    }
};
struct Task { int client_fd; std::function<void()> work; };
class TaskQueue { std::queue<Task> tasks;
     std::mutex mtx;
    std::condition_variable cv;
    bool stop = false;void push(Task t) {
       { std::lock_guard<std::mutex> lock(mtx); tasks.push(t); }
        cv.notify_one();
     }
     bool pop(Task& t) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&] { return !tasks.empty() || stop; });
        if (stop && tasks.empty()) return false;
        t = tasks.front(); tasks.pop();
        return true; }
     void shutdown() {
      { std::lock_guard<std::mutex> lock(mtx); stop = true; }
       cv.notify_all();
    } };
class Router {
    using Handler = std::function<HTTPResponse(const HTTPRequest&)>;
    std::unordered_map<std::string, Handler> routes;
    std::string key(const std::string& m, const std::string& p) { return m + ":" + p; }
public:
    void add(const std::string& m, const std::string& p, Handler h) { routes[key(m, p)] = h; }
    HTTPResponse dispatch(const HTTPRequest& req) {
        auto it = routes.find(key(req.method, req.path));
        return (it != routes.end()) ? it->second(req) : HTTPResponse{404, "Not Found"};
    }
};

Router global_router;
MiddlewarePipeline global_pipeline;
std::unordered_map<int, std::string> client_buffers;
bool set_non_blocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return (flags != -1) && (fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1);
}

HTTPRequest parse_request(const std::string& raw) {
    HTTPRequest req;
    std::istringstream stream(raw);
    std::string line;
    if (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream ls(line);
        ls >> req.method >> req.path >> req.version;
    }
    while (std::getline(stream, line) && line != "\r" && !line.empty()) {
        if (line.back() == '\r') line.pop_back();
        auto pos = line.find(':');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string val = line.substr(pos + 1);
            if (!val.empty() && val[0] == ' ') val.erase(0, 1);
            req.headers[key] = val;
        }
    }
    return req;
}

void handle_client(int client_fd) {
    char buffer[4096];
    ssize_t bytes = recv(client_fd, buffer, sizeof(buffer), 0);
    if (bytes <= 0) {
        if (bytes == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
            close(client_fd);
            client_buffers.erase(client_fd);
        }
        return;
    }
    client_buffers[client_fd].append(buffer, bytes);
    if (client_buffers[client_fd].find("\r\n\r\n") == std::string::npos) return;

    HTTPRequest req = parse_request(client_buffers[client_fd]);
    HTTPResponse res; 

    // Pipeline Execution
    if (global_pipeline.execute(req, res)) {
        res = global_router.dispatch(req);
    }

    std::string status_text;
    switch(res.status_code) {
        case 200: status_text = "OK"; break;
        case 401: status_text = "Unauthorized"; break;
        case 404: status_text = "Not Found"; break;
        case 500: status_text = "Internal Server Error"; break;
        default: status_text = "Unknown";
    }

    std::string response = "HTTP/1.1 " + std::to_string(res.status_code) + " " + status_text + "\r\n" +
                           "Content-Length: " + std::to_string(res.body.size()) + "\r\n" +
                           "Content-Type: text/plain\r\n\r\n" + res.body;
    send(client_fd, response.c_str(), response.size(), 0);
    close(client_fd);
    client_buffers.erase(client_fd);
}

int main() {
    global_pipeline.add(std::make_unique<LoggingMiddleware>());
    global_router.add("GET", "/", [](const HTTPRequest& r) { return HTTPResponse{200, "Hello World"}; });
    return 0;
}