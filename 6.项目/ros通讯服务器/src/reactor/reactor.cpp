#include "mfms/reactor/reactor.hpp"
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <sys/signalfd.h>
#include <sys/eventfd.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <cstring>
#include <cerrno>
#include <spdlog/spdlog.h>

namespace mfms::reactor {

namespace {

bool setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return false;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) >= 0;
}

void setTcpNoDelay(int fd) {
    int flag = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
}

void setTcpKeepalive(int fd) {
    int flag = 1;
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(flag));
}

// Epoll data tag types
constexpr uintptr_t TAG_LISTEN   = 0x1ULL << 60;
constexpr uintptr_t TAG_TIMER    = 0x2ULL << 60;
constexpr uintptr_t TAG_SIGNAL   = 0x3ULL << 60;
constexpr uintptr_t TAG_ROS_EVT  = 0x4ULL << 60;
constexpr uintptr_t TAG_DB_EVT   = 0x5ULL << 60;
constexpr uintptr_t TAG_CLIENT   = 0x6ULL << 60;
constexpr uintptr_t TAG_MASK     = 0xFULL << 60;

} // namespace

Reactor::Reactor(const Config& config)
    : config_(config)
    , ros_status_queue_(10000)
    , ros_completion_queue_(1000)
    , db_completion_queue_(1000)
{}

Reactor::~Reactor() {
    if (listen_fd_ >= 0) close(listen_fd_);
    if (epoll_fd_ >= 0) close(epoll_fd_);
    if (timer_fd_ >= 0) close(timer_fd_);
    if (signal_fd_ >= 0) close(signal_fd_);
    if (ros_eventfd_ >= 0) close(ros_eventfd_);
    if (db_eventfd_ >= 0) close(db_eventfd_);
}

bool Reactor::init() {
    // Create epoll
    epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd_ < 0) {
        spdlog::error("epoll_create1 failed: {}", strerror(errno));
        return false;
    }

    // Create listening socket
    listen_fd_ = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (listen_fd_ < 0) {
        spdlog::error("socket failed: {}", strerror(errno));
        return false;
    }

    int opt = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config_.listen_port);
    inet_pton(AF_INET, config_.listen_addr.c_str(), &addr.sin_addr);

    if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        spdlog::error("bind failed on {}:{}: {}", config_.listen_addr, config_.listen_port, strerror(errno));
        return false;
    }

    if (listen(listen_fd_, config_.listen_backlog) < 0) {
        spdlog::error("listen failed: {}", strerror(errno));
        return false;
    }

    // Create timerfd (1 second interval)
    timer_fd_ = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timer_fd_ < 0) {
        spdlog::error("timerfd_create failed: {}", strerror(errno));
        return false;
    }

    itimerspec its{};
    its.it_value.tv_sec = 1;
    its.it_interval.tv_sec = 1;
    if (timerfd_settime(timer_fd_, 0, &its, nullptr) < 0) {
        spdlog::error("timerfd_settime failed: {}", strerror(errno));
        return false;
    }

    // Create signalfd for SIGINT, SIGTERM
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigprocmask(SIG_BLOCK, &mask, nullptr);

    signal_fd_ = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    if (signal_fd_ < 0) {
        spdlog::error("signalfd failed: {}", strerror(errno));
        return false;
    }

    // Create eventfd for ROS thread wakeup
    ros_eventfd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (ros_eventfd_ < 0) {
        spdlog::error("eventfd (ROS) failed: {}", strerror(errno));
        return false;
    }

    // Create eventfd for DB thread wakeup
    db_eventfd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (db_eventfd_ < 0) {
        spdlog::error("eventfd (DB) failed: {}", strerror(errno));
        return false;
    }

    // Add all FDs to epoll
    addToEpoll(listen_fd_, EPOLLIN, reinterpret_cast<void*>(TAG_LISTEN));
    addToEpoll(timer_fd_, EPOLLIN, reinterpret_cast<void*>(TAG_TIMER));
    addToEpoll(signal_fd_, EPOLLIN, reinterpret_cast<void*>(TAG_SIGNAL));
    addToEpoll(ros_eventfd_, EPOLLIN, reinterpret_cast<void*>(TAG_ROS_EVT));
    addToEpoll(db_eventfd_, EPOLLIN, reinterpret_cast<void*>(TAG_DB_EVT));

    spdlog::info("Reactor initialized, listening on {}:{}", config_.listen_addr, config_.listen_port);
    return true;
}

