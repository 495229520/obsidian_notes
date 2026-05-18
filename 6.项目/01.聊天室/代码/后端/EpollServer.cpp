#include "EpollServer.hpp"
#include <algorithm>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <string>
#include <fstream>

namespace {

// [u32 MAGIC][u8 type=MSG_FILE_DOWNLOAD_REQ][u32 length] + [payload...]
constexpr uint32_t MAGIC = 0x48574600;  // HWF，协议头
constexpr int HEADER_SIZE = 4 + 1 + 4;  // magic(4) + type(1) + length(4)

// 消息类型
enum MsgType : uint8_t {
    MSG_TEXT = 1,               // 普通文本
    MSG_FILE_UPLOAD = 2,        // 文件上传请求
    MSG_FILE_ANNOUNCE = 3,      // 文件公告
    MSG_FILE_DOWNLOAD_REQ = 4,  // 文件下载请求
    MSG_FILE_DOWNLOAD_DATA = 5  // 文件下载数据
};

struct Client {
    int fd;
    std::vector<uint8_t> recvBuffer;
};

// 服务器文件结构
struct FileInfo {
    uint32_t id;
    std::string fileName;
    uint64_t size;
    std::string uploader;
    std::string path;
};

// 文件列表
std::vector<FileInfo> g_files;
uint32_t g_nextFileId = 1;  // 自增文件ID

// 大端序读写
uint32_t read_u32(const uint8_t *p) {
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

uint64_t read_u64(const uint8_t* p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) {
        v = (v << 8) | p[i];
    }
    return v;
}

void write_u32(std::vector<uint8_t> &buf, uint32_t v) {
    buf.push_back((v >> 24) & 0xFF);
    buf.push_back((v >> 16) & 0xFF);
    buf.push_back((v >> 8) & 0xFF);
    buf.push_back(v & 0xFF);
}

void write_u64(std::vector<uint8_t>& buf, uint64_t v) {
    for (int i = 7; i >= 0; --i) {
        buf.push_back((v >> (i * 8)) & 0xFF);
    }
}

bool set_non_blocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return false;
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) return false;
    return true;
}

void broadcast_packet(const std::unordered_map<int, Client>& clients,
                      int from_fd,
                      const std::vector<uint8_t>& packet)
{
    for (const auto &kv : clients) {
        int cfd = kv.first;

        if (cfd == from_fd) {
            continue;
        }

        ::send(cfd, packet.data(), packet.size(), 0);
    }
}

void handle_text(std::unordered_map<int, Client> &clients,
                 int fd,
                 const uint8_t *payload,
                 uint32_t length)
{
    std::string text((char*)payload, length);
    std::cout << "[Server] text: " << text << std::endl;
    // MSG_TEXT
    // [ MAGIC(4字节) ][ type=MSG_TEXT(1字节) ][ length(4字节) ][ 文本数据(length字节) ]
    std::vector<uint8_t> pkt;
    write_u32(pkt, MAGIC);  // 4
    pkt.push_back(MSG_TEXT);// 1
    write_u32(pkt, length); // 4
    pkt.insert(pkt.end(), payload, payload + length);

    broadcast_packet(clients, fd, pkt);
}

