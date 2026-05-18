/**
 * @file DataManager.cpp
 * @brief 数据管理器实现
 */
#include "include/DataManager.h"
#include "include/MysqlDatabase.h"
#include "include/RedisDatabase.h"

// 构造函数：初始化所有具体的数据库实现
DataManager::DataManager() {
    mysqlDb_ = std::make_unique<MysqlDatabase>();
    redisDb_ = std::make_unique<RedisDatabase>();

    mysqlDb_->initialize();
    redisDb_->initialize();
}

DataManager::~DataManager() {}

ReturnTemplate<DeviceInfo> DataManager::getDevice(const std::string& dev_id) {
    // 策略：直接读 MySQL
    return mysqlDb_->getDevice(dev_id);
}

ReturnTemplate<bool> DataManager::updateDevice(const DeviceInfo& info) {
    // 策略：直接写 MySQL
    return mysqlDb_->updateDevice(info);
}

ReturnTemplate<bool> DataManager::updateDevice(const std::string& id, const std::string& new_address) {
    // 策略：直接写 MySQL
    return mysqlDb_->updateDevice(id, new_address);
}

ReturnTemplate<DeviceState> DataManager::getDeviceState(const std::string& dev_id) {
    // 策略：直接读 MySQL (未来可加入 Redis 缓存命中逻辑)
    return mysqlDb_->getDeviceState(dev_id);
}

// 核心逻辑：根据 target 参数决定写入哪些数据库
ReturnTemplate<bool> DataManager::updateDeviceState(
    const std::string& dev_id,
    const std::string& state_enum,
    const nlohmann::json& json_obj,
    StorageTarget target)
{
    ReturnTemplate<bool> ret;
    ret.success = true;

    // 检查位掩码：如果包含 MYSQL 标志，则写入 MySQL
    if ((int)target & (int)StorageTarget::MYSQL) {
        auto r = mysqlDb_->updateDeviceState(dev_id,state_enum, json_obj);
        if (!r.success) {
            ret.success = false;
            ret.message += "[MySQL]: " + r.message + "; ";
        }
    }

    // 检查位掩码：如果包含 REDIS 标志，则写入 Redis
    if ((int)target & (int)StorageTarget::REDIS) {
        auto r = redisDb_->updateDeviceState(dev_id,state_enum, json_obj);
        // 如果 Redis 失败是否视为总失败，取决于业务要求，这里暂不影响 success
    }

    return ret;
}

ReturnTemplate<bool> DataManager::updateDeviceStateEnum(
    const std::string& dev_id,
    const std::string& state_enum,
    StorageTarget target)
{
    ReturnTemplate<bool> ret;
    ret.success = true;

    // 检查位掩码：如果包含 MYSQL 标志，则写入 MySQL
    if ((int)target & (int)StorageTarget::MYSQL) {
        auto r = mysqlDb_->updateDeviceStateEnum(dev_id, state_enum);
        if (!r.success) {
            ret.success = false;
            ret.message += "[MySQL]: " + r.message + "; ";
        }
    }

    // 检查位掩码：如果包含 REDIS 标志，则写入 Redis
    if ((int)target & (int)StorageTarget::REDIS) {
        auto r = redisDb_->updateDeviceStateEnum(dev_id, state_enum);
        // 如果 Redis 失败是否视为总失败，取决于业务要求，这里暂不影响 success
    }

    return ret;
}

ReturnTemplate<bool> DataManager::updateDeviceStateInfo(
    const std::string& dev_id,
    const nlohmann::json& json_obj,
    StorageTarget target)
{
    ReturnTemplate<bool> ret;
    ret.success = true;

    // 检查位掩码：如果包含 MYSQL 标志，则写入 MySQL
    if ((int)target & (int)StorageTarget::MYSQL) {
        auto r = mysqlDb_->updateDeviceStateInfo(dev_id, json_obj);
        if (!r.success) {
            ret.success = false;
            ret.message += "[MySQL]: " + r.message + "; ";
        }
    }

    // 检查位掩码：如果包含 REDIS 标志，则写入 Redis
    if ((int)target & (int)StorageTarget::REDIS) {
        auto r = redisDb_->updateDeviceStateInfo(dev_id, json_obj);
        // 如果 Redis 失败是否视为总失败，取决于业务要求，这里暂不影响 success
    }

    return ret;
}

ReturnTemplate<nlohmann::json> DataManager::getDeviceStateField(
    const std::string& dev_id,
    const std::string& json_path)
{
    return mysqlDb_->getDeviceStateField(dev_id, json_path);
}

ReturnTemplate<bool> DataManager::updateDeviceStateField(
    const std::string& dev_id,
    const std::string& json_path,
    const nlohmann::json& new_value,
    StorageTarget target)
{
    ReturnTemplate<bool> ret;
    ret.success = true;

    // 同样的双写逻辑
    if ((int)target & (int)StorageTarget::MYSQL) {
        auto r = mysqlDb_->updateDeviceStateField(dev_id, json_path, new_value);
        if (!r.success) {
            ret.success = false;
            ret.message += "[MySQL]: " + r.message;
        }
    }

    if ((int)target & (int)StorageTarget::REDIS) {
        redisDb_->updateDeviceStateField(dev_id, json_path, new_value);
    }

    return ret;
}

ReturnTemplate<bool> DataManager::addDevice(const DeviceInfo& info) {
    return mysqlDb_->addDevice(info);
}

ReturnTemplate<bool> DataManager::addDevice(const std::string& id, const std::string& address, long create_ts) {
	return mysqlDb_->addDevice(id, address, create_ts);
}

ReturnTemplate<bool> DataManager::addDeviceState(const DeviceState& state) {
    return mysqlDb_->addDeviceState(state);
}

ReturnTemplate<bool> DataManager::addDeviceState(const std::string& id, const std::string& state_enum, const nlohmann::json& info, int err_code) {
	return mysqlDb_->addDeviceState(id, state_enum, info, err_code);
}
