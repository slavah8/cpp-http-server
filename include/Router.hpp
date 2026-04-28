#ifndef ROUTER_HPP
#define ROUTER_HPP

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"

HttpResponse build_response_for_request(const HttpRequest& request);

#endif
