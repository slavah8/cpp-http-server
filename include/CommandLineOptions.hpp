#ifndef COMMAND_LINE_OPTIONS_HPP
#define COMMAND_LINE_OPTIONS_HPP

#include "ServerConfig.hpp"

bool parse_arguments(int argc, char* argv[], ServerConfig& config);

#endif
