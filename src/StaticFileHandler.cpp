#include "StaticFileHandler.hpp"

#include "ErrorResponse.hpp"

#include <cctype>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

int hex_value(char character) {
    if (character >= '0' && character <= '9') {
        return character - '0';
    }

    if (character >= 'a' && character <= 'f') {
        return character - 'a' + 10;
    }

    if (character >= 'A' && character <= 'F') {
        return character - 'A' + 10;
    }

    return -1;
}

std::string lowercase_extension(const std::string& file_path) {
    const std::size_t dot_position = file_path.find_last_of('.');

    if (dot_position == std::string::npos) {
        return "";
    }

    std::string extension = file_path.substr(dot_position);

    for (char& character : extension) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }

    return extension;
}

}

HttpResponse StaticFileHandler::build_response_for_path(
    const std::string& path,
    const std::string& public_directory
) {
    std::string decoded_path;
    if (!decode_url_path(path, decoded_path)) {
        return ErrorResponse::bad_request();
    }

    std::string normalized_path;
    if (!normalize_url_path(decoded_path, normalized_path)) {
        return ErrorResponse::not_found();
    }

    const std::string file_path = map_url_to_file_path(normalized_path, public_directory);
    std::string file_contents;

    if (!read_file(file_path, file_contents)) {
        return ErrorResponse::not_found();
    }

    HttpResponse response(200, "OK", file_contents);
    response.set_header("Content-Type", detect_content_type(file_path));
    return response;
}

bool StaticFileHandler::decode_url_path(const std::string& path, std::string& decoded_path) {
    decoded_path.clear();

    for (std::size_t i = 0; i < path.size(); ++i) {
        if (path[i] != '%') {
            decoded_path += path[i];
            continue;
        }

        if (i + 2 >= path.size()) {
            return false;
        }

        const int high = hex_value(path[i + 1]);
        const int low = hex_value(path[i + 2]);

        if (high == -1 || low == -1) {
            return false;
        }

        decoded_path += static_cast<char>((high * 16) + low);
        i += 2;
    }

    return true;
}

bool StaticFileHandler::normalize_url_path(const std::string& decoded_path, std::string& normalized_path) {
    normalized_path.clear();

    if (decoded_path.empty() || decoded_path[0] != '/') {
        return false;
    }

    std::vector<std::string> safe_segments;
    std::size_t segment_start = 1;

    while (segment_start <= decoded_path.size()) {
        const std::size_t slash_position = decoded_path.find('/', segment_start);
        const std::size_t segment_end = slash_position == std::string::npos
            ? decoded_path.size()
            : slash_position;
        const std::string segment = decoded_path.substr(segment_start, segment_end - segment_start);

        if (segment == "..") {
            return false;
        }

        if (!segment.empty() && segment != ".") {
            safe_segments.push_back(segment);
        }

        if (slash_position == std::string::npos) {
            break;
        }

        segment_start = slash_position + 1;
    }

    normalized_path = "/";

    for (std::size_t i = 0; i < safe_segments.size(); ++i) {
        if (i > 0) {
            normalized_path += "/";
        }

        normalized_path += safe_segments[i];
    }

    return true;
}

std::string StaticFileHandler::map_url_to_file_path(
    const std::string& normalized_path,
    const std::string& public_directory
) {
    std::string base_directory = public_directory;

    if (!base_directory.empty() && base_directory.back() == '/') {
        base_directory.pop_back();
    }

    if (normalized_path == "/") {
        return base_directory + "/index.html";
    }

    return base_directory + normalized_path;
}

std::string StaticFileHandler::detect_content_type(const std::string& file_path) {
    const std::string extension = lowercase_extension(file_path);

    if (extension == ".html") {
        return "text/html; charset=utf-8";
    }

    if (extension == ".css") {
        return "text/css; charset=utf-8";
    }

    if (extension == ".js") {
        return "application/javascript; charset=utf-8";
    }

    if (extension == ".json") {
        return "application/json; charset=utf-8";
    }

    if (extension == ".txt") {
        return "text/plain; charset=utf-8";
    }

    if (extension == ".png") {
        return "image/png";
    }

    if (extension == ".jpg" || extension == ".jpeg") {
        return "image/jpeg";
    }

    if (extension == ".svg") {
        return "image/svg+xml";
    }

    if (extension == ".ico") {
        return "image/x-icon";
    }

    return "application/octet-stream";
}

bool StaticFileHandler::read_file(const std::string& file_path, std::string& file_contents) {
    std::ifstream file(file_path, std::ios::binary);

    if (!file.is_open()) {
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    file_contents = buffer.str();
    return true;
}