void handle_file_upload(std::unordered_map<int, Client>& clients,
                        int from_fd,
                        const uint8_t* payload,
                        uint32_t length)
{
    // MSG_FILE_UPLOAD
    // [u32 nameLen][name 字节][u64 fileSize][fileData 字节]
    const uint8_t* p   = payload;
    const uint8_t* end = payload + length;

    // [u32 nameLen]
    if (end - p < 4) {
        std::cerr << "[EpollServer] FILE_UPLOAD: payload too short (no nameLen)\n";
        return;
    }
    uint32_t nameLen = read_u32(p);
    p += 4;

    if (nameLen == 0 || nameLen > 1024) {
        std::cerr << "[EpollServer] FILE_UPLOAD: invalid nameLen=" << nameLen << "\n";
        return;
    }
    
    // [nameLen 字节文件名长度] + [8 字节 fileSize]
    if (end - p < static_cast<ptrdiff_t>(nameLen) + 8) {
        std::cerr << "[EpollServer] FILE_UPLOAD: payload too short (no name/fileSize)\n";
        return;
    }

    // [name bytes]
    std::string fileName(reinterpret_cast<const char*>(p), nameLen);
    p += nameLen;

    // [u64 fileSize]
    uint64_t fileSize = read_u64(p);
    p += 8;

    if (end - p < static_cast<ptrdiff_t>(fileSize)) {
        std::cerr << "[EpollServer] FILE_UPLOAD: file data incomplete\n";
        return;
    }

    const uint8_t* fileData = p;

    // 创建目录 files/
    ::mkdir("files", 0755); // 已存在时会返回错误，可忽略

    uint32_t fileId = g_nextFileId++;
    std::string savePath = "files/" + std::to_string(fileId) + "_" + fileName;

    // 写文件到磁盘
    std::ofstream ofs(savePath, std::ios::binary);
    if (!ofs) {
        std::cerr << "[EpollServer] FILE_UPLOAD: cannot open " << savePath << "\n";
        return;
    }
    ofs.write(reinterpret_cast<const char*>(fileData), fileSize);
    ofs.close();

    std::cout << "[EpollServer] File saved: id=" << fileId
              << " name=" << fileName
              << " size=" << fileSize
              << " path=" << savePath << "\n";

    // 记录到文件表
    FileInfo info;
    info.id       = fileId;
    info.fileName = fileName;
    info.size     = fileSize;
    info.uploader = "";       // 以后可以从客户端带上用户名
    info.path     = savePath;

    g_files.push_back(info);

    // MSG_FILE_ANNOUNCE
    // payload: [u32 fileId][u32 nameLen][name][u64 fileSize]
    std::vector<uint8_t> body;
    write_u32(body, fileId);
    write_u32(body, nameLen);
    body.insert(body.end(), fileName.begin(), fileName.end());
    write_u64(body, fileSize);

    // [u32 MAGIC][u8 msgType][u32 bodyLen][body...]
    std::vector<uint8_t> packet;
    write_u32(packet, MAGIC);
    packet.push_back(MSG_FILE_ANNOUNCE);
    write_u32(packet, static_cast<uint32_t>(body.size()));
    packet.insert(packet.end(), body.begin(), body.end());

    broadcast_packet(clients, from_fd, packet);
}

