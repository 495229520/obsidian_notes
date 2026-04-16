#ifndef MYSQL_DATABASE_H
#define MYSQL_DATABASE_H

#include "IDatabase.h"
#include "MysqlConnectionPool.h"

/**
 * @class MysqlDatabase
 * @brief MySQL 数据库实现类
 * 继承自 IDatabase，负责将具体的 CRUD 操作转换为 MySQL 的 PreparedStatement 调用。
 * 内部不持有连接，而是按需向连接池申请。
 */
class MysqlDatabase : public IDatabase {
public:
    MysqlDatabase() {}
    ~MysqlDatabase() {}

    bool initialize() override { return true; } // 连接池已在单例中初始化

    // 具体的接口实现，参见 IDatabase.h 中的定义
    ReturnTemplate<DeviceInfo> getDevice(const std::string& dev_id) override;
    ReturnTemplate<bool> updateDevice(const DeviceInfo& info) override;
    ReturnTemplate<bool> updateDevice(const std::string& id, const std::string& new_address) override;

    ReturnTemplate<DeviceState> getDeviceState(const std::string& dev_id) override;
    ReturnTemplate<bool> updateDeviceState(const std::string& dev_id, const std::string& state_enum, const nlohmann::json& json_obj)override;
    ReturnTemplate<bool> updateDeviceStateEnum(const std::string& dev_id, const std::string& state_enum)override;
    ReturnTemplate<bool> updateDeviceStateInfo(const std::string& dev_id, const nlohmann::json& json_obj)override;

    ReturnTemplate<nlohmann::json> getDeviceStateField(const std::string& dev_id, const std::string& json_path) override;
    ReturnTemplate<bool> updateDeviceStateField(const std::string& dev_id, const std::string& json_path, const nlohmann::json& new_value) override;

    ReturnTemplate<bool> addDevice(const DeviceInfo& info) override;
    ReturnTemplate<bool> addDevice(const std::string& id, const std::string& address, long create_ts) override;
    ReturnTemplate<bool> addDeviceState(const DeviceState& state) override;
    ReturnTemplate<bool> addDeviceState(const std::string& id, const std::string& state_enum, const nlohmann::json& info, int err_code) override;
};

#endif