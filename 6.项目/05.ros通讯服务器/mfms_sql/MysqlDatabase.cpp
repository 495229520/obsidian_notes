#include "MysqlDatabase.h"

#if __has_include(<jdbc/cppconn/prepared_statement.h>)
#include <jdbc/cppconn/prepared_statement.h>
#include <jdbc/cppconn/resultset.h>
#else
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#endif

using json = nlohmann::json;

//第一个注释写的详细一点，后面方法的都大同小异
/**
 * @brief 根据设备 ID 获取设备的基础信息 (DeviceInfo)
 * * @param dev_id 设备的唯一标识符 (对应数据库表中的 id 字段)
 * @return ReturnTemplate<DeviceInfo> 包含查询结果或错误提示的模板对象
 */
ReturnTemplate<DeviceInfo> MysqlDatabase::getDevice(const std::string& dev_id) {
    // 1. 初始化返回对象
    // ReturnTemplate 是我们自定义的通用返回结构，默认 success 为 false
    ReturnTemplate<DeviceInfo> ret;

    try {
        // =========================================================
        // 步骤 1: 获取数据库连接
        // =========================================================
        // 调用单例连接池的 getConnection() 方法。
        // 返回值是一个 std::shared_ptr<sql::Connection>。
        // 关键点：当这个 shared_ptr 超出当前函数作用域时，
        // 它定义在 MysqlConnectionPool 中的自定义删除器会被调用，自动将连接归还给池子，而不是关闭连接。
        auto conn = MysqlConnectionPool::getInstance().getConnection(); //

        // =========================================================
        // 步骤 2: 准备 SQL 语句 (PreparedStatement)
        // =========================================================
        // 使用 std::unique_ptr 管理 stmt 指针，确保函数结束或发生异常时自动释放内存 (RAII)。
        // 使用 prepareStatement 而不是普通的 Statement，主要有两个好处：
        // 1. 安全：防止 SQL 注入攻击（参数中的特殊字符会被自动转义）。
        // 2. 性能：数据库可以预编译 SQL 语句结构。
        std::unique_ptr<sql::PreparedStatement> stmt(
            conn->prepareStatement("SELECT id, address, create_ts FROM device WHERE id=?"));

        // =========================================================
        // 步骤 3: 绑定参数
        // =========================================================
        // 将传入的 dev_id 绑定到 SQL 语句中的第一个占位符 "?"。
        // 索引从 1 开始，而不是 0。
        stmt->setString(1, dev_id);

        // =========================================================
        // 步骤 4: 执行查询并获取结果集
        // =========================================================
        // executeQuery() 专门用于 SELECT 语句，返回一个 ResultSet 对象。
        // 同样使用 unique_ptr 管理 rs，防止内存泄漏。
        std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery());

        // =========================================================
        // 步骤 5: 解析结果
        // =========================================================
        // rs->next() 将游标移动到下一行。
        // 如果返回 true，说明查询到了数据（因为 ID 是唯一的，所以通常只有一行或零行）。
        if (rs->next()) {
            DeviceInfo d;

            // 从当前行中提取名为 "id" 的列，转为 string
            d.id = rs->getString("id"); //

            // 提取 "address" 列
            d.address = rs->getString("address");

            // 提取 "create_ts" 列，注意数据库中的 BIGINT 对应 C++ 的 Int64
            d.create_ts = rs->getInt64("create_ts");

            // 填充返回对象：标记成功，并将数据载入
            ret.success = true;
            ret.data = d;
        }
        else {
            // rs->next() 返回 false，说明结果集为空，即数据库中不存在该 ID
            ret.success = false;
            ret.code = 1; // 业务错误码：1 代表“未找到”
            ret.message = "Device not found";
        }
    }
    catch (std::exception& e) {
        // =========================================================
        // 异常处理
        // =========================================================
        // 捕获所有继承自 std::exception 的异常，包括 sql::SQLException。
        // 常见错误：网络断开、SQL 语法错误、连接池耗尽等。
        ret.success = false;
        // 将具体的错误堆栈信息 (what()) 记录下来，方便调试
        ret.message = "[MySQL Get Error]: " + std::string(e.what());
    }

    // 此时，conn (shared_ptr) 引用计数减一，若无其他引用，连接自动归还连接池。
    // stmt 和 rs (unique_ptr) 自动析构，释放内存。
    return ret;
}

