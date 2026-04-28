#ifndef ROUTER_HPP
#define ROUTER_HPP

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"

#include <string>
#include <unordered_map>

bool match_route_pattern(
    const std::string& pattern,
    const std::string& path,
    std::unordered_map<std::string, std::string>& params
);

HttpResponse build_response_for_request(
    const HttpRequest& request,
    const std::string& public_directory
);

#endif
