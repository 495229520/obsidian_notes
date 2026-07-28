---
tags:
  - 研一上学期
  - 复合机器人开发-AMR底盘开发
---
# 奥博(Aubo)机械臂接口使用手册

> 面向 Qt 客户端开发者
> 版本: 1.1 | 更新日期: 2026-05-22

---

## 1. 概述

数据中台 `mfms_server` 已完成奥博(Aubo)机械臂的全链路封装。Qt 客户端开发者可以通过 `CommunicationInterface` 单例，使用 **信号与槽** 机制完成以下操作：

- 设备发现与连接/断开
- 关节点动（Jog）
- 笛卡尔点动
- 速度/加速度/模式设置
- 实时状态接收（关节位置、笛卡尔位置、速度）

### 设计原则

奥博机械臂与法奥(FR)机械臂共享同一套**控制槽函数**（`armJogJoint`、`armJogCartesian`、`armChangeMode` 等），前端调用方式完全一致，数据中台内部根据设备ID自动路由到对应的代理。

**前端不需要做设备类型推断。** 连接设备时，`connectResult` 信号会携带 `deviceType` 参数，前端据此决定 UI 布局即可（例如隐藏奥博不支持的 IO 面板）。

---

## 2. 快速入门

### 2.1 获取接口单例

```cpp
#include "mfms_server/CommunicationInterface.h"

// 获取通信接口单例（自动初始化）
auto& comm = CommunicationInterface::instance();
```

### 2.2 最小使用示例

```cpp
#include "mfms_server/CommunicationInterface.h"
#include "com_interfaces/msg/aubo_robot_state.hpp"
#include "com_interfaces/msg/fr_robot_state.hpp"

class MyWidget : public QWidget {
    Q_OBJECT
public:
    MyWidget(QWidget* parent = nullptr) : QWidget(parent) {
        auto& comm = CommunicationInterface::instance();

        // 1. 连接结果信号——连接成功时会告知设备类型
        connect(&comm, &CommunicationInterface::connectResult,
                this, &MyWidget::onConnectResult);

        // 2. 连接两种机械臂的状态信号（只有当前连接的那种会推数据）
        connect(&comm, &CommunicationInterface::sendAuboARMState,
                this, &MyWidget::onAuboState);
        connect(&comm, &CommunicationInterface::sendARMState,
                this, &MyWidget::onFrState);

        // 3. 连接控制结果信号（两种机械臂共用）
        connect(&comm, &CommunicationInterface::armControlRes,
                this, &MyWidget::onArmControlResult);

        // 4. 刷新设备列表
        comm.refreshRobotList();
    }

private slots:
    void onConnectResult(bool success, int deviceType, const QString& deviceId) {
        if (!success) {
            qWarning() << "连接失败, 设备:" << deviceId;
            return;
        }
        qDebug() << "连接成功, 设备:" << deviceId << "类型:" << deviceType;

        // 根据设备类型调整 UI
        currentDeviceType_ = deviceType;
        if (deviceType == 3 /*AuboRobot*/) {
            ioPanel_->hide();       // 奥博不支持 IO
        } else if (deviceType == 1 /*FrRobot*/) {
            ioPanel_->show();       // 法奥支持 IO
        }
    }

    void onAuboState(const com_interfaces::msg::AuboRobotState::SharedPtr msg) {
        // 奥博实时状态
        qDebug() << "速度:" << msg->robot_speed << "%";
        for (int i = 0; i < 6; ++i)
            qDebug() << "J" << i+1 << ":" << msg->jt_cur_pos[i];
    }

    void onFrState(const com_interfaces::msg::FrRobotState::SharedPtr msg) {
        // 法奥实时状态
        qDebug() << "速度:" << msg->robot_speed << "%";
        for (int i = 0; i < 6; ++i)
            qDebug() << "J" << i+1 << ":" << msg->jt_cur_pos[i];
    }

    void onArmControlResult(bool success, int errorCode, const QString& message) {
        if (!success) {
            qWarning() << "控制失败, 错误码:" << errorCode << message;
        }
    }

private:
    int currentDeviceType_ = 0;
    QWidget* ioPanel_ = nullptr;  // IO 控制面板
};
```

---

## 3. 设备识别

奥博机械臂通过设备ID前缀自动识别，**前端不需要做任何类型判断**。

| 设备ID前缀 | 示例 | 识别结果 |
|------------|------|---------|
| `robaob` | `robaob0001` | 奥博机器人 |
| `robaub` | `robaub0001` | 奥博机器人（兼容格式） |

