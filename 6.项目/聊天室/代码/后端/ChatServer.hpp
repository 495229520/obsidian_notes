#pragma once

class ChatServer {
public:
    explicit ChatServer(int port);

    // 启动服务器（阻塞运行）
    void start();

private:
    int port_;
};
