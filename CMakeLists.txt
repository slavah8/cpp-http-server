cmake_minimum_required(VERSION 3.16)

project(cpp_http_server VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

set(CPP_HTTP_SERVER_CORE_SOURCES
    src/CommandLineOptions.cpp
    src/ErrorResponse.cpp
    src/HttpRequest.cpp
    src/HttpResponse.cpp
    src/StaticFileHandler.cpp
    src/Router.cpp
)

add_executable(cpp_http_server
    src/main.cpp
    src/Server.cpp
    src/Logger.cpp
    ${CPP_HTTP_SERVER_CORE_SOURCES}
)

target_include_directories(cpp_http_server PRIVATE include)
target_link_libraries(cpp_http_server PRIVATE Threads::Threads)

add_executable(cpp_http_server_tests
    tests/test_main.cpp
    src/Server.cpp
    src/Logger.cpp
    ${CPP_HTTP_SERVER_CORE_SOURCES}
)

target_include_directories(cpp_http_server_tests PRIVATE include)
target_link_libraries(cpp_http_server_tests PRIVATE Threads::Threads)
