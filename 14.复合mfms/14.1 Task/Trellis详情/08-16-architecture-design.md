---
generated_by: trellis-obsidian-task-detail
readonly: true
project: "mfms_Framework"
title: "MFMS 数据中台新版架构设计"
trellis_status: "planning"
cssclasses:
  - trellis-task-detail
---

# MFMS 数据中台新版架构设计

> [!info] 自动生成 · 只读详情
> 此页是 Trellis 源文档在 Obsidian 库内的展示镜像，解决库外 `file://` 链接无法打开的问题。
> 请在 Trellis 项目中修改任务；下一次同步会用权威源重新生成此页。

- 状态：规划中
- 当前阶段：Phase 1 · 规划
- 执行清单：15/52
- 优先级：P0
- 负责人：mfms-core
- 范围：architecture-docs
- 创建时间：2026-08-16
- Trellis Task：`/Users/melene/project/mfms_Framework/.trellis/tasks/08-16-architecture-design`

## 摘要

MFMS 数据中台 v0.4 架构基线：保留下位机唯一控制权和 Stream 合同；中台负责需求，调度将需求拆为订单与运单；纳入 AGV/车载路由器接入、工控机有线/无线双归属、内部交换/路由设备、多机器人和 ROS 广播 IP 的现场事实。

## PRD

来源：`/Users/melene/project/mfms_Framework/.trellis/tasks/08-16-architecture-design/prd.md`

# MFMS 数据中台新版架构设计（v0.4）

## Goal

形成可指导新仓后续实现的 MFMS 数据中台 v0.4 架构基线：保留 v0.3 的下位机唯一控制权权威和 Redis Stream 实时事实边界；将业务持久化职责更新为“数据中台负责需求，调度系统将需求拆分为订单和运单”；纳入一个工厂多台机器人、AGV/车载路由器接入、工控机有线/无线双归属、内部交换/路由设备和 ROS 广播 IP 的现场网络事实。

本任务保持 `planning`：职责边界、订单最小逻辑字段和现场网络事实已经由用户确认；当前 `192.168.192.xxx`/`192.168.193.xxx` 地址分区按 2026-08-28 用户说明记录，但 A03-T2 网段存在文字/截图冲突。物理 DDL、需求拆分规则、运单字段与生命周期、下位机接口、消息 Payload、交换机/路由器关系、接口转发、地址方案和 ROS 广播接口仍需责任团队评审。

**Vault canonical set**：`/Users/melene/Documents/C++/obsidian_notes-main/14.复合mfms/`，由 `MFMS新中台-资料索引与权威源.md` 进入。Repo 实施权威为本仓库 `.trellis/`。

## Source and status discipline

- v0.3 控制权与实时消息基线见 `research/v0.3-baseline.md`。
- 2026-08-22 业务变更见 `research/v0.4-requirement-boundary.md`，其优先级高于 v0.3 的订单持久化模型。
- 2026-08-24 现场网络事实见 `research/2026-08-24-robot-network-topology.md`；只把用户明确描述的五项标为已确认，其余网络细节继续待评审。
- 2026-08-27 两份 DOCX 的脱敏证据见 `research/2026-08-27-robot-ip-and-client-router-evidence.md`；它们核验实验室/样机的 TL-CPE1300D Client-Router、地址分区和 NAT 流程，但不自动冻结全厂地址和端口。
- 2026-08-28 当前地址说明见 `research/2026-08-28-network-address-update.md`；按用户文字记录工厂侧 `192.168.58.1`、路由器 WAN `192.168.58.xxx`、AGV/车载设备 `192.168.192.xxx`、非 AGV 设备 `192.168.193.xxx` 和 `src_controller` 角色；A03-T2 文字/截图网段冲突保持待确认。

| 状态 | 在本任务中的用法 |
| --- | --- |
| **【已确认】** | 系统职责、权威边界、订单逻辑字段、平台规范化消息语义，以及 2026-08-24 五项现场网络事实，可约束实现 |
| **【已核验样例】** | 文档和截图可以证明的实验室/样机配置；可用于设计场景和冲突分析，未评审前不是生产通用默认值 |
| **【推荐基线】** | 表名、物理列名/类型、ID 方案、内部目录和错误映射，评审前不是冻结外部接口 |
| **【待下位机/业务确认】** | 下位机签名/码表、消息字段、拆分基数、运单字段、DDL/状态/恢复，不由中台代答 |
| **【历史废弃】** | 中台控制锁/协调器、业务状态决定真实控制权、v0.3 `order_request/execution/binding` 模型、调度直连 gRPC 基线 |

## Confirmed requirements

