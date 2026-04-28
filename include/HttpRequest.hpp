#ifndef HTTP_REQUEST_HPP
#define HTTP_REQUEST_HPP

#include <unordered_map>
#include <string>

struct HttpRequest {
    std::string method;
    std::string path;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    bool valid;

    static HttpRequest parse(const std::string& request_text);
    bool has_header(const std::string& name) const;
    std::string get_header(const std::string& name) const;
};

#endif