设备列表中，奥博设备与法奥设备统一返回。连接时只需传入设备名称，数据中台自动完成类型判断和代理路由，并通过 `connectResult` 信号将设备类型告知前端。

---

## 4. API 参考

### 4.1 设备管理

#### 刷新设备列表

```cpp
// 槽函数
void refreshRobotList();

// 结果信号
void getRobotList(const QList<QString>& robotList);
```

**说明**：刷新在线设备列表，结果通过 `getRobotList` 信号返回。列表中包含所有类型设备（法奥、奥博、AGV等）。

**使用**：

```cpp
auto& comm = CommunicationInterface::instance();
connect(&comm, &CommunicationInterface::getRobotList,
        this, [](const QList<QString>& list) {
    for (const auto& name : list) {
        qDebug() << "在线设备:" << name;
    }
});
comm.refreshRobotList();
```

---

#### 连接设备

```cpp
// 槽函数
void connectRobot(const QString& name);

// 结果信号
void connectResult(bool success, int deviceType, const QString& deviceId);
```

| 参数 | 类型 | 说明 |
|------|------|------|
| `success` | `bool` | 连接是否成功 |
| `deviceType` | `int` | 设备类型枚举值（见下表） |
| `deviceId` | `QString` | 设备ID（如 `robaob0001`），失败时可能为空 |

**DeviceType 枚举值对照**：

| 值 | 类型 | 说明 |
|----|------|------|
| `0` | `Unknown` | 未知设备 |
| `1` | `FrRobot` | 法奥机器人 |
| `2` | `HsRobot` | 华数机器人 |
| `3` | `AuboRobot` | 奥博机器人 |
| `4` | `SeerAgv` | 仙工AGV |

**说明**：连接指定设备。连接成功后，`deviceType` 告知前端当前连接的设备类型，前端可据此决定：
- 连接哪个状态信号处理函数
- 显示/隐藏哪些 UI 面板（如奥博无 IO 面板）
- 是否启用某些功能按钮

**使用**：

```cpp
connect(&comm, &CommunicationInterface::connectResult,
        this, [this](bool success, int deviceType, const QString& deviceId) {
    if (!success) {
        qWarning() << "连接失败";
        return;
    }
    currentDeviceType_ = deviceType;
    if (deviceType == 3 /*AuboRobot*/) {
        ioPanel_->hide();            // 奥博不支持 IO
        emergencyLabel_->hide();     // 奥博无急停状态
    } else if (deviceType == 1 /*FrRobot*/) {
        ioPanel_->show();
        emergencyLabel_->show();
    }
});
comm.connectRobot("robaob0001");
```

---

#### 断开连接

```cpp
// 槽函数
void disconnectRobot();

// 无专用结果信号
```

**说明**：断开当前连接的设备。内部自动清理对应的代理实例。

---

### 4.2 关节点动控制

#### 关节空间点动

```cpp
// 槽函数
void armJogJoint(const int& number, const double& jogStep);

// 结果信号
void armControlRes(bool res, int errorCode, const QString& message);
```

| 参数 | 类型 | 说明 |
|------|------|------|
| `number` | `int` | 关节编号：正值=正方向，负值=负方向。例如 `1`=J1正转，`-1`=J1反转 |
| `jogStep` | `double` | 步长（度） |

**使用**：

```cpp
// J1 关节正方向点动 1.0 度
comm.armJogJoint(1, 1.0);

// J3 关节负方向点动 0.5 度
comm.armJogJoint(-3, 0.5);
```

---

#### 笛卡尔空间点动

```cpp
// 槽函数
void armJogCartesian(const int& number, const double& jogStep);

// 结果信号
void armControlRes(bool res, int errorCode, const QString& message);
```

| 参数 | 类型 | 说明 |
|------|------|------|
| `number` | `int` | 轴编号：正值=正方向，负值=负方向。`1`~`6` 对应 X, Y, Z, RX, RY, RZ |
| `jogStep` | `double` | 步长（mm 或 度） |

**使用**：

```cpp
// X 轴正方向移动 10mm
comm.armJogCartesian(1, 10.0);

// RZ 轴负方向旋转 5 度
comm.armJogCartesian(-6, 5.0);
```

---

### 4.3 模式切换

```cpp
// 槽函数
void armChangeMode(quint8 mode);

// 结果信号
void armChangeModeRes(bool res, int errorCode, const QString& message);
```