1. 下位机是全部控制权的唯一管理者和唯一裁决者。中台与 Adapter 只能是申请者或持有者。
2. 中台不创建、持久化、续租、转移或恢复控制锁；不得引入 lease、fencing token、`control_epoch`、`confirmation_token` 或跨系统夺权协调。
3. `ControlSnapshot` 仅供展示和预警；普通/强制申请、释放和设备命令最终以下位机响应为准。
4. 下位机发布设备、机械臂、传感器和控制权实时状态；Adapter 发布 AGV 运单实时执行状态，均由 Redis Stream 输入中台。
5. 中台冻结规范化信封 `message_type/schema_version/factory_id/device_id/source_type/source_instance_id/sequence/observed_at/payload`，补充 `received_at/stream_entry_id`，并明确 `UNKNOWN/FRESH/STALE/UNSUPPORTED`；不设计 Redis 拓扑。
6. 数据中台负责接收、校验和持久化原始**需求**，不负责把需求拆成订单或运单。
7. 调度系统读取需求，并单写拆分得到的**订单表**和**运单表**；数据中台只能读取订单和运单用于展示，不得创建、修复、推进、取消或覆盖。
8. 订单表最小业务字段为：工作站点名称/别名、`type`（上料/下料）、`ids[]`、时间戳、数量、优先级。
9. Adapter 执行调度下发的运单并发布可关联到运单的实时事实；中台以只读血缘组合需求、订单、运单和实时状态。
10. 基线不要求调度系统通过 gRPC 直连数据中台；需求/订单/运单通过 MySQL 交换，实时事实通过 Stream 输入。
11. 仙工 AGV/车载网络与工控机无线端点分属不同接入路径；当前 AGV/车载网络经机器人路由器使用 `192.168.192.xxx`。
12. 工控机有一块无线网卡连接外部工厂局域网，同时有一根有线连接接入内部交换机。
13. 一台机器人由 AGV 与工控机构成，机器人内部有一台工控机和一台路由器；一个工厂内有多台机器人。
14. 机器人之间通过 ROS 广播 IP；该事实不冻结 ROS 版本、Topic/消息、端口、QoS、广播域或 MFMS 接入方式。
15. 工厂侧网关/对端按用户说明为 `192.168.58.1`；路由器 WAN 主机地址以 `192.168.58.xxx` 表示。
16. AGV/车载设备使用路由器分配的 `192.168.192.xxx`，`src_controller` 是 AGV 控制器；机械臂、摄像头等非 AGV 设备使用内部交换机分配的 `192.168.193.xxx`。
17. A03-T2 是热点/调试端点；其用户文字地址为 `192.168.193.xxx`，截图标签为 `192.168.192.xxx`，在冲突关闭前不得冻结。

## Verified sample evidence

1. 历史仙工样机通过外接 `TL-CPE1300D` 以 Client-Router 方式接入工厂 WLAN；路由器无线 WAN 与机器人内部有线 LAN 是不同网络区域。
2. 历史样机路由器 LAN 接 AGV 交换机，NAT 和端口转发由 TL-CPE1300D 承担，不由工控机的双归属网卡隐式承担。
3. 历史实验室样例使用 `192.168.58.0/24` 工厂 WLAN 和 `192.168.100.0/24` 机器人内部 LAN；当前用户说明另记录 `192.168.192.xxx`/`192.168.193.xxx` 地址分区，具体掩码、主机地址和跨区可达性仍待现场确认。
4. 源 DOCX 中的凭据已排除，禁止写入 Trellis/Git；候选端口表需逐机器人和安全评审，不能整表复制为生产配置。

## Recommended baseline requirements

1. 单进程模块化单体，按 endpoint/application/domain/capability/persistence/infrastructure 分层；SDK、Redis、MySQL 类型不得进入业务/端点 DTO。
2. 推荐逻辑表与写者：`mfms_requirement`（中台单写）、`mfms_order`（调度单写）、`mfms_waybill`（调度单写）；中台的订单/运单 Repository 在类型/API 层只读。
3. 推荐血缘：`requirement_uid -> order_uid -> waybill_uid`，每个派生行保留 `requirement_uid`，运单同时保留 `order_uid`。
4. 订单物理列候选为 `workstation_alias/operation_type/item_ids/business_timestamp/quantity/priority`；`type` 规范值候选为 `LOAD/UNLOAD`。
5. `ids[]` 的元素含义/顺序/存储方式、时间戳语义、数量与 IDs 的关系、优先级范围和拆分基数必须继续标为待评审。
6. 强制申请需要开发者权限、原因、风险上下文和审计；安全策略建议在审计库不可用时 fail closed，待安全/业务评审。
7. 同一机器人内至少分别建模 AGV/车载网络、路由器热点/调试、工控机无线、工控机有线和非 AGV 设备端点；IP 只作为端点定位信息，不替代稳定机器人/设备身份、来源身份或下位机控制权裁决。
8. 若中台后续消费端点观测，建议使用 `factory_id + robot_unit_id + device_id + endpoint_role + source_instance_id` 关联稳定身份，并把 IP 作为带时间和来源的可变属性；字段名与外部映射仍待评审。
9. 推荐工控机 Agent 从有线侧发现/访问机器人内部设备，经无线工厂 LAN 向中心注册表上报；未经显式网络评审，工控机不默认桥接、路由或 NAT 两个网络。
10. 对采用 Client-Router 的机器人，推荐以稳定机器人身份关联 `router WAN + router LAN + internal target + approved port mapping`；工厂侧不得默认直连机器人内部私网地址。

## Inputs

