---
title: MFMS 新中台 · 架构构造思路（v0.4）
date: 2026-08-16
updated: 2026-08-22
tags:
  - 复合mfms
  - MFMS
  - 架构规划
version: v0.4
status: 当前架构基线
---

# MFMS 新中台 · 架构构造思路（v0.4）

> [!abstract] 本文定位
> 本文是 vault 中的**当前架构入口**，回答“系统由谁负责什么、数据从哪里来、哪些边界绝不能越过”。详细字段、流程与方案比较写在 [[MFMS新中台-逐层设计工作台]]；仍待确认的接口问题只在 [[MFMS新中台-待确定问题清单]] 登记；可执行约束同步到项目 <code>.trellis/</code>。

> [!important] v0.4 的两条核心边界
> **下位机是控制权的唯一管理者和裁决者；数据中台是原始需求的唯一写者。**
> 调度系统读取需求并拆分、单写订单与运单；数据中台只读派生表，不拆单、不推进订单或运单。

## 0. 结论分级

| 标记 | 含义 |
| --- | --- |
| **已确认** | 可进入 Trellis 红线与实现约束 |
| **推荐基线** | v0.4 推荐采用，但仍需接口/数据库评审后冻结 |
| **待确认** | 只能登记问题，不能当成实现事实 |
| **历史废弃** | 旧版假设，禁止继续派生设计 |

## 1. 最高原则【已确认】

1. 下位机独占控制权的存储、裁决、失效与安全拒绝。
2. 数据中台、Adapter 都是控制权申请者或持有者，不是管理者。
3. 中台的二次确认只承担开发者风险提示、权限校验和原因收集，不是锁协议。
4. 调度状态、订单状态和运单状态只用于展示与风险提示，不参与下位机锁判断。
5. 强制夺权造成的旧命令失效、运单暂停或失败，由下位机、Adapter 和调度系统按各自契约处理；中台只观察结果。
6. 缓存中的 <code>ControlSnapshot</code> 不是权限证明；能否执行始终以下位机响应为准。
7. 状态查询默认走 <code>StateManager</code>；只有命令、设置和显式控制权查询进入下位机。
8. 数据中台只接收、校验和持久化需求；调度系统负责把需求拆成订单和运单并单写两类派生记录。
9. 订单表至少包含工作站点名称/别名、上下料 <code>type</code>、<code>ids[]</code>、时间戳、数量和优先级。

> [!danger] 中台内禁止出现
> 控制权状态机、锁表、lease、fencing token、control epoch、confirmation token、跨系统夺权事务、Adapter 暂停协调、调度运单暂停推进。

## 2. 系统上下文

![[图片/SVG/14_1_1.svg|900]]

图中不表达 Redis 的部署拓扑，也不表达下位机内部如何实现锁。中台只冻结自己依赖的消息与调用契约；可编辑源见 [[MFMS新中台-架构图.drawio]]。

## 3. 四方职责

| 组件 | 负责 | 控制权关系 | 明确不负责 |
| --- | --- | --- | --- |
| **数据中台** | 接收/校验/持久化需求、只读查询订单/运单、实时状态视图、调试入口、控制请求、鉴权与审计 | 申请者；不管理锁 | 需求拆单、订单/运单写入、调度选车、执行 DAG |
| **调度系统** | 接管需求，拆分并单写订单与运单，维护派生记录与执行结果 | 需要设备时经 Adapter 访问下位机 | 由中台选主或管理；中台不介入其内部调度 |
| **Adapter** | 接收调度产生的运单、调用 ADK、发布运单执行消息 | 生产控制权的申请者或持有者 | 真实锁管理、需求/订单聚合、由中台协调暂停 |
| **下位机** | 控制物理设备、发布实时状态、管理和裁决控制权、拒绝无权或不安全命令 | **唯一权威** | 内部实现不属于中台设计范围 |

## 4. 中台内部结构【推荐基线】

~~~text
mfms-platform
├── endpoint/
│   ├── HttpEndpoint
│   ├── WebSocketEndpoint
│   └── SiteProtocolAdapter
├── application/
│   ├── RequirementIntakeService
│   ├── RequirementQueryService
│   ├── RealtimeViewService
│   ├── ControlAccessService
│   └── DebugCommandService
├── domain/
│   ├── Requirement
│   ├── DerivedOrder
│   ├── Waybill
│   ├── DeviceSnapshot
│   ├── ControlSnapshot
│   └── LiveExecutionSnapshot
├── capability/
│   ├── StateManager
│   ├── LowerMachineControlClient
│   ├── LowerMachineDebugClient
│   ├── RedisStreamIngestor
│   └── MessageDecoderRegistry
├── persistence/
│   ├── RequirementRepository
│   ├── ReadOnlyOrderRepository
│   ├── ReadOnlyWaybillRepository
│   └── AuditRepository
└── infrastructure/
    ├── CppProxySdkAdapter
    ├── MySqlConnectionPool
    ├── WebSocketPublisher
    └── StructuredLogger
