#include "ChatServer.hpp"
#include "EpollServer.hpp"
#include <iostream>

ChatServer::ChatServer(int port)
    : port_(port) {}

void ChatServer::start() {
    std::cout << "[ChatServer] starting on port " << port_ << std::endl;
    run_epoll_server(port_);
}