void Reactor::run() {
    running_ = true;
    constexpr int MAX_EVENTS = 256;
    epoll_event events[MAX_EVENTS];

    while (running_) {
        int nfds = epoll_wait(epoll_fd_, events, MAX_EVENTS, -1);
        if (nfds < 0) {
            if (errno == EINTR) continue;
            spdlog::error("epoll_wait failed: {}", strerror(errno));
            break;
        }

        for (int i = 0; i < nfds; ++i) {
            auto data = reinterpret_cast<uintptr_t>(events[i].data.ptr);
            auto tag = data & TAG_MASK;

            if (tag == TAG_LISTEN) {
                acceptConnections();
            } else if (tag == TAG_TIMER) {
                uint64_t expirations;
                read(timer_fd_, &expirations, sizeof(expirations));
                checkTimeouts();
                if (handler_) handler_->onTimerTick();
            } else if (tag == TAG_SIGNAL) {
                signalfd_siginfo siginfo;
                if (read(signal_fd_, &siginfo, sizeof(siginfo)) > 0) {
                    spdlog::info("Received signal {}, shutting down", siginfo.ssi_signo);
                    running_ = false;
                }
            } else if (tag == TAG_ROS_EVT) {
                uint64_t val;
                read(ros_eventfd_, &val, sizeof(val));
                ros_wake_pending_ = false;
                drainRosQueue();
            } else if (tag == TAG_DB_EVT) {
                uint64_t val;
                read(db_eventfd_, &val, sizeof(val));
                db_wake_pending_ = false;
                drainDbQueue();
            } else if (tag == TAG_CLIENT) {
                auto client_id = static_cast<ClientId>(data & ~TAG_MASK);
                auto it = sessions_.find(client_id);
                if (it == sessions_.end()) continue;

                auto& session = *it->second;
                if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                    disconnectClient(client_id, "connection error");
                    continue;
                }
                if (events[i].events & EPOLLIN) {
                    handleRead(session);
                }
                // Re-check session exists after handleRead
                if (sessions_.find(client_id) == sessions_.end()) continue;
                if (events[i].events & EPOLLOUT) {
                    handleWrite(session);
                }
            }
        }
    }

    spdlog::info("Reactor stopped");
}

void Reactor::stop() {
    running_ = false;
    // Wake epoll
    uint64_t val = 1;
    write(ros_eventfd_, &val, sizeof(val));
}

void Reactor::acceptConnections() {
    while (true) {
        sockaddr_in addr{};
        socklen_t len = sizeof(addr);
        int client_fd = accept4(listen_fd_, reinterpret_cast<sockaddr*>(&addr), &len,
                                SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            if (errno == EMFILE || errno == ENFILE) {
                spdlog::warn("Too many open files, cannot accept new connections");
                break;
            }
            spdlog::error("accept failed: {}", strerror(errno));
            break;
        }

        if (sessions_.size() >= config_.max_clients) {
            spdlog::warn("Max clients reached, rejecting connection");
            close(client_fd);
            continue;
        }

        setTcpNoDelay(client_fd);
        setTcpKeepalive(client_fd);

        ClientId client_id = next_client_id_++;
        auto session = std::make_unique<Session>(client_id, client_fd, config_);

        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, ip_str, sizeof(ip_str));
        session->setRemoteAddr(std::string(ip_str) + ":" + std::to_string(ntohs(addr.sin_port)));

        spdlog::info("Client {} connected from {}", client_id, session->remoteAddr());

        // Add to epoll with client tag (level-triggered)
        addToEpoll(client_fd, EPOLLIN,
                   reinterpret_cast<void*>(TAG_CLIENT | client_id));

        fd_to_client_[client_fd] = client_id;
        sessions_[client_id] = std::move(session);
    }
}

void Reactor::handleRead(Session& session) {
    uint8_t buf[config_.recv_buffer_size];

    while (true) {
        ssize_t n = recv(session.fd(), buf, sizeof(buf), 0);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            disconnectClient(session.id(), strerror(errno));
            return;
        }
        if (n == 0) {
            disconnectClient(session.id(), "connection closed");
            return;
        }

        session.markActivity();

        // Feed to parser
        std::size_t offset = 0;
        while (offset < static_cast<std::size_t>(n)) {
            std::size_t consumed = session.parser().feed(buf + offset, n - offset);
            offset += consumed;

            if (session.parser().hasError()) {
                disconnectClient(session.id(), "frame parse error");
                return;
            }

            if (session.parser().hasFrame()) {
                processFrame(session);
                // Check if session still exists after processing
                if (sessions_.find(session.id()) == sessions_.end()) return;
            }

            if (consumed == 0) break;
        }
    }
}

void Reactor::handleWrite(Session& session) {
    while (session.hasOutboundData()) {
        int sent = session.flushSendBuffer();
        if (sent < 0) {
            disconnectClient(session.id(), "write error");
            return;
        }
        if (sent == 0) break; // EAGAIN
    }
}

void Reactor::processFrame(Session& session) {
    auto hdr = session.parser().header();
    auto payload = session.parser().takePayload();

    if (handler_) {
        handler_->onFrame(session, hdr, std::move(payload));
    }
}

void Reactor::drainRosQueue() {
    while (true) {
        auto status = ros_status_queue_.tryPop();
        if (!status) break;
        if (handler_) handler_->onRosStatus(*status);
    }

    while (true) {
        auto completion = ros_completion_queue_.tryPop();
        if (!completion) break;
        if (handler_) handler_->onRosServiceComplete(*completion);
    }
}

void Reactor::drainDbQueue() {
    while (true) {
        auto completion = db_completion_queue_.tryPop();
        if (!completion) break;
        completion->callback();
    }
}

