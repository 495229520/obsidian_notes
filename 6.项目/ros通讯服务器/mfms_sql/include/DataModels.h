#ifndef DATA_MODELS_H
#define DATA_MODELS_H

#include <string>
#include "nlohmann/json.hpp"

/**
 * @brief 存储目标枚举
 * 用于在 DataManager 中通过位掩码（Bitmask）控制数据写入的去向。
 * 例如：BOTH = MYSQL | REDIS
 */
enum class StorageTarget {
    MYSQL = 1,  // 仅操作 MySQL
    REDIS = 2,  // 仅操作 Redis
    BOTH = 3    // 同时操作 MySQL 和 Redis
};

/**
 * @brief 设备基础信息结构体
 * 对应数据库中的 device 表结构
 */
struct DeviceInfo {
    std::string id;       // 设备唯一标识符 (Primary Key)
    std::string address;  // 设备物理/网络地址
    long create_ts;       // 创建时间戳
};

/**
 * @brief 设备状态结构体
 * 对应数据库中的 device_state 表结构，包含半结构化的 JSON 数据
 */
struct DeviceState {
    std::string id;         // 设备 ID
    std::string state_enum; // 状态枚举字符串 (e.g., "online", "offline")
    nlohmann::json info;    // 详细状态信息 (JSON 对象)
    int err_code;           // 错误码 (0 表示正常)
};

/**
 * @brief 统一返回模板
 * 封装所有数据库操作的返回结果，统一错误处理方式。
 * @tparam T 返回的数据载荷类型
 */
template <typename T>
struct ReturnTemplate {
    bool success = false;   // 操作是否成功
    int code = -1;          // 状态码 (0通常代表成功，非0代表特定的业务错误)
    std::string message;    // 错误描述或提示信息
    T data;                 // 实际承载的数据
};

#endif