#ifndef ERROR_RESPONSE_HPP
#define ERROR_RESPONSE_HPP

#include "HttpResponse.hpp"

#include <string>

class ErrorResponse {
public:
    static HttpResponse bad_request();
    static HttpResponse not_found();
    static HttpResponse method_not_allowed();
    static HttpResponse internal_server_error();

private:
    static HttpResponse build(int status_code, const std::string& reason_phrase, const std::string& message);
};

#endif