~~~

仍采用**单进程、模块化单体**。SDK 类型只允许停留在 infrastructure/capability 边界，不进入 endpoint、application 或稳定 DTO。

### 4.1 控制访问不是锁服务

<code>ControlAccessService</code> 只做：

~~~text
用户权限检查
  → 读取当前控制权展示快照
  → 生成风险提示与二次确认内容
  → 调用普通申请或强制申请
  → 记录下位机结果
~~~

<code>LowerMachineControlClient</code> 是薄包装，目标调用面为：

~~~text
queryControl(device_id)
acquireControl(device_id, requester, request_id)
forceAcquireControl(device_id, requester, reason, request_id)
releaseControl(device_id, requester, request_id)
~~~

真实函数签名、身份字段和结果码仍需下位机团队确认。

### 4.2 ControlSnapshot 只读

建议展示字段：

~~~text
device_id
locked
owner_ip
owner_port
owner_type
owner_nick_name
owner_time
owner_description
received_at
quality
~~~

字段主要来自 <code>AgvControl</code>。快照过期时页面显示“控制权状态未知”、默认关闭调试按钮，并允许用户显式查询；不得把旧缓存解释为“锁已释放”。

## 5. 实时状态与消息契约

### 5.1 来源【已确认】

- 下位机经 Redis Stream 发布 <code>SeerCtrlState</code>、<code>VirtAgvState</code>、<code>SeerM4State</code>、<code>AgvControl</code>、<code>FrRobotState</code>、<code>AuboRobotState</code>、<code>JointPoint</code>、<code>ForceTorque</code>、<code>ForcePayload</code> 等实时状态。
- Adapter 经 Redis Stream 发布具体 <code>order_id</code> 的运行、进度、拒绝、故障与完成消息。
- 中台不负责 Redis 部署，但必须冻结可消费的数据契约。

统一消息信封至少包含：

~~~text
message_type
schema_version
factory_id
device_id
source_type
source_instance_id
sequence
observed_at
payload
~~~

中台接收时补充 <code>received_at</code> 与 <code>stream_entry_id</code>。

### 5.2 StateManager

| 缓存 | 主要输入 |
| --- | --- |
| <code>AgvRealtimeStateCache</code> | SeerCtrlState、VirtAgvState |
| <code>RobotRealtimeStateCache</code> | FrRobotState、AuboRobotState |
| <code>ControlStateCache</code> | AgvControl |
| <code>LiveOrderStateCache</code> | AgvOrderState、SeerM4State.order_state |
| <code>SensorStateCache</code> | ForceTorque、ForcePayload |
| <code>StateQualityTracker</code> | 新鲜度、乱序、解码失败、断流 |

所有快照携带 <code>observed_at / received_at / sequence / schema_version / quality</code>。质量枚举为 <code>UNKNOWN / FRESH / STALE / UNSUPPORTED</code>；中台重启后先全部置为 <code>UNKNOWN</code>。

> [!warning] 待确认
> <code>SeerM4State.order_state</code> 与 Adapter 的 <code>AgvOrderState</code> 是否描述同一个 <code>order_id</code>。权威来源未定前必须保留来源和序号，禁止无条件互相覆盖。

## 6. 需求、订单与运单持久化边界

### 6.1 职责【已确认】

| 逻辑记录 / 推荐表名 | 权威写者 | 中台权限 | 作用 |
| --- | --- | --- | --- |
| 需求 / <code>mfms_requirement</code> | 数据中台 | 读写 | 原始规范化业务需求与接入审计 |
| 订单 / <code>mfms_order</code> | 调度系统 | 只读 | 调度从需求拆出的上下料订单 |
| 运单 / <code>mfms_waybill</code> | 调度系统 | 只读 | 从订单继续拆出的物理运输/执行记录 |
| <code>mfms_control_audit</code> | 数据中台 | 读写 | 控制申请、强制申请与调试审计；不是锁表 |

推荐血缘为 <code>requirement_uid → order_uid → waybill_uid</code>。表名、ID 类型、外键、基数和状态机仍需 DDL/业务评审。

### 6.2 订单最小业务字段【已确认】

