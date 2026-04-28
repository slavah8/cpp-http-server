#include "ErrorResponse.hpp"

#include <string>

HttpResponse ErrorResponse::bad_request() {
    return build(
        400,
        "Bad Request",
        "The request could not be understood by this server."
    );
}

HttpResponse ErrorResponse::not_found() {
    return build(
        404,
        "Not Found",
        "The requested resource could not be found."
    );
}

HttpResponse ErrorResponse::method_not_allowed() {
    return build(
        405,
        "Method Not Allowed",
        "This method is not supported for the requested route."
    );
}

HttpResponse ErrorResponse::internal_server_error() {
    return build(
        500,
        "Internal Server Error",
        "The server encountered an unexpected error."
    );
}

HttpResponse ErrorResponse::build(
    int status_code,
    const std::string& reason_phrase,
    const std::string& message
) {
    const std::string title = std::to_string(status_code) + " " + reason_phrase;
    const std::string body =
        "<!DOCTYPE html>\n"
        "<html lang=\"en\">\n"
        "<head>\n"
        "    <meta charset=\"UTF-8\">\n"
        "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
        "    <title>" + title + "</title>\n"
        "    <style>\n"
        "        body { margin: 0; font-family: -apple-system, BlinkMacSystemFont, \"Segoe UI\", sans-serif; background: #f4f7fb; color: #1f2937; }\n"
        "        main { max-width: 680px; margin: 80px auto; padding: 32px; background: #ffffff; border: 1px solid #dbe3ee; border-radius: 12px; }\n"
        "        h1 { margin-top: 0; }\n"
        "        a { color: #2563eb; }\n"
        "    </style>\n"
        "</head>\n"
        "<body>\n"
        "    <main>\n"
        "        <h1>" + title + "</h1>\n"
        "        <p>" + message + "</p>\n"
        "        <p><a href=\"/\">Back to home</a></p>\n"
        "    </main>\n"
        "</body>\n"
        "</html>\n";

    HttpResponse response(status_code, reason_phrase, body);
    response.set_header("Content-Type", "text/html; charset=utf-8");
    return response;
}
