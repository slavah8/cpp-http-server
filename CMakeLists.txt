cmake_minimum_required(VERSION 3.16)

project(cpp_http_server VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(cpp_http_server
    src/main.cpp
    src/Server.cpp
    src/HttpRequest.cpp
    src/HttpResponse.cpp
    src/StaticFileHandler.cpp
    src/Router.cpp
)

target_include_directories(cpp_http_server PRIVATE include)
