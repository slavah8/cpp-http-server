#include "Server.hpp"

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "Logger.hpp"
#include "Router.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cctype>
#include <cstddef>
#include <cstring>
#include <exception>
#include <string>
#include <utility>
#include <cerrno>

namespace {

bool parse_content_length(
    const std::string& text,
    std::size_t max_request_size,
    std::size_t& content_length
) {
    if (text.empty()) {
        return false;
    }

    std::size_t value = 0;

    for (char character : text) {
        if (!std::isdigit(static_cast<unsigned char>(character))) {
            return false;
        }

        const std::size_t digit = static_cast<std::size_t>(character - '0');
        if (value > (max_request_size - digit) / 10) {
            return false;
        }

        value = value * 10 + digit;
    }

    content_length = value;
    return true;
}

bool is_receive_timeout_error() {
    return errno == EAGAIN || errno == EWOULDBLOCK;
}

}

Server::Server(ServerConfig config)
    : config_(std::move(config)),
      server_socket_(-1),
      shutdown_requested_(nullptr),
      shutting_down_(false) {
}

Server::~Server() {
    stop_worker_threads();
    close_socket(server_socket_);
}

bool Server::run() {
    bool run_succeeded = true;

    if (!create_socket()) {
        return false;
    }

    if (!bind_socket()) {
        return false;
    }

    if (!listen_for_connections()) {
        return false;
    }

    if (!start_worker_threads()) {
        return false;
    }

    Logger::info("Server started on http://" + config_.host + ":" + std::to_string(config_.port));
    Logger::info("Serving static files from: " + config_.public_directory);
    Logger::info("Worker threads started: " + std::to_string(worker_threads_.size()));
    Logger::info("Max queued clients: " + std::to_string(config_.max_queued_clients));
    Logger::info("Receive timeout: " + std::to_string(config_.receive_timeout_seconds) + " seconds");
    Logger::info("Server is running. Press Ctrl+C to stop it.");

    while (!should_stop()) {
        Logger::info("Waiting for the next client connection...");

        sockaddr_in client_address{};
        socklen_t client_address_size = sizeof(client_address);

        int accepted_socket = accept(
            server_socket_,
            reinterpret_cast<sockaddr*>(&client_address),
            &client_address_size
        );

        if (accepted_socket == -1) {
            if (should_stop()) {
                break;
            }

            if (errno == EINTR) {
                if (should_stop()) {
                    break;
                }

                continue;
            }
            Logger::error("Failed to accept client: " + std::string(std::strerror(errno)));
            run_succeeded = false;
            break;
        }

        char client_ip_buffer[INET_ADDRSTRLEN] = {0};
        const char* converted_ip = inet_ntop(
            AF_INET,
            &client_address.sin_addr,
            client_ip_buffer,
            INET_ADDRSTRLEN
        );

        const std::string client_ip = converted_ip != nullptr
            ? client_ip_buffer
            : "unknown";
        const int client_port = ntohs(client_address.sin_port);

        log_client_address(client_ip, client_port);

        ClientConnection connection{SocketHandle(accepted_socket), client_ip, client_port};

        if (!enqueue_client(std::move(connection))) {
            Logger::error("Rejected client connection from " + client_ip
                + ":" + std::to_string(client_port) + ".");
        }
    }

    if (should_stop()) {
        Logger::info("Shutdown requested. Stopping server...");
    }

    stop_worker_threads();

    if (should_stop()) {
        Logger::info("Worker threads stopped cleanly.");
    }

    return run_succeeded;
}

bool Server::start_worker_threads() {
    {
        std::lock_guard<std::mutex> lock(client_queue_mutex_);
        shutting_down_ = false;
    }

    try {
        for (std::size_t i = 0; i < config_.worker_count; ++i) {
            worker_threads_.emplace_back(&Server::worker_loop, this);
        }
    } catch (const std::exception& exception) {
        Logger::error("Failed to start worker thread: " + std::string(exception.what()));
        stop_worker_threads();
        return false;
    }

    return true;
}

void Server::stop_worker_threads() {
    {
        std::lock_guard<std::mutex> lock(client_queue_mutex_);
        shutting_down_ = true;
    }

    client_available_.notify_all();

    for (std::thread& worker : worker_threads_) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    worker_threads_.clear();
}

void Server::worker_loop() {
    while (true) {
        ClientConnection connection;

        {
            std::unique_lock<std::mutex> lock(client_queue_mutex_);
            client_available_.wait(lock, [this]() {
                return shutting_down_ || !client_queue_.empty();
            });

            if (shutting_down_ && client_queue_.empty()) {
                return;
            }

            connection = std::move(client_queue_.front());
            client_queue_.pop();
        }

        handle_client(
            std::move(connection.client_socket),
            connection.client_ip,
            connection.client_port
        );
    }
}