ReturnTemplate<bool> MysqlDatabase::updateDevice(const DeviceInfo& info) {
    ReturnTemplate<bool> ret;
    try {
        auto conn = MysqlConnectionPool::getInstance().getConnection();
        std::unique_ptr<sql::PreparedStatement> stmt(
            conn->prepareStatement("UPDATE device SET address=?, create_ts=? WHERE id=?"));
        stmt->setString(1, info.address);
        stmt->setInt64(2, info.create_ts);
        stmt->setString(3, info.id);
        stmt->execute();

        ret.success = true;
        ret.data = true;
    }
    catch (std::exception& e) {
        ret.success = false;
        ret.message = "[MySQL Updata Error]: " + std::string(e.what());
    }
    return ret;
}

// 重载实现：仅更新 Address
ReturnTemplate<bool> MysqlDatabase::updateDevice(const std::string& id, const std::string& new_address) {
    ReturnTemplate<bool> ret;
    try {
        auto conn = MysqlConnectionPool::getInstance().getConnection();

        // SQL 语句只包含 address 字段
        std::unique_ptr<sql::PreparedStatement> stmt(
            conn->prepareStatement("UPDATE device SET address=? WHERE id=?"));

        stmt->setString(1, new_address); // 绑定第一个问号：新地址
        stmt->setString(2, id);          // 绑定第二个问号：ID

        // 使用 executeUpdate() 获取受影响的行数
        // 这样我们可以知道 ID 是否真的存在
        int rows_affected = stmt->executeUpdate();

        if (rows_affected == 0) {
            // 如果受影响行数为 0，说明数据库里没有这个 ID
            ret.success = false;
            ret.message = "Device ID not found: " + id;
        }
        else {
            ret.success = true;
            ret.data = true;
        }
    }
    catch (std::exception& e) {
        ret.success = false;
        ret.message = "[MySQL Update Address Error]: " + std::string(e.what());
    }
    return ret;
}

ReturnTemplate<DeviceState> MysqlDatabase::getDeviceState(const std::string& dev_id) {
    ReturnTemplate<DeviceState> ret;
    try {
        auto conn = MysqlConnectionPool::getInstance().getConnection();
        std::unique_ptr<sql::PreparedStatement> stmt(
            conn->prepareStatement("SELECT id, state, info, err_code FROM device_state WHERE id=?"));
        stmt->setString(1, dev_id);
        std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery());

        if (rs->next()) {
            DeviceState s;
            s.id = rs->getString("id");
            s.state_enum = rs->getString("state");

            //解析 String 为 JSON 对象
            std::string info_str = rs->getString("info").asStdString();
            // 1. 先判断是否为空字符串
            if (info_str.empty()) {
                s.info = json::object(); // 给一个默认的空 JSON 对象 {}
            }
            else {
                // 2. 尝试解析，并捕获解析错误
                try {
                    s.info = json::parse(info_str);
                }
                catch (json::parse_error& e) {
                    // 3. 处理错误：如果解析失败
                    // 策略：不中断程序，而是给一个空对象，并标记错误码
                    std::cerr << "[JSON Error] ID: " << s.id << " Parse failed: " << e.what() << std::endl;
                    s.info = json::object(); // 降级处理：使用空对象
                    s.err_code = -999;       // 自定义一个错误码，告诉前端数据有损坏
                }
            }

            s.err_code = rs->isNull("err_code") ? 0 : rs->getInt("err_code");
            ret.success = true;
            ret.data = s;
        }
        else {
            ret.success = false;
            ret.code = 1;
            ret.message = "State not found";
        }
    }
    catch (std::exception& e) {
        ret.success = false;
        ret.message = "[MySQL Get Error]: " + std::string(e.what());
    }
    return ret;
}

