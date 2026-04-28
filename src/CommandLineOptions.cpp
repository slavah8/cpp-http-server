#include "CommandLineOptions.hpp"

#include <cstddef>
#include <exception>
#include <string>

namespace {

bool parse_int_option(const std::string& text, int& value) {
    std::size_t characters_read = 0;

    try {
        value = std::stoi(text, &characters_read);
    } catch (const std::exception&) {
        return false;
    }

    return characters_read == text.size();
}

bool read_next_value(int& index, int argc, char* argv[], std::string& value) {
    if (index + 1 >= argc) {
        return false;
    }

    value = argv[++index];
    return true;
}

}

bool parse_arguments(int argc, char* argv[], ServerConfig& config) {
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        std::string value;

        if (argument == "--port") {
            if (!read_next_value(i, argc, argv, value)) {
                return false;
            }

            if (!parse_int_option(value, config.port)) {
                return false;
            }

            if (config.port < 1 || config.port > 65535) {
                return false;
            }
        } else if (argument == "--public") {
            if (!read_next_value(i, argc, argv, value)) {
                return false;
            }

            config.public_directory = value;

            if (config.public_directory.empty()) {
                return false;
            }
        } else if (argument == "--workers") {
            int worker_count = 0;

            if (!read_next_value(i, argc, argv, value) || !parse_int_option(value, worker_count)) {
                return false;
            }

            if (worker_count < 1) {
                return false;
            }

            config.worker_count = static_cast<std::size_t>(worker_count);
        } else if (argument == "--max-queue") {
            int max_queued_clients = 0;

            if (!read_next_value(i, argc, argv, value) || !parse_int_option(value, max_queued_clients)) {
                return false;
            }

            if (max_queued_clients < 1) {
                return false;
            }

            config.max_queued_clients = static_cast<std::size_t>(max_queued_clients);
        } else if (argument == "--recv-timeout") {
            if (!read_next_value(i, argc, argv, value)) {
                return false;
            }

            if (!parse_int_option(value, config.receive_timeout_seconds)) {
                return false;
            }

            if (config.receive_timeout_seconds < 1) {
                return false;
            }
        } else {
            return false;
        }
    }

    return true;
}