| 模式值 | 说明 |
|--------|------|
| `0` | 自动模式 |
| `1` | 手动模式 |
| `2` | 手动模式2 |
| `3` | 外部模式 |
| `4` | 拖动模式 |

**使用**：

```cpp
// 切换到手动模式
comm.armChangeMode(1);
```

---

### 4.4 状态刷新

```cpp
// 槽函数
void refreshState();

// 无专用结果信号，状态通过对应的状态信号持续推送
```

**说明**：请求立即刷新设备状态。通常在模式切换后调用以获取最新状态。

---

### 4.5 实时状态接收

两种机械臂的状态通过**不同的信号**推送，数据中台根据已连接的设备类型自动选择推送哪个信号：

```cpp
// 奥博机械臂状态信号
void sendAuboARMState(const com_interfaces::msg::AuboRobotState::SharedPtr msg);

// 法奥机械臂状态信号
void sendARMState(const com_interfaces::msg::FrRobotState::SharedPtr msg);
```

**推荐用法**：两个信号都连接，只有当前连接的设备类型会推送数据，不会交叉触发。前端可在 `connectResult` 回调中记录 `deviceType`，然后在状态回调中做差异化处理（如是否更新 IO 显示）。

```cpp
// 构造函数中同时连接两个状态信号
connect(&comm, &CommunicationInterface::sendAuboARMState,
        this, &MyWidget::onAuboState);
connect(&comm, &CommunicationInterface::sendARMState,
        this, &MyWidget::onFrState);
```

---

## 5. AuboRobotState 消息字段

`com_interfaces::msg::AuboRobotState` 的完整字段定义：

| 字段 | 类型 | 说明 |
|------|------|------|
| `robot_index` | `int32` | 机械臂编号（多臂场景下区分） |
| `robot_name` | `string` | 机械臂名称/任务描述 |
| `robot_type` | `string` | 机械臂型号 |
| `robot_soft_index` | `string` | 软件版本 |
| `robot_ip` | `string` | 机器人IP地址 |
| `id` | `int32` | 指令ID |
| `robot_speed` | `float64` | 当前速度（百分比，0~100） |
| `jt_cur_pos[6]` | `float64[6]` | 六轴关节角度（度）：J1, J2, J3, J4, J5, J6 |
| `tl_cur_pos[6]` | `float64[6]` | 笛卡尔位置：X(mm), Y(mm), Z(mm), RX(度), RY(度), RZ(度) |

### 与 FrRobotState 的差异

奥博状态消息相比法奥更精简，**不包含**以下字段：

| 法奥独有字段 | 说明 |
|-------------|------|
| `curtask_index` | 当前任务编号 |
| `curstep_index` | 当前步骤编号 |
| `program_state` | 程序运行状态 |
| `robot_motion_done` | 运动完成标志 |
| `robot_err_code` | 错误码 |
| `robot_mode` | 当前模式 |
| `cl_dgt_output_h/l` | 控制箱数字量IO输出 |
| `cl_dgt_input_h/l` | 控制箱数字量IO输入 |
| `cl_analog_input[2]` | 控制箱模拟输入 |
| `cl_analog_output[2]` | 控制箱模拟输出 |
| `tl_dgt_output_l` | 工具数字量IO输出 |
| `tl_dgt_input_l` | 工具数字量IO输入 |
| `emergency_stop` | 急停标志 |

前端可利用 `connectResult` 中的 `deviceType` 来判断当前设备是否具备这些字段，而不需要通过消息内容倒推设备类型。

---

## 6. 不支持的操作

以下操作对奥博机械臂**不可用**，调用后会收到错误回调：

| 操作 | 错误码 | 说明 |
|------|--------|------|
| IO 读写（`setIO` / `getIO`） | `-2006` (`UNSUPPORTED_OPERATION`) | 奥博接口不支持IO控制 |

当调用这些操作时，会通过 `armControlRes` 信号返回失败结果：

```cpp
void onArmControlResult(bool success, int errorCode, const QString& message) {
    if (errorCode == -2006) {
        qWarning() << "当前设备不支持此操作:" << message;
    }
}
```

**建议**：在 `connectResult` 回调中判断 `deviceType == 3 (AuboRobot)` 时，直接禁用 IO 相关按钮，避免用户触发不支持的操作。

---

## 7. 错误码汇总

