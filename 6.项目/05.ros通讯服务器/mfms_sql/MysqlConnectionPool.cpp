/**
 * @file MysqlConnectionPool.cpp
 * @brief MySQL 连接池实现
 */
#include "include/MysqlConnectionPool.h"
#include <iostream>
#include <sstream>
#include <chrono>

// 获取单例实例
MysqlConnectionPool& MysqlConnectionPool::getInstance() {
    static MysqlConnectionPool instance;
    return instance;
}

// 检查是否已初始化
bool MysqlConnectionPool::isInitialized() {
    return getInstance().initialized_.load();
}

// 使用配置初始化连接池
void MysqlConnectionPool::initializePool(const MysqlPoolConfig& config) {
    auto& inst = getInstance();
    std::lock_guard<std::mutex> lock(inst.mutex_);

    // 防止重复初始化
    if (inst.initialized_.load()) {
        std::cerr << "[Warning] MysqlConnectionPool already initialized, ignoring re-init." << std::endl;
        return;
    }

    // 保存配置
    inst.config_ = config;

    // 获取 MySQL 驱动
    try {
        inst.driver_ = get_driver_instance();
    } catch (sql::SQLException& e) {
        throw std::runtime_error(
            "[MysqlConnectionPool] Failed to get MySQL driver: " + std::string(e.what())
        );
    }

    // 尝试初始化连接池
    try {
        inst.initPoolIfEmpty();
        inst.initialized_.store(true);
        std::cout << "[Info] MysqlConnectionPool initialized successfully with "
                  << inst.allConnections_.size() << " connections to "
                  << config.host << ":" << config.port << "/" << config.database
                  << std::endl;
    } catch (sql::SQLException& e) {
        throw std::runtime_error(
            "[MysqlConnectionPool] Failed to initialize pool: " + std::string(e.what()) +
            " (SQLState: " + e.getSQLState() + ", ErrorCode: " + std::to_string(e.getErrorCode()) + ")"
        );
    }
}

// 构造函数
MysqlConnectionPool::MysqlConnectionPool() {
    // 延迟初始化，等待 initializePool 调用
}

// 析构函数
MysqlConnectionPool::~MysqlConnectionPool() {
    std::unique_lock<std::mutex> lock(mutex_);
    for (auto* conn : allConnections_) {
        if (conn) {
            try {
                conn->close();
            } catch (...) {
                // 忽略关闭异常
            }
            delete conn;
        }
    }
    allConnections_.clear();
    // 清空队列
    std::queue<sql::Connection*> empty;
    std::swap(freeConnections_, empty);
}

// 获取当前配置
const MysqlPoolConfig& MysqlConnectionPool::getConfig() const {
    return config_;
}

// 创建单个数据库连接
sql::Connection* MysqlConnectionPool::createConnection() {
    // 构建连接 URI
    std::ostringstream uri;
    uri << "tcp://" << config_.host << ":" << config_.port;

    sql::Connection* conn = driver_->connect(uri.str(), config_.user, config_.password);
    conn->setSchema(config_.database);
    return conn;
}

// 批量创建连接填充池
void MysqlConnectionPool::initPoolIfEmpty() {
    // 注意：此方法必须在持有锁的情况下调用
    if (!allConnections_.empty()) {
        return;
    }

    for (int i = 0; i < config_.maxPoolSize; ++i) {
        sql::Connection* conn = createConnection();
        if (conn) {
            allConnections_.push_back(conn);
            freeConnections_.push(conn);
        }
    }
}

// 获取连接
std::shared_ptr<sql::Connection> MysqlConnectionPool::getConnection() {
    std::unique_lock<std::mutex> lock(mutex_);

    // 检查是否已初始化
    if (!initialized_.load()) {
        throw std::runtime_error(
            "[MysqlConnectionPool] Pool not initialized. Call initializePool() first."
        );
    }

    // 延迟初始化：如果池为空（可能是初始化时部分失败），尝试重新创建
    if (allConnections_.empty()) {
        try {
            initPoolIfEmpty();
            std::cout << "[Info] Lazy initialization of DB pool success." << std::endl;
        } catch (sql::SQLException& e) {
            throw std::runtime_error(
                "[MysqlConnectionPool] Database unreachable: " + std::string(e.what())
            );
        }
    }

    // 超时等待空闲连接
    if (freeConnections_.empty()) {
        std::cv_status status = cond_.wait_for(
            lock,
            std::chrono::milliseconds(config_.waitTimeoutMs)
        );
        if (status == std::cv_status::timeout) {
            if (freeConnections_.empty()) {
                throw std::runtime_error(
                    "[MysqlConnectionPool] Get connection timeout (" +
                    std::to_string(config_.waitTimeoutMs) + "ms). Pool busy or DB down."
                );
            }
        }
    }

    // 再次检查（防止虚假唤醒）
    if (freeConnections_.empty()) {
        throw std::runtime_error("[MysqlConnectionPool] No free connections available.");
    }

    // 取出连接
    sql::Connection* conn = freeConnections_.front();
    freeConnections_.pop();

    // 连接保活检查
    if (!conn->isValid()) {
        std::cout << "[Warn] Detected broken connection, reconnecting..." << std::endl;
        try {
            // 找到并替换失效连接
            for (auto& ptr : allConnections_) {
                if (ptr == conn) {
                    delete ptr;
                    ptr = createConnection();
                    conn = ptr;
                    break;
                }
            }
        } catch (sql::SQLException& e) {
            throw std::runtime_error(
                "[MysqlConnectionPool] Reconnection failed: " + std::string(e.what())
            );
        }
    }

    // 封装智能指针，配置归还函数
    std::shared_ptr<sql::Connection> sp(conn, [this](sql::Connection* pConn) {
        this->releaseConnection(pConn);
    });

    return sp;
}

// 归还连接
void MysqlConnectionPool::releaseConnection(sql::Connection* conn) {
    std::unique_lock<std::mutex> lock(mutex_);
    freeConnections_.push(conn);
    lock.unlock();
    cond_.notify_one();
}

// 获取空闲连接数
int MysqlConnectionPool::getFreeConnectionCount() {
    std::unique_lock<std::mutex> lock(mutex_);
    return static_cast<int>(freeConnections_.size());
}
