#include "ChatServer.hpp"
#include <cstdlib>
#include <iostream>

int main(int argc, char* argv[]) {
    int port = 8080; // 默认端口

    if (argc >= 2) {
        port = std::atoi(argv[1]);
    }

    ChatServer server(port);
    server.start();

    return 0;
}
