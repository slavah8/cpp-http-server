#include "Server.hpp"

#include <iostream>

int main() {
    std::cout << "Starting C++ HTTP Server..." << std::endl;

    Server server(8080);

    if (!server.run()) {
        return 1;
    }

    return 0;
}
