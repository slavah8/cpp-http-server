#include "Router.hpp"

#include "StaticFileHandler.hpp"

HttpResponse build_response_for_request(const HttpRequest& request) {
    if (!request.valid) {
        HttpResponse response(400, "Bad Request", "400 Bad Request\n");
        response.set_header("Content-Type", "text/plain; charset=utf-8");
        return response;
    }

    if (request.method != "GET") {
        HttpResponse response(405, "Method Not Allowed", "405 Method Not Allowed\n");
        response.set_header("Content-Type", "text/plain; charset=utf-8");
        return response;
    }

    if (request.path == "/health") {
        HttpResponse response(200, "OK", "status: ok\n");
        response.set_header("Content-Type", "text/plain; charset=utf-8");
        return response;
    }

    return StaticFileHandler::build_response_for_path(request.path);
}
