# MFMS 数据中台架构分析文档

> 文档生成日期: 2026-04-20
> 基于代码版本: `58f80da` (main)

---

## 目录

1. [系统总览](#1-系统总览)
2. [数据中台整体架构 (SVG)](#2-数据中台整体架构-svg)
3. [线程模型与跨线程数据交换 (Mermaid)](#3-线程模型与跨线程数据交换-mermaid)
4. [CommunicationInterface 通信链分析](#4-communicationinterface-通信链分析)
5. [数据库结构图 (Mermaid ER)](#5-数据库结构图-mermaid-er)
6. [上下位机数据库交互协议](#6-上下位机数据库交互协议)
7. [代理层 (hyrms_export) 调用链](#7-代理层-hyrms_export-调用链)
8. [接口状态总表](#8-接口状态总表)
9. [关键源码索引](#9-关键源码索引)

---

## 1. 系统总览

MFMS (Multi-Functional Manufacturing System) 数据中台是一个基于 **ROS 2 Humble + Qt5** 的工业设备集成控制平台。它通过单例通信接口将 Qt 前端与 ROS 设备控制、MySQL 数据库事件驱动三大系统统一封装，对 UI 层提供线程安全的信号/槽契约。

### 核心设计原则

| 原则 | 实现方式 |
|------|---------|
| **UI 线程隔离** | `CommunicationInterfaceImpl` 单例 + `QThread` + `Qt::QueuedConnection` |
| **门面模式** | `MfmsGatewayImpl` 统一封装 DB/ROS/CMD 三大服务 |
| **设备代理** | `RobotProxyAdapter` / `AgvProxyAdapter` 通过 PIMPL 隐藏 `hyrms_export` 实现 |
| **数据库事件驱动** | MySQL 触发器 + 轮询 `*_ui_event` 表实现上下位机异步通信 |

---

## 2. 数据中台整体架构 (SVG)

![[图片/SVG/mfms_data_center_architecture.svg|825]]

---

## 3. 线程模型与跨线程数据交换 (Mermaid)

### 3.1 下行命令流 (UI → 物理设备)

```mermaid
sequenceDiagram
    participant UI as Qt UI 主线程
    participant Impl as CommunicationInterfaceImpl<br/>(单例)
    participant Worker as CommunicationWorker<br/>(QThread)
    participant GW as MfmsGatewayImpl<br/>(门面)
    participant CMD as MfmsCommandService
    participant Proxy as ProxyAdapter<br/>(PIMPL)
    participant Export as hyrms_export<br/>(ROS Node)
    participant Device as 物理设备

    UI->>Impl: comm.armJogJoint(0, 1.0)
    Note over UI,Impl: 同一线程 (UI Thread)

    Impl->>Impl: if (initialized_)<br/>emit requestArmJogJoint(0, 1.0)
    Note over Impl,Worker: Qt::QueuedConnection<br/>跨线程信号

    Worker->>Worker: doArmJogJoint(0, 1.0)<br/>deviceId = currentDeviceId_
    Worker->>GW: gateway_->jogRobotAxis(deviceId, 1, 1, 1.0)
    GW->>CMD: cmd_service_->jogRobotAxis(deviceId, 1, 1, 1.0)
    CMD->>Proxy: robotProxyAdapter_->moveAxid(deviceId, 1, 1, 1.0)
    Proxy->>Export: proxy->moveAxis(1, 1, 1.0)
    Export->>Export: send_request()<br/>call_service(req_, res_)
    Note over Export,Device: ROS 2 Service Call<br/>/{device}_cmd
    Export->>Device: FrCmdInterface.srv Request

    Device-->>Export: FrCmdInterface.srv Response
    Export-->>Proxy: result = 0 (success)
    Proxy-->>CMD: emit commandExecuted(deviceId, true, 0, "关节点动成功")
    CMD-->>GW: emit robotMotionExecuted(...)
    GW-->>Worker: emit robotMotionExecuted(...)
    Worker-->>Impl: emit armControlResult(true, 0, "关节点动成功")
    Note over Worker,Impl: Qt::QueuedConnection<br/>跨线程信号
    Impl-->>UI: emit armControlRes(true, 0, msg)
```

### 3.2 上行状态流 (物理设备 → UI)

```mermaid
sequenceDiagram
    participant Device as 物理设备
    participant Adapter as FrAdapterPublisher<br/>(ROS Node)
    participant ROS as ROS 2 Topic<br/>/{device}_state
    participant Bridge as MfmsRosBridge
    participant GW as MfmsGatewayImpl
    participant Worker as CommunicationWorker<br/>(QThread)
    participant Impl as CommunicationInterfaceImpl
    participant UI as Qt UI

    Device->>Adapter: TCP 8083 状态数据
    Note over Device,Adapter: 50ms 间隔
    Adapter->>ROS: publish FrRobotState

    ROS->>Bridge: subscription callback
    Bridge->>GW: emit robotStatusUpdated(status)
    GW->>Worker: 信号转发

    Worker->>Worker: handleRobotStatus(status)
    alt isAgvType
        Worker->>Worker: 构造 SeerCtrlState msg
        Worker->>Impl: emit agvStateReceived(msg)
    else isRobotType
        Worker->>Worker: 构造 FrRobotState msg
        Worker->>Impl: emit armStateReceived(msg)
    end

    Note over Worker,Impl: Qt::QueuedConnection
    Impl->>UI: emit sendARMState(msg) / sendAGVState(msg)
```

### 3.3 数据库事件流 (上下位机通信)

```mermaid
sequenceDiagram
    participant UI as 上位机 (Qt)
    participant DbSvc as MfmsDbService
    participant DB as MySQL<br/>MFMS_BASE
    participant Trigger as MySQL Trigger
    participant Lower as 下位机

    rect rgb(230, 245, 255)
    Note over UI,Lower: 设备加载流程
    UI->>DbSvc: loadDevice("robFro0001")
    DbSvc->>DB: UPDATE device_state SET state='load'
    DB->>Trigger: trg_device_state_event FIRES
    Trigger->>DB: INSERT device_state_event<br/>(下位机消费)

    Lower->>DB: 轮询 device_state_event
    Lower->>Lower: 执行设备加载
    Lower->>DB: UPDATE device_state SET state='online'
    DB->>Trigger: trg_device_state_event FIRES
    Trigger->>DB: INSERT device_ui_event<br/>"设备加载完成"

    DbSvc->>DB: 轮询 device_ui_event<br/>WHERE id > last_event_id
    DbSvc->>UI: emit deviceStateChanged<br/>("load"→"online", "设备加载完成")
    end

    rect rgb(255, 245, 230)
    Note over UI,Lower: Lua 脚本执行流程
    UI->>DbSvc: startLuaScript(scriptId, groupId)
    DbSvc->>DB: UPDATE lua_state SET state='ready'
    DB->>Trigger: trg_lua_state_event FIRES
    Trigger->>DB: INSERT lua_state_event<br/>(下位机消费)

    Lower->>DB: 轮询 lua_state_event
    Lower->>Lower: 加载并执行 Lua 脚本
    Lower->>DB: UPDATE lua_state SET state='running'
    DB->>Trigger: trg_lua_state_event FIRES
    Trigger->>DB: INSERT lua_ui_event<br/>"脚本开始执行"

    DbSvc->>DB: 轮询 lua_ui_event
    DbSvc->>UI: emit luaStateChanged<br/>("ready"→"running")

    alt 执行成功
        Lower->>DB: UPDATE lua_state SET state='wait'
    else 执行失败
        Lower->>DB: UPDATE lua_state SET state='aborted'<br/>reason='错误信息'
    end

    DB->>Trigger: FIRES
    Trigger->>DB: INSERT lua_ui_event
    DbSvc->>UI: emit luaStateChanged(...)
    end
```

### 3.4 线程全景图

```mermaid
graph TD
    subgraph "Qt UI 主线程"
        UI[hybrid_robot_system / Control / ...]
        Impl["CommunicationInterfaceImpl<br/>getInstance() 单例"]
    end

    subgraph "Communication Thread (QThread)"
        Worker[CommunicationWorker]
        GW[MfmsGatewayImpl]
        DB[MfmsDbService]
        Bridge[MfmsRosBridge]
        CMD[MfmsCommandService]
    end

    subgraph "RobotProxy Executor Thread"
        RExec["MultiThreadedExecutor<br/>spin_some(10ms)"]
        FrProxy["RobotProxy::FrRobot<br/>(rclcpp::Node)"]
    end

    subgraph "AgvProxy Executor Thread"
        AExec["MultiThreadedExecutor<br/>spin_some(10ms)"]
        AgvProxy["AgvProxy::SeerCtrl<br/>(rclcpp::Node)"]
    end

    subgraph "ROS 2 Network"
        Topic["/{device}_state Topic"]
        Service["/{device}_cmd Service"]
    end

    UI <-->|"Qt::QueuedConnection"| Impl
    Impl <-->|"emit requestXxx / resultXxx"| Worker
    Worker --> GW
    GW --> DB
    GW --> Bridge
    GW --> CMD
    CMD --> RExec
    CMD --> AExec
    RExec --> FrProxy
    AExec --> AgvProxy
    FrProxy <--> Service
    AgvProxy <--> Service
    Bridge <-- Topic
    DB <-->|"轮询 *_ui_event"| MySQL[(MySQL)]

    style UI fill:#E8F5E9,stroke:#4CAF50
    style Impl fill:#E3F2FD,stroke:#1976D2
    style Worker fill:#E3F2FD,stroke:#1976D2
    style GW fill:#FFF3E0,stroke:#F57C00
    style DB fill:#F3E5F5,stroke:#7B1FA2
    style Bridge fill:#F3E5F5,stroke:#7B1FA2
    style CMD fill:#F3E5F5,stroke:#7B1FA2
    style FrProxy fill:#FFEBEE,stroke:#C62828
    style AgvProxy fill:#FFEBEE,stroke:#C62828
    style MySQL fill:#FFF9C4,stroke:#F9A825
```

---

## 4. CommunicationInterface 通信链分析

### 4.1 接口层级调用链

每个 `CommunicationInterface` 公开接口的完整调用链如下：

#### 设备列表刷新

```
refreshRobotList()
  → CommunicationInterfaceImpl::refreshRobotList()
    → emit requestRefreshRobotList()                    [跨线程]
      → CommunicationWorker::doRefreshRobotList()
        → MfmsGatewayImpl::refreshDeviceList()
          → MfmsRosBridge::refreshDeviceList()
            → DB 查询 device_state JOIN device
              → emit deviceListUpdated(devices)
                → CommunicationWorker::emitRobotList()
                  → emit robotListUpdated(names)        [跨线程]
                    → CommunicationInterface::getRobotList(names)
```

#### 设备连接（状态机驱动）

```
connectRobot(name)
  → CommunicationInterfaceImpl::connectRobot(name)
    → emit requestConnectRobot(name)                    [跨线程]
      → CommunicationWorker::doConnectRobot(name)
        → resolveDeviceId(name)                         // 名字→设备ID
        → currentDeviceId_ = deviceId

        Case 1: AGV 设备 (online/connected)
          → gateway_->connectAgv(deviceId)
            → MfmsCommandService::connectAgv()
              → AgvProxyAdapter::connectAgv()
                → SeerCtrl::connect()                   // hyrms_export
            → robotConnected 信号
              → subscribeDeviceById()
                → subscribeResult → connectionResult

        Case 2: Robot (connected)
          → gateway_->connectRobot(deviceId)
            → MfmsCommandService::connectRobot()
              → RobotProxyAdapter::connectRobot()
                → FrRobot::connect()                    // hyrms_export
            → robotConnected → subscribe → connectionResult

        Case 3: Robot (online) — 等待下位机 TCP
          → connectingDeviceId_ = deviceId
          → 等待 handleDeviceStateTransition(→ connected)
            → gateway_->connectRobot(deviceId)

        Case 4: Robot (offline) — 走完整状态机
          → gateway_->loadDevice(deviceId)              // DB: state='load'
          → 等待 device_ui_event: load → online
          → 等待 device_ui_event: online → connected
          → gateway_->connectRobot(deviceId)
```

#### 机械臂关节点动

```
armJogJoint(number, jog_step_)
  → emit requestArmJogJoint(number, jog_step_)         [跨线程]
    → CommunicationWorker::doArmJogJoint(jointNum, step)
      → gateway_->jogRobotAxis(deviceId, jointNum+1, direction, abs(step))
        → MfmsCommandService::jogRobotAxis(deviceId, axisId, direction, step)
          → RobotProxyAdapter::moveAxid(deviceId, axisId, direction, step)
            → FrRobot::moveAxis(axisId, direction, step)
              → send_request() → ROS Service /{device}_cmd
```

#### AGV 站点导航

```
exeToStation(stationName)
  → emit requestExeToStation(stationName)               [跨线程]
    → CommunicationWorker::doExeToStation(stationName)
      → gateway_->executeAgvToStation(deviceId, stationName)
        → MfmsCommandService::executeAgvToStation(deviceId, stationName)
          → AgvProxyAdapter::navigateToStation(deviceId, station)
            → SeerCtrl::guideGoTarget(station)
              → send_request() → ROS Service /{device}_cmd
```

#### AGV 路径资源管理

```
getPaths()
  → emit requestGetPaths()                              [跨线程]
    → CommunicationWorker::doGetPaths()
      → resolveCurrentAgvDeviceId()                     // 必须当前已选中 AGV
      → gateway_->queryAgvPaths(deviceId)
        → MfmsCommandService::queryAgvPaths(deviceId)
          → DB 查询 agv_path WHERE device_id = ?
            → emit agvPathsReceived(deviceId, pathNames)

exeToPath(pathName)
  → emit requestExeToPath(pathName)                     [跨线程]
    → CommunicationWorker::doExeToPath(pathName)
      → gateway_->executeAgvToPath(deviceId, pathName)
        → MfmsCommandService::executeAgvToPath(deviceId, pathName)
          → DB 查询 agv_path + agv_path_station → station_list
          → 复用 executeAgvToStationList(deviceId, station_list)
            → AgvProxyAdapter::navigateToStationList()
              → SeerCtrl::guideGoTargetList(station_list)

addPath(pathName, stationList)
  → emit requestAddPath(pathName, stationList)          [跨线程]
    → CommunicationWorker::doAddPath(pathName, stationList)
      → gateway_->addAgvPath(deviceId, pathName, stationList)
        → MfmsCommandService::addAgvPath(deviceId, pathName, stationList)
          → DB 事务: INSERT agv_path + INSERT agv_path_station
```

### 4.2 不可用接口及原因

```mermaid
graph LR
    A["agvMoveForward<br/>agvMoveBackward<br/>agvTurnLeft<br/>agvTurnRight"] -->|"直接返回<br/>不支持"| B["CommunicationWorker"]
    B -.-x C["MfmsGatewayImpl"]

    style A fill:#FFCDD2,stroke:#C62828
    style B fill:#E3F2FD,stroke:#1976D2
    style C fill:#FFF3E0,stroke:#F57C00
```

**原因**: `hyrms_export::SeerCtrl` 仅提供站点导航协议 (`guideGoTarget`, `guideGoTargetList`)，`SeerCtrlCmdInterface.srv` 没有 `speed/distance/angle` 字段。要打通需要下位机先扩展命令语义。

---

## 5. 数据库结构图 (Mermaid ER)

### 5.1 完整 ER 图

```mermaid
erDiagram
    device {
        varchar_10 id PK "设备ID: type_3+module_3+id_4"
        int group_id "设备组ID (标识下位机控制器)"
        varchar_50 address "设备地址 (IP)"
        bigint create_ts "创建时间戳"
    }

    device_state {
        varchar_10 id PK "设备ID"
        enum state "offline|online|load|unload|connected"
        json info "设备详细状态(JSON)"
        int err_code "错误码"
    }

    device_state_event {
        bigint id PK "自增ID"
        varchar_10 device_id "设备ID"
        enum from_state "原状态"
        enum to_state "新状态"
        varchar_255 reason "变更原因"
        timestamp created_at "创建时间"
    }

    device_ui_event {
        bigint id PK "自增ID"
        varchar_10 device_id "设备ID"
        enum from_state "原状态"
        enum to_state "新状态"
        varchar_255 reason "变更原因"
        timestamp created_at "创建时间"
    }

    lua_script {
        bigint id PK "脚本ID"
        varchar_100 script_name "脚本名称"
        text script_content "脚本内容"
        varchar_100 comments "备注"
    }

    lua_state {
        bigint script_id PK "脚本ID"
        int group_id PK "设备组ID"
        enum state "wait|ready|running|pause|paused|resume|abort|aborted"
        text reason "更新原因"
        varchar_255 script_name "脚本名字"
    }

    lua_state_event {
        bigint id PK "自增ID"
        bigint script_id "脚本ID"
        int group_id "设备组ID"
        enum from_state "原状态"
        enum to_state "新状态"
        text reason "原因"
        timestamp created_at "创建时间"
    }

    lua_ui_event {
        bigint id PK "自增ID"
        bigint script_id "脚本ID"
        int group_id "设备组ID"
        enum from_state "原状态"
        enum to_state "新状态"
        text reason "原因"
        timestamp created_at "创建时间"
    }

    agv_path {
        bigint id PK "路径ID"
        varchar_10 device_id FK "设备ID"
        varchar_64 path_name "路径名称"
        timestamp created_at "创建时间"
        timestamp updated_at "更新时间"
    }

    agv_path_station {
        bigint id PK "自增ID"
        bigint path_id FK "路径ID"
        int station_index "站点顺序"
        varchar_128 station_name "站点名称"
    }

    device ||--|| device_state : "1:1 状态"
    device ||--o{ agv_path : "1:N 路径资源"
    agv_path ||--o{ agv_path_station : "1:N 站点序列"
    lua_script ||--o{ lua_state : "1:N 执行状态"
    device_state ||--o{ device_state_event : "触发器写入 (下位机消费)"
    device_state ||--o{ device_ui_event : "触发器写入 (上位机消费)"
    lua_state ||--o{ lua_state_event : "触发器写入 (下位机消费)"
    lua_state ||--o{ lua_ui_event : "触发器写入 (上位机消费)"
```

### 5.2 设备ID命名规则

| 前缀 (type_3) | 含义 | module_3 示例 | 完整 ID 示例 |
|:-:|:-:|:-:|:-:|
| `rob` | Robot Arm | `Fro` (Fairino) | `robFro0001` |
| `rbt` | Robot | `Hsu` (华数) | `rbtHsu0001` |
| `agv` | AGV | `Src` (仙工/Seer) | `agvSrc0001` |
| `plc` | PLC | `Sie` (Siemens) | `plcSie0001` |
| `vit` | Virtual | `Dev` | `vitDev0001` |

### 5.3 触发器机制

```mermaid
flowchart LR
    subgraph "device_state 表 UPDATE"
        DS["UPDATE device_state<br/>SET state = 'X'"]
    end

    subgraph "trg_device_state_event"
        T1{"NEW.state = ?"}
        T1 -->|"load"| E1["INSERT device_state_event<br/>(下位机消费)"]
        T1 -->|"unload"| E2["INSERT device_state_event<br/>(下位机消费)"]
        T1 -->|"online"| E3["INSERT device_ui_event<br/>'设备加载完成'<br/>(上位机消费)"]
        T1 -->|"connected"| E4["INSERT device_ui_event<br/>'设备连接成功'<br/>(上位机消费)"]
        T1 -->|"offline"| E5["INSERT device_ui_event<br/>根据 OLD.state 区分原因<br/>(上位机消费)"]
    end

    DS --> T1

    style E1 fill:#E3F2FD
    style E2 fill:#E3F2FD
    style E3 fill:#E8F5E9
    style E4 fill:#E8F5E9
    style E5 fill:#FFEBEE
```

---

## 6. 上下位机数据库交互协议

### 6.1 设备状态机

```mermaid
stateDiagram-v2
    [*] --> offline
    offline --> load : 上位机 loadDevice()<br/>写 state='load'
    load --> online : 下位机加载成功<br/>写 state='online'
    load --> offline : 下位机加载失败<br/>写 state='offline'
    online --> connected : 下位机 TCP 连接成功<br/>写 state='connected'
    online --> offline : 设备异常离线
    connected --> offline : 设备连接断开
    connected --> unload : 上位机 unloadDevice()<br/>写 state='unload'
    online --> unload : 上位机 unloadDevice()
    unload --> offline : 卸载完成<br/>写 state='offline'

    note right of load
        触发器 → device_state_event
        (下位机轮询消费)
    end note

    note right of online
        触发器 → device_ui_event
        "设备加载完成"
        (上位机轮询消费)
    end note

    note right of connected
        触发器 → device_ui_event
        "设备连接成功"
    end note
```

### 6.2 Lua 脚本状态机

```mermaid
stateDiagram-v2
    [*] --> wait
    wait --> ready : 上位机 start()<br/>写 state='ready'
    ready --> running : 下位机加载脚本<br/>写 state='running'
    running --> wait : 执行完成<br/>写 state='wait'
    running --> aborted : 执行出错<br/>写 state='aborted'<br/>⚠️ 上位机需弹窗
    running --> pause : 上位机 pause()<br/>写 state='pause'
    pause --> paused : 下位机确认暂停<br/>写 state='paused'
    paused --> ready : 上位机 resume()<br/>写 state='ready'
    aborted --> wait : 用户确认错误后<br/>写 state='wait'

    note right of ready
        触发器 → lua_state_event
        (下位机轮询消费)
    end note

    note right of running
        触发器 → lua_ui_event
        "脚本开始执行"
    end note

    note right of aborted
        触发器 → lua_ui_event
        "脚本执行异常"
        ⚠️ 上位机弹窗提示
    end note
```

### 6.3 完整上下位机交互时序

```mermaid
sequenceDiagram
    participant Host as 上位机 (MfmsDbService)
    participant DB as MySQL (MFMS_BASE)
    participant TRG as MySQL Trigger
    participant DEvt as device_state_event
    participant UEvt as device_ui_event
    participant Low as 下位机 (控制器)

    Note over Host,Low: ════ 设备加载 ══════

    Host->>DB: UPDATE device_state<br/>SET state='load' WHERE id='robFro0001'
    DB->>TRG: AFTER UPDATE fires
    TRG->>DEvt: INSERT (from='offline', to='load')

    Low->>DEvt: SELECT WHERE id > last_id (轮询)
    Low->>Low: 检测到 load 事件<br/>执行设备加载
    Low->>DB: UPDATE device_state<br/>SET state='online'
    DB->>TRG: AFTER UPDATE fires
    TRG->>UEvt: INSERT (from='load', to='online')<br/>reason='设备加载完成'

    Host->>UEvt: SELECT WHERE id > last_id (轮询)
    Host->>Host: emit deviceStateChanged<br/>("load"→"online", "设备加载完成")

    Note over Host,Low: ══════ Lua 脚本执行 ══════

    Host->>DB: UPDATE lua_state<br/>SET state='ready'
    DB->>TRG: fires trg_lua_state_event
    TRG->>DEvt: INSERT lua_state_event

    Low->>Low: 轮询到 ready 事件<br/>加载脚本
    Low->>DB: UPDATE lua_state SET state='running'
    TRG->>UEvt: INSERT lua_ui_event "脚本开始执行"

    alt 执行成功
        Low->>DB: UPDATE lua_state SET state='wait'
        TRG->>UEvt: INSERT "脚本执行完成"
    else 执行失败
        Low->>DB: UPDATE lua_state SET state='aborted'<br/>reason='目标位姿无法到达'
        TRG->>UEvt: INSERT "脚本执行异常" ⚠️
    end

    Host->>UEvt: 轮询增量事件
    Host->>Host: emit luaStateChanged(...)
```

---

## 7. 代理层 (hyrms_export) 调用链

### 7.1 代理层架构

```mermaid
classDiagram
    class HyDevProxy {
        <<abstract>>
        #client_ : ClientBase
        #subscription_ : SubscriptionBase
        +get_service_name() string
        +get_topic_name() string
        +init_client()* int32_t
        +init_subscription()* int32_t
    }

    class HySimpleDevProxy~CmdIf, StateT~ {
        #req_ : CmdIf::Request
        #res_ : CmdIf::Response
        #future_timeout_ : int32_t = 8s
        +call_service(req, res) int32_t
        +state_received(state) void
    }

    class FrRobot {
        +msg : string
        +joint_pose[6] : double
        +desc_pose[6] : double
        +state_pkg : FrRobotState
        +connect() int32_t
        +disconnect() int32_t
        +moveJ(pos, joint_desc) int32_t
        +moveL(pos, joint_desc) int32_t
        +moveAxis(axid, dir, deg) int32_t
        +setVec(speed) int32_t
        +setAcc(acc) int32_t
        +setMode(mode) int32_t
        +setIO(io, idx, val) int32_t
        +getJointPose() int32_t
        +getDescPose() int32_t
    }

    class SeerCtrl {
        +msg : string
        +task_status : int
        +task_type : int
        +target_station : string
        +station_list : vector~AgvStation~
        +connect() int32_t
        +disconnect() int32_t
        +guideGoTarget(station) int
        +guideGoTargetList(list) int
        +guidePause() int
        +guideContinue() int
        +guideCancel() int
        +checkGuide() int
        +checkStation() int
    }

    class RobotProxyAdapter {
        -impl_ : unique_ptr~Impl~
        -running_ : atomic~bool~
        +start() bool
        +stop() void
        +connectRobot(deviceId) int32_t
        +disconnectRobot(deviceId) int32_t
        +moveAxid(deviceId, axis, dir, step) int32_t
        +moveJ(deviceId, pos, jointDesc) int32_t
        +moveL(deviceId, pos, jointDesc) int32_t
        +setMode(deviceId, mode) int32_t
    }

    class AgvProxyAdapter {
        -impl_ : unique_ptr~Impl~
        -running_ : atomic~bool~
        +start() bool
        +stop() void
        +connectAgv(deviceId) int32_t
        +disconnectAgv(deviceId) int32_t
        +queryStations(deviceId) int32_t
        +navigateToStation(deviceId, station) int32_t
        +navigateToStationList(deviceId, stations) int32_t
        +pauseNavigation(deviceId) int32_t
        +resumeNavigation(deviceId) int32_t
        +cancelNavigation(deviceId) int32_t
        +queryNavigationStatus(deviceId) int32_t
    }

    HyDevProxy <|-- HySimpleDevProxy : inherits
    HySimpleDevProxy <|-- FrRobot : "CmdIf=FrCmdInterface<br/>StateT=FrRobotState"
    HySimpleDevProxy <|-- SeerCtrl : "CmdIf=SeerCtrlCmdInterface<br/>StateT=SeerCtrlState"
    RobotProxyAdapter o-- FrRobot : "PIMPL 持有 map<deviceId, FrRobot>"
    AgvProxyAdapter o-- SeerCtrl : "PIMPL 持有 map<deviceId, SeerCtrl>"

    note for HyDevProxy "继承 rclcpp::Node<br/>所有代理都是 ROS 节点"
    note for RobotProxyAdapter "内部 MultiThreadedExecutor<br/>独立线程驱动 spin_some"
```

### 7.2 FrRobot 命令下发详细流程

```mermaid
flowchart TB
    A["RobotProxyAdapter::moveAxid(deviceId, axis, dir, step)"] --> B["QMutexLocker lock"]
    B --> C{"running_ ?"}
    C -->|No| D["return NOT_RUNNING"]
    C -->|Yes| E["proxy = impl_->getProxy(deviceId)"]
    E --> F{"proxy != null ?"}
    F -->|No| G["return DEVICE_NOT_CONNECTED"]
    F -->|Yes| H["proxy->moveAxis(axisId, direction, step)"]

    H --> I["填充 req_ (FrCmdInterface::Request)"]
    I --> J["req_->id = MOVE_AXIS"]
    J --> K["req_->axid = axisId"]
    K --> L["req_->dir = direction"]
    L --> M["req_->deg = step"]
    M --> N["send_request()"]

    N --> O["call_service(req_, res_)"]
    O --> P["client_->async_send_request(req)"]
    P --> Q["future.wait_for(8 seconds)"]
    Q --> R{"timeout ?"}
    R -->|Yes| S["return ERR_TIMEOUT (-1001)"]
    R -->|No| T["res_ = future.get()"]
    T --> U["result = res_->err_code"]
    U --> V["更新 state_pkg / joint_pose / desc_pose"]

    V --> W["return result"]
    W --> X["emit commandExecuted(deviceId, success, result, msg)"]

    style A fill:#FFEBEE,stroke:#C62828
    style H fill:#FFCDD2,stroke:#C62828
    style N fill:#FFCDD2,stroke:#C62828
    style P fill:#E3F2FD,stroke:#1976D2
```

### 7.3 SeerCtrl 导航流程

```mermaid
flowchart TB
    A["AgvProxyAdapter::navigateToStationList(deviceId, stations)"] --> B["转换 QStringList → vector<string>"]
    B --> C["QMutexLocker lock"]
    C --> D{"running_ && proxy exists ?"}
    D -->|No| E["return error"]
    D -->|Yes| F["waitForServiceReady(proxy)"]
    F --> G{"service ready ?"}
    G -->|No| H["return SERVICE_NOT_READY"]
    G -->|Yes| I["proxy->guideGoTargetList(stationList)"]

    I --> J["填充 req_ (SeerCtrlCmdInterface::Request)"]
    J --> K["req_->id = GUIDE_GO_TARGET_LIST"]
    K --> L["req_->station_list = stations"]
    L --> M["send_request()"]
    M --> N["call_service → ROS Service /{device}_cmd"]
    N --> O["下位机 SeerCtrl Adapter 处理"]
    O --> P["AGV 开始多站导航"]
    P --> Q["return result"]
    Q --> R["emit commandExecuted(deviceId, success, result, msg)"]

    style A fill:#FFEBEE,stroke:#C62828
    style I fill:#FFCDD2,stroke:#C62828
    style N fill:#E3F2FD,stroke:#1976D2
    style P fill:#ECEFF1,stroke:#455A64
```

### 7.4 代理层 ROS 服务接口

| 代理类 | ROS Service 名 | .srv 类型 | 关键请求字段 | 关键响应字段 |
|:------|:--------------|:---------|:-----------|:-----------|
| `FrRobot` | `/{device}_cmd` | `FrCmdInterface` | `id, position[6], axid, dir, deg, mode, speed, io_index, io_value` | `err_code, msg, jt_pos[6], tl_pos[6], register_val, io_val` |
| `SeerCtrl` | `/{device}_cmd` | `SeerCtrlCmdInterface` | `id, station, station_list[]` | `err_code, msg, task_status, task_type, target_station, station_list[]` |

### 7.5 代理层错误码

| 错误码 | 宏定义 | 含义 |
|:---:|:------|:-----|
| `0` | `ERR_DEV_PROXY_SEND_OK` | 成功 |
| `-1001` | `ERR_DEV_PROXY_SEND_TIMEOUT` | ROS Service 调用超时 (8s) |
| `-1002` | `ERR_DEV_PROXY_SEND_PARAM` | 参数错误 |
| `-1009` | `ERR_DEV_PROXY_SEND_OTHERS` | 其他错误 |
| `-1110` | `ERR_HYRMS_DEV_WARN` | 设备警告 |
| `-1120` | `ERR_HYRMS_DEV_ERROR` | 设备错误 |
| `-1130` | `ERR_HYRMS_DEV_EMERGENCY_STOP` | 急停 |
| `-1140` | `ERR_HYRMS_DEV_FATAL` | 致命错误 |
| `-1199` | `ERR_HYRMS_DEV_REG` | 设备未注册 |

---

## 8. 接口状态总表

| 接口 | 功能 | 状态 | 走 hyrms_export | 调用终点 |
|:-----|:-----|:----:|:----:|:---------|
| `refreshRobotList` | 刷新可选设备列表 | ✅ 可用 | ❌ | DB → MfmsRosBridge |
| `connectRobot` | 连接设备 | ✅ 可用 | ✅ | FrRobot::connect / SeerCtrl::connect |
| `disconnectRobot` | 断开设备 | ✅ 可用 | ✅ | FrRobot::disconnect / SeerCtrl::disconnect |
| `armJogJoint` | 机械臂关节点动 | ✅ 可用 | ✅ | FrRobot::moveAxis |
| `armJogCartesian` | 机械臂笛卡尔点动 | ✅ 可用 | ✅ | FrRobot::moveL |
| `armChangeMode` | 切换机械臂模式 | ✅ 可用 | ✅ | FrRobot::setMode |
| `refreshState` | 请求状态刷新 | ⚠️ 被动 | ❌ | 等待 topic 下次上报 (~50ms) |
| `getStations` | 查询 AGV 站点 | ✅ 可用 | ✅ | SeerCtrl::checkStation |
| `exeToStation` | AGV 单站导航 | ✅ 可用 | ✅ | SeerCtrl::guideGoTarget |
| `exeToStationList` | AGV 多站导航 | ✅ 可用 | ✅ | SeerCtrl::guideGoTargetList |
| `pauseNavigation` | 暂停导航 | ✅ 可用 | ✅ | SeerCtrl::guidePause |
| `resumeNavigation` | 继续导航 | ✅ 可用 | ✅ | SeerCtrl::guideContinue |
| `cancelNavigation` | 取消导航 | ✅ 可用 | ✅ | SeerCtrl::guideCancel |
| `queryNavigationStatus` | 查询导航状态 | ✅ 可用 | ✅ | SeerCtrl::checkGuide |
| `getPaths` | 查询路径名列表 | ✅ 可用 | ❌ | DB 查询 agv_path |
| `exeToPath` | 执行命名路径 | ✅ 可用 | ✅ | DB → SeerCtrl::guideGoTargetList |
| `addPath` | 保存路径 | ✅ 可用 | ❌ | DB 写入 agv_path + agv_path_station |
| `agvMoveForward` | AGV 前进 | ❌ 不可用 | — | 直接返回"不支持" |
| `agvMoveBackward` | AGV 后退 | ❌ 不可用 | — | 直接返回"不支持" |
| `agvTurnLeft` | AGV 左转 | ❌ 不可用 | — | 直接返回"不支持" |
| `agvTurnRight` | AGV 右转 | ❌ 不可用 | — | 直接返回"不支持" |

---

## 9. 关键源码索引

| 文件路径 | 职责 |
|:---------|:-----|
| `src/mfms_server/client_api/include/mfms_server/CommunicationInterface.h` | 纯虚 Qt 接口，定义 UI 侧信号/槽契约 |
| `src/mfms_server/client_api/include/mfms_server/CommunicationInterfaceImpl.h` | Meyers 单例实现，跨线程请求转发 |
| `src/mfms_server/client_api/src/CommunicationInterfaceImpl.cpp` | 单例初始化、Worker 创建、信号连接 |
| `src/mfms_server/client_api/src/CommunicationWorker.h` | Worker 头文件，定义独立线程接口 |
| `src/mfms_server/client_api/src/CommunicationWorker.cpp` | Worker 实现：ROS 初始化、设备管理、状态映射 |
| `src/mfms_server/gateway/include/mfms_server/MfmsGateway.h` | 门面抽象接口 |
| `src/mfms_server/gateway/include/mfms_server/MfmsGatewayImpl.h` | 门面实现头文件 |
| `src/mfms_server/gateway/src/MfmsGatewayImpl.cpp` | 门面实现：三大服务编排与信号转发 |
| `src/mfms_server/cmd_service/include/cmd_service/MfmsCommandService.h` | 命令服务接口 |
| `src/mfms_server/cmd_service/src/MfmsCommandService.cpp` | 命令服务实现 |
| `src/mfms_server/cmd_service/src/RobotProxyAdapter.cpp` | 机械臂代理适配器 (PIMPL) |
| `src/mfms_server/cmd_service/src/AgvProxyAdapter.cpp` | AGV 代理适配器 (PIMPL) |
| `HyRMS_export_.../hyrms_export/include/dev/robot/robot.hpp` | FrRobot 代理类 (hyrms_export) |
| `HyRMS_export_.../hyrms_export/include/dev/agv/agv.hpp` | SeerCtrl 代理类 (hyrms_export) |
| `HyRMS_export_.../hyrms_export/include/dev/dev_proxy.hpp` | 代理基类 HyDevProxy + HySimpleDevProxy |
| `src/com_interfaces/srv/FrCmdInterface.srv` | 机械臂 ROS Service 定义 |
| `src/com_interfaces/srv/SeerCtrlCmdInterface.srv` | AGV ROS Service 定义 |
| `MFMS_BASE_04171715.sql` | 数据库完整 Schema + Triggers |

---

> **文档维护说明**: 本文档基于 `58f80da` 提交的代码生成。当 `CommunicationInterface.h` 接口变更或新增设备类型时，应同步更新本文档。
