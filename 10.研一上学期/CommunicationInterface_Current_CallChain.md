# CommunicationInterface 当前调用链与接口状态说明

## 1. 文档目的

这份文档面向 Qt 和中台开发者，说明当前版本中：

- `CommunicationInterface.h` 里哪些接口已经打通
- 哪些接口存在但当前下位机协议不支持
- 哪些接口仍是占位接口或受限接口
- 运行时真实调用链路如何从 `client_api` 下沉到 `gateway`、`cmd_service`、`hyrms_export`

本文描述的是当前代码现状，不是理想设计稿。

## 2. 本次任务需求与约束

### 2.1 需求目标

本次任务的目标不是给下位机新增“路径协议”，而是在数据中台内部补齐路径资源管理能力：

- 补齐 `CommunicationInterface` 中的 `getPaths()`、`exeToPath()`、`addPath()`
- 由数据中台自己维护路径资源表
- 路径执行时先查库得到 `station_list`
- 再复用当前已经打通的 AGV 多站导航链

### 2.2 明确允许修改

本次任务允许修改的范围如下：

- `MFMS_BASE_04171715.sql`
- `src/mfms_server/client_api/...`
- `src/mfms_server/gateway/...`
- `src/mfms_server/cmd_service/...`
- `src/mfms_server/design/...`

数据库侧允许：

- 新增路径资源表
- 调整“新增路径表”的结构

当前实际采用的路径表语义为：

- 路径资源按 `device_id` 独占
- 不按 `group_id` 共享

### 2.3 需要谨慎修改

以下内容允许改，但必须谨慎，避免破坏现有行为或兼容性：

- `CommunicationInterface` 的旧接口签名要尽量保留，避免大面积破坏编译
- `addPath(const QList<QString>&)` 仅做兼容保留，固定返回失败
- `exeToPath(QString& stationName)` 参数名虽然历史上不合理，但当前不做大范围改名
- 路径相关接口必须只基于“当前明确选中的 AGV”执行
- 不能静默回退到“第一个在线 AGV”
- 路径执行必须继续复用现有 `executeAgvToStationList()`
- 如果当前不做“保存前站点合法性校验”，必须在文档中明确写清限制

### 2.4 明确禁止修改

以下边界在本次任务中明确不能动：

- 不修改任何旧表结构
- 不修改任何旧表字段
- 不修改任何 trigger
- 不把路径数据塞进旧表或旧表 JSON 字段
- 不碰 `AgvProxyAdapter`
- 不碰 `hyrms_export::SeerCtrl`
- 不新造 AGV 命令协议
- 不去实现 AGV 手动运动接口
- 不把本次任务扩展到 `qt_file` 绑定联动

特别不能动的旧表包括：

- `device`
- `device_state`
- `device_state_event`
- `device_ui_event`
- `lua_script`
- `lua_state`
- `lua_state_event`
- `lua_ui_event`
- `trigger_table`

## 3. 当前总体结构

### 3.1 模块分层

```text
Qt Frontend
  -> CommunicationInterface / CommunicationInterfaceImpl
  -> CommunicationWorker (QThread)
  -> MfmsGateway / MfmsGatewayImpl
  -> MfmsCommandService / MfmsRosBridge / MfmsDbService
  -> RobotProxyAdapter / AgvProxyAdapter
  -> hyrms_export::FrRobot / hyrms_export::SeerCtrl
  -> 下位机 ROS Service / ROS Topic / DB 状态机
```

### 3.2 职责划分

- `CommunicationInterface`
  - 给 Qt 暴露顶层槽函数和回传信号
- `CommunicationInterfaceImpl`
  - 单例实现，负责把 Qt 主线程请求转发到通信线程
- `CommunicationWorker`
  - 真正运行在独立 `QThread`
  - 维护当前连接设备、订阅状态、向 `gateway` 发请求
- `MfmsGatewayImpl`
  - 把 `db_service`、`ros_bridge`、`cmd_service` 这三块统一封装
- `MfmsCommandService`
  - 负责命令下发
  - 机器人命令走 `RobotProxyAdapter`
  - AGV 命令走 `AgvProxyAdapter`
