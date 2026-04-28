#include "CommandLineOptions.hpp"
#include "Server.hpp"
#include "ServerConfig.hpp"

#include <cerrno>
#include <csignal>
#include <cstring>
#include <iostream>
#include <atomic>

namespace {
    std::atomic<bool> shutdown_requested{false};

    void handle_sigint(int) {
        shutdown_requested.store(true);
    }

    bool install_signal_handler() {
        struct sigaction action {};
        action.sa_handler = handle_sigint;
        sigemptyset(&action.sa_mask);
        action.sa_flags = 0;

        return sigaction(SIGINT, &action, nullptr) == 0;
    }
}

void print_usage(const char* program_name) {
    std::cerr << "Usage: " << program_name
              << " [--port PORT]"
              << " [--public DIRECTORY]"
              << " [--workers COUNT]"
              << " [--max-queue COUNT]"
              << " [--recv-timeout SECONDS]\n";
}

int main(int argc, char* argv[]) {
    if (!install_signal_handler()) {
        std::cerr << "Failed to install SIGINT handler: "
                  << std::strerror(errno) << "\n";
        return 1;
    }

    ServerConfig config;

    if (!parse_arguments(argc, argv, config)) {
        print_usage(argv[0]);
        return 1;
    }

    std::cout << "Starting C++ HTTP Server..." << std::endl;

    Server server(config);
    server.set_shutdown_requested(&shutdown_requested);

    if (!server.run()) {
        return 1;
    }

    return 0;
}
