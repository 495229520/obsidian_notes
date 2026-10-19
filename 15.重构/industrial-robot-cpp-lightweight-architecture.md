# 工业复合机器人系统 C++ 轻量化重构设计

**版本：1.0｜日期：2026-10-15｜适用场景：博赛德**  
**文档性质：新系统目标架构，不是现有代码结构，也不表示相关类已经实现。**

---

## 1. 方案总览

新系统采用：

> **三个独立 C++ 核心服务，一个独立 Qt 界面；每个服务三层代码、一条业务协调线程；复杂操作通过明确的工作线程隔离。**

三个核心服务分别是：

| 程序 | 负责什么 | 不负责什么 |
|---|---|---|
| `mfms_server`：中台服务 | 接单、业务校验、幂等落库、任务交接、执行事实、门控转接、外部反馈、设备管理和界面数据 | 不选机器人，不规划路径，不逐步审批机械臂动作 |
| `rds_server`：调度服务 | 选车、路径规划、运单拆分、执行推进、站点占用、机器人租约、充电、执行事件上报 | 不替上游维护库位业务，不直接操作厂商 SDK，不修改中台业务任务状态 |
| `hyrms_server`：下位机服务 | 设备加载、厂商接口、导航与机械臂等动作执行、Lua 运行、执行事实与遥测上报 | 不重新决定上游搬运需求，不承担全局多机器人调度 |
| `mfms_ui`：桌面客户端 | Qt 页面、RViz、设备监控、手动操作入口、日志和任务展示 | 不保存权威业务状态，不直接写业务数据库，不直接持有运动 SDK |

**中台原 Python 物流服务和 RDS 原 Python 在线服务都迁入新的 C++ 程序。目标版本不再依赖 Python 接单、门控、调度或 FastAPI 管理服务。** Python 离线工具、测试和 ROS launch 文件不属于必须翻译的运行核心。Lua 保留为下位机动作脚本，而不是隐藏的第二套调度系统。

这次重构不沿用旧代码的层层包装。旧 `CommunicationInterface → Worker → Gateway → Service → Proxy` 链只作为迁移期兼容入口，目标后端改为直接的 `接口 → 业务服务 → 技术适配器`。

### 1.1 “轻量化”的具体含义

轻量化不是删除必要的持久化和恢复逻辑，而是减少人为引入的框架机制：

- 不引入微服务网关、服务发现、Kafka、Kubernetes、分布式事务框架和自动故障接管集群。
- 不使用大型 ORM、依赖注入容器、通用工作流引擎、全局事件总线或运行时插件系统。
- 业务规则优先使用普通函数、`struct`、`enum class`、`std::variant` 和少量有状态的类。
- 同进程内直接调用函数；跨线程传递值对象；跨进程保留现有 MySQL、Redis、ROS 协议。
- 使用成熟网络和数据库库，但不再自研 HTTP/2、Redis 协议或线程调度器。

这里的“模块化单体”是指：**每个服务内部是一个清楚分块的程序，不再拆成十几个业务微服务。三个服务仍然独立部署、独立重启。**

### 1.2 本设计的依据与限制

本设计沿用前一轮分析及已提供的协议材料 [P1–P5]，不声称本轮重新读取了最新仓库或取得了未提交补丁。中台 `f293bf8` 的 GitHub 版本与本地十字段适配仍需一起冻结；SDK 3.0.0 必须核对 `202610081612` 批次的头文件、动态库和 `com_interfaces`。

本文给出确定的目标结构。涉及新 UI 接口、新命令账本、异常状态映射的变化，集中列在第 16 节；它们不是已有实现或已获准的数据库修改。工程学院、循光仍使用独立的场景配置、SDK 和部署，不自动套用博赛德规则。

---

## 2. 整体框架与进程边界

### 2.1 目标结构图

```text
                         上游任务 / 物流 / 工厂门控
                                  ⇅ HTTP/2
                     接单、状态反馈、OPEN / OPENED
                                  ⇅
┌────────────────────────────────────────────────────────────┐
│ mfms_server：C++ 中台                                       │
│                                                            │
│ TaskService       DeviceService       NotificationPump       │
│ 接单/状态/门控    设备管理/手动操作     回执/反馈/门请求重试    │
│                                                            │
│ MfmsStore       HTTP接口       Redis适配       ROS适配        │
└───────┬───────────────┬───────────────────────────┬─────────┘
        │               │                           │
        │ 新UI专用API   │ MySQL任务交接             │ ROS命令/遥测
        ⇅               │ Redis事件/回执            │ 设备/Lua数据库事件
┌─────────────┐         ⇅                           ⇅
│ mfms_ui     │  ┌─────────────────────┐  ┌───────────────────────┐
│ Qt / RViz   │  │ rds_server：C++调度  │  │ hyrms_server：C++执行  │
│ WebEngine   │  │                     │  │                       │
│ 不承担后台  │  │ Scheduler           │  │ CommandDispatcher     │
│ 任务生命周期│  │ TaskRunner          │  │ ExecutionRun          │
└─────────────┘  │ RoutePlanner        │  │ DeviceRegistry        │
                 │ TrafficManager      │  │ DeviceSession         │
                 │ RdsStore            │  │ LuaEngine             │
                 └─────────┬───────────┘  │ CommandStore          │
                           │              └──────────┬────────────┘
                           └── Redis指令/回执/状态 ──┘
                                                     ⇅
                                         导航/机械臂/视觉/RFID/夹爪
                                              厂商SDK或厂商进程

      持久化：继续使用 MFMS_BOSAIDE，按表划分写入职责
      消息与实时状态：继续使用既有 Redis DB0、Streams 和状态键
```

**图中的箭头都是双向交互，不是单向流水线。** 接口响应、动作完成、业务事件、门回调和数据库事件都有各自的返回路径。

### 2.2 为什么这样划分

中台将原 C++ 设备管理与 Python 物流业务合并到同一个无界面后台进程，但分成独立模块。二者不再需要额外的进程间调用，也不能混成一个 `Manager`。

RDS 保持独立：GUI 关闭不应终止调度；中台重启不应直接销毁 RDS 的在途任务对象；调度与设备执行也不应共享地址空间。

HyRMS 保持靠近设备：厂商库、Lua VM、设备句柄及设备线程不进入中台和调度进程。已经要求独立运行的厂商 ROS 节点或二进制组件可以继续独立，不为满足“四个程序”的图示强行嵌入。

第一版只允许一个拥有正式下发权限的 RDS 实例。另一份用于影子验证的程序必须没有正式数据库写权限和机器人指令发布权限。暂不建设自动主备切换。

---

## 3. 代码只分三层

### 3.1 分层定义

| 层 | 目录 | 放什么 | 不能放什么 |
|---|---|---|---|
| 接口层 | `interfaces/` | HTTP 路由、Redis 消息解码后的入口、ROS 回调入口、UI API；把外部请求转成内部类型 | 路径规划、事务拼接、取放流程判断 |
| 业务层 | `core/` | 任务模型、状态规则、业务服务、执行状态机、算法；以及必要的外部能力接口声明 | SQL、Redis 命令、HTTP 会话、QObject、厂商头文件 |
| 技术适配层 | `adapters/` | MySQL 存储、Redis、HTTP 客户端、ROS、SDK、Lua 绑定 | 重新定义业务状态、按设备名字猜业务流程 |

`main.cpp` 和 `app.cpp` 负责创建对象、连接回调和控制生命周期，**不是第四个业务层**。

```text
                   main / App：显式装配
                         │
              ┌──────────┼───────────┐
              ▼          ▼           ▼
          interfaces    core      adapters
              │          ▲           │
              └──────────┘           │
                     声明的端口 ◄─────┘
```

编译依赖指向 `core`；运行时由业务对象通过端口调用适配器。这里的端口通常只是几个虚函数或回调，不引入专门的“六边形架构框架”。

### 3.2 类与函数的使用原则

有持续状态或需要管理资源时才建立类，例如一个活跃任务对应一个 `TaskRunner`。单纯的校验、状态判断、错误映射写成函数，不创建 `TaskValidatorFactory`、`StateTransitionManager` 等包装层。

