#include "HttpResponse.hpp"

#include <string>
#include <utility>

HttpResponse::HttpResponse(int status_code, std::string reason_phrase, std::string body)
    : version_("HTTP/1.1"),
      status_code_(status_code),
      reason_phrase_(std::move(reason_phrase)),
      body_(std::move(body)) {
}

HttpResponse HttpResponse::json(int status_code, std::string reason_phrase, std::string body) {
    HttpResponse response(status_code, std::move(reason_phrase), std::move(body));
    response.set_header("Content-Type", "application/json; charset=utf-8");
    return response;
}

void HttpResponse::set_header(const std::string& name, const std::string& value) {
    headers_[name] = value;
}

void HttpResponse::set_body(std::string body) {
    body_ = std::move(body);
}

int HttpResponse::status_code() const {
    return status_code_;
}

std::string HttpResponse::reason_phrase() const {
    return reason_phrase_;
}

std::string HttpResponse::to_string() const {
    std::string response = version_ + " "
        + std::to_string(status_code_) + " "
        + reason_phrase_ + "\r\n";

    for (const auto& header : headers_) {
        response += header.first + ": " + header.second + "\r\n";
    }

    response += "Content-Length: " + std::to_string(body_.size()) + "\r\n";
    response += "Connection: close\r\n";
    response += "\r\n";
    response += body_;

    return response;
}