| Material | Location | Role |
| --- | --- | --- |
| v0.4 business change | `research/v0.4-requirement-boundary.md` | 本轮需求/订单/运单职责与订单字段来源 |
| 2026-08-24 site network facts | `research/2026-08-24-robot-network-topology.md` | AGV/车载接入路径、工控机有线/无线双归属、内部交换/路由设备、多机器人和 ROS 广播 IP 的直接用户来源 |
| 2026-08-27 robot IP / Client-Router evidence | `research/2026-08-27-robot-ip-and-client-router-evidence.md` | 两份 DOCX 的哈希、脱敏事实、TL-CPE1300D Client-Router/NAT 样例、地址区域、端口证据与冲突 |
| 2026-08-28 current address update | `research/2026-08-28-network-address-update.md` | 用户确认的工厂侧网关、路由器 WAN、AGV/车载与非 AGV 地址分区、`src_controller` 角色、A03-T2 文字/截图冲突 |
| v0.3 retained baseline | `research/v0.3-baseline.md` | 控制权、Stream 与安全边界来源摘要 |
| Vault authority index | `14.复合mfms/MFMS新中台-资料索引与权威源.md` | 人类文档来源优先级 |
| Architecture/workbench | `14.复合mfms/MFMS新中台-架构构造思路.md`、`MFMS新中台-逐层设计工作台.md` | 当前讨论与分层推进 |
| Decision register | `14.复合mfms/MFMS新中台-待确定问题清单.md` | Q 项与外部待确认问题 |
| Repo architecture | `.trellis/ARCHMAP.md`、`spec/constraints.md`、`spec/contracts/index.md` | 实施约束入口 |
| Legacy evidence | `/Users/melene/project/mfms` | 冻结只读，不复制实现 |

## Acceptance Criteria

- [x] ARCHMAP/constraints 保留下位机唯一控制权红线和 Stream 来源边界
- [x] 架构与持久化合同明确中台只写需求，调度只写订单与运单，中台对派生表只读
- [x] 订单最小逻辑字段包含工作站点名称/别名、上下料 type、ids[]、时间戳、数量和优先级
- [x] v0.3 `order_request/execution/binding/change_request` 核心模型从活跃架构中移除并标为历史
- [x] Repository、恢复和质量规范禁止中台写订单/运单
- [x] Trellis ARCHMAP、constraints、合同和任务文档记录五项现场网络事实，并区分已确认事实与待确认接口
- [x] 多机器人与 AGV/车载、热点/调试、工控机有线/无线及非 AGV 设备端点建模不把 IP 当作稳定身份或控制权证明
- [x] 两份 DOCX 已逐页核验并以哈希、页数和脱敏规则记录到 task research
- [x] 网络合同区分工厂 WLAN、AGV Client-Router WAN/LAN、内部目标和工控机独立无线/有线端点
- [x] 源文档口令未复制到 Trellis/Git，候选端口映射明确标为需要逐现场安全评审
- [x] 2026-08-28 当前地址分区、`src_controller` AGV 控制器角色和 A03-T2 热点/调试端点已记录，并保留文字/截图网段冲突
- [x] Vault canonical 文档、DrawIO/SVG 和 Trellis Task 镜像已同步到 2026-08-22 v0.4 基线
- [ ] 2026-08-24 现场网络补充在后续纳入 Vault/DrawIO 同步范围时完成镜像更新
- [ ] 业务/调度/DBA 冻结需求、订单、运单 DDL 与 ID/外键/索引/迁移
- [ ] 业务方确认 `ids[]`、时间戳、数量、优先级、上下料拆单和需求变更语义
- [ ] 调度/Adapter 冻结运单字段、生命周期、重试替换和消息关联 ID
- [ ] 下位机/Adapter 确认每类 Payload、时间/序号规则和同一运单状态的来源权威
- [ ] 下位机团队确认控制权函数签名、请求者身份、`AgvControl.time`、变化通知、重启和错误码
- [ ] 现场/下位机团队确认 Client-Router 样例适用范围、逐机器人地址/端口白名单、工控机转发/桥接策略、ROS 广播接口/安全边界及机器人稳定身份映射
- [ ] 首个代码任务完成构建工具选择、目录落地和纵向场景验证

## Out of Scope

- 设计下位机内部锁算法或 Adapter/调度内部状态机。
- 由数据中台实现需求拆分、选车、订单/运单推进、调度 DAG、重试或故障转移。
- 决定 Redis 部署、Stream 数量、消费者组、ACK/Pending、保留和高可用拓扑。
- 修改工厂无线网络、机器人内部交换机/路由器、工控机接口转发、ROS 配置、IP 地址或防火墙；本任务只记录事实和约束。
- 在缺少现场证据时冻结 ROS Topic/消息、网络端口、网段、NAT/桥接、DHCP、VLAN 或广播安全方案。
- 把源 DOCX 中的 Wi-Fi、路由器或设备明文口令复制到仓库、日志、测试或默认配置。
- 把推荐表名、物理字段、类型、枚举或未确认运单字段直接执行为生产迁移或外部接口。
- 修改 legacy 仓库、真机数据库、下位机代码或 SDK 二进制。

## Notes

- 详细技术设计见 `design.md`；执行/评审门槛见 `implement.md`。
- 本轮只更新规划与架构文档，不启动生产代码实现、不迁移数据库、不改变 Task 的 planning 状态。

## 设计

来源：`/Users/melene/project/mfms_Framework/.trellis/tasks/08-16-architecture-design/design.md`

# MFMS 数据中台架构设计（v0.4）