只为真正需要替身的外部依赖定义接口，例如 `IMfmsStore`、`IRdsStore`、`ICommandStore` 和 `IDeviceDriver`。普通内部类不机械增加 `I` 接口、实现类、工厂类三件套。

**不设计一个全系统共用的 `BaseService`。** 中台、调度、设备的业务模型分别定义；共享的是消息契约、错误类型和基础设施工具，不是一个包含所有表的万能 Repository。

---

## 4. 确定使用的技术栈

### 4.1 技术选择表

| 能力 | 默认选择 | 用法与代价 |
|---|---|---|
| C++ 标准 | **C++17** | 与当前工程及 Humble 基线衔接；不把 C++20 协程作为重构前提 [T1] |
| 构建 | CMake + Ninja；ROS 包使用 ament/colcon | 普通核心库可脱离 ROS 编译；需要 ROS 的目标再链接 rclcpp |
| 异步 I/O、定时器 | **Boost.Asio** | 一个业务 `io_context`；网络、定时器、工作结果统一回到业务线程 [T2] |
| HTTP/1.1 | **Boost.Beast** | RDS 既有管理接口和新增 UI 接口；不把 Beast 当作 HTTP/2 库 [T3] |
| HTTP/2 服务端 | **libnghttp2 C API + Asio** | 中台物流入口使用专用 HTTP/2 适配器；TLS 模式配 OpenSSL [T4] |
| 外部 HTTP 客户端 | **libcurl** | 门请求、物流反馈；独立限时与重试，检查实际协商协议 [T5] |
| MySQL | **libmysqlclient C API** | RAII 包装连接/语句/结果；显式 SQL 和事务，不引入 ORM [T6] |
| Redis | **hiredis** | 封装 Streams、状态键、现有 Redis Lua 原子脚本 [T7] |
| JSON | **nlohmann/json** | 固定 DTO 编解码；重复键必须在解析阶段检测，不能默认后键覆盖前键 |
| 日志 | **spdlog** | 统一结构化字段、滚动日志；不再自建日志线程框架 |
| ROS | **ROS 2 Humble / rclcpp** | 只存在于需要 ROS 的适配器；保持现有 msg/srv 与 SDK 配套 |
| 设备动作脚本 | **Lua 5.4 C API** | VM 和 C++ 绑定留在 HyRMS；脚本不接管业务调度 [T8] |
| 桌面界面 | **Qt5 Widgets / QtNetwork / RViz / WebEngine** | 保留界面技术，业务访问改为 `MfmsClient` [T9] |
| 测试 | GoogleTest/GoogleMock + CTest | 纯函数、状态机、适配器契约、真实存储集成分别测试 |
| 部署 | systemd + 明确的配置文件 | 三个服务独立启停；不以进程自动重启代替业务恢复 |

所有依赖使用锁定版本与哈希。表中的项目不是要求安装各项目“最新版本”；实际版本以 Ubuntu 22.04 / Linux x86-64、Qt、ROS 和 SDK 的兼容构建结果确定。

### 4.2 HTTP/2 是唯一不能假装“几行代码就解决”的网络部分

libnghttp2 负责 HTTP/2 协议处理，不负责 socket I/O；Asio 负责网络和定时器。需要实现少量、可独立测试的 `Http2Session` 适配代码，包括流生命周期、请求体限制、发送背压、连接关闭和流取消。[T4]

不自行实现 HPACK、帧解析或 HTTP/2 流控算法。也不采用已经在原项目中弃用的旧 `libnghttp2_asio` 封装作为新项目基础。[T10]

默认保持外部 `/handle_job` 的 HTTP/2 要求，不通过代理静默降级。h2c 直连、HTTP/1 Upgrade、TLS/ALPN 的实际使用方式必须从当前可用客户端和部署配置固定；不能只测 TLS/h2 就宣称原有 h2c 客户端兼容。

RDS 管理接口和 UI 内部接口使用 HTTP/1.1，不改变外部物流要求。外部物流与 RDS 的返回码分别封装，禁止全局统一成一种 `code`。

### 4.3 不选择的技术

不使用 gRPC/Protobuf 替换现有 JSON，不使用新消息队列替换 Redis，不引入 Redis 分布式事务来包装 MySQL，不使用全局线程池承载所有 SDK、SQL 和网络请求。

路径规划使用 STL 图结构和优先队列，不引入另一套大型图计算平台。目标是容易阅读、调试、测试和交付，不是以最少依赖数量为代价重新实现成熟协议。

---

## 5. 工程目录与构建产物

以下是逻辑工作区。`middleware + ui` 对应中台仓库，`scheduler` 对应 RDS，`robot` 对应 HyRMS；**不要求现在合并三个 Git 仓库**。

```text
robot-system/
├── common/                         # 版本化的公共CMake包
│   ├── contracts/                  # 固定线协议DTO、JSON编解码、错误类型
│   ├── runtime/                    # Asio入口、有限工作队列、取消与日志
│   └── io/                         # MySQL/Redis/HTTP的窄封装
│       ├── mysql/
│       ├── redis/
│       └── http/
│
├── middleware/
│   ├── main.cpp
│   ├── MfmsApp.hpp / .cpp
│   ├── core/
│   │   ├── models.hpp
│   │   ├── task_rules.hpp / .cpp
│   │   ├── TaskService.hpp / .cpp
│   │   ├── DeviceService.hpp / .cpp
│   │   ├── NotificationPump.hpp / .cpp
│   │   └── ports.hpp
│   ├── interfaces/                # HTTP路由、Redis入口、UI API
│   └── adapters/                  # MfmsStore、ROS、设备/Lua事件桥
│
├── scheduler/
│   ├── main.cpp
│   ├── RdsApp.hpp / .cpp
│   ├── core/
│   │   ├── models.hpp
│   │   ├── Scheduler.hpp / .cpp
│   │   ├── TaskRunner.hpp / .cpp
│   │   ├── RoutePlanner.hpp / .cpp
│   │   ├── TrafficManager.hpp / .cpp
│   │   ├── charge_policy.hpp / .cpp
│   │   └── ports.hpp
│   ├── interfaces/                # 原RDS HTTP、回执/设备事件入口
│   └── adapters/                  # RdsStore、事件发布、机器人通道
│
├── robot/
│   ├── main.cpp
│   ├── HyrmsApp.hpp / .cpp
│   ├── core/
│   │   ├── models.hpp
│   │   ├── CommandDispatcher.hpp / .cpp
│   │   ├── ExecutionRun.hpp / .cpp
│   │   ├── DeviceRegistry.hpp / .cpp
│   │   └── ports.hpp
│   ├── interfaces/                # Redis指令、ROS命令、DB事件入口
│   └── adapters/
│       ├── CommandStore.hpp / .cpp
│       ├── DeviceSession.hpp / .cpp
│       ├── LuaEngine.hpp / .cpp
│       └── drivers/               # 按实际厂商实现，不按命令建类
│
├── ui/                            # Qt页面、MfmsClient、展示模型
├── scenarios/
│   ├── bosaide/                   # 配置、脚本、地图资源、SDK清单
│   ├── engineering-college/
│   └── xunguang/
├── tests/                         # 各模块单测、协议回放、故障测试
└── deploy/                        # systemd、示例配置、只读preflight
```

公共包只维护一份源代码，发布带版本的 CMake 安装包供三个仓库使用，禁止复制三份后分别修改。`common` 不包含 `Scheduler`、任务状态机或厂商 SDK。

库按目标链接：RDS 不因为使用公共包就链接 Qt、ROS 或机械臂 SDK。厂商库使用明确的 CMake imported target 和运行时加载路径，不全局设置 `link_directories()`，不依赖目录名称判断版本。

产物为三个后台程序、一个桌面程序，以及按需的 ROS/厂商配套组件；不要求四个程序放在同一台机器。

---

## 6. 领域模型：先把“数据对象”分清

### 6.1 核心数据对象