// MSG_FILE_DOWNLOAD_REQ
// payload: [u32 fileId]
void handle_file_download_req(std::unordered_map<int, Client>& clients,
                              int to_fd,
                              const uint8_t* payload,
                              uint32_t length)
{
    if (length < 4) {
        std::cerr << "[EpollServer] FILE_DOWNLOAD_REQ: payload too short\n";
        return;
    }

    uint32_t fileId = read_u32(payload);

    // 在文件表中查找
    auto it = std::find_if(g_files.begin(), g_files.end(),
                           [fileId](const FileInfo& info) {
                               return info.id == fileId;
                           });
    if (it == g_files.end()) {
        std::cerr << "[EpollServer] FILE_DOWNLOAD_REQ: fileId "
                  << fileId << " not found\n";

        // 给客户端发一条文本提示
        std::string msg = "[Server] File not found: id = " + std::to_string(fileId);
        std::vector<uint8_t> body(msg.begin(), msg.end());

        std::vector<uint8_t> packet;
        write_u32(packet, MAGIC);
        packet.push_back(MSG_TEXT);
        write_u32(packet, static_cast<uint32_t>(body.size()));
        packet.insert(packet.end(), body.begin(), body.end());
        ::send(to_fd, packet.data(), packet.size(), 0);
        return;
    }

    const FileInfo& info = *it;

    // 打开文件
    std::ifstream ifs(info.path, std::ios::binary);
    if (!ifs) {
        std::cerr << "[EpollServer] FILE_DOWNLOAD_REQ: cannot open "
                  << info.path << "\n";

        std::string msg = "[Server] Cannot open file on server: " + info.fileName;
        std::vector<uint8_t> body(msg.begin(), msg.end());

        std::vector<uint8_t> packet;
        write_u32(packet, MAGIC);
        packet.push_back(MSG_TEXT);
        write_u32(packet, static_cast<uint32_t>(body.size()));
        packet.insert(packet.end(), body.begin(), body.end());
        ::send(to_fd, packet.data(), packet.size(), 0);
        return;
    }

    // 读取文件全部内容
    std::vector<uint8_t> fileData(
        (std::istreambuf_iterator<char>(ifs)),
         std::istreambuf_iterator<char>());
    ifs.close();

    uint64_t fileSize = fileData.size();
    std::string fileName = info.fileName;
    uint32_t nameLen = static_cast<uint32_t>(fileName.size());

    // 构造 body:
    // [u32 fileId][u32 nameLen][name][u64 fileSize][fileData]
    std::vector<uint8_t> body;
    write_u32(body, fileId);
    write_u32(body, nameLen);
    body.insert(body.end(), fileName.begin(), fileName.end());
    write_u64(body, fileSize);
    body.insert(body.end(), fileData.begin(), fileData.end());

    // 封装为完整数据包：MAGIC + type + length + body
    std::vector<uint8_t> packet;
    write_u32(packet, MAGIC);
    packet.push_back(MSG_FILE_DOWNLOAD_DATA);
    write_u32(packet, static_cast<uint32_t>(body.size()));
    packet.insert(packet.end(), body.begin(), body.end());

    // 只发送给请求方，不广播
    ::send(to_fd, packet.data(), packet.size(), 0);

    std::cout << "[EpollServer] FILE_DOWNLOAD_DATA sent: id=" << fileId
              << " name=" << fileName
              << " size=" << fileSize
              << " bytes\n";
}

// 处理客户端发送的缓冲区数据，拆包
void process_client_buffer(std::unordered_map<int, Client> &clients, int fd)
{
    auto it = clients.find(fd);
    if (it == clients.end()) return;

    Client &cli = it->second;
    auto &buf = cli.recvBuffer;

    while (true) {
        if (buf.size() < HEADER_SIZE) return;

        const uint8_t *p = buf.data();
        uint32_t magic = read_u32(p);
        uint8_t type = p[4];
        uint32_t length = read_u32(p + 5);

        if (magic != MAGIC) {
            std::cerr << "MAGIC mismatch, disconnect\n";
            ::close(fd);
            clients.erase(fd);
            return;
        }

        if (buf.size() < HEADER_SIZE + length)
            return;

        const uint8_t *payload = p + HEADER_SIZE;

        switch (type) {
            case MSG_TEXT:
                handle_text(clients, fd, payload, length);
                break;

            case MSG_FILE_UPLOAD:
                handle_file_upload(clients, fd, payload, length);
                break;

            case MSG_FILE_DOWNLOAD_REQ:
                // 处理文件下载请求
                handle_file_download_req(clients, fd, payload, length);
                break;

            default: {
                // 未知消息类型：记录日志 + 给该客户端回一条提示
                std::cerr << "[EpollServer] Unknown msg type: " << int(type)
                        << " from fd=" << fd << std::endl;

                std::string err =
                    "[Server] Unknown message type: " + std::to_string(int(type));
                std::vector<uint8_t> body(err.begin(), err.end());

                std::vector<uint8_t> packet;
                write_u32(packet, MAGIC);
                packet.push_back(MSG_TEXT);
                write_u32(packet, static_cast<uint32_t>(body.size()));
                packet.insert(packet.end(), body.begin(), body.end());

                ::send(fd, packet.data(), packet.size(), 0);
                break;
            }
        }

        // 移除已处理的包
        buf.erase(buf.begin(), buf.begin() + HEADER_SIZE + length);
    }
}

} // namespace

