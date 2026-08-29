---
title: MFMS 新中台 · 逐层设计工作台
date: 2026-08-14
updated: 2026-08-22
tags:
  - 复合mfms
  - MFMS
  - 架构规划
version: v0.4
status: 活跃工作台
---

# MFMS 新中台 · 逐层设计工作台

> [!abstract] 本文定位
> 本文是 <code>14.复合mfms/</code> 下的**唯一活跃详细设计工作台**。当前架构结论见 [[MFMS新中台-架构构造思路]]，所有未决项见 [[MFMS新中台-待确定问题清单]]，项目落位见 [[MFMS新中台-Trellis工程结构方案]]。

> [!important] v0.4 设计锚点
> 控制权继续由下位机唯一裁决；业务持久化更新为“中台负责需求，调度拆分并单写订单与运单”。本文只设计中台的需求接入、只读组合视图、控制访问、消息消费、权限与审计。

## 0. 工作约定

### 0.1 结论标签

- 【已确认】：用户明确确认，可写入红线。
- 【推荐基线】：本版首选方案，需要联合评审后冻结物理字段/接口。
- 【待确认】：列入问题清单，不进入实现。
- 【历史废弃】：保留迁移提醒，不再扩展。

### 0.2 v0.3 → v0.4 变更

| 主题 | v0.3 | v0.4 |
| --- | --- | --- |
| 控制权 | 下位机唯一管理和裁决 | 保持不变 |
| 实时状态 | 下位机/Adapter 通过 Redis Stream 发布规范消息 | 保持不变；Adapter 消息改以运单关联 ID 关联 |
| 中台业务对象 | 中台保存 order_request | 中台保存原始需求，不创建订单/运单 |
| 调度持久化 | 调度写 execution/binding | 调度从需求拆分并单写订单与运单 |
| 订单字段 | 推荐通用 payload/执行摘要 | 已确认站点别名、上下料 type、ids[]、时间戳、数量、优先级 |
| 统一视图 | OrderView | RequirementView：需求 → 订单 → 运单 → 实时状态 |

## 1. 系统边界

### 1.1 四方职责【已确认】

| 组件 | 输入 | 输出 | 不进入中台设计的内部细节 |
| --- | --- | --- | --- |
| 数据中台 | 客户端需求、Stream 状态、下位机调用结果、MySQL 订单/运单只读数据 | 需求、RequirementView、控制/调试请求、审计 | 不拆单、不写订单/运单、不管理真实控制锁 |
| 调度系统 | 待处理需求、设备/运单消息 | 订单、运单、发往 Adapter 的运单 | 拆分、选车、交通、重试、故障转移 |
| Adapter | 调度运单、下位机状态 | ADK 调用、AGV 运单实时消息 | 内部状态机、崩溃恢复、控制权申请策略 |
| 下位机 | 控制请求、调试命令、Adapter ADK 调用 | 物理动作、控制权裁决、设备/控制状态 | 锁实现、安全互锁、设备驱动 |

### 1.2 两条业务链

**需求执行链**：

~~~text
MES / Qt / Web
  → 数据中台写 mfms_requirement
  → 调度读取并接管需求
  → 调度拆出并单写 mfms_order
  → 调度继续拆出并单写 mfms_waybill
  → Adapter 调 ADK
  → 下位机执行
  → Adapter/下位机发 Stream 消息
  → 调度更新订单/运单持久状态
  → 中台只读组合 RequirementView 与实时展示
~~~

**调试链**：

~~~text
开发者客户端
  → 中台鉴权/风险提示/二次确认
  → 下位机查询或裁决控制权
  → 中台返回并审计下位机结果
  → 持权后发送调试命令
  → 下位机再次校验控制权与安全状态
~~~

两条链只在“页面组合展示”和“下位机控制权裁决”处相遇，中台不创建跨链事务。

## 2. 控制访问设计

![[图片/SVG/14_1_2.svg|900]]

### 2.1 页面上下文

| 展示信息 | 来源 | 是否可作为控制依据 |
| --- | --- | --- |
| 当前锁定与持有者 | <code>AgvControl</code> / 显式 QueryControl | 仅下位机当前响应可作为最终依据 |
| AGV 运单号与实时进度 | Adapter 的 <code>AgvOrderState</code> 或 SeerM4State | 否，只用于风险提示 |
| 需求 ID | <code>mfms_requirement</code> | 否 |
| 订单/运单与调度状态 | <code>mfms_order</code> + <code>mfms_waybill</code> | 否 |
| AGV 位置、电量、故障 | SeerCtrlState / VirtAgvState | 设备安全仍以下位机判断为准 |
| 机械臂模式、程序、错误 | FrRobotState / AuboRobotState | 同上 |