| 类型 | 生命周期与关键内容 | 权威位置 |
|---|---|---|
| `JobRequest` | 一次上游需求；严格十字段、原样 taskCode | 中台 Record 中的原始需求 |
| `BusinessTask` | 对外业务任务；需求、业务状态、已应用序号 | 中台维护的 Record |
| `DispatchTask` | RDS 内部任务；业务关联、机器人、执行阶段 | RDS 任务表 |
| `Waybill` | 一个导航/取货/放货/检测/充电等执行单元 | RDS 运单表 |
| `DeviceCommand` | 一条可投递指令；command/task/waybill/sequence 与参数 | 下发前由 RDS 保存；下位机保存接受与执行事实 |
| `CommandAcceptance` | 指令被接受、拒绝或尚不能确定 | 下位机执行入口 |
| `ExecutionFact` | 实际执行进度/最终结果、关联身份、原始错误 | 下位机及 RDS 执行记录 |
| `SchedulerEvent` | 固定五字段；已发生的业务执行事件 | RDS 事件表 |
| `MiddlewareReceipt` | 固定八字段；收件或关联门结果 | 中台通知表 |
| `DoorRequest` | 固定四字段；稳定请求ID、任务、业务库位、OPEN | 中台门意图表 |
| `RobotSnapshot` | 位置、在线性、负载、执行身份、观测时间 | 下位机观测；中台/RDS各保存自己的只读视图 |
| `GraphSnapshot` | 固定地图版本、节点、有向平行边、代价 | RDS 从既有地图表加载的不可变对象 |
| `DeviceProfile` | 完整实例ID、prefix、厂商、消息类型、单位转换 | 已核对的设备资料与场景配置 |

**`JobRequest` 是数据，不需要自己的线程；`BusinessTask` 不是一个数据库连接；`Waybill` 不是一个进程。**

StorageCode 与 StationCode 必须是不同类型。机器人业务代码、数据库设备 ID、ROS 实例 ID、ROS service prefix 也不能互相替代。[P1、P2]

### 6.2 C++ 类型示意

```cpp
struct TaskCode    { std::string value; };
struct StorageCode { std::string value; };
struct StationCode { std::string value; };
struct DeviceId    { std::string value; };
struct RobotCode   { std::string value; };

enum class EndStorageType : int {
    Ground = 1, Rack = 2, Inspection = 5
};
enum class ContainerSize { Large, Small };

struct JobRequest {
    TaskCode task_code;
    StorageCode start_storage;
    StorageCode end_storage;
    EndStorageType end_type;
    StationCode start_station;
    StationCode end_station;
    std::string container_code;
    ContainerSize container_size;
    bool start_open_door;
    bool end_open_door;
};
```

内部名称可以统一为 C++ 风格；JSON 编解码层必须输出原来的十个字段名：

```text
taskCode / startStorageCode / endStorageCode / endStorageType /
startStationCode / endStationCode / containerCode / containerSize /
startStorageOpenDoor / endStorageOpenDoor
```

类型严格限制：endStorageType 是整数1/2/5，containerSize 是字符串A/B，两个开门字段是JSON布尔值；不接受字符串布尔或以整数替代。startStationCode与endStationCode允许相同；业务起终库位继续遵守原来的不同约束。禁止因为内部改名而改变报文。[P2 §2]

历史六字段记录使用单独的 `LegacyJobRequest` 读取路径，不为缺失的站点或终点类型编造默认值。新 HTTP 请求仍只接受十字段。[P2 §2]

### 6.3 状态至少分成三个维度

```cpp
enum class ExecutionState {
    Ready, Dispatching, Accepted, Running,
    Completed, Failed, Cancelled, Unknown
};

enum class DeliveryState {
    Pending, AwaitingReceipt, Confirmed, RetryDue
};

enum class ApplyState {
    Received, Applied, Rejected
};
```

这三个枚举用于内部，不作为新字段自动加入旧协议。

业务状态 4/5/6/7/8/9/10/201/301/601 仍由明确的编码函数映射。OutBox 的 0/1/2/3 另设 `HandoffState`，禁止与搬运进度共用枚举。

---

## 7. 中台服务的类、对象与实现方法

### 7.1 主要类

| 类 | 对象数量 | 主要方法 | 持有的状态 / 职责 |
|---|---:|---|---|
| `MfmsApp` | 每进程1个 | `start()`、`begin_shutdown()` | 装配所有对象，管理启动/关闭，不写具体业务 |
| `TaskService` | 1个 | `accept_job()`、`on_scheduler_event()`、`on_door_opened()` | 接单、事件与门回调的用例协调 |
| `DeviceService` | 1个 | `load_device()`、`unload_device()`、`submit_manual()`、`control_script()`、`snapshot()` | 设备管理、命令关联、最新展示快照 |
| `NotificationPump` | 1个 | `poll_due()`、`on_delivery_result()` | 发送已经持久化的回执、物流反馈、门请求 |
| `MfmsStore` | 1个 | `async_accept()`、`async_apply_event()`、`async_apply_opened()`、`async_load_notifications()` | 只在 DB 工作线程执行完整事务 |
| `RosAdapter` | 1个运行时；多个endpoint对象 | `send_command()`、`subscribe_state()` | 原 ROS endpoint、消息转换与返回关联 |
| `DeviceDbBridge` | 1个 | `request_load()`、`request_lua()`、`poll_ui_events()` | 保留设备/Lua事件表和触发器协议 |

HTTP route 注册、五字段解析、状态转换表写成函数，不再额外创建多层 Controller/Facade/Handler 类。

### 7.2 中台对象关系

```text
MfmsApp
├── ServiceRuntime
├── DbWorker ── MySqlConnection
├── RedisTransport
├── HttpServer / HttpClient
├── RosAdapter
├── MfmsStore
├── DeviceDbBridge
├── TaskService ────────────────借用──► MfmsStore
├── DeviceService ──────────────借用──► RosAdapter / DeviceDbBridge
└── NotificationPump ──────────借用──► MfmsStore / Redis / HttpClient
```

“借用”通过构造函数引用传入，不表示双方都拥有对方。销毁决定者是 `MfmsApp`。

### 7.3 接单怎么实现

```text
HttpRoute
  → 严格解析十字段 → JobRequest
  → TaskService.accept_job()
  → MfmsStore.async_accept()
       DB线程：BEGIN
         查询/锁定相同taskCode；校验已存内容
         新单：写Record + 独立主键OutBox
         重复同内容：返回原结果
         重复不同内容：拒绝，不覆盖
       COMMIT
  → 业务线程获得明确提交结果
  → HTTP返回原code=200 / 400 / 409等语义
```

具体方法：prepared SQL、唯一约束兜底、事务内比较完整内容。并发同时插入相同编号时处理唯一键冲突并读回，不能只做“先查再插”。

JSON 键顺序和空白排版不影响内容相等；字段类型、字符串内容和字段集合必须一致。taskCode 只做合法性判断，不裁剪、数值化或 Unicode 归一化。还要核对 MySQL 当前唯一索引和 collation，防止数据库把不同大小写的编号当作相同；约束调整另行迁移。

提交响应丢失时返回“结果待核验”，不声称一定回滚。相同 taskCode 重试后读取结果。Record 与 OutBox 使用独立主键和既有关联，不更改成同一个 ID。[P2 §2]

### 7.4 业务状态怎么实现

`task_rules.cpp` 中保存纯函数：

```text
validate_request(request)
validate_event_identity(task, event)
decide_transition(task, event, door_fact)
needs_start_door(request)
needs_end_door(request)
should_feedback(status)
```

`decide_transition()` 返回“允许后的状态变化”或“拒绝原因”，不执行 SQL，也不发送 HTTP。

应用事件的事务在 DB 线程读取当前行、调用这些纯函数、保存状态和通知意图后提交。业务线程只接收事务结果，不在多个异步回调之间拼装同一事务。

正常路径固定为：[P2 §3]

```text
type1：4 → [真实OPENED产生5] → 6 → 7 → 201

type2/5：4 → [真实OPENED产生5] → 6 → 7
             → [真实OPENED产生8] → 9 → 201
```

方括号仅在需要门时执行。type1 没有终点放货动作，不因为保存的终点开门标志为 true 而增加门请求。历史10允许读取及按历史规则完成，不生成假10。

### 7.5 门与外部反馈怎么实现

门不是普通 HTTP RPC：请求200只说明接收，`TaskService::on_door_opened()` 必须核对请求ID、任务、库位和当前有效阶段。