bool Server::enqueue_client(ClientConnection connection) {
    bool queue_is_full = false;

    try {
        {
            std::lock_guard<std::mutex> lock(client_queue_mutex_);

            if (client_queue_.size() >= config_.max_queued_clients) {
                queue_is_full = true;
            } else {
                client_queue_.push(std::move(connection));
            }
        }
    } catch (const std::exception& exception) {
        Logger::error("Failed to add client to queue: " + std::string(exception.what()));
        send_service_unavailable(connection.client_socket.get());
        return false;
    }

    if (queue_is_full) {
        Logger::error("Client queue is full. Sending 503 Service Unavailable.");
        send_service_unavailable(connection.client_socket.get());
        return false;
    }

    client_available_.notify_one();
    return true;
}

void Server::handle_client(SocketHandle client_socket, std::string client_ip, int client_port) const {
    std::string request_text;
    HttpRequest request{"", "", "", {}, "", false};
    std::string request_method_for_log;
    std::string request_path_for_log;

    const int client_fd = client_socket.get();

    if (!set_receive_timeout(client_fd)) {
        return;
    }

    if (!receive_request(
        client_fd,
        request_text,
        request,
        request_method_for_log,
        request_path_for_log
    )) {
        return;
    }

    if (!request_text.empty()) {
        send_response(
            client_fd,
            request,
            request_method_for_log,
            request_path_for_log
        );
    }

    Logger::info("Closed client connection from " + client_ip
        + ":" + std::to_string(client_port) + ".");
}

bool Server::set_receive_timeout(int client_socket) const {
    timeval timeout {};
    timeout.tv_sec = config_.receive_timeout_seconds;
    timeout.tv_usec = 0;

    if (setsockopt(
        client_socket,
        SOL_SOCKET,
        SO_RCVTIMEO,
        &timeout,
        sizeof(timeout)
    ) == -1) {
        Logger::error("Failed to set client receive timeout: "
            + std::string(std::strerror(errno)));
        return false;
    }

    return true;
}

bool Server::receive_request(
    int client_socket,
    std::string& request_text,
    HttpRequest& request,
    std::string& request_method_for_log,
    std::string& request_path_for_log
) const {
    const std::size_t chunk_size = 1024;
    const std::size_t max_request_size = 8192;
    const std::string header_terminator = "\r\n\r\n";

    char buffer[chunk_size] = {0};
    const auto mark_invalid_request = [&request, &request_method_for_log, &request_path_for_log]() {
        request = HttpRequest{"", "", "", {}, "", false};
        request_method_for_log = "INVALID";
        request_path_for_log = "(unknown)";
    };

    bool headers_complete = false;
    bool max_size_exceeded = false;

    while (request_text.find(header_terminator) == std::string::npos) {
        ssize_t bytes_received = recv(client_socket, buffer, chunk_size, 0);

        if (bytes_received == -1) {
            if (is_receive_timeout_error()) {
                Logger::error("Receive timed out while waiting for complete HTTP headers.");
                return false;
            }

            Logger::error("Failed to receive data: " + std::string(std::strerror(errno)));
            return false;
        }

        if (bytes_received == 0) {
            if (request_text.empty()) {
                Logger::info("Client connected but sent no data.");
            } else {
                Logger::error("Client closed connection before sending complete headers.");
            }
            break;
        }

        request_text.append(buffer, static_cast<std::size_t>(bytes_received));

        if (request_text.size() > max_request_size) {
            max_size_exceeded = true;
            break;
        }
    }

    headers_complete = request_text.find(header_terminator) != std::string::npos;

    Logger::info("Request received (" + std::to_string(request_text.size()) + " total bytes).");
    Logger::info(headers_complete ? "HTTP headers are complete." : "HTTP headers are incomplete.");

    if (max_size_exceeded) {
        Logger::error("Request exceeded maximum size of "
            + std::to_string(max_request_size) + " bytes.");
        mark_invalid_request();
        return true;
    }

    if (request_text.empty()) {
        request = HttpRequest{"", "", "", {}, "", false};
        return true;
    }

    if (!headers_complete) {
        mark_invalid_request();
        return true;
    }

    request = HttpRequest::parse(request_text);
    request_method_for_log = request.method.empty() ? "INVALID" : request.method;
    request_path_for_log = request.path.empty() ? "(unknown)" : request.path;

    if (!request.valid) {
        Logger::error("Request line was missing or malformed.");
        return true;
    }

    std::size_t expected_body_length = 0;

    if (request.has_header("Content-Length")) {
        if (!parse_content_length(
            request.get_header("Content-Length"),
            max_request_size,
            expected_body_length
        )) {
            Logger::error("Content-Length header was invalid.");
            mark_invalid_request();
            return true;
        }
    }

    Logger::info("Expected body length: " + std::to_string(expected_body_length) + " bytes.");

    const std::size_t body_start = request_text.find(header_terminator) + header_terminator.size();

    if (body_start + expected_body_length > max_request_size) {
        Logger::error("Request body would exceed maximum request size of "
            + std::to_string(max_request_size) + " bytes.");
        mark_invalid_request();
        return true;
    }

    std::size_t received_body_length = request_text.size() - body_start;

    while (received_body_length < expected_body_length) {
        ssize_t bytes_received = recv(client_socket, buffer, chunk_size, 0);

        if (bytes_received == -1) {
            if (is_receive_timeout_error()) {
                Logger::error("Receive timed out while waiting for complete HTTP body.");
                return false;
            }

            Logger::error("Failed to receive body data: " + std::string(std::strerror(errno)));
            return false;
        }

        if (bytes_received == 0) {
            Logger::error("Client closed connection before sending complete body.");
            mark_invalid_request();
            return true;
        }

        request_text.append(buffer, static_cast<std::size_t>(bytes_received));

        if (request_text.size() > max_request_size) {
            Logger::error("Request exceeded maximum size of "
                + std::to_string(max_request_size) + " bytes.");
            mark_invalid_request();
            return true;
        }

        received_body_length = request_text.size() - body_start;
    }

    request = HttpRequest::parse(request_text);

    if (request.body.size() > expected_body_length) {
        request.body.resize(expected_body_length);
    }

    Logger::info("Received body length: " + std::to_string(request.body.size()) + " bytes.");

    if (request.valid) {
        Logger::info(
            "Parsed request line: method=" + request.method
            + ", path=" + request.path
            + ", version=" + request.version
        );
        Logger::info("Parsed headers count: " + std::to_string(request.headers.size()));

        if (request.has_header("Host")) {
            Logger::info("Host header: " + request.get_header("Host"));
        }

        if (request.has_header("User-Agent")) {
            Logger::info("User-Agent header: " + request.get_header("User-Agent"));
        }
    }

    return true;
}