ReturnTemplate<bool> MysqlDatabase::updateDeviceStateInfo(const std::string& dev_id, const nlohmann::json& json_obj) {
    ReturnTemplate<bool> ret;
    try {
        auto conn = MysqlConnectionPool::getInstance().getConnection();
        std::unique_ptr<sql::PreparedStatement> stmt(
            conn->prepareStatement("UPDATE device_state SET info=? WHERE id=?"));

        // 存入时将 JSON 对象转为 String
        stmt->setString(1, json_obj.dump());
        stmt->setString(2, dev_id);
        stmt->execute();

        ret.success = true;
        ret.data = true;
    }
    catch (std::exception& e) {
        ret.success = false;
        ret.message = "[MySQL Updata Error]: " + std::string(e.what());
    }
    return ret;
}

// 2.只更新 State
ReturnTemplate<bool> MysqlDatabase::updateDeviceStateEnum(const std::string& dev_id, const std::string& state_enum) {
    ReturnTemplate<bool> ret;
    try {
        auto conn = MysqlConnectionPool::getInstance().getConnection();
        // SQL 极其精简，只改状态
        std::unique_ptr<sql::PreparedStatement> stmt(
            conn->prepareStatement("UPDATE device_state SET state=? WHERE id=?"));

        stmt->setString(1, state_enum);
        stmt->setString(2, dev_id);
        stmt->execute();

        ret.success = true;
        ret.data = true;
    }
    catch (std::exception& e) {
        ret.success = false;
        ret.message = "[MySQL Update Enum Error]: " + std::string(e.what());
    }
    return ret;
}

// 3.全量更新 (用于设备初始化或重要状态变更)
ReturnTemplate<bool> MysqlDatabase::updateDeviceState(const std::string& dev_id, const std::string& state_enum, const nlohmann::json& json_obj) {
    ReturnTemplate<bool> ret;
    try {
        auto conn = MysqlConnectionPool::getInstance().getConnection();
        // SQL 同时更新两个字段
        std::unique_ptr<sql::PreparedStatement> stmt(
            conn->prepareStatement("UPDATE device_state SET state=?, info=? WHERE id=?"));

        stmt->setString(1, state_enum);
        stmt->setString(2, json_obj.dump());
        stmt->setString(3, dev_id);
        stmt->execute();

        ret.success = true;
        ret.data = true;
    }
    catch (std::exception& e) {
        ret.success = false;
        ret.message = "[MySQL Update Full Error]: " + std::string(e.what());
    }
    return ret;
}

ReturnTemplate<nlohmann::json> MysqlDatabase::getDeviceStateField(const std::string& dev_id, const std::string& json_path) {
    ReturnTemplate<nlohmann::json> ret;
    try {
        auto conn = MysqlConnectionPool::getInstance().getConnection();
        // 使用 MySQL 5.7+ JSON_EXTRACT
        std::unique_ptr<sql::PreparedStatement> stmt(
            conn->prepareStatement("SELECT JSON_EXTRACT(info, ?) AS val FROM device_state WHERE id=?"));
        stmt->setString(1, json_path);
        stmt->setString(2, dev_id);
        std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery());

        if (rs->next()) {
            std::string val_str = rs->getString("val").asStdString();
            if (val_str.empty()) {
                // 如果提取结果为空，视为未找到
                ret.success = false;
                ret.message = "Field is empty";
            }
            else {
                try {
                    ret.data = json::parse(val_str);
                    ret.success = true;
                }
                catch (json::parse_error& e) {
                    // 这里如果是坏数据，直接返回失败，因为我们要查的就是这个字段
                    ret.success = false;
                    ret.message = "Invalid JSON format: " + std::string(e.what());
                }
            }
        }
        else {
            ret.success = false;
            ret.message = "Not found";
        }
    }
    catch (std::exception& e) {
        ret.success = false;
        ret.message = "[MySQL Get Error]: " + std::string(e.what());
    }
    return ret;
}

