#ifndef SERVER_CONFIG_HPP
#define SERVER_CONFIG_HPP

#include <string>
#include <cstddef>

struct ServerConfig {
    int port = 8080;
    int backlog = 10;
    std::string host = "127.0.0.1";
    std::string public_directory = "public";
    std::size_t worker_count = 4;
    std::size_t max_queued_clients = 64;
    int receive_timeout_seconds = 5;
};

#endif