> 日期：2026-08-24；网络证据更新：2026-08-28
> 状态：需求/订单/运单职责、现场多机器人网络事实以及当前 `192.168.192.xxx`/`192.168.193.xxx` 地址分区已按用户说明记录；历史 TL-CPE1300D Client-Router/NAT 样例已核验；A03-T2 网段、物理 DDL、拆分规则、运单合同、外部签名、Payload、生产地址/端口策略与 ROS 广播接口仍待评审。

## 1. Context and authority

```text
MES / Qt / Web
      │ requirement / control / view
      ▼
MFMS data platform
  ├─ writes Requirement ───────────────> MySQL
  ├─ reads derived Order + Waybill <──── Scheduler writes
  ├─ SDK control/debug request ─────────> Lower machine
  └─ Redis Stream ingest <──── Lower machine + Adapter

Scheduler reads Requirement
  └─ splits -> Order -> Waybill -> Adapter -> ADK/SDK -> Lower machine -> devices
```

【已确认】权威分工：

| 权威 | 组件 |
| --- | --- |
| 原始业务需求 | 数据中台 |
| 需求拆分、订单与运单持久化 | 调度系统 |
| AGV 运单执行与实时事实 | Adapter |
| 物理设备执行 | 下位机 |
| 控制权管理与裁决 | **下位机** |
| 需求/订单/运单组合展示与开发调试入口 | 数据中台 |

需求、订单、运单或持久摘要都不构成控制权。下位机可以拒绝无权或不安全命令；中台不得覆盖该结果。

### 1.1 Physical deployment and addressing

```text
Factory external WLAN / gateway (user description: 192.168.58.1)
└─ Robot router WAN: 192.168.58.xxx
   └─ Robot internal network
      ├─ A03-T2 hotspot/debug endpoint
      │     text: 192.168.193.xxx
      │     screenshot: 192.168.192.xxx (conflict, pending)
      ├─ AGV/vehicle devices: 192.168.192.xxx
      │     └─ src_controller (AGV controller)
      └─ Internal switch assigned: 192.168.193.xxx
            ├─ robot arm
            ├─ camera
            └─ other non-AGV devices

Each robot also keeps separate industrial-PC factory-WLAN and wired internal-switch endpoints.

Robot peers: broadcast IP through ROS
```

【已确认】一个工厂有多台机器人；每台机器人由仙工 AGV 与工控机构成，内部有一台路由器；AGV/车载网络与工控机无线端点分属不同接入路径；工控机有线接内部交换机、无线接外部工厂 LAN；机器人之间通过 ROS 广播 IP。

【2026-08-28 当前地址说明】工厂侧网关/对端为 `192.168.58.1`；路由器 WAN 主机地址以 `192.168.58.xxx` 表示；AGV/车载设备由路由器分配 `192.168.192.xxx`，`src_controller` 是 AGV 控制器；机械臂、摄像头等非 AGV 设备由内部交换机分配 `192.168.193.xxx`。A03-T2 是热点/调试端点，用户文字和截图的网段标签不一致，不能在冲突关闭前写死。

【历史样机证据】2026-08-27 两份 DOCX 显示，仙工侧“外接无线网卡”由 TL-CPE1300D 这类外接无线 Client-Router 实现。它的 WAN 通过工厂 WLAN 获得 `192.168.58.x`，LAN 接 AGV 交换机并服务历史样例 `192.168.100.0/24` 内部网，NAT/端口映射把经批准的内部服务暴露到 WAN。该 NAT 与工控机的独立 WLAN/有线双归属无关，不能据此令工控机承担透明网关。

【推荐基线】设计上把稳定身份与网络定位分离：一台机器人至少记录 AGV Client-Router WAN/LAN、工控机无线、工控机有线和内部设备端点。IP 变化或冲突不能创建新的设备身份、串并其他机器人状态或授予控制权。工厂侧访问内部设备时解析“稳定身份 → 当前 WAN → 经批准的 NAT 映射”，不能假设内部 IP 可直达。工控机 Agent 可从有线侧发现/访问机器人内部设备，经无线工厂 LAN 向中心注册表上报；它不默认充当网桥或路由器。ROS 广播只是一项现场寻址事实，不自动成为 MFMS 的实时状态或命令通道。详细合同见 `../../spec/contracts/deployment-network-contract.md`。

```text
Robot internal devices ── wired switch ── IPC Discovery Agent
                                             │ normalized registration/heartbeat
                                             ▼
                                      IPC factory-WLAN
                                             │
                                             ▼
                                  Central Device Registry
```

## 2. MFMS module design（【推荐基线】）

```text
endpoint
  HTTP / WebSocket / site protocol adapters
application
  RequirementIntake / RequirementQuery / RealtimeView / ControlAccess / DebugCommand
domain
  Requirement / DerivedOrder / Waybill / RequirementView /
  DeviceSnapshot / ControlSnapshot / RobotEndpointObservation / source-quality types
capability
  StateManager / LowerMachineControlClient / LowerMachineDebugClient /
  RedisStreamIngestor / MessageDecoderRegistry
persistence
  RequirementRepository / read-only OrderRepository / read-only WaybillRepository /
  AuditRepository / legacy event repositories
infrastructure
  CppProxySdkAdapter / MySQL / Redis Stream / WebSocket / Auth / StructuredLogger
```

