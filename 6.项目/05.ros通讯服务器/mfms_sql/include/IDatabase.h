#ifndef I_DATABASE_H
#define I_DATABASE_H

#include "DataModels.h"

/**
 * @class IDatabase
 * @brief 数据库访问接口类 (Interface)
 * 定义了所有具体数据库实现（如 MysqlDatabase, RedisDatabase）必须具备的方法。
 * 使用纯虚函数实现多态，方便上层 DataManager 统一调用。
 */
class IDatabase {
public:
    virtual ~IDatabase() {}

    // 初始化数据库连接或资源
    virtual bool initialize() = 0;

    // ============ 查询接口 ============

    // 获取设备基础信息
    virtual ReturnTemplate<DeviceInfo> getDevice(const std::string& dev_id) = 0;

    // 获取设备完整状态对象
    virtual ReturnTemplate<DeviceState> getDeviceState(const std::string& dev_id) = 0;

    // 获取设备状态 JSON 中的特定字段 (支持 JSON Path)
    virtual ReturnTemplate<nlohmann::json> getDeviceStateField(const std::string& dev_id, const std::string& json_path) = 0;

    // ============ 修改接口 ============

    // 更新设备基础信息
    virtual ReturnTemplate<bool> updateDevice(const DeviceInfo& info) = 0;
    //我觉得如果每次调用这个函数都要创建个对象会有点麻烦，所以写了个重载函数可以直接传参
    virtual ReturnTemplate<bool> updateDevice(const std::string& id, const std::string& new_address) = 0;

    // 更新设备状态
	//1同时修改 state_enum 和 info
    virtual ReturnTemplate<bool> updateDeviceState(const std::string& dev_id, const std::string& state_enum, const nlohmann::json& json_obj) = 0;	
    //2单独修改 state_enum
    virtual ReturnTemplate<bool> updateDeviceStateEnum(const std::string& dev_id, const std::string& state_enum) = 0;
	//3单独修改 info
    virtual ReturnTemplate<bool> updateDeviceStateInfo(const std::string& dev_id, const nlohmann::json& json_obj) = 0;

    // 更新设备状态 JSON 中的特定字段 (支持 JSON Path)
    virtual ReturnTemplate<bool> updateDeviceStateField(const std::string& dev_id, const std::string& json_path, const nlohmann::json& new_value) = 0;

    // ============ 新增接口 ============

	// 插入新设备
    virtual ReturnTemplate<bool> addDevice(const DeviceInfo& info) = 0;
    virtual ReturnTemplate<bool> addDevice(const std::string& id, const std::string& address, long create_ts) = 0;

    // 插入新设备状态记录
    virtual ReturnTemplate<bool> addDeviceState(const DeviceState& state) = 0;
	virtual ReturnTemplate<bool> addDeviceState(const std::string& id, const std::string& state_enum, const nlohmann::json& info, int err_code) = 0;
};

#endif