一个有效 OPENED 事务同时保存门事实、业务状态5/8和相应通知。`NotificationPump` 发布原4/7的门结果。重复回调返回幂等结果，不能产生第二次放行。[P2 §2–4]

`NotificationPump` 使用简单的 `NotificationKind` 分派：

| 类型 | 投递渠道 | 判定 |
|---|---|---|
| `MiddlewareReceipt` | Redis返回Stream | 持久通知按原 messageId 重投 |
| `LogisticsFeedback` | 外部HTTP | 仅6/201/601；HTTP成功且业务整数code=200 |
| `DoorOpenRequest` | 外部HTTP | 原请求ID重试；请求200不等于OPENED |

三者可以共用重试计算函数，但不能共用一种业务成功定义。外部 HTTP 很慢时，状态6的中台收件确认仍独立完成；不增加第二次6，不擅自发 CLOSE。[P2]

### 7.6 手动控制和设备管理怎么实现

`DeviceService` 只负责权限校验、任务冲突检查、端点解析、关联请求和结果展示。真正的设备互斥与执行在 HyRMS/控制器侧完成。

`resolve_endpoint(DeviceProfile, Operation)` 必须遵循实际资料：状态 topic 用完整实例 ID；命令 service 优先使用数据库配置的 prefix，缺失时才按既有规则处理；Fr/Aubo/Seer 等消息类型和单位各自转换，不统一猜字符串。[P1 §ROS/CMD]

无 requestId 的旧接口必须按端点串行、保留“未决”状态；超时后不能立即发下一条同类运动，再把迟到结果错配给新请求。新中台内部有 requestId，不意味着旧 SDK 已支持该字段。

---

## 8. RDS 的类、对象与实现方法

### 8.1 主要类

| 类 | 对象数量 | 主要方法 | 职责 |
|---|---:|---|---|
| `RdsApp` | 每进程1个 | `start()`、`reconcile()`、`begin_shutdown()` | 装配、恢复与关闭 |
| `Scheduler` | 1个 | `poll_demands()`、`schedule()`、`on_plan_ready()` | 读取需求、选车、创建内部任务，维护活跃任务表 |
| `TaskRunner` | 每个活跃内部任务1个 | `advance()`、`on_command_ack()`、`on_execution_fact()`、`on_receipt()`、`cancel()` | 一个任务的有限状态机，不负责整个车队 |
| `RoutePlanner` | 1个；计算可在小型CPU池并行 | `plan()`、`estimate_cost()` | 地图搜索、候选路径与成本；无数据库写入 |
| `TrafficManager` | 1个 | `try_acquire_robot()`、`reserve_next()`、`on_arrival()`、`renew()` | 占用和租约协调，保持不同资源的不同语义 |
| `RdsStore` | 1个 | `async_claim_demand()`、`async_save_transition()`、`async_apply_receipt()` | RDS持久化事务，不写中台Record状态 |
| `SchedulerEventPump` | 1个 | `poll_due()`、`on_publish_result()` | 扫描持久事件并可靠重投，不产生新的业务事实 |

充电准入、候选车排序、运单动作展开尽量写成纯函数，不增加单独的 `ChargeSchedulerService` 或通用流程引擎。

### 8.2 对象与数据的关系

```text
Scheduler
├── robots: map<RobotCode, RobotSnapshot>
├── graph: shared_ptr<const GraphSnapshot>
├── runners: map<TaskId, unique_ptr<TaskRunner>>
└── 借用 RoutePlanner / TrafficManager / RdsStore

TaskRunner（任务A）
├── DispatchTask
├── vector<Waybill>
├── 当前waybill_id / command_id / event_sequence
├── 当前执行阶段 / 确认等待 / deadline / generation
└── 无独立线程，无自己的数据库连接
```

任务对象可以有数百个，但不会因此创建数百条线程。RDS 的同一条业务线程顺序处理各任务事件，只有路径计算放入有界 CPU 池。

### 8.3 调度与运单拆分

```text
领取需求
 → 保存内部任务
 → 筛选可用机器人
 → 基于固定地图和机器人快照计算候选路线
 → 业务线程核对规划代次、地图版本和机器人可用性
 → 取得机器人执行权、保存选车/运单结果
 → 创建TaskRunner
```

拆分器使用 `std::vector<Waybill>` 和类型化动作，不使用可配置 DAG/BPMN 引擎。例如货架搬运生成“起点导航、取货、终点导航、卸货”等运单；门等待是既有到达事件的业务门槛，不变成中台机械臂动作审批。

终点类型1只移动到目标地码，不增加卸货；类型2使用原卸货动作；类型5使用该版本已有的倒置/检测台相关动作，不自行扩展成通用检测流程。检测台起点的回正等动作按已核对的下位机脚本执行，不由中台补猜。

导航路径中的每次访问保留独立位置索引，不能把动作简单地按 StationCode 去重。相同站点服务两个业务库位时，取放动作仍然不同。[P2、P5 §8]

### 8.4 TaskRunner 状态机

```text
Ready
  → WaitingRobot / Planning
  → WaitingResource
  → Dispatching
  → WaitingCommandAcceptance
  → Running
  → WaitingMiddlewareReceipt
  → 下一运单 或 Completed

任一需要核验的通信异常 → Reconciling / Unknown
取消请求                → Cancelling → 已确认停止/取消 或 Unknown
```

`WaitingMiddlewareReceipt` 包含普通收件等待，以及4/7对应的门完成等待；实际 gate 类型保存在内部，不能只看 `SUCCEEDED` 一个字符串。

`TaskRunner` 每次只做一个有限推进：检查条件、发起异步事务或外部操作、返回事件循环。禁止在 `advance()` 中写循环一直等待数据库、设备或开门。

设备接受 ACK 与设备完成是两件事：接受后还要核对 command/task/waybill/sequence、设备状态和实际到站事实。心跳在线、RPC成功、Redis XACK 都不能触发下一运单。[P3、P5 §1]

### 8.5 路径规划具体技术

采用 STL 邻接表保存**有向多重图**，节点使用稳定索引，边保存原 edge ID、方向、代价及业务属性。用 `std::priority_queue` 实现与原行为对齐的 A*，不再依赖 Python NetworkX。

迁移规则以已有规划输出为基线：[P5 §8]

| 项目 | 新实现规则 |
|---|---|
| 方向 | 不自动补反向边 |
| 平行边 | 保留，按原代价与边编码的决策规则选边 |
| 成本 | 分别保存接近成本、运输成本和总成本，不悄悄重定义单位 |
| 起点接入 | 保留原虚拟起点接入规则，不擅自改成只连最近点 |
| 同价路径 | 固定排序和打破平局的规则；不能只比较总成本 |
| 地图变化 | 在途计划绑定地图版本；新图使用新快照 |
| 候选机器人 | 保留原筛选与排序，包含可用性和载货限制 |

Dijkstra 可作为测试中的最短代价对照，不在迁移时悄悄替换实际算法。即使最短代价相同，访问序列、边ID或动作位置不同也可能影响占用，需要单独批准行为变化。

规划请求携带 `map_version + plan_generation`。结果返回时重新核对相关机器人位置/任务归属；过时结果丢弃。不能用每次心跳都会变化的版本号导致所有规划结果永远失效。

### 8.6 占用、租约与充电

`TrafficManager` 内部区分三种资源：

| 资源 | 权威与处理 |
|---|---|
| 节点物理占用/预占 | 保持既有 MySQL 权威；Redis为派生索引；实际离开后才释放 |
| RDS机器人执行租约 | Redis现有Lua比较owner续租/释放；租约丢失后停止继续下发 |
| 控制器控制权 | 由真实控制器提供；中台和RDS不能另建一套内存锁冒充 |

DB事务与Redis更新不是跨库原子操作。保留DB权威，使用可重试的索引修复，不让两处互相覆盖。

第一版不引入自动主备。租约过期不证明旧实例失去物理控制，也不证明车已停；无法证明旧实例停止下发时不自动接管。内部 generation 只防迟到回调，不冒充设备端 fencing。