ReturnTemplate<bool> MysqlDatabase::updateDeviceStateField(const std::string& dev_id, const std::string& json_path, const nlohmann::json& new_value) {
    ReturnTemplate<bool> ret;
    try {
        auto conn = MysqlConnectionPool::getInstance().getConnection();

        // =========================================================
        // 步骤 1: 先检查路径是否存在 (Pre-check)
        // 使用 JSON_CONTAINS_PATH(json_doc, 'one', path)
        // 返回 1 表示存在，0 表示不存在
        // =========================================================
        std::unique_ptr<sql::PreparedStatement> checkStmt(
            conn->prepareStatement("SELECT JSON_CONTAINS_PATH(info, 'one', ?) FROM device_state WHERE id=?"));

        checkStmt->setString(1, json_path);
        checkStmt->setString(2, dev_id);

        std::unique_ptr<sql::ResultSet> rs(checkStmt->executeQuery());

        if (rs->next()) {
            // 获取检查结果 (0 或 1)
            int pathExists = rs->getInt(1);
            if (pathExists == 0) {
                // 核心逻辑：如果路径不存在，直接返回失败
                ret.success = false;
                ret.message = "JSON Path not found: " + json_path;
                return ret;
            }
        }
        else {
            // 如果连 ID 都查不到
            ret.success = false;
            ret.message = "Device ID not found: " + dev_id;
            return ret;
        }

        // =========================================================
        // 步骤 2: 路径确认存在后，执行更新
        // =========================================================
        std::unique_ptr<sql::PreparedStatement> stmt(
            conn->prepareStatement("UPDATE device_state SET info = JSON_REPLACE(info, ?, ?) WHERE id=?"));

        stmt->setString(1, json_path);
        stmt->setString(2, new_value.dump());
        stmt->setString(3, dev_id);

        // 执行更新
        int updateCount = stmt->executeUpdate();

        ret.success = true;
        ret.data = true;
    }
    catch (std::exception& e) {
        ret.success = false;
        ret.message = "[SQL Update Error]: " + std::string(e.what());
    }
    return ret;
}

ReturnTemplate<bool> MysqlDatabase::addDevice(const DeviceInfo& info) {
    ReturnTemplate<bool> ret;
    try {
        auto conn = MysqlConnectionPool::getInstance().getConnection();

        // 使用 INSERT INTO 语句
        std::unique_ptr<sql::PreparedStatement> stmt(
            conn->prepareStatement("INSERT INTO device (id, address, create_ts) VALUES (?, ?, ?)"));

        stmt->setString(1, info.id);
        stmt->setString(2, info.address);
        stmt->setInt64(3, info.create_ts);

        stmt->execute();

        ret.success = true;
        ret.data = true;
    }
    catch (std::exception& e) {
        ret.success = false;
        ret.message = "[MySQL Insert Error]: " + std::string(e.what());
    }
    return ret;
}

ReturnTemplate<bool> MysqlDatabase::addDevice(const std::string& id, const std::string& address, long create_ts) {
    // 1. 临时创建一个结构体对象
    DeviceInfo tempInfo;

    // 2. 将传入的三个散变量填进去
    tempInfo.id = id;
    tempInfo.address = address;
    tempInfo.create_ts = create_ts;

    // 3. 直接调用上面那个写好了 SQL 逻辑的函数
    return addDevice(tempInfo);
}

ReturnTemplate<bool> MysqlDatabase::addDeviceState(const DeviceState& state) {
    ReturnTemplate<bool> ret;
    try {
        auto conn = MysqlConnectionPool::getInstance().getConnection();

        std::unique_ptr<sql::PreparedStatement> stmt(
            conn->prepareStatement(
                "INSERT INTO device_state (id, state, info, err_code) VALUES (?, ?, ?, ?)"));

        stmt->setString(1, state.id);
        stmt->setString(2, state.state_enum);
        stmt->setString(3, state.info.dump()); // JSON 对象转字符串存储
        stmt->setInt(4, state.err_code);

        stmt->execute();

        ret.success = true;
        ret.data = true;
    }
    catch (std::exception& e) {
        ret.success = false;
        ret.message = "[MySQL Insert Error]: " + std::string(e.what());
    }
    return ret;
}

ReturnTemplate<bool> MysqlDatabase::addDeviceState(const std::string& id,
    const std::string& state_enum,
    const nlohmann::json& info,
    int err_code)
{
    // 1. 创建临时结构体
    DeviceState tempState;

    // 2. 填充数据
    tempState.id = id;
    tempState.state_enum = state_enum;
    tempState.info = info;       // nlohmann::json 支持直接赋值
    tempState.err_code = err_code;

    // 3. 复用已有逻辑
    return addDeviceState(tempState);
}
