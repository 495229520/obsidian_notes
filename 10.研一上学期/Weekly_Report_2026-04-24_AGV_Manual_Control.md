# MFMS 数据中台 AGV 手动控制联调周报

> 日期: 2026-04-24
> 范围: `src/mfms_server`、`src/com_interfaces`、`HyRMS_export_202601251449_bszydxh-HP/hyrms_export`
> 重点: `CommunicationInterface.h` 中 AGV 相关接口到 `MFMS_BASE`、下位机代理层、AGV 模拟设备的链路验证

## 1. 本周结论

本周已完成 AGV 手动控制链路的适配、构建和联调验证。当前上位机和下位机通过本机 `MFMS_BASE` 交换设备信息，下位机已运行，测试侧只从 `CommunicationInterface.h` 的公开接口发起调用。

本次验证结果表明:

- `CommunicationInterface.h` 的 AGV 设备发现、连接、站点查询、到站执行、手动前进、手动后退、左转、右转、停止手动控制接口均已打通。
- AGV 手动控制已从 Qt 接口下沉到 `AgvProxy::SeerCtrl::startManualCtrl(...)`。
- 停止手动控制已下沉到 `AgvProxy::SeerCtrl::stopManualCtrl()`。
- 当前测试目标设备为 `MFMS_BASE` 中发现的 `agvSrc0001`。

## 2. 本周完成内容

### 2.1 适配新版下位机导出包

针对新版 `HyRMS_export_202601251449_bszydxh-HP/hyrms_export`，完成 AGV 手动控制调用链适配:

```text
CommunicationInterfaceImpl
  -> CommunicationWorker
  -> MfmsGatewayImpl
  -> MfmsCommandService
  -> AgvProxyAdapter
  -> AgvProxy::SeerCtrl
  -> hyrms_export / ROS service / 下位机 AGV
```

关键适配点:

- 前进/后退将速度、距离、方向转换为 `manual_x` 和 `manual_duration`。
- 左转/右转将角速度由 `deg/s` 转换为 `rad/s` 后写入 `manual_w`。
- `manual_duration` 根据 `distance / speed` 或 `angle / angular_speed` 计算。
- 新增 `stopAgvManualControl()`，用于显式停止手动控制。

### 2.2 修复构建与接口不一致问题

为适配新版 `com_interfaces` 和导出包，完成以下处理:

- `com_interfaces` 改为显式列出当前存在的 `msg/srv` 文件，避免旧接口残留参与构建。
- `mfms_server` 移除对旧构建目录头文件的回退依赖。
- 移除 `HsRobotState` 相关订阅/转换分支，避免新版接口删除后链接失败。
- 保持 AGV 中台最小可编译、可联调范围。

### 2.3 增加联调测试

新增测试程序:

- `src/mfms_server/tests/test_agv_manual_interface.cpp`

该测试只从 `CommunicationInterface.h` 的公开接口发起调用，覆盖:

- 刷新设备列表
- 连接 AGV
- AGV 前进
- AGV 后退
- AGV 左转
- AGV 右转
- 停止手动控制

同时补充 `int32_t` Qt 元类型注册，修复跨线程命令结果信号投递时的类型注册问题。

## 3. 构建与联调结果

### 3.1 构建验证

执行命令:

```bash
source /opt/ros/humble/setup.bash
colcon build --packages-select com_interfaces mfms_server --event-handlers console_direct+
```

结果:

- `com_interfaces` 构建成功
- `mfms_server` 构建成功

### 3.2 AGV 手动控制接口联调

执行测试:

```bash
source /opt/ros/humble/setup.bash
source install/setup.bash
ROS_LOG_DIR=/tmp/codex_ros_logs install/mfms_server/lib/mfms_server/test_agv_manual_interface
```

联调结果:

- 从 `MFMS_BASE` 发现 AGV 设备 `agvSrc0001`、`agvSrc9001`
- 连接 `agvSrc0001` 成功
- `agvMoveForward(0.2, 0.2)` 成功
- `agvMoveBackward(0.2, 0.2)` 成功
- `agvTurnLeft(10.0, 10.0)` 成功
- `agvTurnRight(10.0, 10.0)` 成功
- `stopAgvManualControl()` 成功

