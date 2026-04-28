#include "HttpRequest.hpp"

#include <sstream>
#include <string>

namespace {

std::string trim(const std::string& text) {
    const std::string whitespace = " \t\r\n";
    const std::size_t start = text.find_first_not_of(whitespace);

    if (start == std::string::npos) {
        return "";
    }

    const std::size_t end = text.find_last_not_of(whitespace);
    return text.substr(start, end - start + 1);
}

}

HttpRequest HttpRequest::parse(const std::string& request_text) {
    HttpRequest request{"", "", "", {}, false};

    std::istringstream request_stream(request_text);
    std::string request_line;

    if (!std::getline(request_stream, request_line)) {
        return request;
    }

    if (!request_line.empty() && request_line.back() == '\r') {
        request_line.pop_back();
    }

    std::istringstream line_stream(request_line);

    if (!(line_stream >> request.method >> request.path >> request.version)) {
        return request;
    }

    std::string extra_text;
    if (line_stream >> extra_text) {
        return request;
    }

    request.valid = true;

    std::string header_line;
    while (std::getline(request_stream, header_line)) {
        if (!header_line.empty() && header_line.back() == '\r') {
            header_line.pop_back();
        }

        if (header_line.empty()) {
            break;
        }

        const std::size_t colon_position = header_line.find(':');
        if (colon_position == std::string::npos) {
            continue;
        }

        const std::string header_name = trim(header_line.substr(0, colon_position));
        const std::string header_value = trim(header_line.substr(colon_position + 1));

        if (header_name.empty()) {
            continue;
        }

        request.headers[header_name] = header_value;
    }

    return request;
}

bool HttpRequest::has_header(const std::string& name) const {
    return headers.find(name) != headers.end();
}

std::string HttpRequest::get_header(const std::string& name) const {
    const auto iterator = headers.find(name);

    if (iterator == headers.end()) {
        return "";
    }

    return iterator->second;
}