端点不碰 SQL/SDK；SDK/Redis/MySQL structs 不进入 application/domain；Stream callback 只解码/投递；StateManager 按设备和来源串行更新。MFMS 不得出现 `OrderWriter`、`WaybillWriter` 或拆单服务。

## 3. Control access

v0.3 控制权设计不因本轮业务变更而改变：MFMS 做权限、风险提示、调用、结果映射和审计；下位机做唯一裁决。缓存不是授权，调用超时可能是结果未知，禁止自动重发强制申请。

## 4. Realtime state

- 下位机发布 AGV、机械臂、传感器和 `AgvControl` 等状态。
- Adapter 发布运单的运行、进度、拒绝、故障和完成实时事实，并携带经评审的 `waybill_uid` 或外部运单关联 ID。
- 中台保留 `message_type/schema_version/factory_id/device_id/source_type/source_instance_id/sequence/observed_at/payload`，补充 `received_at/stream_entry_id`。
- `AgvOrderState` 与 `SeerM4State.order_state` 权威关系未确认前按来源隔离，不按到达时间静默覆盖。
- ROS IP 广播不替代上述实时消息合同。若未来接入，端点观测必须按工厂、机器人、设备、端点角色和来源隔离，并具有独立新鲜度。

## 5. Requirement, order, and waybill persistence

### 5.1 Confirmed responsibility

```text
MFMS:      receive + validate + persist Requirement
Scheduler: read Requirement + split + persist Order + persist Waybill
Adapter:   execute Waybill + publish live facts
MFMS:      read Order/Waybill + compose RequirementView
```

MFMS 不拆单、不选车、不推进或修复订单/运单状态，也不实现通用 DAG/补偿/重试。

### 5.2 Recommended table and lineage model

```text
mfms_requirement   MFMS writes
  requirement_uid
      │ 1..N (cardinality pending)
      ▼
mfms_order         Scheduler writes; MFMS reads
  order_uid + requirement_uid
      │ 0..N (cardinality pending)
      ▼
mfms_waybill       Scheduler writes; MFMS reads
  waybill_uid + order_uid + requirement_uid
```

表名与 ID 类型是推荐方案，不是已部署 schema。旧 `mfms_order_request`、`mfms_order_execution`、`mfms_order_execution_binding`、`mfms_order_change_request` 不再是活跃核心模型。

### 5.3 Confirmed order business fields

| 业务字段 | 语义 |
| --- | --- |
| 工作站点名称/别名 | 订单关联的业务工作站点 |
| `type` | 上料或下料；规范值候选 `LOAD/UNLOAD` |
| `ids[]` | 业务对象 ID 集合；元素类型和顺序待确认 |
| 时间戳 | 订单业务时间；具体是请求/计划/创建时间待确认 |
| 数量 | 本次上下料数量；与 `ids[]` 的关系待确认 |
| 优先级 | 调度排序输入；范围、方向和并列规则待确认 |

推荐物理列候选：`workstation_alias`、`operation_type`、`item_ids`、`business_timestamp`、`quantity`、`priority`。一个订单行是否只能表达一个 `LOAD` 或 `UNLOAD` 动作由业务/调度评审冻结。

### 5.4 Waybill boundary

本轮只确认“调度系统创建并拥有运单表”，没有提供完整运单字段。推荐最小技术血缘为 `waybill_uid/order_uid/requirement_uid`；外部运单 ID、设备、路线、状态、时间、重试和替换语义仍待调度/Adapter 确认，不能作为已冻结 DDL。

### 5.5 Concurrency and changes

调度多实例怎样认领需求、原子落地拆分、故障转移和重复拆分去重，归调度/数据库合同。MFMS 不管理调度 leader、lease 或 failover。需求 revision/cancel/change 只能通过经评审的异步交接边界处理，不能让中台修改派生订单/运单。

## 6. User view

```text
RequirementView
├── requirement             MFMS MySQL
├── orders[]                Scheduler MySQL
│   └── waybills[]          Scheduler MySQL
├── liveWaybillStates       source-aware StateManager
└── controlDisplay          lower-machine ControlSnapshot
```

UI 必须标出更新时间、来源和质量；不得将控制权变化推导为运单终态，也不得将实时静默推导为持久失败/完成。

## 7. Runtime and recovery

1. 启动连接 MySQL、读取需求与只读订单/运单视图，但所有实时、控制和端点快照先为 `UNKNOWN`。
2. 重新消费 Stream、启动 SDK 并显式查询必要控制状态；若后续接入 ROS 端点广播，也必须重新观测当前地址；客户端重连获取全量快照。
3. Stream 中断使状态 `STALE`，不修改真实控制权或持久订单/运单。
4. MySQL 不可用时停止新需求受理；实时查看可继续。强制申请审计 fail-closed 是推荐安全基线。
5. 调度不可用时只显示订单/运单数据过期，中台不拆分、不接管、不恢复。
6. shutdown 停止新写需求、受限排空、关闭 SDK/Stream/DB，不持久化实时控制快照作为恢复依据。

## 8. Decision convergence

