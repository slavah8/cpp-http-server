#include "CommandLineOptions.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "Router.hpp"
#include "Server.hpp"
#include "ServerConfig.hpp"
#include "SocketHandle.hpp"
#include "StaticFileHandler.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>
#include <unistd.h>

namespace {

int tests_run = 0;
int tests_failed = 0;

void expect_true(bool condition, const std::string& test_name) {
    ++tests_run;

    if (condition) {
        std::cout << "PASS: " << test_name << '\n';
        return;
    }

    ++tests_failed;
    std::cout << "FAIL: " << test_name << '\n';
}

template <typename Expected, typename Actual>
void expect_equal(const Expected& expected, const Actual& actual, const std::string& test_name) {
    ++tests_run;

    if (expected == actual) {
        std::cout << "PASS: " << test_name << '\n';
        return;
    }

    ++tests_failed;
    std::cout << "FAIL: " << test_name
              << " | expected: " << expected
              << " | actual: " << actual << '\n';
}

bool contains(const std::string& text, const std::string& expected_text) {
    return text.find(expected_text) != std::string::npos;
}

struct HttpClientResult {
    bool connected = false;
    std::string response;
};

void write_test_file(const std::string& file_path, const std::string& contents) {
    std::ofstream file(file_path, std::ios::binary);
    file << contents;
}

void expect_static_file_content_type(
    const std::string& public_directory,
    const std::string& path,
    const std::string& expected_content_type,
    const std::string& test_name
) {
    HttpResponse response = StaticFileHandler::build_response_for_path(path, public_directory);

    expect_equal(200, response.status_code(), test_name + " returns 200");
    expect_true(
        contains(response.to_string(), "Content-Type: " + expected_content_type),
        test_name + " sets Content-Type"
    );
}

HttpRequest make_request(
    const std::string& method,
    const std::string& path,
    const std::string& body = ""
) {
    return HttpRequest{method, path, "HTTP/1.1", {}, body, true};
}

bool parse_test_arguments(const std::vector<std::string>& arguments, ServerConfig& config) {
    std::vector<char*> argv;

    for (const std::string& argument : arguments) {
        argv.push_back(const_cast<char*>(argument.c_str()));
    }

    return parse_arguments(static_cast<int>(argv.size()), argv.data(), config);
}

void test_command_line_options() {
    ServerConfig default_config;

    expect_true(parse_test_arguments({"cpp_http_server"}, default_config), "CLI parses default config");
    expect_equal(8080, default_config.port, "CLI default port");
    expect_equal(std::string("public"), default_config.public_directory, "CLI default public directory");
    expect_equal(static_cast<std::size_t>(4), default_config.worker_count, "CLI default worker count");
    expect_equal(static_cast<std::size_t>(64), default_config.max_queued_clients, "CLI default max queue");
    expect_equal(5, default_config.receive_timeout_seconds, "CLI default receive timeout");

    ServerConfig custom_config;

    expect_true(
        parse_test_arguments(
            {
                "cpp_http_server",
                "--port",
                "9090",
                "--public",
                "public",
                "--workers",
                "2",
                "--max-queue",
                "16",
                "--recv-timeout",
                "3"
            },
            custom_config
        ),
        "CLI parses custom config"
    );
    expect_equal(9090, custom_config.port, "CLI custom port");
    expect_equal(std::string("public"), custom_config.public_directory, "CLI custom public directory");
    expect_equal(static_cast<std::size_t>(2), custom_config.worker_count, "CLI custom worker count");
    expect_equal(static_cast<std::size_t>(16), custom_config.max_queued_clients, "CLI custom max queue");
    expect_equal(3, custom_config.receive_timeout_seconds, "CLI custom receive timeout");

    ServerConfig invalid_config;

    expect_true(
        !parse_test_arguments({"cpp_http_server", "--workers", "0"}, invalid_config),
        "CLI rejects zero workers"
    );
    expect_true(
        !parse_test_arguments({"cpp_http_server", "--max-queue", "0"}, invalid_config),
        "CLI rejects zero max queue"
    );
    expect_true(
        !parse_test_arguments({"cpp_http_server", "--recv-timeout", "0"}, invalid_config),
        "CLI rejects zero receive timeout"
    );
    expect_true(
        !parse_test_arguments({"cpp_http_server", "--port", "70000"}, invalid_config),
        "CLI rejects invalid port"
    );
}

int connect_to_server(int port) {
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (client_socket == -1) {
        return -1;
    }

    sockaddr_in server_address {};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);