bool Server::create_socket() {
    server_socket_ = socket(AF_INET, SOCK_STREAM, 0);

    if (server_socket_ == -1) {
        Logger::error("Failed to create socket: " + std::string(std::strerror(errno)));
        return false;
    }

    int reuse_address = 1;
    if (setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &reuse_address, sizeof(reuse_address)) == -1) {
        Logger::error("Failed to set socket options: " + std::string(std::strerror(errno)));
        return false;
    }

    return true;
}

bool Server::bind_socket() {
    sockaddr_in server_address{};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(config_.port);

    if (inet_pton(AF_INET, config_.host.c_str(), &server_address.sin_addr) != 1) {
        Logger::error("Invalid host address: " + config_.host);
        return false;
    }

    if (bind(server_socket_, reinterpret_cast<sockaddr*>(&server_address), sizeof(server_address)) == -1) {
        Logger::error("Failed to bind socket: " + std::string(std::strerror(errno)));
        return false;
    }

    return true;
}

bool Server::listen_for_connections() {
    if (listen(server_socket_, config_.backlog) == -1) {
        Logger::error("Failed to listen on socket: " + std::string(std::strerror(errno)));
        return false;
    }

    return true;
}

void Server::log_client_address(const std::string& client_ip, int client_port) const {
    if (client_ip == "unknown") {
        Logger::info("Client connected, but the IP address could not be displayed.");
        return;
    }

    Logger::info("Client connected from " + client_ip
        + ":" + std::to_string(client_port));
}

bool Server::send_response(
    int client_socket,
    const HttpRequest& request,
    const std::string& request_method_for_log,
    const std::string& request_path_for_log
) const {
    HttpResponse response = build_response_for_request(request, config_.public_directory);
    std::string response_text = response.to_string();

    if (!send_all(client_socket, response_text)) {
        return false;
    }

    Logger::info("Response sent to client.");
    Logger::request_summary(
        request_method_for_log,
        request_path_for_log,
        response.status_code(),
        response.reason_phrase()
    );
    return true;
}

bool Server::send_service_unavailable(int client_socket) const {
    HttpResponse response(
        503,
        "Service Unavailable",
        "Server is busy. Please try again later.\n"
    );
    response.set_header("Content-Type", "text/plain; charset=utf-8");

    if (!send_all(client_socket, response.to_string())) {
        return false;
    }

    Logger::request_summary(
        "QUEUE",
        "(overloaded)",
        response.status_code(),
        response.reason_phrase()
    );
    return true;
}

bool Server::send_all(int client_socket, const std::string& response_text) const {
    std::size_t total_sent = 0;

    while (total_sent < response_text.size()) {
        ssize_t bytes_sent = send(
            client_socket,
            response_text.c_str() + total_sent,
            response_text.size() - total_sent,
            0
        );

        if (bytes_sent == -1) {
            Logger::error("Failed to send response: " + std::string(std::strerror(errno)));
            return false;
        }

        total_sent += static_cast<std::size_t>(bytes_sent);
    }

    return true;
}

void Server::close_socket(int& socket_fd) {
    if (socket_fd != -1) {
        close(socket_fd);
        socket_fd = -1;
    }
}

void Server::set_shutdown_requested(const std::atomic<bool>* shutdown_requested) {
    shutdown_requested_ = shutdown_requested;
}

bool Server::should_stop() const {
    return shutdown_requested_ != nullptr && shutdown_requested_->load();
}
