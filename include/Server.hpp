#ifndef SERVER_HPP
#define SERVER_HPP

#include "HttpRequest.hpp"
#include "ServerConfig.hpp"
#include "SocketHandle.hpp"

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>
#include <atomic>

class Server {
public:
    explicit Server(ServerConfig config);

    ~Server();

    bool run();
    void set_shutdown_requested(const std::atomic<bool>* shutdown_requested);

private:
    struct ClientConnection {
        SocketHandle client_socket;
        std::string client_ip;
        int client_port = 0;
    };

    bool create_socket();
    bool bind_socket();
    bool listen_for_connections();
    bool start_worker_threads();
    void stop_worker_threads();
    void worker_loop();
    bool enqueue_client(ClientConnection connection);
    bool should_stop() const;

    void handle_client(SocketHandle client_socket, std::string client_ip, int client_port) const;
    bool set_receive_timeout(int client_socket) const;
    bool receive_request(
        int client_socket,
        std::string& request_text,
        HttpRequest& request,
        std::string& request_method_for_log,
        std::string& request_path_for_log
    ) const;
    bool send_response(
        int client_socket,
        const HttpRequest& request,
        const std::string& request_method_for_log,
        const std::string& request_path_for_log
    ) const;
    bool send_service_unavailable(int client_socket) const;
    bool send_all(int client_socket, const std::string& response_text) const;
    void log_client_address(const std::string& client_ip, int client_port) const;
    void close_socket(int& socket_fd);

    ServerConfig config_;
    int server_socket_;
    const std::atomic<bool>* shutdown_requested_;
    std::vector<std::thread> worker_threads_;
    std::queue<ClientConnection> client_queue_;
    std::mutex client_queue_mutex_;
    std::condition_variable client_available_;
    bool shutting_down_;
};

#endif