    if (inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr) != 1) {
        close(client_socket);
        return -1;
    }

    if (connect(
        client_socket,
        reinterpret_cast<sockaddr*>(&server_address),
        sizeof(server_address)
    ) == -1) {
        close(client_socket);
        return -1;
    }

    return client_socket;
}

bool send_all_for_test(int client_socket, const std::string& request_text) {
    std::size_t total_sent = 0;

    while (total_sent < request_text.size()) {
        ssize_t bytes_sent = send(
            client_socket,
            request_text.c_str() + total_sent,
            request_text.size() - total_sent,
            0
        );

        if (bytes_sent == -1) {
            return false;
        }

        total_sent += static_cast<std::size_t>(bytes_sent);
    }

    return true;
}

HttpClientResult send_http_request_to_test_server(int port, const std::string& request_text) {
    SocketHandle client_socket(connect_to_server(port));

    if (!client_socket.valid()) {
        return {};
    }

    timeval timeout {};
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    setsockopt(client_socket.get(), SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    if (!send_all_for_test(client_socket.get(), request_text)) {
        return {true, ""};
    }

    std::string response;
    char buffer[1024] = {0};

    while (true) {
        ssize_t bytes_received = recv(client_socket.get(), buffer, sizeof(buffer), 0);

        if (bytes_received > 0) {
            response.append(buffer, static_cast<std::size_t>(bytes_received));
            continue;
        }

        break;
    }

    return {true, response};
}

std::string build_get_request(const std::string& path, int port) {
    return "GET " + path + " HTTP/1.1\r\n"
        "Host: 127.0.0.1:" + std::to_string(port) + "\r\n"
        "Connection: close\r\n"
        "\r\n";
}

std::string build_post_request(const std::string& path, int port, const std::string& body) {
    return "POST " + path + " HTTP/1.1\r\n"
        "Host: 127.0.0.1:" + std::to_string(port) + "\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "Connection: close\r\n"
        "\r\n"
        + body;
}

bool wait_for_test_server(int port) {
    for (int attempt = 0; attempt < 60; ++attempt) {
        HttpClientResult result = send_http_request_to_test_server(
            port,
            build_get_request("/health", port)
        );

        if (contains(result.response, "HTTP/1.1 200 OK")) {
            return true;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    return false;
}

void wake_accept_loop(int port) {
    SocketHandle client_socket(connect_to_server(port));
}

void test_socket_handle() {
    int release_pipe[2] = {-1, -1};
    expect_equal(0, pipe(release_pipe), "SocketHandle test pipe for release opens");

    SocketHandle releasable_socket(release_pipe[0]);
    expect_true(releasable_socket.valid(), "SocketHandle starts valid");
    expect_equal(release_pipe[0], releasable_socket.get(), "SocketHandle exposes raw fd");

    const int released_fd = releasable_socket.release();
    expect_equal(release_pipe[0], released_fd, "SocketHandle release returns fd");
    expect_equal(-1, releasable_socket.get(), "SocketHandle release invalidates handle");

    close(released_fd);
    close(release_pipe[1]);

    int move_pipe[2] = {-1, -1};
    expect_equal(0, pipe(move_pipe), "SocketHandle test pipe for move opens");

    SocketHandle original_socket(move_pipe[0]);
    SocketHandle moved_socket(std::move(original_socket));

    expect_equal(-1, original_socket.get(), "SocketHandle move constructor invalidates source");
    expect_equal(move_pipe[0], moved_socket.get(), "SocketHandle move constructor transfers fd");

    close(move_pipe[1]);

    int destructor_pipe[2] = {-1, -1};
    expect_equal(0, pipe(destructor_pipe), "SocketHandle test pipe for destructor opens");

    const int fd_closed_by_destructor = destructor_pipe[0];

    {
        SocketHandle auto_closing_socket(fd_closed_by_destructor);
    }

    errno = 0;
    expect_equal(-1, fcntl(fd_closed_by_destructor, F_GETFD), "SocketHandle destructor closes fd");
    expect_equal(EBADF, errno, "SocketHandle closed fd reports EBADF");

    close(destructor_pipe[1]);
}

void test_http_request() {
    HttpRequest root_request = HttpRequest::parse(
        "GET / HTTP/1.1\r\n"
        "Host: 127.0.0.1:8080\r\n"
        "User-Agent: TestClient/1.0\r\n"
        "\r\n"
    );

    expect_true(root_request.valid, "HttpRequest parses valid GET request");
    expect_equal(std::string("GET"), root_request.method, "HttpRequest parses method");
    expect_equal(std::string("/"), root_request.path, "HttpRequest parses root path");
    expect_equal(std::string("HTTP/1.1"), root_request.version, "HttpRequest parses version");
    expect_equal(std::string("127.0.0.1:8080"), root_request.get_header("Host"), "HttpRequest parses Host header");
    expect_equal(std::string("TestClient/1.0"), root_request.get_header("User-Agent"), "HttpRequest parses User-Agent header");

    HttpRequest health_request = HttpRequest::parse(
        "GET /health HTTP/1.1\r\n"
        "Host: 127.0.0.1:8080\r\n"
        "\r\n"
    );

    expect_equal(std::string("/health"), health_request.path, "HttpRequest parses /health path");

    HttpRequest body_request = HttpRequest::parse(
        "POST /api/echo HTTP/1.1\r\n"
        "Host: 127.0.0.1:8080\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "hello"
    );

    expect_true(body_request.valid, "HttpRequest parses valid POST request");
    expect_equal(std::string("hello"), body_request.body, "HttpRequest parses request body");

    HttpRequest malformed_request = HttpRequest::parse(
        "GET /missing-version\r\n"
        "Host: 127.0.0.1:8080\r\n"
        "\r\n"
    );

    expect_true(!malformed_request.valid, "HttpRequest rejects malformed request line");
}

void test_http_response() {
    HttpResponse response(200, "OK", "hello");
    response.set_header("Content-Type", "text/plain; charset=utf-8");

    const std::string raw_response = response.to_string();

    expect_true(contains(raw_response, "HTTP/1.1 200 OK\r\n"), "HttpResponse includes status line");
    expect_true(contains(raw_response, "Content-Type: text/plain; charset=utf-8\r\n"), "HttpResponse includes custom Content-Type");
    expect_true(contains(raw_response, "Content-Length: 5\r\n"), "HttpResponse includes correct Content-Length");
    expect_true(contains(raw_response, "\r\n\r\nhello"), "HttpResponse includes blank line before body");

    HttpResponse json_response = HttpResponse::json(200, "OK", "{\"status\":\"ok\"}");
    const std::string raw_json_response = json_response.to_string();

    expect_true(contains(raw_json_response, "HTTP/1.1 200 OK\r\n"), "HttpResponse JSON includes status line");
    expect_true(
        contains(raw_json_response, "Content-Type: application/json; charset=utf-8\r\n"),
        "HttpResponse JSON includes Content-Type"
    );
    expect_true(contains(raw_json_response, "Content-Length: 15\r\n"), "HttpResponse JSON includes correct Content-Length");
    expect_true(contains(raw_json_response, "\r\n\r\n{\"status\":\"ok\"}"), "HttpResponse JSON includes body");
}

void test_router() {
    HttpResponse health_response = build_response_for_request(
        make_request("GET", "/health"),
        "public"
    );

    expect_equal(200, health_response.status_code(), "Router returns 200 for GET /health");

    HttpResponse status_response = build_response_for_request(
        make_request("GET", "/api/status"),
        "public"
    );

    const std::string raw_status_response = status_response.to_string();

    expect_equal(200, status_response.status_code(), "Router returns 200 for GET /api/status");
    expect_true(
        contains(raw_status_response, "Content-Type: application/json; charset=utf-8\r\n"),
        "Router sets JSON Content-Type for GET /api/status"
    );
    expect_true(
        contains(raw_status_response, "\r\n\r\n{\"status\":\"ok\",\"message\":\"server running\"}\n"),
        "Router returns JSON body for GET /api/status"
    );

    HttpResponse echo_response = build_response_for_request(
        make_request("POST", "/api/echo", "echo body"),
        "public"
    );

    expect_equal(200, echo_response.status_code(), "Router returns 200 for POST /api/echo");
    expect_true(contains(echo_response.to_string(), "\r\n\r\necho body"), "Router echoes POST body");

    HttpResponse missing_response = build_response_for_request(
        make_request("GET", "/missing.html"),
        "public"
    );

    expect_equal(404, missing_response.status_code(), "Router returns 404 for missing route");

    HttpResponse unsupported_method_response = build_response_for_request(
        make_request("PUT", "/"),
        "public"
    );

    expect_equal(405, unsupported_method_response.status_code(), "Router returns 405 for unsupported method");
}

void test_route_parameters() {
    std::unordered_map<std::string, std::string> params;

    const bool users_route_matched = match_route_pattern(
        "/api/users/:id",
        "/api/users/123",
        params
    );

    expect_true(users_route_matched, "Route params match /api/users/:id");
    expect_equal(std::string("123"), params["id"], "Route params extract id");

    const bool files_route_matched = match_route_pattern(
        "/api/files/:filename",
        "/api/files/report.txt",
        params
    );

    expect_true(files_route_matched, "Route params match /api/files/:filename");
    expect_equal(std::string("report.txt"), params["filename"], "Route params extract filename");

    const bool nonmatching_route_matched = match_route_pattern(
        "/api/users/:id",
        "/api/users/123/profile",
        params
    );

    expect_true(!nonmatching_route_matched, "Route params reject paths with extra segments");

    HttpResponse user_response = build_response_for_request(
        make_request("GET", "/api/users/123"),
        "public"
    );

    const std::string raw_user_response = user_response.to_string();

    expect_equal(200, user_response.status_code(), "Router returns 200 for GET /api/users/:id");
    expect_true(
        contains(raw_user_response, "Content-Type: application/json; charset=utf-8\r\n"),
        "Router sets JSON Content-Type for GET /api/users/:id"
    );
    expect_true(
        contains(raw_user_response, "\r\n\r\n{\"id\":\"123\",\"message\":\"user route matched\"}\n"),
        "Router returns route parameter JSON body"
    );
}

void test_static_file_handler() {
    HttpResponse index_response = StaticFileHandler::build_response_for_path("/", "public");

    expect_equal(200, index_response.status_code(), "StaticFileHandler maps / to index.html");
    expect_true(contains(index_response.to_string(), "Content-Type: text/html; charset=utf-8"), "StaticFileHandler sets HTML MIME type");

    HttpResponse css_response = StaticFileHandler::build_response_for_path("/style.css", "public");

    expect_equal(200, css_response.status_code(), "StaticFileHandler serves CSS file");
    expect_true(contains(css_response.to_string(), "Content-Type: text/css; charset=utf-8"), "StaticFileHandler sets CSS MIME type");

    HttpResponse missing_response = StaticFileHandler::build_response_for_path("/missing.html", "public");

    expect_equal(404, missing_response.status_code(), "StaticFileHandler returns 404 for missing file");

    HttpResponse unsafe_response = StaticFileHandler::build_response_for_path("/../secret.txt", "public");

    expect_equal(404, unsafe_response.status_code(), "StaticFileHandler blocks unsafe path");

    HttpResponse encoded_unsafe_response = StaticFileHandler::build_response_for_path("/%2E%2E/secret.txt", "public");

    expect_equal(404, encoded_unsafe_response.status_code(), "StaticFileHandler blocks encoded traversal path");

    HttpResponse malformed_encoding_response = StaticFileHandler::build_response_for_path("/%ZZ", "public");

    expect_equal(400, malformed_encoding_response.status_code(), "StaticFileHandler rejects malformed percent encoding");
}

void test_static_file_mime_types() {
    const std::string public_directory = "build/test_public_mime";

    std::filesystem::create_directories(public_directory);

    write_test_file(public_directory + "/index.html", "<h1>Home</h1>");
    write_test_file(public_directory + "/style.css", "body {}");
    write_test_file(public_directory + "/app.js", "console.log('test');");
    write_test_file(public_directory + "/data.json", "{}");
    write_test_file(public_directory + "/notes.txt", "hello");
    write_test_file(public_directory + "/image.png", "fake png");
    write_test_file(public_directory + "/photo.jpg", "fake jpg");
    write_test_file(public_directory + "/photo.jpeg", "fake jpeg");
    write_test_file(public_directory + "/icon.svg", "<svg></svg>");
    write_test_file(public_directory + "/favicon.ico", "fake ico");
    write_test_file(public_directory + "/download.bin", "binary");

    expect_static_file_content_type(public_directory, "/", "text/html; charset=utf-8", "MIME .html");
    expect_static_file_content_type(public_directory, "/style.css", "text/css; charset=utf-8", "MIME .css");
    expect_static_file_content_type(public_directory, "/app.js", "application/javascript; charset=utf-8", "MIME .js");
    expect_static_file_content_type(public_directory, "/data.json", "application/json; charset=utf-8", "MIME .json");
    expect_static_file_content_type(public_directory, "/notes.txt", "text/plain; charset=utf-8", "MIME .txt");
    expect_static_file_content_type(public_directory, "/image.png", "image/png", "MIME .png");
    expect_static_file_content_type(public_directory, "/photo.jpg", "image/jpeg", "MIME .jpg");
    expect_static_file_content_type(public_directory, "/photo.jpeg", "image/jpeg", "MIME .jpeg");
    expect_static_file_content_type(public_directory, "/icon.svg", "image/svg+xml", "MIME .svg");
    expect_static_file_content_type(public_directory, "/favicon.ico", "image/x-icon", "MIME .ico");
    expect_static_file_content_type(public_directory, "/download.bin", "application/octet-stream", "MIME unknown extension");
}

void test_integration_server() {
    const int test_port = 18080;
    std::atomic<bool> shutdown_requested{false};
    bool server_result = false;

    ServerConfig config;
    config.port = test_port;
    config.public_directory = "public";

    Server server(config);
    server.set_shutdown_requested(&shutdown_requested);

    std::thread server_thread([&server, &server_result]() {
        server_result = server.run();
    });

    const bool server_ready = wait_for_test_server(test_port);
    expect_true(server_ready, "Integration server starts on test port");

    if (server_ready) {
        HttpClientResult health_response = send_http_request_to_test_server(
            test_port,
            build_get_request("/health", test_port)
        );

        expect_true(contains(health_response.response, "HTTP/1.1 200 OK"), "Integration GET /health returns 200");
        expect_true(contains(health_response.response, "\r\n\r\nstatus: ok\n"), "Integration GET /health returns body");

        HttpClientResult status_response = send_http_request_to_test_server(
            test_port,
            build_get_request("/api/status", test_port)
        );

        expect_true(contains(status_response.response, "HTTP/1.1 200 OK"), "Integration GET /api/status returns 200");
        expect_true(
            contains(status_response.response, "Content-Type: application/json; charset=utf-8"),
            "Integration GET /api/status returns JSON Content-Type"
        );
        expect_true(
            contains(status_response.response, "{\"status\":\"ok\",\"message\":\"server running\"}\n"),
            "Integration GET /api/status returns JSON body"
        );

        HttpClientResult user_response = send_http_request_to_test_server(
            test_port,
            build_get_request("/api/users/123", test_port)
        );

        expect_true(contains(user_response.response, "HTTP/1.1 200 OK"), "Integration GET /api/users/123 returns 200");
        expect_true(
            contains(user_response.response, "{\"id\":\"123\",\"message\":\"user route matched\"}\n"),
            "Integration GET /api/users/123 returns JSON body"
        );

        HttpClientResult echo_response = send_http_request_to_test_server(
            test_port,
            build_post_request("/api/echo", test_port, "hello integration")
        );

        expect_true(contains(echo_response.response, "HTTP/1.1 200 OK"), "Integration POST /api/echo returns 200");
        expect_true(
            contains(echo_response.response, "\r\n\r\nhello integration"),
            "Integration POST /api/echo returns posted body"
        );

        HttpClientResult missing_response = send_http_request_to_test_server(
            test_port,
            build_get_request("/missing.html", test_port)
        );

        expect_true(contains(missing_response.response, "HTTP/1.1 404 Not Found"), "Integration missing file returns 404");
    }

    shutdown_requested.store(true);
    wake_accept_loop(test_port);

    if (server_thread.joinable()) {
        server_thread.join();
    }

    expect_true(server_result, "Integration server shuts down cleanly");
}

}

int main() {
    std::cout << "Running C++ HTTP Server tests...\n";

    test_command_line_options();
    test_socket_handle();
    test_http_request();
    test_http_response();
    test_router();
    test_route_parameters();
    test_static_file_handler();
    test_static_file_mime_types();
    test_integration_server();

    std::cout << "\nTests run: " << tests_run << '\n';
    std::cout << "Tests failed: " << tests_failed << '\n';

    if (tests_failed != 0) {
        return 1;
    }

    return 0;
}
