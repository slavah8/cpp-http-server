#include "StaticFileHandler.hpp"

#include <fstream>
#include <sstream>
#include <string>

HttpResponse StaticFileHandler::build_response_for_path(const std::string& path) {
    if (!is_safe_path(path)) {
        HttpResponse response(404, "Not Found", "404 Not Found\n");
        response.set_header("Content-Type", "text/plain; charset=utf-8");
        return response;
    }

    const std::string file_path = map_url_to_file_path(path);
    std::string file_contents;

    if (!read_file(file_path, file_contents)) {
        HttpResponse response(404, "Not Found", "404 Not Found\n");
        response.set_header("Content-Type", "text/plain; charset=utf-8");
        return response;
    }

    HttpResponse response(200, "OK", file_contents);
    response.set_header("Content-Type", detect_content_type(file_path));
    return response;
}

bool StaticFileHandler::is_safe_path(const std::string& path) {
    if (path.empty() || path[0] != '/') {
        return false;
    }

    if (path.find("..") != std::string::npos) {
        return false;
    }

    return true;
}

std::string StaticFileHandler::map_url_to_file_path(const std::string& path) {
    if (path == "/") {
        return "public/index.html";
    }

    return "public" + path;
}

std::string StaticFileHandler::detect_content_type(const std::string& file_path) {
    if (file_path.size() >= 5 && file_path.substr(file_path.size() - 5) == ".html") {
        return "text/html; charset=utf-8";
    }

    if (file_path.size() >= 4 && file_path.substr(file_path.size() - 4) == ".css") {
        return "text/css; charset=utf-8";
    }

    if (file_path.size() >= 3 && file_path.substr(file_path.size() - 3) == ".js") {
        return "application/javascript; charset=utf-8";
    }

    if (file_path.size() >= 4 && file_path.substr(file_path.size() - 4) == ".txt") {
        return "text/plain; charset=utf-8";
    }

    return "application/octet-stream";
}

bool StaticFileHandler::read_file(const std::string& file_path, std::string& file_contents) {
    std::ifstream file(file_path);

    if (!file.is_open()) {
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    file_contents = buffer.str();
    return true;
}
