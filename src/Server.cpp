#include "Server.hpp"

#include "HttpRequest.hpp"
#include "Router.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <string>

Server::Server(int port)
    : port_(port),
      backlog_(1),
      server_socket_(-1),
      client_socket_(-1),
      request_{"", "", "", {}, false},
      client_port_(0) {
}

Server::~Server() {
    close_socket(client_socket_);
    close_socket(server_socket_);
}

bool Server::run() {
    if (!create_socket()) {
        return false;
    }

    if (!bind_socket()) {
        return false;
    }

    if (!listen_for_connections()) {
        return false;
    }

    std::cout << "Listening on http://127.0.0.1:" << port_ << std::endl;
    std::cout << "Server is running. Press Ctrl+C to stop it." << std::endl;

    while (true) {
        reset_client_state();
        std::cout << "Waiting for the next client connection..." << std::endl;

        if (!accept_one_client()) {
            return false;
        }

        log_client_address();

        if (!receive_request()) {
            close_socket(client_socket_);
            return false;
        }

        if (!request_text_.empty() && !send_response()) {
            close_socket(client_socket_);
            return false;
        }

        close_socket(client_socket_);
        std::cout << "Closed client connection." << std::endl;
    }
}

void Server::reset_client_state() {
    close_socket(client_socket_);
    request_text_.clear();
    request_ = HttpRequest{"", "", "", {}, false};
    response_text_.clear();
    client_ip_.clear();
    client_port_ = 0;
}

bool Server::create_socket() {
    server_socket_ = socket(AF_INET, SOCK_STREAM, 0);

    if (server_socket_ == -1) {
        std::cerr << "Failed to create socket: " << std::strerror(errno) << std::endl;
        return false;
    }

    int reuse_address = 1;
    if (setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &reuse_address, sizeof(reuse_address)) == -1) {
        std::cerr << "Failed to set socket options: " << std::strerror(errno) << std::endl;
        return false;
    }

    return true;
}

bool Server::bind_socket() {
    sockaddr_in server_address{};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port_);
    server_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(server_socket_, reinterpret_cast<sockaddr*>(&server_address), sizeof(server_address)) == -1) {
        std::cerr << "Failed to bind socket: " << std::strerror(errno) << std::endl;
        return false;
    }

    return true;
}

bool Server::listen_for_connections() {
    if (listen(server_socket_, backlog_) == -1) {
        std::cerr << "Failed to listen on socket: " << std::strerror(errno) << std::endl;
        return false;
    }

    return true;
}

bool Server::accept_one_client() {
    sockaddr_in client_address{};
    socklen_t client_address_size = sizeof(client_address);

    client_socket_ = accept(
        server_socket_,
        reinterpret_cast<sockaddr*>(&client_address),
        &client_address_size
    );

    if (client_socket_ == -1) {
        std::cerr << "Failed to accept client: " << std::strerror(errno) << std::endl;
        return false;
    }

    char client_ip_buffer[INET_ADDRSTRLEN] = {0};
    const char* converted_ip = inet_ntop(
        AF_INET,
        &client_address.sin_addr,
        client_ip_buffer,
        INET_ADDRSTRLEN
    );

    if (converted_ip != nullptr) {
        client_ip_ = client_ip_buffer;
    } else {
        client_ip_ = "unknown";
    }

    client_port_ = ntohs(client_address.sin_port);
    return true;
}

void Server::log_client_address() const {
    if (client_ip_ == "unknown") {
        std::cout << "Client connected, but the IP address could not be displayed." << std::endl;
        return;
    }

    std::cout << "Client connected from " << client_ip_
              << ":" << client_port_ << std::endl;
}

bool Server::receive_request() {
    const std::size_t buffer_size = 1024;
    char buffer[buffer_size] = {0};

    ssize_t bytes_received = recv(client_socket_, buffer, buffer_size - 1, 0);

    if (bytes_received == -1) {
        std::cerr << "Failed to receive data: " << std::strerror(errno) << std::endl;
        return false;
    }

    if (bytes_received == 0) {
        std::cout << "Client connected but sent no data." << std::endl;
        request_text_.clear();
        request_ = HttpRequest{"", "", "", {}, false};
        return true;
    }

    buffer[bytes_received] = '\0';
    request_text_ = buffer;

    std::cout << "Received " << bytes_received << " bytes:" << std::endl;
    std::cout << request_text_ << std::endl;

    request_ = HttpRequest::parse(request_text_);

    if (request_.valid) {
        std::cout << "Parsed request line: "
                  << "method=" << request_.method
                  << ", path=" << request_.path
                  << ", version=" << request_.version << std::endl;
        std::cout << "Parsed headers count: " << request_.headers.size() << std::endl;

        if (request_.has_header("Host")) {
            std::cout << "Host header: " << request_.get_header("Host") << std::endl;
        }

        if (request_.has_header("User-Agent")) {
            std::cout << "User-Agent header: " << request_.get_header("User-Agent") << std::endl;
        }
    } else {
        std::cout << "Request line was missing or malformed." << std::endl;
    }

    return true;
}

bool Server::send_response() {
    response_text_ = build_response_for_request(request_).to_string();

    std::size_t total_sent = 0;

    while (total_sent < response_text_.size()) {
        ssize_t bytes_sent = send(
            client_socket_,
            response_text_.c_str() + total_sent,
            response_text_.size() - total_sent,
            0
        );

        if (bytes_sent == -1) {
            std::cerr << "Failed to send response: " << std::strerror(errno) << std::endl;
            return false;
        }

        total_sent += static_cast<std::size_t>(bytes_sent);
    }

    std::cout << "Sent HTTP response to client." << std::endl;
    return true;
}

void Server::close_socket(int& socket_fd) {
    if (socket_fd != -1) {
        close(socket_fd);
        socket_fd = -1;
    }
}
