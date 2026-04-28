#ifndef SERVER_HPP
#define SERVER_HPP

#include "HttpRequest.hpp"

#include <string>

class Server {
public:
    explicit Server(int port);
    ~Server();

    bool run();

private:
    void reset_client_state();
    bool create_socket();
    bool bind_socket();
    bool listen_for_connections();
    bool accept_one_client();
    bool receive_request();
    bool send_response();
    void log_client_address() const;
    void close_socket(int& socket_fd);

    int port_;
    int backlog_;
    int server_socket_;
    int client_socket_;
    std::string request_text_;
    HttpRequest request_;
    std::string response_text_;
    std::string client_ip_;
    int client_port_;
};

#endif
