#include "Router.hpp"

#include "ErrorResponse.hpp"
#include "StaticFileHandler.hpp"

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

std::vector<std::string> split_path_segments(const std::string& path) {
    std::vector<std::string> segments;

    if (path.empty() || path[0] != '/') {
        return segments;
    }

    if (path == "/") {
        return segments;
    }

    std::size_t segment_start = 1;

    while (segment_start <= path.size()) {
        const std::size_t slash_position = path.find('/', segment_start);
        const std::size_t segment_end = slash_position == std::string::npos
            ? path.size()
            : slash_position;

        segments.push_back(path.substr(segment_start, segment_end - segment_start));

        if (slash_position == std::string::npos) {
            break;
        }

        segment_start = slash_position + 1;
    }

    return segments;
}

std::string escape_json_string(const std::string& text) {
    std::string escaped;

    for (char character : text) {
        if (character == '"') {
            escaped += "\\\"";
        } else if (character == '\\') {
            escaped += "\\\\";
        } else if (character == '\n') {
            escaped += "\\n";
        } else if (character == '\r') {
            escaped += "\\r";
        } else if (character == '\t') {
            escaped += "\\t";
        } else {
            escaped += character;
        }
    }

    return escaped;
}

}

bool match_route_pattern(
    const std::string& pattern,
    const std::string& path,
    std::unordered_map<std::string, std::string>& params
) {
    const std::vector<std::string> pattern_segments = split_path_segments(pattern);
    const std::vector<std::string> path_segments = split_path_segments(path);

    if (pattern_segments.size() != path_segments.size()) {
        params.clear();
        return false;
    }

    std::unordered_map<std::string, std::string> matched_params;

    for (std::size_t i = 0; i < pattern_segments.size(); ++i) {
        const std::string& pattern_segment = pattern_segments[i];
        const std::string& path_segment = path_segments[i];

        if (!pattern_segment.empty() && pattern_segment[0] == ':') {
            const std::string param_name = pattern_segment.substr(1);

            if (param_name.empty() || path_segment.empty()) {
                params.clear();
                return false;
            }

            matched_params[param_name] = path_segment;
            continue;
        }

        if (pattern_segment != path_segment) {
            params.clear();
            return false;
        }
    }

    params = std::move(matched_params);
    return true;
}

HttpResponse build_response_for_request(
    const HttpRequest& request,
    const std::string& public_directory
) {
    if (!request.valid) {
        return ErrorResponse::bad_request();
    }

    if (request.method == "POST") {
        if (request.path != "/api/echo") {
            return ErrorResponse::not_found();
        }

        const std::string body = request.body.empty()
            ? "No body received\n"
            : request.body;

        HttpResponse response(200, "OK", body);
        response.set_header("Content-Type", "text/plain; charset=utf-8");
        return response;
    }

    if (request.method != "GET") {
        return ErrorResponse::method_not_allowed();
    }

    if (request.path == "/health") {
        HttpResponse response(200, "OK", "status: ok\n");
        response.set_header("Content-Type", "text/plain; charset=utf-8");
        return response;
    }

    if (request.path == "/api/status") {
        return HttpResponse::json(
            200,
            "OK",
            "{\"status\":\"ok\",\"message\":\"server running\"}\n"
        );
    }

    std::unordered_map<std::string, std::string> params;
    if (match_route_pattern("/api/users/:id", request.path, params)) {
        return HttpResponse::json(
            200,
            "OK",
            "{\"id\":\"" + escape_json_string(params["id"])
                + "\",\"message\":\"user route matched\"}\n"
        );
    }

    return StaticFileHandler::build_response_for_path(request.path, public_directory);
}