| ID | v0.4 state |
| --- | --- |
| SITE-NET-2026-08-24 | AGV/车载接入路径、工控机有线/无线双归属、内部交换/路由设备、单工厂多机器人和 ROS 广播 IP 为已确认现场事实；接口与安全细节待确认 |
| SITE-NET-2026-08-28 | 工厂侧 `192.168.58.1`、路由器 WAN `192.168.58.xxx`、AGV/车载 `192.168.192.xxx`、非 AGV 设备 `192.168.193.xxx`、`src_controller` 角色已记录；A03-T2 文字/截图网段冲突待确认 |
| SITE-NET-DOCX-2026-08-27 | TL-CPE1300D Client-Router/NAT、工厂 WLAN 与机器人内部 LAN 分层、样例地址和候选端口表已脱敏核验；生产适用范围与端口安全策略待评审 |
| Q-31 | 消息信封与语义合同保留；Redis 拓扑移出范围 |
| Q-32 | 职责收敛为中台需求、调度订单/运单；订单逻辑字段已确认，DDL/拆分/运单/状态待评审 |
| Q-33 | 中台范围关闭；下位机唯一控制权权威 |
| Q-34 | Adapter 内部契约移出；只要求规范运单实时消息与关联 ID |
| Q-02-rev | 无 scheduler→MFMS gRPC 基线；MySQL 交换需求/订单/运单，Stream 传实时事实 |

## 9. Pending confirmations

- 业务/调度/DBA：需求、订单、运单 DDL；ID/外键/索引；`ids[]`、时间戳、数量、优先级；上下料拆分；revision/change/cancel；多实例 claim/failover。
- 调度/Adapter：运单 payload、生命周期、设备/路线、重试替换、消息关联 ID 和持久终态。
- 消息生产者：Payload 映射、单位、时间编码、序号 reset/wrap、source instance 生命周期、新鲜度阈值和多源权威。
- 下位机：真实控制接口、请求者身份、`AgvControl.time`、错误码、变化通知、重启、控制域和 requestId 查询。
- 现场/下位机：TL-CPE1300D Client-Router 样例适用范围；逐机器人 WAN/LAN 地址、DHCP/保留租约和最小端口白名单；文档 `502/503` 冲突；工控机接口和转发策略；ROS 版本、Topic/消息、广播域、周期、超时与安全；机器人稳定身份映射；MFMS 是否消费该广播。

## 10. Historical exclusions

【历史废弃】：中台控制权状态机、lease/fencing/control epoch/confirmation token、跨系统夺权协调、业务状态决定真实控制权、调度 gRPC 直连基线、由中台设计 Redis 拓扑，以及 v0.3 “中台 order_request + 调度 execution/binding”持久化模型。

## 实施计划

来源：`/Users/melene/project/mfms_Framework/.trellis/tasks/08-16-architecture-design/implement.md`

# MFMS v0.4 architecture implementation/readiness plan

> This remains a planning task. The 2026-08-22 business change, 2026-08-24 site-network facts, sanitized 2026-08-27 historical Client-Router/IP evidence, and 2026-08-28 user-confirmed address split are synchronized into Trellis architecture artifacts, but production implementation waits for the review gates below.

## 0. P0 contract review child tasks

- [ ] `08-23-p0-0-evidence-baseline` — confirm the current SDK/message/service evidence baseline.
- [ ] `08-23-p0-a-lower-machine-control-contract` — close Q-35～Q-44 through lower-machine review.
- [ ] `08-23-p0-b-redis-stream-contract` — close payload/time/sequence/source-authority questions through producer review.
- [ ] `08-23-p0-c-requirement-order-waybill-contract` — close DDL, lineage, split, lifecycle and ownership questions through business/Scheduler/DBA/Adapter review.

The children may produce candidate review packets in parallel. P0-A/P0-B cannot freeze external facts before P0-0 is signed. An Orca `worker_done` event means “candidate result available”, not Trellis completion. Only a human-approved, integrated result may be archived and counted as complete.

## 1. Converge the v0.4 platform boundary

- [x] Retain the lower machine as the sole control-rights manager/arbiter.
- [x] Record MFMS as the sole writer of requirements, not orders or waybills.
- [x] Record Scheduler as the sole writer that splits requirements into orders and waybills.
- [x] Freeze the minimum logical order field set: workstation alias, load/unload type, ids[], timestamp, quantity, priority.
- [x] Retire the active v0.3 order_request/execution/binding/change-request model.
- [x] Record the confirmed site topology: AGV/vehicle network through the robot router, industrial-PC WLAN + wired switch link, one router per robot, multiple robots per factory, and ROS IP broadcast between robots.
- [x] Separate mutable IP endpoints from stable robot/device identity and lower-machine control authority.

## 2. Synchronize planning and architecture artifacts

- [x] Update ARCHMAP, constraints, contract index, persistence contract, and affected backend guidelines.
- [x] Update this existing architecture-design PRD, design, implementation plan, context, and task metadata.
- [x] Update Vault canonical notes, decision register, system diagram, DrawIO source, and SVG assets.
- [x] Regenerate the read-only Obsidian Trellis Task mirror.
- [x] Add the 2026-08-24 user-source record and deployment-network contract to `.trellis` and task context.
- [x] Render and inspect both 2026-08 DOCX files; record hashes, sample address zones, Client-Router/NAT behavior, candidate ports, conflicts, and credential-exclusion rules.
- [ ] Propagate the 2026-08-24 network facts to Vault/DrawIO when that external documentation is explicitly in scope.