- `MfmsRosBridge`
  - 负责实时状态 topic 订阅和在线设备列表刷新
- `hyrms_export`
  - 真实下位机代理层
  - `FrRobot` 和 `SeerCtrl` 都是 ROS 2 node，需要 executor 驱动

## 4. 线程与数据流

### 4.1 下行命令线程

```text
Qt UI Thread
  -> CommunicationInterfaceImpl
  -> Qt::QueuedConnection
  -> CommunicationWorker Thread
  -> MfmsGatewayImpl
  -> MfmsCommandService
  -> ProxyAdapter
  -> hyrms_export proxy node
  -> ROS Service
```

### 4.2 上行状态线程

```text
下位机 topic
  -> MfmsRosBridge (communication_worker_node 上订阅)
  -> robotStatusUpdated
  -> CommunicationWorker::handleRobotStatus
  -> sendARMState / sendAGVState
  -> Qt Frontend
```

补充说明：

- “命令下发”主链已切到 `hyrms_export`
- “实时状态上报”主链仍主要走 `MfmsRosBridge`
- 因此不能把“所有接口都走 proxy”理解成“所有上行下行都由 proxy 完成”

## 5. 当前已打通的调用链

### 5.1 设备列表

```text
CommunicationInterface::refreshRobotList()
  -> CommunicationInterfaceImpl::refreshRobotList()
  -> CommunicationWorker::doRefreshRobotList()
  -> MfmsGatewayImpl::refreshDeviceList()
  -> MfmsRosBridge::refreshDeviceList()
  -> DB 查询 device_state / device
  -> deviceListUpdated
  -> CommunicationWorker::emitRobotList()
  -> CommunicationInterface::getRobotList
```

这条链不走 `hyrms_export`。

### 5.2 连接与断开

```text
CommunicationInterface::connectRobot(name)
  -> CommunicationInterfaceImpl::connectRobot(name)
  -> CommunicationWorker::doConnectRobot(name)
  -> MfmsGatewayImpl::connectRobot(deviceId)
     -> 非 AGV: MfmsCommandService::connectRobot()
              -> RobotProxyAdapter
              -> hyrms_export::FrRobot::connect()
     -> AGV:   MfmsCommandService::connectAgv()
              -> AgvProxyAdapter
              -> hyrms_export::SeerCtrl::connect()
  -> robotConnected
  -> subscribeDeviceById
  -> subscribeResult
  -> CommunicationInterface::connectResult
```

断开链路同理。

### 5.3 机器人点动与模式切换

#### 关节点动

```text
armJogJoint()
  -> CommunicationWorker::doArmJogJoint()
  -> MfmsGatewayImpl::jogRobotAxis()
  -> MfmsCommandService::jogRobotAxis()
  -> RobotProxyAdapter::moveAxid()
  -> hyrms_export::FrRobot::moveAxis()
```

#### 笛卡尔点动

```text
armJogCartesian()
  -> CommunicationWorker::doArmJogCartesian()
  -> MfmsGatewayImpl::jogRobotCartesian()
  -> MfmsCommandService::jogRobotCartesian()
  -> RobotProxyAdapter::getDescPose()
  -> RobotProxyAdapter::moveL()
  -> hyrms_export::FrRobot
```

#### 模式切换

```text
armChangeMode()
  -> CommunicationWorker::doArmChangeMode()
  -> MfmsGatewayImpl::setRobotMode()
  -> MfmsCommandService::setRobotMode()
  -> RobotProxyAdapter::setMode()
  -> hyrms_export::FrRobot::setMode()
```

### 5.4 AGV 站点导航

#### 站点查询

```text
getStations()
  -> CommunicationWorker::doGetStations()
  -> MfmsGatewayImpl::queryAgvStations()
  -> MfmsCommandService::queryAgvStations()
  -> AgvProxyAdapter::queryStations()
  -> hyrms_export::SeerCtrl::checkStation()
```

#### 单站导航

```text
exeToStation()
  -> CommunicationWorker::doExeToStation()
  -> MfmsGatewayImpl::executeAgvToStation()
  -> MfmsCommandService::executeAgvToStation()
  -> AgvProxyAdapter::navigateToStation()
  -> hyrms_export::SeerCtrl::guideGoTarget()
```

