#ifndef HTTP_RESPONSE_HPP
#define HTTP_RESPONSE_HPP

#include <unordered_map>
#include <string>

class HttpResponse {
public:
    HttpResponse(int status_code, std::string reason_phrase, std::string body = "");

    void set_header(const std::string& name, const std::string& value);
    void set_body(std::string body);
    std::string to_string() const;

private:
    std::string version_;
    int status_code_;
    std::string reason_phrase_;
    std::unordered_map<std::string, std::string> headers_;
    std::string body_;
};

#endif