void run_epoll_server(int port) {
    int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return;
    }

    int opt = 1;
    // 设置 SO_REUSEADDR 允许地址快速重用，绑定到处于 TIME_WAIT 状态的端口
    ::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // 0.0.0.0
    addr.sin_port = htons(port);

    if (::bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        perror("bind");
        ::close(listen_fd);
        return;
    }

    if (::listen(listen_fd, 128) < 0) {
        perror("listen");
        ::close(listen_fd);
        return;
    }

    if (!set_non_blocking(listen_fd)) {
        perror("fcntl listen_fd");
        ::close(listen_fd);
        return;
    }

    std::cout << "[EpollServer] Listening on 0.0.0.0:" << port << std::endl;

    // 创建epoll实例
    int epfd = ::epoll_create1(0);
    if (epfd < 0) {
        perror("epoll_create1");
        ::close(listen_fd);
        return;
    }

    // 把 listen_fd 加入 epoll 实例
    epoll_event ev {};
    // 监听可读事件
    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    if (::epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev) < 0) {
        perror("epoll_ctl listen_fd");
        ::close(listen_fd);
        ::close(epfd);
        return;
    }
    
    // 维护一个在线 fd 列表
    std::unordered_map<int, Client> clients;

    const int MAX_EVENTS = 1024;
    std::vector<epoll_event> events(MAX_EVENTS);

    while (true) {
        int n = ::epoll_wait(epfd, events.data(), MAX_EVENTS, -1);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            break;  // 退出循环，结束服务器
        }

        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;
            uint32_t evts = events[i].events;

            if (fd == listen_fd) {
                // 新客户端连接
                sockaddr_in cli_addr {};
                socklen_t cli_len = sizeof(cli_addr);
                int conn_fd = ::accept(listen_fd, 
                    reinterpret_cast<sockaddr*>(&cli_addr), 
                    &cli_len);
                if (conn_fd < 0) {
                    perror("accept");
                    continue;
                }

                set_non_blocking(conn_fd);

                epoll_event cev {};
                cev.events = EPOLLIN;
                cev.data.fd = conn_fd;
                if (::epoll_ctl(epfd, EPOLL_CTL_ADD, conn_fd, &cev) < 0) {
                    perror("epoll_ctl conn_fd");
                    ::close(conn_fd);
                    continue;
                }

                Client c;
                c.fd = conn_fd;
                clients[conn_fd] = std::move(c);

                char ip[INET_ADDRSTRLEN] = {0};
                ::inet_ntop(AF_INET, &cli_addr.sin_addr, ip, sizeof(ip));
                std::cout << "[EpollServer] New client fd=" << conn_fd
                          << " from " << ip << ":" << ntohs(cli_addr.sin_port)
                          << std::endl;

            } else if (evts & EPOLLIN) {
                // 某个客户端发来数据
                char buf[4096];
                int len = ::recv(fd, buf, sizeof(buf), 0);
                if (len <= 0) {
                    if (len < 0) perror("recv");
                    std::cout << "[EpollServer] client fd=" << fd 
                        << " disconnected" << std::endl;

                    ::epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
                    ::close(fd);
                    clients.erase(fd);
                } else {
                    auto it = clients.find(fd);
                    if (it == clients.end()) {
                        std::cerr << "[EpollServer] unknown client fd=" << fd << std::endl;
                        ::close(fd);
                        continue;
                    }

                    Client& client = it->second;
                    client.recvBuffer.insert(client.recvBuffer.end(), buf, buf + len);

                    // 处理消息
                    process_client_buffer(clients, fd);
                }
            }
        }
    }

    for (auto &kv : clients) {
        int cfd = kv.first;
        ::epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, nullptr);
        ::close(cfd);
    }
    clients.clear();

    ::close(listen_fd);
    ::close(epfd);

}