自动充电由 `evaluate_charge(RobotSnapshot, ChargePolicy)` 产生独立内部任务，再复用同一 `TaskRunner` 和资源竞争流程。没有上游业务任务，不创建虚假中台需求或物流事件。电量单位、阈值比较、停靠点选择和结束条件沿用既有验证规则；任务完成不等于空载，载罐机器人能否充电须单独受资格条件控制。[P5 §8]

---

## 9. HyRMS 的类、对象与实现方法

### 9.1 主要类

| 类 | 对象数量 | 主要方法 | 职责 |
|---|---:|---|---|
| `HyrmsApp` | 每进程1个 | `start()`、`reconcile()`、`begin_shutdown()` | 运行时与所有设备资源的装配根 |
| `CommandDispatcher` | 1个 | `submit()`、`on_command_saved()`、`cancel()` | 统一接收Redis/ROS/脚本管理请求，鉴权、幂等、互斥准入 |
| `ExecutionRun` | 每个在执行的运单/动作组1个 | `start()`、`on_device_fact()`、`request_stop()` | 单次执行过程、代次、关联ID、结果状态 |
| `DeviceRegistry` | 1个 | `load()`、`unload()`、`find()` | 按完整DeviceId管理设备会话 |
| `DeviceSession` | 每设备1个 | `submit()`、`query()`、`request_stop()`、`close()` | SDK句柄、设备操作队列与厂商适配器的唯一所有者 |
| `IDeviceDriver`及厂商实现 | 每DeviceSession一个实例 | 类型化的启动、查询、停止能力 | 隔离厂商API、错误码与单位 |
| `LuaEngine` | 每个需要独立动作上下文的机器人/设备组1个 | `start_script()`、`resume()`、`request_abort()` | Lua VM、动作绑定、脚本执行与取消 |
| `CommandStore` | 1个 | `async_accept()`、`async_record_fact()`、`async_find()` | 持久命令身份、参数与执行结果 |

状态发布以 `HyrmsApp` 的定时任务调用适配器完成，初版不需要另设一个管理所有状态的超级 `StateManager`。

### 9.2 对象所有权图

```text
HyrmsApp
├── ServiceRuntime / DbWorker / RedisTransport / RosAdapter
├── CommandStore
├── DeviceRegistry
│   ├── DeviceSession[AGV实例]
│   │   └── AgvDriver + SDK句柄 + 所属工作线程
│   ├── DeviceSession[机械臂实例]
│   │   └── ArmDriver + SDK句柄 + 所属工作线程
│   └── DeviceSession[视觉/RFID/夹爪等]
├── LuaEngine[机器人/设备组]
│   └── Lua VM + 所属执行线程
└── CommandDispatcher
    └── map<CommandId, unique_ptr<ExecutionRun>>
```

厂商有进程级全局非线程安全状态时，多个 DeviceSession 需要共用一个厂商执行线程，而不是“每设备一线程”后并发调用同一个全局库。

### 9.3 一条设备命令如何执行

```text
Redis或ROS收到命令
 → 解析为类型化DeviceCommand
 → CommandDispatcher校验身份、参数和当前准入条件
 → CommandStore持久化“接受/已知重复/冲突”
 → 提交成功后返回接受结果
 → ExecutionRun驱动DeviceSession / LuaEngine
 → 厂商执行与传感器事实返回
 → 保存ExecutionFact
 → 发布原设备状态与可重放结果
```

首次接受必须建立持久身份，但这仍不等于物理恰好执行一次。进程可能在“保存开始”和“真正调用SDK”之间，或SDK已执行但结果未保存时崩溃。因此恢复默认进入核验，不因记录里写着 Accepted/Running 就自动重做动作。

同 command_id、同完整内容：返回已知接受/结果，不重复创建执行对象。同 command_id、不同内容：冲突并报警。指纹只作快速比较，还应保留原规范化参数供核验。

### 9.4 SDK 封装方法

按厂商或实际能力编写 `SeerDriver`、`FairinoDriver` 等具体适配器；不为每种命令建一棵继承树。命令参数使用 `std::variant<Navigate, MoveJoint, Grip, ReadTag, ...>` 等类型化结构，禁止核心层到处传“字符串命令名 + 任意JSON”。

厂商头文件、单位转换、原始错误码只在 `adapters/drivers/` 出现。返回统一的内部结果结构，同时保留原始错误码，不把所有失败压缩为一个 `false`。

ROS请求、Redis运单和Lua动作最终必须经过同一设备准入/仲裁位置；但内部对象统一不意味着外部endpoint或厂商msg被统一改名。

控制器停止命令必须遵守厂商允许的调用方式。不可把“停止”排在一个可能永久阻塞的普通SDK调用后，再声称具有可靠停机能力。必须确认独立停止通道、控制器安全能力或进程隔离方案；通用C++工作线程不是硬实时安全控制器。

### 9.5 Lua 的位置与边界

保留 Lua 表达已经验证的取货、放货、检测台动作及工艺参数。C++ 管理 VM、执行上下文、错误、日志、超时和取消，Lua 不直接连接 MySQL/Redis，也不决定业务任务最终状态。

Lua VM 只在所属线程使用。脚本阻塞点通过已审核的 C++ 绑定处理；采用可让出的绑定时使用 Lua 的 continuation API，不能跨不允许 yield 的 C 调用栈强行恢复。[T8]

不能在其他线程强行销毁正在运行的 VM。C API 的错误/让出可能采用 longjmp，绑定层必须避开跨越仍需析构的 C++ RAII 对象；在受保护入口转换错误，不能让异常越过 C 边界。[T8]

动作脚本及参数按版本绑定到一次执行。在途动作不能热替换脚本，也不序列化 Lua 调用栈来自动恢复运动；重启后根据设备事实和检查点决定人工恢复或受控继续。

### 9.6 卸载设备

`DeviceRegistry::unload()` 的顺序是：关闭该设备新准入，处理或取消队列，核验在途执行，注销回调，最后在SDK所属线程释放句柄。只从 map 中删除 `unique_ptr` 不构成完整卸载。

原设备/Lua数据库请求与UI事件触发器继续存在；新 `DeviceDbBridge`/下位机事件入口适配这些行为，不能因物流交接不用触发器就删除它们。[P3 §2]

---

## 10. 线程、对象寿命与关闭

### 10.1 一个服务一条业务协调线程

```text
                 ┌──────────────────────────┐
HTTP/Redis/ROS →  │ 主业务线程：Asio io_context│
                 │ 只修改业务对象与状态机     │
                 └───┬────────┬────────┬─────┘
                     │        │        │
                 DB工作线程 CPU工作池 SDK/Lua所属线程
                     │        │        │
                     └────值类型结果post回来──┘
```

Asio的完成回调在运行 `io_context::run()` 的线程执行；本设计只让一条线程运行该业务context，从结构上减少共享业务状态的锁。[T2]

**串行处理内存状态不等于多个异步操作形成了事务。** 两次数据库请求之间仍可能交错，必须靠事务内行锁/条件更新和结果版本检查保持一致性。

### 10.2 默认线程配置

以下是初始设计，不是性能测量结果；数量按实际负载验证。

| 工作 | 默认方式 | 关键要求 |
|---|---|---|
| 业务协调与异步HTTP | 每服务1条主线程 | 不执行阻塞等待、不长时间跑算法 |
| SQL | 每服务1条专用DB线程 | 连接在该线程建立、查询、事务、销毁；整笔事务作为一次工作 |
| Redis阻塞收件 | 每个必须独立消费的group/channel设置读取任务/线程 | 使用有限BLOCK；不能占用发布/续租连接 |
| Redis发布、ACK、租约 | 独立快速工作队列与连接 | 续租不排在阻塞XREAD后；连接不并发使用 |
| CPU规划 | RDS初始2个工作线程 | 队列有上限；线程只处理不可变输入 |
| 外部HTTP | 中台2个有界客户端工作线程 | 门请求和物流反馈限额隔离；每次请求有超时 |
| ROS | 需要ROS的进程一个受管理executor线程起步 | 回调只转换并post；厂商特定executor另行保留 |
| SDK | 每非线程安全域一个固定工作线程 | 不在任意线程池间搬运句柄 |
| Lua | 按机器人/设备组设置所属线程 | 与SDK执行分工，取消必须合作式 |