## 3. Business/database review gate

- [ ] Freeze requirement/order/waybill DDL, IDs, foreign keys, indexes, repository permissions, migrations, and retention.
- [ ] Define order ids[] element/order/storage semantics.
- [ ] Define timestamp meaning/encoding, quantity validation/unit, priority range/order/default, and LOAD/UNLOAD row rules.
- [ ] Define requirement-to-order and order-to-waybill cardinality, split sequencing, idempotency, revision/change/cancel, claim, and failover.
- [ ] Freeze waybill payload, lifecycle, assignment, terminal result, retry/replacement, and Adapter handoff.

Rollback point: retain the confirmed ownership boundary and logical field set; replace only unapproved proposed names/types with reviewed DDL. Never let MFMS write Scheduler order/waybill rows.

## 4. Lower-machine and producer review gate

- [ ] Review exact control operation signatures, requester identity, result codes, timeout/result lookup, restart, and control-domain behavior.
- [ ] Review `AgvControl.time` and control-change publication guarantees.
- [ ] Freeze payload field/unit maps, timestamps, sequence/reset/wrap, source-instance identity, and freshness thresholds for each message family.
- [ ] Freeze Adapter waybill correlation ID and resolve Adapter state vs `SeerM4State.order_state` authority by field/lifecycle.

## 5. First vertical implementation gate

- [ ] Select/build C++ tooling and establish the recommended layer skeleton without speculative order/waybill writers.
- [ ] Implement MFMS-owned requirement intake against reviewed migrations.
- [ ] Implement read-only order/waybill query and RequirementView lineage against reviewed Scheduler schema.
- [ ] Implement one live waybill slice with exact envelope decoding, source attribution, quality, correlation, and restart-to-`UNKNOWN` tests.
- [ ] Implement one lower-machine control query/request slice with rejection vs ambiguous timeout tests and no local ownership state.
- [ ] If endpoint discovery is implemented, test multiple robots with separate AGV/vehicle, router-hotspot/debug, IPC-WLAN, IPC-wired, and non-AGV-device endpoints, interface loss, IP change/reuse/conflict, stale observations, and restart-to-`UNKNOWN`; receiving a ROS broadcast must not authorize control or replace Stream state.
- [ ] If Client-Router routing is implemented, test repeated internal subnets behind distinct robot WAN endpoints, DHCP WAN changes, approved port resolution, duplicate-port rejection, unmapped-service denial, and secret-free configuration/logging.
- [ ] Run format/lint/build/unit/contract/integration/fault tests and update specs from verified behavior.

## 5.1 Site-network review gate

- [x] Confirm the documented lab sample: TL-CPE1300D is separate from the AGV switch, its LAN connects to the switch, and it runs Client-Router with NAT/port forwarding.
- [x] Record the current user-described address roles: factory-side `192.168.58.1`, router WAN `192.168.58.xxx`, AGV/vehicle `192.168.192.xxx`, non-AGV devices `192.168.193.xxx`, and `src_controller` as the AGV controller.
- [ ] Resolve the A03-T2 hotspot/debug address conflict (`192.168.193.xxx` in text versus `192.168.192.xxx` in the screenshot) before freezing the endpoint or enabling automatic debug routing.
- [ ] Confirm which robot models/factories use this profile and document alternatives; do not apply the lab IP/port sample globally.
- [ ] Confirm AGV/industrial-PC wired and wireless addressing, interface binding, route priority, address family, lease/change rules, reachability and isolation.
- [ ] Confirm whether the industrial PC may route, bridge or NAT between wired and wireless networks; default to disabled until reviewed.
- [ ] Freeze a per-robot minimal NAT allowlist, resolve the documented `502/503` mapping conflict, and review Redis/Debug/RDP/HTTP management exposure with security owners.
- [ ] Define how the registry binds stable robot identity to current Client-Router WAN, router LAN, internal targets and approved mappings when DHCP addresses change.
- [ ] Confirm ROS version, sender/receiver, Topic/message, broadcast domain, period, timeout, namespace and authenticity/replay handling.
- [ ] Confirm the authoritative robot-unit identity and its mapping to AGV `device_id`, industrial-PC identity and endpoint roles.
- [ ] Decide whether MFMS consumes ROS IP broadcasts; if yes, freeze the adapter boundary and normalized observation contract before implementation.
- [ ] Decide whether an industrial-PC Discovery Agent owns local wired discovery and central registration over factory WLAN.

## 6. Architecture completion gate

- [ ] All pending confirmations are linked to signed/reviewed artifacts.
- [ ] Vault diagrams and current architecture notes match the reviewed external contracts and physical schema.
- [ ] Vault diagrams and current architecture notes include the reviewed multi-robot physical network topology and do not invent unconfirmed routing/ROS details.
- [ ] No active document presents recommended table/column names, waybill fields, or external codes as confirmed facts.
- [ ] Task can move from planning only after user/team review; do not archive or start implementation automatically.

## Task JSON

来源：`/Users/melene/project/mfms_Framework/.trellis/tasks/08-16-architecture-design/task.json`

