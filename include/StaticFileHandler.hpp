#ifndef STATIC_FILE_HANDLER_HPP
#define STATIC_FILE_HANDLER_HPP

#include "HttpResponse.hpp"

#include <string>

class StaticFileHandler {
public:
    static HttpResponse build_response_for_path(const std::string& path);

private:
    static bool is_safe_path(const std::string& path);
    static std::string map_url_to_file_path(const std::string& path);
    static std::string detect_content_type(const std::string& file_path);
    static bool read_file(const std::string& file_path, std::string& file_contents);
};

#endif