| 错误码 | 常量名 | 说明 |
|--------|--------|------|
| `0` | `SUCCESS` | 操作成功 |
| `-2001` | `NOT_RUNNING` | 代理适配器未启动 |
| `-2002` | `DEVICE_NOT_CONNECTED` | 设备未连接 |
| `-2003` | `PROXY_CREATE_FAILED` | 代理实例创建失败 |
| `-2004` | `CONNECT_FAILED` | 连接设备失败 |
| `-2005` | `EXECUTOR_ERROR` | ROS Executor 异常 |
| `-2006` | `UNSUPPORTED_OPERATION` | 设备不支持此操作（如奥博的IO控制） |
| `3005` | — | 服务调用失败 |
| `3008` | — | 参数无效 |

错误信息通过以下信号返回：

```cpp
void armControlRes(bool res, int errorCode, const QString& message);
void armChangeModeRes(bool res, int errorCode, const QString& message);
void errorOccurred(int code, const QString& message);
```

---

## 8. 完整信号一览

### 需要连接的信号

| 信号 | 用途 | 必须连接 |
|------|------|---------|
| `connectResult(bool, int, QString)` | 连接结果（含设备类型和设备ID） | **是** |
| `sendAuboARMState(AuboRobotState::SharedPtr)` | 奥博实时状态推送 | **是** |
| `sendARMState(FrRobotState::SharedPtr)` | 法奥实时状态推送 | 如需支持法奥 |
| `armControlRes(bool, int, QString)` | 点动/使能等控制结果 | 是 |
| `armChangeModeRes(bool, int, QString)` | 模式切换结果 | 是 |
| `errorOccurred(int, QString)` | 全局错误通知 | 推荐 |
| `getRobotList(QList<QString>)` | 设备列表更新 | 按需 |

### 可调用的槽函数

| 槽函数 | 用途 |
|--------|------|
| `refreshRobotList()` | 刷新在线设备列表 |
| `connectRobot(QString)` | 连接设备 |
| `disconnectRobot()` | 断开当前设备 |
| `armJogJoint(int, double)` | 关节空间点动 |
| `armJogCartesian(int, double)` | 笛卡尔空间点动 |
| `armChangeMode(quint8)` | 切换操作模式 |
| `refreshState()` | 请求状态刷新 |

---

## 9. 同时支持法奥和奥博的代码模板

如果界面需要同时适配两种机械臂，推荐以下模式：

```cpp
class RobotControlPanel : public QWidget {
    Q_OBJECT

public:
    RobotControlPanel(QWidget* parent = nullptr) : QWidget(parent) {
        auto& comm = CommunicationInterface::instance();

        // 连接结果信号——连接成功时根据设备类型调整 UI
        connect(&comm, &CommunicationInterface::connectResult,
                this, &RobotControlPanel::onConnectResult);

        // 同时连接两种机械臂的状态信号（只有当前设备类型会推数据）
        connect(&comm, &CommunicationInterface::sendARMState,
                this, &RobotControlPanel::onFrState);
        connect(&comm, &CommunicationInterface::sendAuboARMState,
                this, &RobotControlPanel::onAuboState);

        // 控制结果信号是共用的
        connect(&comm, &CommunicationInterface::armControlRes,
                this, &RobotControlPanel::onControlResult);
        connect(&comm, &CommunicationInterface::armChangeModeRes,
                this, &RobotControlPanel::onModeChangeResult);
    }

private slots:
    // ==================== 连接结果 ====================
    void onConnectResult(bool success, int deviceType, const QString& deviceId) {
        if (!success) {
            statusLabel_->setText("连接失败");
            return;
        }
        currentDeviceType_ = deviceType;
        statusLabel_->setText(QString("已连接: %1").arg(deviceId));

        // 根据设备类型切换 UI 布局
        bool isFr   = (deviceType == 1);  // FrRobot
        bool isAubo = (deviceType == 3);  // AuboRobot

        ioPanel_->setVisible(isFr);             // 仅法奥显示 IO 面板
        emergencyLabel_->setVisible(isFr);       // 仅法奥显示急停状态
        programStateLabel_->setVisible(isFr);    // 仅法奥显示程序状态
        // 点动、模式切换等面板两种臂都可用，无需判断
    }

    // ==================== 状态回调 ====================
    void onFrState(const com_interfaces::msg::FrRobotState::SharedPtr msg) {
        updateJointDisplay(msg->jt_cur_pos);
        updateCartesianDisplay(msg->tl_cur_pos);
        updateSpeedDisplay(msg->robot_speed);
        // FR 独有
        updateIODisplay(msg->cl_dgt_output_l, msg->cl_dgt_input_l);
        emergencyLabel_->setText(msg->emergency_stop ? "急停!" : "正常");
    }

    void onAuboState(const com_interfaces::msg::AuboRobotState::SharedPtr msg) {
        updateJointDisplay(msg->jt_cur_pos);
        updateCartesianDisplay(msg->tl_cur_pos);
        updateSpeedDisplay(msg->robot_speed);
        // 奥博：无 IO / 急停 / 程序状态
    }

    // ==================== 控制结果 ====================
    void onControlResult(bool success, int errorCode, const QString& message) {
        if (!success) {
            if (errorCode == -2006) {
                // UNSUPPORTED_OPERATION — 奥博不支持 IO
                statusLabel_->setText("当前设备不支持此操作");
            } else {
                statusLabel_->setText(QString("操作失败: %1").arg(message));
            }
        }
    }

    void onModeChangeResult(bool success, int errorCode, const QString& message) {
        Q_UNUSED(errorCode);
        Q_UNUSED(message);
        if (success) {
            CommunicationInterface::instance().refreshState();
        }
    }

private:
    void updateJointDisplay(const auto& joints) {
        for (int i = 0; i < 6; ++i)
            jointLabels_[i]->setText(QString::number(joints[i], 'f', 3));
    }

    void updateCartesianDisplay(const auto& cart) {
        // cart[0..2] = X, Y, Z (mm)
        // cart[3..5] = RX, RY, RZ (度)
        for (int i = 0; i < 6; ++i)
            cartLabels_[i]->setText(QString::number(cart[i], 'f', 3));
    }

    void updateSpeedDisplay(double speed) {
        speedBar_->setValue(static_cast<int>(speed));
    }

    int currentDeviceType_ = 0;
    QLabel* statusLabel_ = nullptr;
    QLabel* emergencyLabel_ = nullptr;
    QLabel* programStateLabel_ = nullptr;
    QWidget* ioPanel_ = nullptr;
    QLabel* jointLabels_[6] = {};
    QLabel* cartLabels_[6] = {};
    QProgressBar* speedBar_ = nullptr;
};
```