`boost::asio::thread_pool` 只提供固定线程池，不自动给出业务所需的有界准入。外层需要简单的排队计数/令牌限制：队列满就拒绝新的未接受工作，已接收的持久任务保留在数据库等待，不能无限堆内存。[T11]

遥测可以合并为每设备最新快照；命令、回执和执行事实不允许当作低价值数据丢弃。共享的快照和地图采用不可变副本，工作线程不改 `TaskRunner`。

### 10.3 所有权约定

| 对象 | 所有权规则 |
|---|---|
| 服务级对象 | `App`值成员或`unique_ptr`拥有，构造函数传引用借用 |
| TaskRunner / ExecutionRun | 对应registry中的`unique_ptr`拥有，不由网络回调拥有 |
| 不可变地图 | `shared_ptr<const GraphSnapshot>`，允许多个规划任务共享 |
| 网络会话 | 可用`shared_ptr`覆盖在途异步回调；关闭后禁止新的业务投递 |
| MySQL/hiredis/curl/SDK句柄 | RAII包装，每句柄一个明确线程所有者 |
| Qt对象 | 按Qt父子关系和线程亲和性管理，不混入后端对象所有权 |
| Lua VM | LuaEngine拥有，所有调用与关闭在所属线程 |

禁止全局可变单例、无主 `detach()` 线程、异步任务无条件捕获即将销毁对象的裸 `this`。取消后仍返回的回调必须携带任务ID和 generation，先查活跃对象及代次。

`shared_ptr` 只解决对象存活，不解决对象数据并发安全。析构不负责发一条长时间阻塞的设备停止命令；停止是显式阶段，析构只做最终资源释放。

### 10.4 关闭顺序

```text
拒绝新业务/停止新下发
 → 标记关闭代次，停止普通定时调度
 → 合作取消与已批准的设备停止流程
 → 保留回执、设备事实接收及必要租约处理，进行有界收尾
 → 保存未决任务/命令/通知状态
 → 等工作任务退出，停止Lua和SDK活动
 → 停止ROS executor并注销回调
 → 所属线程关闭数据库、Redis、HTTP与SDK句柄
 → 最后停止业务事件循环和日志
```

若SDK不可取消且一直不返回，普通线程无法提供无条件的有界关闭。此类厂商适配应放入独立辅助进程，或把该能力列为上线阻断项。杀进程仍不等于真机停止；安全控制必须独立核验。

---

## 11. 数据库、消息和恢复的最小实现

### 11.1 数据表继续按职责使用

| 数据 | 新程序写入者 | 规则 |
|---|---|---|
| `logisticsTaskRecord` | MFMS | RDS只读；业务状态由MFMS唯一维护 |
| `logisticsTaskOutBox` | MFMS创建，RDS维护交接 | 独立主键，关联Record；交接状态不当进度 |
| `logisticsBridgeEvent/Notification/Door/Diagnostic/LegacyAck` | MFMS | 复用已有事件、通知、门和诊断持久化 |
| `taskRecord/taskMergeRecord/waybillRecord/taskLog/schedulerEventRecord` | RDS执行链 | 保存调度、运单、结果和待投递事件 |
| 地图与设备资料 | 保持既有管理写入职责 | RDS读取地图；不因重构转移全部配置所有权 |
| 节点占用 | RDS相关存储 | 保持既有权威及释放条件 |
| `device_state/lua_state`及事件表 | 按请求/执行事实分别由MFMS、HyRMS处理 | 保持触发器、键结构和返回链 |
| **`robotCommandJournal`，拟新增** | **HyRMS写，RDS只读恢复** | 见下一节；必须单独批准迁移 |

前三组现有表的具体列、索引和写入权限以实际DDL为准，不从类名推导表结构。[P3 §2]

### 11.2 必须补齐的持久命令账本

为了避免只靠进程内256条之类缓存处理重启重复，目标增加一张 HyRMS 自有的 `robotCommandJournal`，仍放在 `MFMS_BOSAIDE`，不另建业务库。[P5 §4、§7]

建议保存的**概念字段**：

```text
command_id（唯一）
robot_code / task_id / waybill_id / sequence
canonical_payload / payload_fingerprint
acceptance_state / execution_state
acceptance_result / execution_result / original_error
accepted_at / started_at / finished_at
execution_generation / recovery_note
待发布接受结果、最终结果的标记
```

这是逻辑结构，不是可直接执行的DDL。字段长度、JSON类型、索引和授权要在实际数据库核对后确定。RDS只读这张表核验未决设备命令，不得改写下位机结果。

采用中心库账本的明确代价是：**DB不可用时不接受新的普通运动命令。** 安全停止通道不依赖这笔落库；已经在执行的动作按设备安全规则处理。若最终结果尚未落库时再次崩溃，仍可能需要人工核验，不能靠账本声称物理恰好一次。

本方案不同时引入SQLite和另一套本地同步日志。若未来要求断开中心DB后继续独立执行，应另做离线执行与本地持久化方案，而不是悄悄改变这一可用性边界。

### 11.3 四个本地事务

| 事务 | 同一事务中保存 | 提交之后 |
|---|---|---|
| MFMS接单 | Record + OutBox | HTTP接单成功 |
| RDS执行推进 | 任务/运单变化 + 稳定eventId/sequence的事件 | 事件发布器XADD |
| MFMS收件/门完成 | Inbox、合法状态变化、回执/门/反馈意图 | 输入XACK；通知发布器独立发送 |
| RDS收回执 | 对应事件确认、幂等结果 | 回执XACK |

下位机命令接受也必须先建立持久身份再作接受承诺。任意两个进程之间都没有跨MySQL、Redis、SDK的全局事务。

DB工作线程一次执行完整事务：读取当前行、调用纯规则函数、写入、提交。发生死锁可只重试明确可安全重做的数据库事务；提交结果未知时先核验，不能通用地重跑包含外部副作用的业务函数。

### 11.4 保持两套不同的 Redis 协议

| 链路 | 物理Stream | JSON字段 |
|---|---|---|
| RDS → MFMS | `bosaide:scheduler:events` | `data` |
| MFMS → RDS | `bosaide:middleware:results` | `data` |
| RDS → HyRMS | `{robotCode}:stream` | `payload` |
| HyRMS → RDS | `vehicle:execution:ack:stream` | `payload` |

固定事件五字段：

```text
eventId / taskCode / sequence / taskStatus / storageCode
```

固定回执八字段：

```text
messageId / sourceEventId / taskCode / taskStatus / status /
requestOpenDoorId / storageCode / reason
```

成功严格为 `SUCCEEDED`，不适用字段保留 `null`。新内部日志的 traceId、generation、错误详情不自动加进这两种报文。机器人代码与设备ID通过已有资料映射，不靠截取字符串猜测。[P2 §3、P3 §3]

### 11.5 发布与消费方法

发布器只扫描持久待发记录，按原业务身份和内容重投。XADD返回丢失后重发可能产生多个Redis条目，但它们指向同一个eventId/messageId。

使用一个普通 `RetryPolicy` 值对象计算指数退避、随机抖动和最大间隔；尝试次数、下次时间和最后错误持久保存或由现有状态安全推导。达到告警阈值不等于删除已接受的业务事实。通知重试期限与搬运业务期限分开。

收到重复eventId时，即使原通知已经标记发送，也应能根据持久事实重新发布原messageId的回执。这用于恢复“中台曾发回执，但RDS仍未收到”的情况；不能重新开门或重复应用业务状态。

消费流程为：读取新消息或Pending → 解码 → 事务持久处理 → 提交明确成功 → XACK。提交成功后、XACK前崩溃，重投只能恢复原处理结果，不能重复推进或生成新的门动作。

恢复既包括自身Pending，也包括确认旧消费者失效后的孤儿接管。Redis 6.2及以上可用 `XAUTOCLAIM`；更早的Streams版本使用 `XPENDING/XCLAIM`。必须检查现场版本和返回结构。[T12]

消费组初始化不能从`$`跳过待处理历史；接管不能删除消费组。流裁剪按最大恢复窗口、持久源和未处理项制定，不能无限增长，也不能把Pending指向的内容提前丢掉。

