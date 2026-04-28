#include "Logger.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

namespace {

std::mutex logger_mutex;

}

std::string Logger::make_timestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t current_time = std::chrono::system_clock::to_time_t(now);

    std::tm local_time{};
    localtime_r(&current_time, &local_time);

    std::ostringstream output;
    output << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S");
    return output.str();
}

void Logger::log(const std::string& level, const std::string& message) {
    std::lock_guard<std::mutex> lock(logger_mutex);

    std::cout << "[" << make_timestamp() << "]";

    if (!level.empty()) {
        std::cout << " [" << level << "]";
    }

    std::cout << " " << message << std::endl;
}

void Logger::info(const std::string& message) {
    log("", message);
}

void Logger::error(const std::string& message) {
    log("ERROR", message);
}

void Logger::request_summary(
    const std::string& method,
    const std::string& path,
    int status_code,
    const std::string& reason_phrase
) {
    log("", method + " " + path + " -> "
        + std::to_string(status_code) + " " + reason_phrase);
}
