#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <string>

class Logger {
public:
    static void info(const std::string& message);
    static void error(const std::string& message);
    static void request_summary(
        const std::string& method,
        const std::string& path,
        int status_code,
        const std::string& reason_phrase
    );

private:
    static std::string make_timestamp();
    static void log(const std::string& level, const std::string& message);
};

#endif