页面可显示：

~~~text
控制权持有者：Adapter-01 / 10.0.0.25
MES 订单：MES-20260818-001
AGV 运单：AGV-ORDER-10025
设备：agvSee0003
运单状态：Executing
进度：3 / 8
控制状态质量：FRESH
~~~

### 2.2 ControlAccessService【已确认边界】

允许：

1. 校验用户和动作权限；
2. 读取展示用 ControlSnapshot；
3. 生成风险说明，要求强制申请填写原因；
4. 可在执行前显式刷新一次控制权显示；
5. 调用下位机；
6. 将下位机原始结果映射为稳定 DTO；
7. 记录审计。

禁止：

- 写锁表或把审计表当锁表；
- 生成 lease、epoch、fencing token 或 confirmation token；
- 等待调度系统暂停或 Adapter 释放；
- 修改业务状态、运单状态或 Adapter 控制状态；
- 因缓存显示“空闲”就绕过下位机校验；
- 对超时强制申请做盲目重试。

### 2.3 无人持权时

~~~mermaid
sequenceDiagram
    participant UI as 开发者客户端
    participant MFMS as 数据中台
    participant LM as 下位机
    participant RS as Redis Stream

    UI->>MFMS: 查询控制上下文
    MFMS-->>UI: ControlSnapshot(locked=false)
    UI->>MFMS: 申请控制权
    MFMS->>LM: AcquireControl(device_id, requester, request_id)
    activate LM
    LM->>LM: 判断并写真实控制权
    LM-->>MFMS: success / error
    deactivate LM
    LM->>RS: 新 AgvControl
    MFMS-->>UI: 下位机结果
~~~

图中没有“中台先写已锁定”。Stream 更新可以晚于同步结果；页面必须能表示这段短暂不一致。

### 2.4 已有其他持有者时

~~~mermaid
sequenceDiagram
    participant UI as 开发者客户端
    participant MFMS as 数据中台
    participant LM as 下位机
    participant RS as Redis Stream

    UI->>MFMS: 打开调试页
    MFMS-->>UI: 控制者 + 订单 + 运单 + 风险提示
    UI->>UI: 二次确认并填写原因
    UI->>MFMS: 强制申请
    MFMS->>LM: ForceAcquireControl(device_id, requester, reason, request_id)
    activate LM
    LM->>LM: 内部裁决旧持有者和新持有者
    LM-->>MFMS: success / rejected / device_error
    deactivate LM
    LM->>RS: AgvControl 与后续设备状态
    MFMS-->>UI: 返回结果并写审计
~~~

中台不推断强制申请对当前运单的后果。后续变化从 Stream 和调度订单/运单表观察。

### 2.5 调试命令

~~~mermaid
sequenceDiagram
    participant UI
    participant MFMS as 数据中台
    participant LM as 下位机

    UI->>MFMS: jog / navigate / IO / robot command
    MFMS->>MFMS: 鉴权、参数校验、审计准备
    MFMS->>LM: 发送命令(request_id, device_id)
    activate LM
    LM->>LM: 校验控制权与设备安全状态
    alt 允许执行
        LM-->>MFMS: OK
    else 当前调用者无控制权
        LM-->>MFMS: CONTROL_NOT_OWNED
    else 当前设备拒绝
        LM-->>MFMS: DEVICE_REJECTED
    end
    deactivate LM
    MFMS-->>UI: 唯一终态
~~~

中台可以提前禁用按钮，但安全闭环在下位机。

### 2.6 下位机最小控制契约【待联合冻结】

~~~text
QueryControl(device_id)
AcquireControl(device_id, requester_identity, request_id)
ForceAcquireControl(device_id, requester_identity, reason, request_id)
ReleaseControl(device_id, requester_identity, request_id)
~~~

建议统一响应：

~~~text
request_id
success
error_code
error_message
locked
owner_ip
owner_port
owner_type
owner_nick_name
owner_time
owner_description
~~~

建议结果类别（名称由下位机最终定义）：

~~~text
OK
ALREADY_OWNED_BY_REQUESTER
OWNED_BY_OTHER
FORCE_ACQUIRE_REJECTED
DEVICE_OFFLINE
DEVICE_BUSY
DEVICE_FAULT
INVALID_REQUESTER
INVALID_PARAMETER
REQUEST_TIMEOUT
INTERNAL_ERROR
~~~

