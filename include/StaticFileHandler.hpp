#ifndef STATIC_FILE_HANDLER_HPP
#define STATIC_FILE_HANDLER_HPP

#include "HttpResponse.hpp"

#include <string>

class StaticFileHandler {
public:
    static HttpResponse build_response_for_path(
        const std::string& path,
        const std::string& public_directory
    );

private:
    static bool decode_url_path(const std::string& path, std::string& decoded_path);
    static bool normalize_url_path(const std::string& decoded_path, std::string& normalized_path);
    static std::string map_url_to_file_path(
        const std::string& normalized_path,
        const std::string& public_directory
    );
    static std::string detect_content_type(const std::string& file_path);
    static bool read_file(const std::string& file_path, std::string& file_contents);
};

#endif