void Reactor::checkTimeouts() {
    auto now = Clock::now();
    auto idle_timeout = std::chrono::seconds(config_.idle_timeout_sec);
    auto upload_timeout = std::chrono::seconds(config_.upload_timeout_sec);

    std::vector<ClientId> to_disconnect;

    for (auto& kv : sessions_) {
        ClientId id = kv.first;
        auto& session = kv.second;
        // Check idle timeout
        if (now - session->lastActivity() > idle_timeout) {
            to_disconnect.push_back(id);
            continue;
        }

        // Check upload timeout
        if (session->hasActiveUpload()) {
            if (now - session->uploadState().last_chunk_at > upload_timeout) {
                spdlog::warn("Client {}: upload timeout", id);
                session->clearUpload();
                sendError(id, 0, ErrorCode::UPLOAD_TIMEOUT, "Upload timed out");
            }
        }

        // Check in-flight request timeouts
        session->checkTimeouts(now, [this, client_id = id](const InflightRequest& req) {
            spdlog::warn("Client {}: request {} timed out", client_id, req.request_id);
            sendError(client_id, req.request_id, ErrorCode::SERVICE_TIMEOUT, "Service request timed out");
        });
    }

    for (auto id : to_disconnect) {
        disconnectClient(id, "idle timeout");
    }
}

void Reactor::postRosStatus(RosStatusUpdate update) {
    ros_status_queue_.push(std::move(update));
    // Coalesce wakeups: only signal if no pending wake
    bool expected = false;
    if (ros_wake_pending_.compare_exchange_strong(expected, true)) {
        uint64_t val = 1;
        write(ros_eventfd_, &val, sizeof(val));
    }
}

void Reactor::postRosServiceCompletion(RosServiceCompletion completion) {
    ros_completion_queue_.push(std::move(completion));
    bool expected = false;
    if (ros_wake_pending_.compare_exchange_strong(expected, true)) {
        uint64_t val = 1;
        write(ros_eventfd_, &val, sizeof(val));
    }
}

void Reactor::postDbCompletion(std::function<void()> callback) {
    db_completion_queue_.push(DbCompletion{std::move(callback)});
    bool expected = false;
    if (db_wake_pending_.compare_exchange_strong(expected, true)) {
        uint64_t val = 1;
        write(db_eventfd_, &val, sizeof(val));
    }
}

bool Reactor::sendResponse(ClientId client_id, const protocol::FrameHeader& hdr,
                           const std::vector<std::uint8_t>& payload) {
    auto* session = getSession(client_id);
    if (!session) return false;

    if (!session->enqueueFrame(hdr, payload)) {
        disconnectClient(client_id, "send buffer overflow");
        return false;
    }

    // Trigger write
    handleWrite(*session);
    return true;
}

bool Reactor::sendError(ClientId client_id, RequestId request_id, ErrorCode code, const std::string& detail) {
    protocol::ErrorPayload err;
    err.code = static_cast<uint16_t>(code);
    err.detail = detail;

    std::vector<uint8_t> payload;
    err.serialize(payload);

    protocol::FrameHeader hdr;
    hdr.msg_type = protocol::MsgType::ERROR;
    hdr.request_id = request_id;
    hdr.payload_len = static_cast<uint32_t>(payload.size());

    return sendResponse(client_id, hdr, payload);
}

bool Reactor::sendAck(ClientId client_id, RequestId request_id) {
    protocol::AckPayload ack;
    ack.status = 0;

    std::vector<uint8_t> payload;
    ack.serialize(payload);

    protocol::FrameHeader hdr;
    hdr.msg_type = protocol::MsgType::ACK;
    hdr.request_id = request_id;
    hdr.payload_len = static_cast<uint32_t>(payload.size());

    return sendResponse(client_id, hdr, payload);
}

void Reactor::disconnectClient(ClientId client_id, const std::string& reason) {
    auto it = sessions_.find(client_id);
    if (it == sessions_.end()) return;

    auto& session = *it->second;
    spdlog::info("Client {} disconnected: {}", client_id, reason);

    if (handler_) {
        handler_->onDisconnect(session, reason);
    }

    removeFromEpoll(session.fd());
    fd_to_client_.erase(session.fd());
    sessions_.erase(it);
}

Session* Reactor::getSession(ClientId id) {
    auto it = sessions_.find(id);
    return it != sessions_.end() ? it->second.get() : nullptr;
}

void Reactor::addToEpoll(int fd, uint32_t events, void* ptr) {
    epoll_event ev{};
    ev.events = events;
    ev.data.ptr = ptr;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) < 0) {
        spdlog::error("epoll_ctl ADD failed for fd {}: {}", fd, strerror(errno));
    }
}

void Reactor::modifyEpoll(int fd, uint32_t events, void* ptr) {
    epoll_event ev{};
    ev.events = events;
    ev.data.ptr = ptr;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) < 0) {
        spdlog::error("epoll_ctl MOD failed for fd {}: {}", fd, strerror(errno));
    }
}

void Reactor::removeFromEpoll(int fd) {
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
}

} // namespace mfms::reactor