测试日志中可确认底层调用:

- `startManualCtrl()` 被前进、后退、左转、右转调用
- `stopManualCtrl()` 被停止手动控制调用

## 4. CommunicationInterface.h 已测试成功接口表

| 接口/信号 | 类型 | 功能 | 测试方式 | 测试结果 |
|-----------|------|------|----------|----------|
| `refreshRobotList()` | slot | 刷新设备列表，从 `MFMS_BASE` 获取当前可用设备 | 调用后等待 `getRobotList(...)` | 成功，返回 `agvSrc0001`、`agvSrc9001`、`robFro0001` |
| `getRobotList(const QList<QString>&)` | signal | 向上层返回设备列表 | `refreshRobotList()` 后监听信号 | 成功，AGV 设备可被识别 |
| `connectRobot(const QString& name)` | slot | 连接指定设备；AGV 场景下创建 AGV proxy 并订阅状态 | 使用 `agvSrc0001` 调用 | 成功 |
| `connectResult(const bool& res)` | signal | 返回连接结果 | `connectRobot("agvSrc0001")` 后监听信号 | 成功，返回 `true` |
| `sendAGVState(const SeerCtrlState::SharedPtr msg)` | signal | 返回 AGV 实时状态 | 连接 AGV 后订阅 `/agvSrc0001_state` | 成功，站点流程测试中收到状态并到达目标站点 |
| `getStations()` | slot | 查询 AGV 站点列表 | 连接 AGV 后调用 | 成功，返回 `AP71` 到 `AP80` 等站点 |
| `returnStations(const QList<QString>&)` | signal | 返回站点列表 | `getStations()` 后监听信号 | 成功 |
| `exeToStation(QString& stationName)` | slot | 执行 AGV 到指定站点 | 使用站点列表中的目标站点调用 | 成功 |
| `returnExeToStationRes(bool flag)` | signal | 返回执行到站命令结果 | `exeToStation(...)` 后监听信号 | 成功，返回 `true` |
| `agvMoveForward(const double& speed, const double& distance)` | slot | AGV 按速度和距离前进 | 调用 `agvMoveForward(0.2, 0.2)` | 成功，底层调用 `startManualCtrl()` |
| `agvMoveBackward(const double& speed, const double& distance)` | slot | AGV 按速度和距离后退 | 调用 `agvMoveBackward(0.2, 0.2)` | 成功，底层调用 `startManualCtrl()` |
| `agvTurnLeft(const double& speed, const double& angle)` | slot | AGV 按角速度和角度左转 | 调用 `agvTurnLeft(10.0, 10.0)` | 成功，底层调用 `startManualCtrl()` |
| `agvTurnRight(const double& speed, const double& angle)` | slot | AGV 按角速度和角度右转 | 调用 `agvTurnRight(10.0, 10.0)` | 成功，底层调用 `startManualCtrl()` |
| `stopAgvManualControl()` | slot | 停止 AGV 手动控制 | 调用 `stopAgvManualControl()` | 成功，底层调用 `stopManualCtrl()` |
| `agvControlRes(bool res, int errorCode, const QString& message)` | signal | 返回 AGV 控制命令结果 | 监听前进、后退、左转、右转、停止的结果 | 成功，所有手动控制命令返回 `res=true,errorCode=0` |

## 5. 当前遗留问题

当前 AGV 手动控制主链路已打通，但仍有两个后续可优化点:

- 进程退出时偶现 `QSqlDatabasePrivate::removeDatabase: connection ... is still in use` 警告，说明数据库连接释放顺序还可以进一步整理。
- 本次重点验证 AGV 设备发现、连接、站点执行和手动控制；路径规划相关接口未纳入本次手动控制联调表。

## 6. 下周计划

- 将 `test_agv_manual_interface` 纳入后续 AGV 回归测试清单。
- 单独清理 `MfmsRosBridge` / 数据库连接的释放顺序，消除退出警告。
- 若下位机继续扩展手动控制语义，可进一步验证持续控制、急停、速度边界和异常参数返回。