---

## 10. 注意事项

1. **线程安全**：`CommunicationInterface` 的所有槽函数可在 UI 线程直接调用，内部通过 `Qt::QueuedConnection` 自动跨线程分发到工作线程。

2. **连接时获取设备类型**：`connectResult(bool success, int deviceType, const QString& deviceId)` 在连接成功时携带设备类型。前端应在此回调中记录 `deviceType`，用于后续 UI 适配。**不需要**自行解析设备ID来推断类型。

3. **状态信号自动分流**：连接奥博设备后只有 `sendAuboARMState` 会推数据，`sendARMState` 不会触发（反之亦然）。前端可以同时 connect 两个信号，无需担心交叉触发。

4. **IO 功能不可用**：奥博接口目前不支持 IO 读写操作。建议在 `connectResult` 回调中判断 `deviceType == 3` 时直接隐藏/禁用 IO 面板，而非等用户点击后才报错。

5. **控制接口统一**：`armJogJoint`、`armJogCartesian`、`armChangeMode` 等控制槽函数对两种臂的调用方式完全相同，底层差异（如奥博用 `setSpeed(double)` 而法奥用 `setVec(int32_t)`）由数据中台透明处理。

6. **初始化时序**：首次调用 `CommunicationInterface::instance()` 会触发初始化（包括 ROS 节点创建、Gateway 启动），此过程最长耗时 10 秒。建议在应用启动阶段尽早调用。

7. **元类型注册**：`com_interfaces::msg::AuboRobotState::SharedPtr` 已在数据中台内部注册为 Qt 元类型，客户端**无需**手动调用 `qRegisterMetaType`。

---

## 附录 A: 头文件引用

```cpp
// 通信接口（必须）
#include "mfms_server/CommunicationInterface.h"

// 奥博状态消息（如需直接使用消息字段）
#include "com_interfaces/msg/aubo_robot_state.hpp"

// 法奥状态消息（如需同时支持法奥）
#include "com_interfaces/msg/fr_robot_state.hpp"
```

## 附录 B: CMakeLists.txt 依赖

确保客户端 `CMakeLists.txt` 中包含以下依赖：

```cmake
find_package(com_interfaces REQUIRED)
find_package(mfms_server REQUIRED)

target_link_libraries(your_target
    mfms_server::client_api
    ${com_interfaces_LIBRARIES}
)
```
