/**
 * @file MysqlConnectionPool.h
 * @brief MySQL 连接池 (Robust Version)
 *
 * 提供线程安全的 MySQL 连接池，支持连接有效性检测、自动重连机制和超时处理。
 * 支持通过配置结构体进行参数化初始化。
 */
#ifndef MYSQL_CONNECTION_POOL_H
#define MYSQL_CONNECTION_POOL_H

#if __has_include(<jdbc/mysql_driver.h>)
#include <jdbc/mysql_driver.h>
#include <jdbc/mysql_connection.h>
#else
#include <cppconn/driver.h>
#include <cppconn/connection.h>
#endif
#include <mutex>
#include <queue>
#include <condition_variable>
#include <vector>
#include <memory>
#include <atomic>
#include <string>
#include <cstdint>

/**
 * @struct MysqlPoolConfig
 * @brief MySQL 连接池配置结构体
 *
 * 用于初始化连接池的所有配置参数。
 */
struct MysqlPoolConfig {
    std::string host{"127.0.0.1"};       // 数据库主机地址
    uint16_t port{3306};                  // 数据库端口
    std::string database{"mfms_data"};    // 数据库名称
    std::string user{"root"};             // 用户名
    std::string password;                 // 密码
    int maxPoolSize{8};                   // 连接池最大连接数
    int waitTimeoutMs{3000};              // 获取连接最大等待时间（毫秒）
};

/**
 * @class MysqlConnectionPool
 * @brief MySQL 连接池单例类
 *
 * 提供线程安全的数据库连接管理，支持：
 * - 连接池化复用
 * - 连接有效性检测 (isValid/ping)
 * - 自动重连机制
 * - 超时等待处理
 * - 配置化初始化
 */
class MysqlConnectionPool {
public:
    /**
     * @brief 获取单例实例
     * @return 连接池单例引用
     */
    static MysqlConnectionPool& getInstance();

    /**
     * @brief 使用配置初始化连接池
     * @param config 连接池配置
     * @throw std::runtime_error 如果初始化失败
     * @note 必须在首次 getConnection() 之前调用，且只能调用一次
     */
    static void initializePool(const MysqlPoolConfig& config);

    /**
     * @brief 检查连接池是否已初始化
     * @return true 如果已初始化
     */
    static bool isInitialized();

    /**
     * @brief 获取一个有效连接
     * @return shared_ptr<sql::Connection> 自动归还的连接智能指针
     * @throw std::runtime_error 如果连接池未初始化、无法获取连接或等待超时
     */
    std::shared_ptr<sql::Connection> getConnection();

    /**
     * @brief 归还连接到池中
     * @param conn 要归还的连接指针
     */
    void releaseConnection(sql::Connection* conn);

    /**
     * @brief 获取当前空闲连接数
     * @return 空闲连接数量
     */
    int getFreeConnectionCount();

    /**
     * @brief 获取当前配置（只读）
     * @return 连接池配置的常量引用
     */
    const MysqlPoolConfig& getConfig() const;

    // 禁止拷贝和赋值
    MysqlConnectionPool(const MysqlConnectionPool&) = delete;
    MysqlConnectionPool& operator=(const MysqlConnectionPool&) = delete;

private:
    MysqlConnectionPool();
    ~MysqlConnectionPool();

    /**
     * @brief 创建一个新的数据库连接
     * @return 新创建的连接指针
     * @throw sql::SQLException 如果连接失败
     */
    sql::Connection* createConnection();

    /**
     * @brief 初始化连接池（内部方法）
     * @note 必须在持有锁的情况下调用
     */
    void initPoolIfEmpty();

private:
    sql::Driver* driver_{nullptr};

    // 保存所有连接的指针（用于析构清理）
    std::vector<sql::Connection*> allConnections_;

    // 空闲连接队列
    std::queue<sql::Connection*> freeConnections_;

    // 配置参数
    MysqlPoolConfig config_;

    // 初始化标志
    std::atomic<bool> initialized_{false};

    // 同步原语
    mutable std::mutex mutex_;
    std::condition_variable cond_;
};

#endif // MYSQL_CONNECTION_POOL_H