```json
{
  "id": "architecture-design",
  "name": "architecture-design",
  "title": "MFMS 数据中台新版架构设计",
  "description": "MFMS 数据中台 v0.4 架构基线：保留下位机唯一控制权和 Stream 合同；中台负责需求，调度将需求拆为订单与运单；纳入 AGV/车载路由器接入、工控机有线/无线双归属、内部交换/路由设备、多机器人和 ROS 广播 IP 的现场事实。",
  "status": "planning",
  "dev_type": null,
  "scope": "architecture-docs",
  "package": null,
  "priority": "P0",
  "creator": "mfms-core",
  "assignee": "mfms-core",
  "createdAt": "2026-08-16",
  "completedAt": null,
  "branch": null,
  "base_branch": "main",
  "worktree_path": null,
  "commit": null,
  "pr_url": null,
  "subtasks": [],
  "children": [
    "08-23-p0-0-evidence-baseline",
    "08-23-p0-a-lower-machine-control-contract",
    "08-23-p0-b-redis-stream-contract",
    "08-23-p0-c-requirement-order-waybill-contract"
  ],
  "parent": null,
  "relatedFiles": [
    ".trellis/ARCHMAP.md",
    ".trellis/spec/constraints.md",
    ".trellis/spec/contracts/index.md",
    ".trellis/spec/contracts/deployment-network-contract.md",
    ".trellis/spec/contracts/control-access-contract.md",
    ".trellis/spec/contracts/realtime-stream-contract.md",
    ".trellis/spec/contracts/order-persistence-contract.md",
    ".trellis/spec/contracts/bus-contracts.md",
    ".trellis/spec/contracts/db-event-contracts.md",
    ".trellis/spec/backend/directory-structure.md",
    ".trellis/spec/backend/quality-guidelines.md",
    ".trellis/spec/backend/runtime-recovery.md",
    ".trellis/spec/backend/index.md",
    ".trellis/tasks/08-16-architecture-design/research/gpt-pro-submodule-planning-handoff.md",
    ".trellis/tasks/08-16-architecture-design/research/2026-08-24-robot-network-topology.md",
    ".trellis/tasks/08-16-architecture-design/research/2026-08-27-robot-ip-and-client-router-evidence.md",
    ".trellis/tasks/08-16-architecture-design/research/2026-08-28-network-address-update.md"
  ],
  "notes": "v0.4 boundary is current: MFMS owns requirements; Scheduler owns derived orders and waybills; MFMS reads derived tables only. Confirmed site facts: AGV/vehicle network and industrial-PC factory-WLAN path are distinct; the industrial PC also has a wired internal-network link; each robot contains the AGV, one industrial PC and one router; a factory has multiple robots; robot peers broadcast IP through ROS. The 2026-08-28 user update records factory-side 192.168.58.1, router WAN 192.168.58.xxx, AGV/vehicle devices on 192.168.192.xxx, non-AGV devices on 192.168.193.xxx, and src_controller as the AGV controller. A03-T2 is the router hotspot/debug endpoint, but its text address (192.168.193.xxx) conflicts with the screenshot label (192.168.192.xxx), so the endpoint CIDR remains pending. Sanitized 2026-08-27 DOCX evidence remains historical TL-CPE1300D Client-Router/NAT evidence; sample IPs/ports are not production defaults and credentials are excluded. Task remains planning until profile applicability, A03-T2 conflict, per-robot address/port policy, network/ROS details, lower-machine signatures/codes, and requirement/order/waybill contracts are reviewed. Vault canonical root: 14.复合mfms/MFMS新中台-资料索引与权威源.md",
  "meta": {
    "architecture_baseline": "v0.4",
    "baseline_date": "2026-08-24",
    "change_source": "Direct user-confirmed requirement/order/waybill boundary on 2026-08-22; robot network topology on 2026-08-24; current address split and A03-T2 discrepancy on 2026-08-28",
    "network_facts_status": "confirmed_physical_topology_interface_details_pending",
    "ipc_network_role": "wired_to_switch_and_wireless_to_factory_lan",
    "network_evidence_update_date": "2026-08-28",
    "current_address_update_date": "2026-08-28",
    "factory_gateway_or_peer": "192.168.58.1",
    "router_wan_pattern": "192.168.58.xxx",
    "agv_vehicle_network_pattern": "192.168.192.xxx",
    "non_agv_device_network_pattern": "192.168.193.xxx",
    "agv_controller_role": "src_controller",
    "router_hotspot_debug_endpoint": "A03-T2",
    "a03_t2_address_status": "text_192.168.193.xxx_vs_screenshot_192.168.192.xxx_pending",
    "agv_wireless_profile": "tl-cpe1300d_client-router_wan_wifi_lan_switch_nat_sample_verified",
    "network_source_sha256": [
      "f2b8550140c0873811300d006df855b9ad278f91678b2ebd4f3a22677d5a6a14",
      "1dcb4ac2bf8eef68a43ed1a5be460dd50c8fe3a38ffe62e631a3e2ee15815dc5"
    ],
    "credential_handling": "plaintext credentials excluded from trellis and git",
    "superseded_v0_3_source_sha256": "8a5bf99998cca03837f2c7e32d438b02a382d5744d7819111a6121170e784705",
    "orca_run_id": "run_709bea11811c"
  }
}
```