乱序 N+1 先到先持久保存，不越过缺失N应用。RDS从原事件记录补投N，不创建一个新序号替代，也不伪造状态10。

### 11.6 回执与异常的边界

| 情况 | 处理 |
|---|---|
| 普通事件已持久收件 | 可按原协议返回SUCCEEDED收件确认；业务是否应用另有记录 |
| 4/7需要门 | 不能用普通收件成功提前放行；等待有效OPENED形成原4/7门结果 |
| 4/7无需门 | 校验业务阶段与库位通过后返回普通确认 |
| 消息冲突或非法门阶段 | 持久诊断，不产生放行动作；不滥用仅表示明确门拒绝的FAILED |
| 501/504 | 目标对齐内部失败记录；外部反馈仍只6/201/601；约束变更先迁移 |
| 已201，仅回执超时 | 保持201，继续投递或核验通知，不改301 |
| 物理动作结果未知 | 暂停后续运动，进入核验；不自动换ID重做 |

五字段事件不携带完整下位机错误详情。首版将原始错误保留在RDS/HyRMS执行记录，中台至少保存准确的事件身份和状态；不能从501/504编造具体原因。需要跨服务查询详细原因时走单独的诊断接口，不扩充旧事件字段。

`SUCCEEDED`、XACK、OPENED、CommandAcceptance、ExecutionFact、业务201必须在代码类型和日志中区分。

### 11.7 重启恢复不是“清空重新开始”

启动分成：配置/SDK/表结构只读检查 → 加载未决记录 → 核验设备和原执行身份 → 恢复可靠投递 → 对已确认安全的任务恢复推进。

不在启动时结束所有旧任务、清队列、释放全部站点或自动重发运动。对结果未知的任务提供受权限控制的恢复操作：补投事实、核验完成、确认失败、确认停止后解除资源、以新任务执行补偿。每次记录操作者、理由、证据和前后状态。

---

## 12. Qt 客户端如何变薄

### 12.1 新UI对象

| 类/对象 | 数量 | 职责 |
|---|---:|---|
| `MfmsClient` | 1个 | QtNetwork异步请求，连接、超时、请求ID与错误转换 |
| `UiStore` | 1个 | 最新显示快照、连接状态、未完成界面请求；不作业务权威 |
| 页面/ViewModel | 每页面或功能区1个 | 把UiStore数据映射到控件，发用户意图 |

不再保留后端业务使用的Qt信号转发套娃。界面内部继续用signals/slots是合适的；不要把signals扩展成跨整个机器人系统的全局消息总线。

### 12.2 UI专用API

新增内部接口，与外部物流入口使用不同路由和鉴权：

| 示例接口 | 用途 |
|---|---|
| `GET /ui/v1/snapshot` | 当前设备、任务摘要和连接状态 |
| `GET /ui/v1/tasks/{taskCode}` | 任务详情和已记录的业务事实 |
| `POST /ui/v1/device-commands` | 手动设备操作申请，返回可关联的请求身份 |
| `GET /ui/v1/device-commands/{requestId}` | 查询接受/执行/未知结果 |
| `POST /ui/v1/devices/{deviceId}/load` | 加载/卸载等受控管理请求 |
| `POST /ui/v1/scripts/{scriptId}/control` | 携带groupId的脚本管理请求 |

上述为**目标新增接口**，具体DTO须在实现时形成独立版本契约；它们不是现有的外部物流API。

第一版使用可调频率的快照轮询，例如设备面板250ms、任务列表1s，作为初始配置而非实时保证。按需要使用长轮询减少空请求，不引入WebSocket框架作为前置条件。接口保留快照版本和生成时间，重连后重新获取完整快照。

RViz可保留只读ROS可视化通道，以满足高频姿态显示；不借此绕过中台直接执行设备命令。图形控件只在GUI线程操作；`QNetworkAccessManager`及回复在其所属线程使用和销毁。[T9]

UI断连后，已接受任务仍由后台继续执行。UI按钮超时不自动重发运动，重新连接后查询原请求。内部API默认仅本机受控访问；远程访问需要独立认证、网络边界及TLS，不能因为“局域网”就关闭权限检查。

---

## 13. 错误、配置、日志和可观测性

### 13.1 错误类型

内部使用明确结果，例如 `std::variant<T, Error>`；预期失败不依靠到处抛异常控制状态机。

`Error` 至少区分：参数非法、业务冲突、暂时不可用、明确拒绝、超时但结果未知、确定执行失败、协议不兼容。保留原始设备错误和来源。

同一个内部错误由各接口自己的编码函数转换。物流成功code=200，RDS管理接口成功code=0，UI接口使用独立版本定义，不互相复用错误码表。[P4 §已兼容部分]

### 13.2 配置方法

每服务一个JSON配置文件，敏感值只保存环境变量名或密钥文件引用。配置解析后成为不可变 `Config` 值对象。

配置分成：连接信息、场景/SDK身份、消息键和组、线程/队列上限、超时/重试、业务参数、外部路由/鉴权。三个场景使用独立profile，不散落 `if (scene == ...)` 到每个模块。

不在启动时自动建库、建表或重建触发器。`preflight` 只读检查并报告差异；DDL迁移由独立工具和独立授权执行。门请求URL与回调路径必须同时配置；未配置时不能伪造门成功。

进程内超时使用单调时钟；持久时间使用统一时区时间，并在重启时重新计算剩余期限，不能保存一个跨重启无意义的steady_clock值。

初版不热更新数据库地址、SDK、地图解释规则或在途脚本。确需热更新的展示和日志参数单独列白名单。

### 13.3 日志与状态

日志统一包含：

```text
service / scene / build_commit / timestamp / level
trace_id / taskCode / task_id / waybill_id / command_id
eventId / sourceEventId / sequence / robotCode / deviceId
old_state / new_state / duration_ms / error_source / error_code
```

不适用字段为空；内部关联ID不自动进入固定协议。账号密码、Redis认证、门控token不写日志。

首版使用日志、`/health/live`和`/health/ready`及诊断快照，不强制部署一整套监控集群。live只说明进程在运转；ready表示允许新工作，不表示真机已安全。关键指标包括Pending年龄、事件缺号、通知重试、DB队列长度、业务循环延迟、未知命令数和租约续期状态。

---

## 14. 核心功能与实现落点速查

| 具体功能 | 实现类/函数 | 使用方法 |
|---|---|---|
| HTTP/2接单 | Http2Session → TaskService → MfmsStore | nghttp2/Asio、严格DTO、MySQL本地事务 |
| taskCode幂等 | `async_accept()` | 唯一约束、完整内容比较、提交未知核验 |
| 任务交接 | Scheduler + RdsStore | 既有OutBox轮询/领取，不新建消息系统 |
| 业务状态应用 | `decide_transition()` + MfmsStore | 纯函数转换规则、行锁事务、序号连续应用 |
| 选车与路径 | Scheduler + RoutePlanner | 固定快照、STL图、A*、稳定平局规则 |
| 运单推进 | TaskRunner | 显式枚举状态机、异步事件驱动 |
| 开门 | TaskService + NotificationPump | 稳定请求ID、HTTP接收与真实OPENED分离 |
| 外部反馈 | NotificationPump | 持久意图、libcurl、独立重试 |
| 设备接受/执行 | CommandDispatcher + ExecutionRun | 持久身份、准入仲裁、接受与完成分离 |
| SDK与Lua | DeviceSession + Driver + LuaEngine | 所属线程、类型化参数、合作取消 |
| 占用与租约 | TrafficManager | DB权威占用、Redis Lua owner校验、核验后释放 |
| 充电 | `evaluate_charge()` + TaskRunner | 独立内部任务，复用执行与资源协调 |
| 消息恢复 | 两类Pump + RedisTransport | 同ID重投、提交后XACK、Pending接管 |
| 重启恢复 | App.reconcile + Store | 持久记录对账，不清空、不盲目重做 |
| GUI数据 | DeviceService快照 + MfmsClient/UiStore | 独立UI API、异步请求、断线重同步 |

---