#### 多站导航

```text
exeToStationList()
  -> CommunicationWorker::doExeToStationList()
  -> MfmsGatewayImpl::executeAgvToStationList()
  -> MfmsCommandService::executeAgvToStationList()
  -> AgvProxyAdapter::navigateToStationList()
  -> hyrms_export::SeerCtrl::guideGoTargetList()
```

#### 导航生命周期

```text
pauseNavigation()   -> guidePause()
resumeNavigation()  -> guideContinue()
cancelNavigation()  -> guideCancel()
queryNavigationStatus() -> checkGuide()
```

对应链路均为：

```text
CommunicationInterfaceImpl
  -> CommunicationWorker
  -> MfmsGatewayImpl
  -> MfmsCommandService
  -> AgvProxyAdapter
  -> hyrms_export::SeerCtrl
```

### 5.5 AGV 路径资源管理

#### 路径列表查询

```text
getPaths()
  -> CommunicationInterface::getPaths()
  -> CommunicationInterfaceImpl::getPaths()
  -> CommunicationWorker::doGetPaths()
  -> MfmsGatewayImpl::queryAgvPaths(deviceId)
  -> MfmsCommandService::queryAgvPaths(deviceId)
  -> DB 查询 agv_path
  -> returnPaths(pathNames)
```

补充约束：

- 只允许基于“当前明确选中的 AGV”查询
- 不会自动回退到第一个在线 AGV
- `device_id` 必须先通过 `device` 表存在性校验

#### 路径执行

```text
exeToPath(pathName)
  -> CommunicationInterface::exeToPath()
  -> CommunicationInterfaceImpl::exeToPath()
  -> CommunicationWorker::doExeToPath(pathName)
  -> MfmsGatewayImpl::executeAgvToPath(deviceId, pathName)
  -> MfmsCommandService::executeAgvToPath(deviceId, pathName)
  -> DB 查询 agv_path / agv_path_station
  -> 得到 station_list
  -> 复用 executeAgvToStationList(deviceId, station_list)
  -> AgvProxyAdapter::navigateToStationList()
  -> hyrms_export::SeerCtrl::guideGoTargetList()
```

补充约束：

- 路径资源按 `device_id` 独占，不按 `group_id` 共享
- 非 AGV 设备会直接失败
- 不改 `AgvProxyAdapter` / `SeerCtrl` / 下位机协议

#### 路径保存

```text
addPath(pathName, stationList)
  -> CommunicationInterface::addPath(pathName, stationList)
  -> CommunicationInterfaceImpl::addPath(pathName, stationList)
  -> CommunicationWorker::doAddPath(pathName, stationList)
  -> MfmsGatewayImpl::addAgvPath(deviceId, pathName, stationList)
  -> MfmsCommandService::addAgvPath(deviceId, pathName, stationList)
  -> DB 事务写入 agv_path / agv_path_station
  -> returnExeToPathRes(true/false)
```

补充约束：

- 旧接口 `addPath(const QList<QString>&)` 仍保留，但固定失败
- `device_id` 必须存在于 `device` 表
- `agv_path.device_id` 受外键约束保护

## 6. 当前不能打通的接口

### 6.1 AGV 手动运动接口

以下接口当前存在于 `CommunicationInterface.h`，但明确不可用：

- `agvMoveForward`
- `agvMoveBackward`
- `agvTurnLeft`
- `agvTurnRight`

原因不是上层没写，而是下位机公开协议不支持。

当前 `hyrms_export/include/dev/agv/agv.hpp` 中，`SeerCtrl` 只提供：

- `guideGoTarget(station)`
- `guideGoTargetList(station_list)`
- `guidePause()`
- `guideContinue()`
- `guideCancel()`
- `checkGuide()`
- `checkStation()`

当前 `SeerCtrlCmdInterface.srv` 请求字段只有：

- `id`
- `station`
- `station_list`

没有：

- `speed`
- `distance`
- `angle`
- `vx / vy / wz`

因此当前代码中，这四个接口在 `CommunicationWorker` 里会直接返回“不支持”，不会继续往下走。