接口问题统一登记在 [[MFMS新中台-待确定问题清单#2.2 下位机控制接口|Q-35～Q-44]]。

## 3. 实时消息接入

### 3.1 来源注册表【已确认】

| 来源 | 消息 | 中台用途 |
| --- | --- | --- |
| 下位机 | SeerCtrlState、VirtAgvState | AGV 实时视图 |
| 下位机 | SeerM4State | M4/订单关联视图；权威范围待 Q-45 |
| 下位机 | AgvControl | ControlSnapshot |
| 下位机 | FrRobotState、AuboRobotState | 机械臂实时视图 |
| 下位机 | JointPoint、ForceTorque、ForcePayload | 关节/力传感器视图 |
| Adapter | AgvOrderState 及进度、拒绝、故障、完成消息 | LiveExecutionSnapshot |

Redis 的实例部署、分片和运维不属于中台架构。中台只要求它收到的每条记录可识别、可演进、可排序、可追源。

### 3.2 统一信封【P0 冻结项】

| 字段 | 生产者提供 | 语义 |
| --- | :---: | --- |
| <code>message_type</code> | ✓ | 选择解码器，禁止靠 payload 猜类型 |
| <code>schema_version</code> | ✓ | Payload 演进与兼容 |
| <code>factory_id</code> | ✓ | 部署/数据隔离 |
| <code>device_id</code> | ✓ | 规范设备标识 |
| <code>source_type</code> | ✓ | LOWER_MACHINE / ADAPTER 等 |
| <code>source_instance_id</code> | ✓ | 区分具体实例 |
| <code>sequence</code> | ✓ | 来源内去重、乱序检测 |
| <code>observed_at</code> | ✓ | 事实在源端发生/观测的时间 |
| <code>payload</code> | ✓ | 对应版本的业务内容 |
| <code>received_at</code> | 中台补 | 中台接收时间 |
| <code>stream_entry_id</code> | 中台补 | 传输记录定位 |

不得把 <code>received_at</code> 冒充设备观测时间，也不得跨不同 source_instance 直接比较 sequence。

### 3.3 解码与缓存

~~~text
RedisStreamIngestor
  → EnvelopeValidator
  → MessageDecoderRegistry(message_type, schema_version)
  → TypedObservation
  → StateManager（按 device_id 串行）
  → SnapshotStore
  → WebSocketPublisher
~~~

| 缓存 | 合并键 | 质量关注 |
| --- | --- | --- |
| AgvRealtimeStateCache | device_id + message_type + source | 新鲜度、位置/电量时间 |
| RobotRealtimeStateCache | device_id + source | 模式、程序、错误 |
| ControlStateCache | device_id + source | 过期即 UNKNOWN，不推断已释放 |
| LiveWaybillStateCache | waybill/execution correlation ID + source | 多源不能互相覆盖 |
| SensorStateCache | device_id + sensor_type | 频率、单位、是否支持 |

### 3.4 状态质量

每份快照保存：

~~~text
observed_at
received_at
sequence
schema_version
source_type
source_instance_id
quality
~~~

质量状态：

- <code>UNKNOWN</code>：启动后尚无可信事实，或控制状态无法确认；
- <code>FRESH</code>：在该消息类型的新鲜度窗口内；
- <code>STALE</code>：断流、超时或连接异常；
- <code>UNSUPPORTED</code>：消息版本/设备能力不受当前中台支持。

解码失败、倒退序号和未知版本必须可观测，不能静默丢弃后继续显示旧值为 FRESH。

## 4. 需求、订单与运单持久化

![[图片/SVG/14_1_3.svg|900]]

### 4.1 写者与血缘【已确认边界】

~~~text
mfms_requirement   数据中台单写
  requirement_uid
      ↓ 调度拆分
mfms_order         调度系统单写，中台只读
  order_uid + requirement_uid
      ↓ 调度拆分
mfms_waybill       调度系统单写，中台只读
  waybill_uid + order_uid + requirement_uid
~~~

表名和 ID 类型是推荐候选；“中台只写需求、调度只写订单和运单”是已确认职责。中台不得用通用 update/save 接口创建、修复、推进、取消或覆盖派生记录。

### 4.2 需求【中台拥有】

需求保存原始规范化业务意图和接入审计。推荐技术字段：

| 字段 | 作用 |
| --- | --- |
| <code>requirement_uid</code> | 内部需求 ID |
| <code>factory_id</code> | 工厂边界 |
| <code>source_system/source_requirement_id</code> | 来源与来源侧幂等身份 |
| <code>schema_version/payload_json</code> | 版本化需求正文 |
| <code>intake_status</code> | 仅接入校验/可用状态，不是执行状态 |
| <code>revision</code> | 需求定义版本 |
| <code>created_by/created_at</code> | 审计 |

幂等键、revision、取消/变更交接仍待业务确认。任何机制都不能让中台反写已派生的订单或运单。

### 4.3 订单【调度拥有，中台只读】

订单最小业务字段已经确认：

| 逻辑字段 | 作用 |
| --- | --- |
| 工作站点名称/别名 | 订单关联的业务工作站点 |
| <code>type</code> | 上料或下料；规范值候选 <code>LOAD/UNLOAD</code> |
| <code>ids[]</code> | 业务对象 ID 集合 |
| 时间戳 | 订单业务时间 |
| 数量 | 本次上下料数量 |
| 优先级 | 调度排序输入 |

推荐物理列候选：

~~~text
order_uid, requirement_uid,
workstation_alias, operation_type, item_ids,
business_timestamp, quantity, priority,
split_sequence, scheduler_instance_id,
status, created_at, updated_at, finished_at
~~~

其中 <code>item_ids</code> 用 JSON 还是子表、ID 元素含义/顺序、时间戳具体含义、数量与 IDs 的关系、优先级范围与方向、一个订单行是否只能表达一个上下料动作，均需业务/调度/DBA 冻结。

### 4.4 运单【调度拥有，中台只读】

本轮只确认调度系统生成并拥有运单表，未给出完整业务字段。推荐最小技术血缘为：

~~~text
waybill_uid, order_uid, requirement_uid,
external_waybill_id, device_id,
status, created_at, updated_at, finished_at
~~~

路线/动作 payload、设备分配时机、生命周期、重试替换、终态和 Adapter 交接均待调度/Adapter 确认。Adapter 实时消息必须携带经评审的运单关联 ID。

### 4.5 多调度实例与拆分【待评审】

调度实例怎样认领需求、保证拆分幂等、原子写入订单/运单、处理实例崩溃和重新接管，属于调度/数据库合同。中台只显示调度写入结果和更新时间，不管理 leader、lease 或 failover。

## 5. RequirementView

~~~text
RequirementView
├── requirement             ← mfms_requirement
├── orders[]                ← mfms_order
│   └── waybills[]          ← mfms_waybill
├── liveWaybillStates       ← StateManager / Adapter
└── controlDisplay          ← lower-machine ControlSnapshot
~~~

中台不自行从实时消息推导订单/运单持久终态。Stream 提供“现在发生什么”，调度表提供“业务上怎样记录”，下位机响应提供“当前是否允许控制”。

## 6. 模块边界

| 模块 | 允许依赖 | 禁止 |
| --- | --- | --- |
| endpoint | application DTO | SDK 类型、SQL、控制权判断 |
| application | domain、capability/persistence port | 锁状态机、Adapter 内部流程 |
| domain | 标准库和稳定值对象 | Qt/Redis/MySQL/SDK 类型 |
| capability | infrastructure adapter、domain snapshot | 对外协议类型 |
| persistence | DB adapter、domain record | 修改别的组件权威字段 |
| infrastructure | Qt/SDK/MySQL/Redis 客户端 | 业务决策 |

建议顶层目录与类名见 [[MFMS新中台-架构构造思路#4. 中台内部结构【推荐基线】|架构快照 §4]]。

## 7. 故障恢复

### 7.1 中台重启

~~~text
连接 MySQL
  → 加载设备注册表
  → 加载需求与只读订单/运单视图
  → 所有实时状态初始化 UNKNOWN
  → 重新消费 Redis Stream
  → 重建 SDK 连接
  → 显式查询控制权
  → 客户端重新拉全量快照
~~~

绝不根据进程重启前的缓存恢复“我仍持有控制权”。

### 7.2 Stream 中断

- 受影响状态置 STALE；
- 控制权视图置未知；
- 默认关闭调试按钮；
- 不修改真实控制权；
- 恢复后按来源/序号重建快照。

### 7.3 下位机调用超时

超时表示结果未知，不表示操作一定失败：

1. 返回“结果未知/查询确认中”；
2. 调用 QueryControl；
3. 以查询结果更新页面；
4. 下位机若支持 request_id 结果查询，再使用该幂等能力；
5. 不盲目重发 ForceAcquireControl。

### 7.4 MySQL 或调度不可用

| 故障 | 可继续 | 必须停止/降级 |
| --- | --- | --- |
| MySQL 不可用 | 实时状态查看、普通快照推送 | 新需求、历史查询；推荐禁止强制申请（无法可靠审计） |
| 调度不可用 | 实时设备状态；按下位机控制权进行调试 | 不拆分/恢复订单或运单；派生视图显示过期 |

## 8. 安全与审计

### 8.1 角色【推荐】

| 动作 | viewer | operator | developer | admin |
| --- | :---: | :---: | :---: | :---: |
| 查看状态 | ✓ | ✓ | ✓ | ✓ |
| 普通申请空闲设备 |  | ✓ | ✓ | ✓ |
| 点动与调试 |  |  | ✓ | ✓ |
| 强制申请 |  |  | ✓ | ✓ |
| 用户/模板管理 |  |  |  | ✓ |

### 8.2 强制申请审计

~~~text
request_id
device_id
operator_id
operator_role
reason
displayed_control_owner
displayed_requirement_uid
displayed_order_uid
displayed_waybill_uid
lower_machine_result_code
lower_machine_result_message
requested_at
completed_at
~~~

审计事实只表达“谁向下位机发起什么请求、下位机返回什么”，不表达中台拥有锁。

## 9. 部署与线程模型【推荐基线】

仍采用局域网小主机上的单进程模块化单体：

~~~text
mfms-platform
├── HTTP / WebSocket IO
├── Redis Stream 消费
├── StateManager 串行执行上下文
├── Cpp-Proxy-SDK executor
├── MySQL 连接池
├── WebSocket 推送
└── 审计日志
~~~

线程规则：

- Stream 回调只解码和投递，不执行数据库长事务；
- StateManager 对同一设备串行合并；
- SDK 回调先转成内部 DTO，再离开适配层；
- endpoint 只处理稳定 DTO；
- 审计写入失败对强制申请采用 fail-closed；
- 不恢复旧 Gateway 逐信号转发结构。

## 10. 实施切片

| 阶段 | 交付物 | 验收 |
| --- | --- | --- |
| P0-A 控制契约 | 四个控制调用、身份、错误码、超时查询、控制域 | 与下位机团队联合评审；Q-35～Q-44 有结论 |
| P0-B Stream 契约 | 统一信封、消息目录、schema 版本、质量/乱序规则 | 样例消息可被解码；未知版本可观测 |
| P0-C 需求/订单/运单契约 | 三类核心记录、订单字段、血缘、写者矩阵和 DDL | 中台/调度/DBA 评审；中台无订单/运单写权限 |
| P1 能力骨架 | Ingestor、DecoderRegistry、StateManager、下位机薄客户端 | 真实/录制消息生成 UNKNOWN/FRESH/STALE 快照 |
| P2 需求竖切 | 中台需求接入、订单/运单只读查询与血缘 | requirement_uid 可追到订单、运单与实时关联 ID |
| P3 调试竖切 | 查询/申请/强制申请/释放、命令、权限、审计 | 下位机拒绝能稳定映射；中台无锁状态 |
| P4 统一视图 | RequirementView + 设备/机器人/控制快照 + WS | 断流、重启、调度不可用均有明确降级 |
| P5 工厂协议 | SiteProtocolAdapter、模板和现场配置 | 不修改核心领域与能力边界即可接入 |

## 11. 与 Trellis 的衔接

| 本文 | Trellis 落位 |
| --- | --- |
| §1/§6 系统与模块边界 | <code>.trellis/ARCHMAP.md</code>、backend directory spec |
| §2 控制访问 | <code>spec/contracts/control-access-contract.md</code> |
| §3 实时消息 | <code>spec/contracts/realtime-stream-contract.md</code> |
| §4/§5 需求/订单/运单与视图 | <code>spec/contracts/order-persistence-contract.md</code> |
| legacy 双事件表 | <code>spec/contracts/db-event-contracts.md</code>，不得混入需求/订单/运单表 |
| SDK/总线适配 | <code>spec/contracts/bus-contracts.md</code> |
| §7～§9 | backend error/logging/quality/runtime-recovery specs |
| 未决项 | <code>.trellis/tasks/08-16-architecture-design/</code> |

已确认约束先进入 spec；推荐方案在任务 design 中保留评审门槛；未决接口只登记问题，禁止伪装成已冻结代码要求。

## 12. 关联

- [[MFMS新中台-架构构造思路]] —— v0.4 架构摘要
- [[MFMS新中台-待确定问题清单]] —— 未决项与关闭规则
- [[MFMS新中台-Trellis工程结构方案]] —— 项目落位与 gap
- [[MFMS新中台-资料索引与权威源]] —— 历史输入与证据
- [[MFMS新中台-架构图.drawio]] —— v0.4 图源