| 字段 | 说明 |
| --- | --- |
| 工作站点名称/别名 | 订单关联的工作站点 |
| <code>type</code> | 上料或下料；规范值候选 <code>LOAD/UNLOAD</code> |
| <code>ids[]</code> | 业务对象 ID 集合；元素语义与顺序待确认 |
| 时间戳 | 业务时间；请求/计划/创建含义待确认 |
| 数量 | 本次上下料数量；与 IDs 的关系待确认 |
| 优先级 | 调度排序输入；范围、方向和并列规则待确认 |

推荐物理列候选为 <code>workstation_alias / operation_type / item_ids / business_timestamp / quantity / priority</code>。运单字段、需求拆分基数、重试替换和终态规则由调度/Adapter 评审，不由中台发明。

## 7. 控制与调试流程

~~~mermaid
sequenceDiagram
    participant UI as 开发者客户端
    participant MFMS as 数据中台
    participant LM as 下位机
    participant RS as Redis Stream

    UI->>MFMS: 打开调试页
    MFMS-->>UI: 控制者 + MES 订单 + AGV 运单 + 风险提示
    UI->>UI: 二次确认并填写 reason
    UI->>MFMS: forceAcquireControl
    MFMS->>LM: 强制申请(device_id, requester, reason, request_id)
    LM->>LM: 裁决旧持有者与新持有者
    LM-->>MFMS: success / rejected / device_error
    LM->>RS: 发布新的 AgvControl 与设备状态
    MFMS-->>UI: 返回并审计下位机结果
~~~

~~~mermaid
sequenceDiagram
    participant UI
    participant MFMS as 数据中台
    participant LM as 下位机

    UI->>MFMS: jog / navigate / IO / robot command
    MFMS->>MFMS: 鉴权、参数校验、审计
    MFMS->>LM: 设备控制指令
    LM->>LM: 校验控制权和安全状态
    alt 允许
        LM-->>MFMS: success
    else 无控制权
        LM-->>MFMS: CONTROL_NOT_OWNED
    else 设备拒绝
        LM-->>MFMS: DEVICE_REJECTED
    end
    MFMS-->>UI: 映射后的唯一终态
~~~

## 8. 故障恢复与安全底线

| 场景 | 中台行为 |
| --- | --- |
| 中台重启 | 加载需求与只读订单/运单视图；实时状态全部 UNKNOWN；重建 Stream 消费与 SDK 连接；显式查询控制权 |
| Stream 中断 | 状态置 STALE，控制权视图未知，默认关闭调试；不修改真实控制权 |
| 下位机调用超时 | “结果未知”不等于失败；先 QueryControl 确认，不盲目重复强制夺权 |
| MySQL 不可用 | 暂停新需求与历史查询；实时状态可继续；因无法审计，默认禁止强制夺权 |
| 调度系统不可用 | 订单/运单视图标记过期；中台不拆分或恢复；调试仍以下位机控制权为准 |

建议角色：<code>viewer / operator / developer / admin</code>。强制夺权审计记录请求人、原因、页面当时展示的控制者/订单/执行 ID、下位机结果与时间；审计只证明“发起过什么请求、收到什么结果”。

## 9. v0.4 对旧问题的处理

| 问题 | v0.4 状态 |
| --- | --- |
| Q-31 | 中台不设计 Redis 拓扑；范围收敛为 Stream 信封、Payload、来源、设备 ID、时间与序号契约 |
| Q-32 | 职责已更新为中台需求、调度订单/运单；订单逻辑字段已确认，物理 DDL、拆分和运单合同待评审 |
| Q-33 | **中台范围内关闭**：互斥与夺权权威在下位机；中台只做访问体验、调用与审计 |
| Q-34 | Adapter 内部 ADK 编排/恢复移出中台范围；中台只要求规范运单实时消息和关联 ID |
| Q-02-rev | 基线不需要调度系统 gRPC 直连中台；需求/订单/运单经 MySQL，实时状态经 Stream；健康/管理 API 另评估 |

仍待确认的问题见 [[MFMS新中台-待确定问题清单#2. P0 接口待确认]]。

## 10. 关联

- [[MFMS新中台-逐层设计工作台]] —— v0.4 详细契约与实施切片
- [[MFMS新中台-待确定问题清单]] —— 唯一未决项登记簿
- [[MFMS新中台-Trellis工程结构方案]] —— 本方案在项目中的 spec/task 落位
- [[MFMS新中台-资料索引与权威源]] —— 版本、历史材料和证据边界
- [[MFMS新中台-架构图.drawio]] —— v0.4 可编辑图源