对应接口的业务语义与限制如下：

| 接口 | 接口功能 | 当前无法实现的原因 | 如果以后要实现，需要补什么 |
| --- | --- | --- | --- |
| `agvMoveForward` | 让 AGV 按给定速度前进指定距离 | 当前下位机协议没有“速度 + 距离”型手动运动命令；`SeerCtrl` 只有站点导航和导航生命周期接口 | 下位机先新增前进命令语义；再同步扩展 `SeerCtrlCmdInterface.srv`、`hyrms_export::SeerCtrl`、`AgvProxyAdapter`、`MfmsCommandService`、`MfmsGateway`、`CommunicationWorker` |
| `agvMoveBackward` | 让 AGV 按给定速度后退指定距离 | 当前下位机协议没有“速度 + 距离”型后退命令；上层没有可下发的对应字段 | 下位机先新增后退命令语义；再把 `speed/distance/direction` 一路补到 service、proxy、adapter 和 client_api |
| `agvTurnLeft` | 让 AGV 按给定角速度左转指定角度 | 当前下位机协议没有“角速度 + 角度”型转向命令；`SeerCtrlCmdInterface.srv` 也没有 `angle` 或角速度字段 | 下位机先新增左转命令语义；再扩展 `angle/angular_speed` 一路到 `SeerCtrl`、`AgvProxyAdapter`、`MfmsCommandService` 与 `CommunicationWorker` |
| `agvTurnRight` | 让 AGV 按给定角速度右转指定角度 | 当前下位机协议没有“角速度 + 角度”型右转命令；现有链路只支持 `station/station_list` | 下位机先新增右转命令语义；再扩展命令字段和整条调用链，并补对应错误处理与状态反馈 |

### 6.2 路径资源管理接口

以下接口当前已落地到数据中台路径表：

- `getPaths`
- `exeToPath`
- `addPath`

当前实现语义：

- 路径资源按 `device_id` 独占，不按 `group_id` 共享
- `exeToPath` 先查路径表得到 `station_list`，再复用 `guideGoTargetList(station_list)`
- 非 AGV 设备不会进入路径查询/保存/执行逻辑
- 当前未选中 AGV 时，路径接口会直接失败，不会自动回退到其他 AGV
- 路径表记录要求 `device_id` 在 `device` 表中真实存在

当前限制：

- `addPath` 只校验站点名非空
- `addPath` 当前不会在保存前校验站点是否真实存在于 AGV 站点表
- 因此调用方需要自行保证站点名合法，否则可能把错误站点名持久化到路径表

## 7. `CommunicationInterface.h` 接口状态总表

