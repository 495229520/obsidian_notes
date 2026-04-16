#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

#include "IDatabase.h"
#include "MysqlDatabase.h"
#include <memory>

/**
 * @class DataManager
 * @brief 数据管理门面类 (Facade)
 * 封装了 MySQL 和 Redis 的实例。
 * 上层业务代码通过此类进行所有数据交互，可根据参数控制数据写入到哪个存储后端。
 */
class DataManager {
public:
    DataManager();
    ~DataManager();

    ReturnTemplate<DeviceInfo> getDevice(const std::string& dev_id);
    ReturnTemplate<bool> updateDevice(const DeviceInfo& info);
    ReturnTemplate<bool> updateDevice(const std::string& id, const std::string& new_address);

    ReturnTemplate<DeviceState> getDeviceState(const std::string& dev_id);
    // 注意：部分 Update 接口增加了 StorageTarget 参数，用于控制双写策略
    //1同时修改 state_enum 和 info
    ReturnTemplate<bool> updateDeviceState(const std::string& dev_id, const std::string& state_enum, const nlohmann::json& json_obj, StorageTarget target);
    //2单独修改 state_enum
    ReturnTemplate<bool> updateDeviceStateEnum(const std::string& dev_id, const std::string& state_enum, StorageTarget target);
    //3单独修改 info
    ReturnTemplate<bool> updateDeviceStateInfo(const std::string& dev_id, const nlohmann::json& json_obj, StorageTarget target);

    ReturnTemplate<nlohmann::json> getDeviceStateField(const std::string& dev_id, const std::string& json_path);
    ReturnTemplate<bool> updateDeviceStateField(const std::string& dev_id, const std::string& json_path, const nlohmann::json& new_value, StorageTarget target);

    ReturnTemplate<bool> addDevice(const DeviceInfo& info);
    ReturnTemplate<bool> addDevice(const std::string& id, const std::string& address, long create_ts);

    ReturnTemplate<bool> addDeviceState(const DeviceState& state);
    ReturnTemplate<bool> addDeviceState(const std::string& id, const std::string& state_enum, const nlohmann::json& info, int err_code);

private:
    std::unique_ptr<IDatabase> mysqlDb_; // MySQL 实现实例
    std::unique_ptr<IDatabase> redisDb_; // Redis 实现实例
};

#endif