## 15. 如何落地，而不是再造一层旧系统包装

### 15.1 实现顺序

| 阶段 | 写什么 | 完成标志 |
|---|---|---|
| A：新骨架和契约 | 公共DTO/编解码、三个App、运行时、状态规则、旧数据回放 | 无ROS/SDK也能运行核心测试；十/五/八字段和历史读取通过 |
| B：新中台 | HTTP/2、MfmsStore、TaskService、通知、门控、DeviceService与UI客户端 | C++接单到交接/回执/反馈在隔离环境闭环；Python桥接不再是必需进程 |
| C：新RDS | C++规划、TaskRunner、TrafficManager、充电及原管理HTTP | 对照旧输入验证路径/运单与资源行为；只有新实例或旧实例之一有下发权 |
| D：新HyRMS | 设备会话、统一准入、受管理执行、命令账本、Lua与SDK适配 | 崩溃/重复/取消/未知结果可测试；原接口和真实SDK成套验证 |
| E：受控替换 | 冻结部署包、状态接管、旧模块退役 | Linux及经批准的真机/门控验收后逐项切换 |

新代码可以独立设计，不需要先把旧代码“美化一遍”才能实现新框架。旧系统保留为行为对照和回滚版本；兼容包装放在 `interfaces/legacy/`，只用于迁移，不渗透新核心。

本轮不执行上述阶段，不修改仓库、生产数据库、调度启停或设备状态。

### 15.2 最小验收门槛

协议测试必须覆盖十字段严格类型、重复键、原样编号、同单冲突、历史六字段读取、三种终点乘四种门标志组合、固定事件/回执字段及原HTTP成功码。

故障测试必须覆盖每个COMMIT/XADD/XACK前后退出、Pending恢复、乱序缺号、201回执丢失、设备接受回执丢失、最终结果未落库、SDK阻塞、取消和迟到回调。

算法测试比较完整站点访问、边ID、成本和动作位置，不只比较“能到终点”。资源测试覆盖租约丢失、占用冲突、载货资格、充电边界和恢复。

Mac/ARM用于编辑、纯核心测试与模拟。正式构建必须在Linux/x86-64、ROS 2 Humble、Qt和成套SDK环境验证；模拟成功不等于门或设备已经验收。

### 15.3 回滚边界

切换时只允许一个任务写入者和一个机器人下发者。旧新服务不能同时消费正式任务并执行。

回滚程序不回滚物理事实。已经取货、开门、写入新状态或进入未知执行的任务，必须先对账；不能重启旧RDS并让其启动清理逻辑“解决”未决任务。

---

## 16. 明确改变的内容与保留的内容

### 16.1 本设计提出的结构或行为变化

| 编号 | 变化 | 实施条件 |
|---|---|---|
| C01 | 中台变为无界面后台服务，设备管理与物流共进程；Qt单独运行 | 新UI API、权限和生命周期联调后切换；不是当前已经这样部署 |
| C02 | 接单/门控/反馈/RDS管理与调度全部由C++实现 | 保持原外部接口和状态语义，不长期保留Python在线核心 |
| C03 | 在既有库增加下位机命令账本，RDS只读核验 | 独立DDL/权限/备份/回滚审批；DB不可用时新普通运动不接收 |
| C04 | 不再因201回执超时转301；事件交付与业务终态分离 | 三方异常路径协议测试通过 |
| C05 | 对齐501/504的内部记录，保留外部反馈白名单 | 核对Record约束；必要时迁移；不自动扩展物流反馈 |
| C06 | 重启从“清空/结束旧任务”改为对账恢复 | 在途身份、资源状态及未知结果都有可核验来源 |
| C07 | 所有在线指令入口汇聚到设备仲裁和唯一会话所有者 | 保留各ROS/Redis/DB入口；逐条验证厂商语义 |

这些变化是方案的一部分，但不等于允许直接实施。不能把新增内部数据模型或API当作已存在的生产能力。

### 16.2 不改变的业务约束

上游负责需求与库位/站点映射；中台不做路径规划或检测台调度。Record/OutBox边界、共享库、两套Redis通道、五/八字段、门四字段、HTTP/2物流入口、真实OPENED、三类正常完成路径和旧10读取均保留。[P2、P3]

不自动把一般业务containerCode转换成数字RFID。前轮发现的罐号格式约束差异必须以SDK批次和实际业务身份解决；该问题不能靠删前缀、`stoi()`或填默认值掩盖。[P5 §4]

### 16.3 最终框架应呈现的样子

```text
用户看到：一个Qt界面。
部署人员管理：中台、调度、下位机三个C++服务。
开发人员阅读：每服务interfaces / core / adapters三层。
业务人员核对：原有任务、库位、状态、门控和执行语义。
排障人员追踪：稳定ID、持久记录、可解释的状态转换。
```

**核心不是把旧类重新命名，而是让每个业务状态只有明确写入者，每个在途任务只有明确执行对象，每个SDK句柄只有明确所有者，每次外部操作都有明确结果与恢复办法。**

---

## 17. 依据与参考资料

### 17.1 已提供的项目材料

| 编号 | 材料 | 本文使用范围 |
|---|---|---|
| P1 | `mfms-architecture.md`，第25–107行；第113–141行 | 旧调用链、端点、Qt/ROS/SDK边界；历史LightCore不代表已实现 |
| P2 | `bosaide-task-system-fields.md`，第7–59行；第74–106行 | 职责、十字段、门、三类终点、状态反馈与兼容读取 |
| P3 | `bosaide-site-database-redis.md`，第15–79行 | 共享数据库、表所有权、设备/Lua触发器、实际Stream键；其中历史六字段接单描述由P2更新规则覆盖 |
| P4 | `report.md`，第13–68行 | 正常链路兼容、501/504、201/301、重投/Pending及测试边界 |
| P5 | `analysis-and-migration-plan.md`，前一轮分析，第0、4、6、7、8节 | GitHub/补丁边界、已读源码发现、线程风险、规划与恢复基线；不是本轮重新扫描结果 |

### 17.2 技术库官方资料

以下资料用于核对库能力和使用边界；对象划分、类名和迁移方案是本文件的目标设计。依赖版本仍需按目标Linux环境锁定。

- **T1 — ROS REP-2000**：Humble的平台与语言基线。https://github.com/ros-infrastructure/rep/blob/master/rep-2000.rst
- **T2 — Boost.Asio：Threads and Boost.Asio**：完成回调、io_context与线程关系。https://www.boost.org/latest/doc/html/boost_asio/overview/core/threads.html
- **T3 — Boost.Beast Introduction**：HTTP/1与WebSocket能力边界。https://www.boost.org/doc/libs/latest/libs/beast/doc/html/beast/introduction.html
- **T4 — nghttp2 Programmers' Guide**：协议处理与I/O分离、session和线程约束。https://nghttp2.org/documentation/programmers-guide.html
- **T5 — libcurl CURLOPT_HTTP_VERSION**：HTTP版本请求、协商及连接复用。https://curl.se/libcurl/c/CURLOPT_HTTP_VERSION.html
- **T6 — MySQL C API Threaded Clients**：连接并发与线程初始化约束。https://dev.mysql.com/doc/c-api/8.4/en/c-api-threaded-clients.html
- **T7 — Redis hiredis guide**：Redis官方C客户端入口。https://redis.io/docs/latest/develop/clients/hiredis/
- **T8 — Lua 5.4 Reference Manual，§4.4–4.5**：错误、longjmp、continuation与yield。https://www.lua.org/manual/5.4/manual.html
- **T9 — Qt5.15线程与网络对象**：https://doc.qt.io/archives/qt-5.15/threads-qobject.html ；https://doc.qt.io/archives/qt-5.15/qnetworkaccessmanager.html
- **T10 — 原libnghttp2_asio文档中的弃用与迁移说明**：https://nghttp2.org/documentation/libnghttp2_asio.html
- **T11 — Boost.Asio thread_pool**：固定大小线程池的职责。https://www.boost.org/latest/doc/html/boost_asio/reference/thread_pool.html
- **T12 — Redis XAUTOCLAIM**：Pending所有权接管与版本要求。https://redis.io/docs/latest/commands/xautoclaim/