| 接口                      | 功能              | 当前状态 | 是否走 `hyrms_export` | 说明                                         |
| ----------------------- | --------------- | ---- | ------------------ | ------------------------------------------ |
| `refreshRobotList`      | 刷新可选设备列表        | 可用   | 否                  | 走 DB + `MfmsRosBridge`                     |
| `connectRobot`          | 连接当前选中的机器人或 AGV | 可用   | 是                  | Robot 走 `FrRobot`，AGV 分流到 `SeerCtrl`       |
| `disconnectRobot`       | 断开当前已连接设备       | 可用   | 是                  | 同上                                         |
| `agvMoveForward`        | 控制 AGV 前进指定距离   | 不可用  | 否                  | 当前直接返回“不支持”                                |
| `agvMoveBackward`       | 控制 AGV 后退指定距离   | 不可用  | 否                  | 当前直接返回“不支持”                                |
| `agvTurnLeft`           | 控制 AGV 左转指定角度   | 不可用  | 否                  | 当前直接返回“不支持”                                |
| `agvTurnRight`          | 控制 AGV 右转指定角度   | 不可用  | 否                  | 当前直接返回“不支持”                                |
| `armJogJoint`           | 机械臂关节点动         | 可用   | 是                  | 走 `FrRobot`                                |
| `armJogCartesian`       | 机械臂笛卡尔点动        | 可用   | 是                  | 走 `FrRobot`                                |
| `armChangeMode`         | 切换机械臂运行模式       | 可用   | 是                  | 走 `FrRobot`                                |
| `refreshState`          | 请求尽快看到最新设备状态    | 可调用  | 否                  | 被动等待下一次状态上报                                |
| `getStations`           | 查询当前 AGV 站点列表   | 可用   | 是                  | 走 `SeerCtrl::checkStation()`               |
| `exeToStation`          | 执行 AGV 单站导航     | 可用   | 是                  | 走 `SeerCtrl::guideGoTarget()`              |
| `exeToStationList`      | 执行 AGV 多站导航     | 可用   | 是                  | 走 `SeerCtrl::guideGoTargetList()`          |
| `pauseNavigation`       | 暂停当前 AGV 导航任务   | 可用   | 是                  | 走 `SeerCtrl::guidePause()`                 |
| `resumeNavigation`      | 继续当前 AGV 导航任务   | 可用   | 是                  | 走 `SeerCtrl::guideContinue()`              |
| `cancelNavigation`      | 取消当前 AGV 导航任务   | 可用   | 是                  | 走 `SeerCtrl::guideCancel()`                |
| `queryNavigationStatus` | 主动查询 AGV 导航状态   | 可用   | 是                  | 走 `SeerCtrl::checkGuide()`                 |
| `getPaths`              | 查询当前 AGV 的路径名列表 | 可用   | 否                  | 查数据中台 `agv_path`，按 `device_id` 返回路径名列表     |
| `exeToPath`             | 执行当前 AGV 的命名路径  | 可用   | 是                  | 查数据中台路径后复用 `SeerCtrl::guideGoTargetList()` |
| `addPath`               | 保存当前 AGV 的命名路径  | 可用   | 否                  | 写入数据中台路径表；当前不校验站点是否真实存在                    |

## 8. 给 Qt 开发者的直接结论

### 8.1 当前可以放心绑定的接口

- `refreshRobotList`
- `connectRobot`
- `disconnectRobot`
- `armJogJoint`
- `armJogCartesian`
- `armChangeMode`
- `getStations`
- `exeToStation`
- `exeToStationList`
- `pauseNavigation`
- `resumeNavigation`
- `cancelNavigation`
- `queryNavigationStatus`
- `getPaths`
- `exeToPath`
- `addPath`

### 8.2 当前不要绑定成正式功能入口的接口

- `agvMoveForward`
- `agvMoveBackward`
- `agvTurnLeft`
- `agvTurnRight`

原因分别是：

- 前四项是下位机协议不支持
- 路径接口虽然已实现，但 `addPath` 当前不会校验站点是否真实存在于 AGV 站点表

### 8.3 状态刷新不要误解

- `sendARMState` 和 `sendAGVState` 来自 `MfmsRosBridge` 的 topic 订阅
- `refreshState()` 不是主动 service 拉取
- `queryNavigationStatus()` 才是 AGV 导航状态的主动查询接口

## 9. 代码落点

关键代码文件如下：

- `src/mfms_server/client_api/include/mfms_server/CommunicationInterface.h`
- `src/mfms_server/client_api/src/CommunicationInterface.cpp`
- `src/mfms_server/client_api/src/CommunicationInterfaceImpl.cpp`
- `src/mfms_server/client_api/src/CommunicationWorker.cpp`
- `src/mfms_server/gateway/src/MfmsGatewayImpl.cpp`
- `src/mfms_server/cmd_service/src/MfmsCommandService.cpp`
- `src/mfms_server/cmd_service/src/RobotProxyAdapter.cpp`
- `src/mfms_server/cmd_service/src/AgvProxyAdapter.cpp`
- `MFMS_BASE_04171715.sql`
- `HyRMS_export_202601251449_bszydxh-HP/hyrms_export/include/dev/robot/robot.hpp`
- `HyRMS_export_202601251449_bszydxh-HP/hyrms_export/include/dev/agv/agv.hpp`
- `src/com_interfaces/srv/SeerCtrlCmdInterface.srv`

## 10. 一句话总结

当前版本已经把“机器人控制链”“AGV 站点导航链”和“AGV 路径资源管理链”打通；其中 AGV 手动运动接口仍因下位机协议不支持而明确不可走通，路径执行继续复用已有 `station_list` 导航链。
