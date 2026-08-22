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
- 执行清单：9/25
- 优先级：P0
- 负责人：mfms-core
- 范围：architecture-docs
- 创建时间：2026-08-16
- Trellis Task：`/Users/melene/project/mfms_Framework/.trellis/tasks/08-16-architecture-design`

## 摘要

MFMS 数据中台 v0.3 架构基线：下位机统一管理控制权，Stream 消息合同与按职责分表/字段单写的订单持久化边界收敛；外部接口和业务 DDL 继续评审。

## PRD

来源：`/Users/melene/project/mfms_Framework/.trellis/tasks/08-16-architecture-design/prd.md`

# MFMS 数据中台新版架构设计（v0.3）

## Goal

形成可指导新仓后续实现的 MFMS 数据中台 v0.3 架构基线：中台是独立 C++ 服务；下位机统一管理和裁决控制权；下位机与 Adapter 通过 Redis Stream 提供实时事实；中台冻结消息语义而不设计 Redis 拓扑；订单持久化采用按职责分表、字段单写作为推荐基线。

本任务保持 `planning`：职责边界和中台内部合同已经收敛，但下位机真实接口/错误码、消息 Payload 映射、订单 DDL/业务状态聚合和 legacy 双事件表迁移仍需责任团队确认。

**Vault canonical set**：`/Users/melene/Documents/C++/obsidian_notes-main/14.复合mfms/`，由 `MFMS新中台-资料索引与权威源.md` 进入。Repo 实施权威为本仓库 `.trellis/`。

## Source and status discipline

本轮 v0.3 源摘要见 `research/v0.3-baseline.md`，附件 SHA-256 为 `8a5bf99998cca03837f2c7e32d438b02a382d5744d7819111a6121170e784705`。

| 状态 | 在本任务中的用法 |
| --- | --- |
| **【已确认】** | 系统职责、权威边界、平台规范化消息语义，可约束实现 |
| **【推荐基线】** | 订单表/字段/枚举、内部目录和错误映射，评审前不是冻结外部接口 |
| **【待下位机/业务确认】** | 下位机签名/码表、消息字段、业务 DDL/聚合/恢复，不由中台代答 |
| **【历史废弃】** | 中台控制权状态机/lease/epoch/协调器、业务状态决定真实控制权、调度直连 gRPC 基线 |

## Confirmed requirements

1. 下位机是全部控制权的唯一管理者和唯一裁决者。中台与 Adapter 只能是申请者或持有者。
2. 中台不创建、持久化、续租、转移或恢复控制锁；不得引入 lease、fencing token、`control_epoch`、`confirmation_token` 或跨系统夺权协调。
3. `ControlSnapshot` 仅供展示和预警；普通/强制申请、释放和设备命令最终以下位机响应为准。二次确认只做权限/风险/原因/审计。
4. 下位机发布设备、机械臂、传感器和控制权实时状态；Adapter 发布 AGV 运单实时执行状态，均由 Redis Stream 输入中台。
5. 中台冻结规范化信封 `message_type/schema_version/factory_id/device_id/source_type/source_instance_id/sequence/observed_at/payload`，补充 `received_at/stream_entry_id`，并明确 `UNKNOWN/FRESH/STALE/UNSUPPORTED`；不设计 Redis 拓扑。
6. 多来源状态在权威关系未确认时不得无条件覆盖；保留来源、实例、执行 ID、序号和时间。
7. 状态查询走 StateManager；中台重启后全部实时/控制缓存从 `UNKNOWN` 重建。
8. v0.3 基线不要求调度系统通过 gRPC 直连数据中台；订单通过 MySQL 交换，实时事实通过 Stream 输入。
9. Adapter 内部状态机、ADK 编排和崩溃恢复移出数据中台架构范围；中台只约束其输出消息和关联 ID。

## Recommended baseline requirements

1. 单进程模块化单体，按 endpoint/application/domain/capability/persistence/infrastructure 分层；SDK、Redis、MySQL 类型不得进入业务/端点 DTO。
2. 订单按职责四表、字段单写边界：
   - `mfms_order_request`：中台单写；
   - `mfms_order_execution`、`mfms_order_execution_binding`：调度单写，中台只读；
   - `mfms_order_change_request`：中台创建请求字段、调度处理结果字段，列所有权必须隔离。
3. 中台保存和校验订单定义；调度系统解释、拆分、选车、执行聚合；Adapter 执行 AGV 运单。中台不实现通用执行 DAG。
4. 强制申请需要开发者权限、原因、风险上下文和审计；安全策略建议在审计库不可用时 fail closed，待安全/业务评审。
5. 订单推荐表名、字段、唯一约束、状态枚举和调度 claim 方式必须继续标为推荐，直到 DDL/业务评审冻结。

## Inputs

| Material | Location | Role |
| --- | --- | --- |
| v0.3 source | `research/v0.3-baseline.md`（原附件哈希已记录） | 本轮职责与方案来源摘要 |
| Vault authority index | `14.复合mfms/MFMS新中台-资料索引与权威源.md` | 人类文档来源优先级 |
| Architecture/workbench | `14.复合mfms/MFMS新中台-架构构造思路.md`、`MFMS新中台-逐层设计工作台.md` | 当前讨论与分层推进 |
| Decision register | `14.复合mfms/MFMS新中台-待确定问题清单.md` | Q 项与外部待确认问题 |
| Repo architecture | `.trellis/ARCHMAP.md`、`spec/constraints.md`、`spec/contracts/index.md` | 实施约束入口 |
| Legacy evidence | `/Users/melene/project/mfms` | 冻结只读，不复制实现 |
| SDK evidence | `/Users/melene/Downloads/HyRMS_export_0810` | 下位机 adapter gap；`.so` 哈希 `a1b990...10c` |

## Acceptance Criteria

- [x] ARCHMAP/constraints 将下位机唯一控制权权威写成最高红线，并删除中台协调锁的实施含义
- [x] Q-31 收敛为消息合同、Q-33 在中台范围关闭、Q-34 Adapter 内部移出范围、Q-02-rev 无 gRPC 直连基线
- [x] realtime contract 冻结来源/序号/时间/质量语义且明确 Redis 拓扑不在范围
- [x] order contract 将核心表单写、变更请求字段拆分写成推荐基线，而非已部署/冻结 schema
- [x] control contract 明确缓存非授权、超时结果未知和下位机最终响应
- [x] backend 目录、数据库、错误、日志、质量和运行恢复规范补全并互相链接
- [ ] 下位机团队确认控制权函数签名、请求者身份、`AgvControl.time`、变化通知、重启和错误码
- [ ] 下位机/Adapter 确认每类 Payload、时间/序号规则和同一执行状态的来源权威
- [ ] 业务/调度团队评审订单 DDL、状态聚合、变更请求、claim 与故障恢复
- [ ] `device_state` / `lua_state` 拆分通过上下位机双端评审
- [ ] 用 2026-08-10 SDK 权威包完成可执行 gap 分析并冻结 adapter 接口
- [ ] 首个代码任务完成构建工具选择、目录落地和纵向场景验证

## Out of Scope

- 设计下位机内部锁算法或 Adapter/调度内部状态机。
- 决定 Redis 部署、Stream 数量、消费者组、ACK/Pending、保留和高可用拓扑。
- 把推荐订单字段/错误码直接执行为生产迁移或外部接口。
- 修改 legacy 仓库、真机数据库、下位机代码或 SDK 二进制。

## Notes

- 详细技术设计见 `design.md`；执行/评审门槛见 `implement.md`。
- 层卡片跟随首个代码任务建立，不提前创建空规范。

## 设计

来源：`/Users/melene/project/mfms_Framework/.trellis/tasks/08-16-architecture-design/design.md`

# MFMS 数据中台架构设计（v0.3）

> 日期：2026-08-18
> 状态：职责和平台合同基线已收敛；外部签名、Payload 与业务 DDL 仍待评审。任何示例字段/码/状态均服从文中的【已确认 / 推荐基线 / 待确认 / 历史废弃】标记。

## 1. Context and authority

```text
MES / Qt / Web
      │ order/control/view
      ▼
MFMS data platform
  ├─ MySQL order request ──────────────> Scheduler
  ├─ read scheduler execution/binding <─ Scheduler
  ├─ SDK control/debug request ─────────> Lower machine
  └─ Redis Stream ingest <──── Lower machine + Adapter

Scheduler ── work orders ──> Adapter ── ADK/SDK ──> Lower machine ──> devices
```

【已确认】权威分工：

| 权威 | 组件 |
| --- | --- |
| 订单定义 | 数据中台 |
| 订单拆解与执行聚合 | 调度系统 |
| AGV 运单执行 | Adapter |
| 物理设备执行 | 下位机 |
| 控制权管理与裁决 | **下位机** |
| 实时展示与开发调试入口 | 数据中台 |

业务订单、运单或持久摘要不构成控制权。下位机可以拒绝无权或不安全命令；中台不得覆盖该结果。

## 2. MFMS module design（【推荐基线】）

```text
endpoint
  HTTP / WebSocket / site protocol adapters
application
  OrderIntake / OrderQuery / RealtimeView / ControlAccess / DebugCommand
domain
  OrderRequest / OrderExecutionSummary / DeviceSnapshot / ControlSnapshot / source-quality types
capability
  StateManager / LowerMachineControlClient / LowerMachineDebugClient /
  RedisStreamIngestor / MessageDecoderRegistry
persistence
  OrderRequestRepository / read-only Execution+Binding repositories /
  ChangeRequestRepository / AuditRepository / legacy event repositories
infrastructure
  CppProxySdkAdapter / MySQL / Redis Stream / WebSocket / Auth / StructuredLogger
```

端点不碰 SQL/SDK；SDK/Redis/MySQL structs 不进入 application/domain；Stream callback 只解码/投递；StateManager 按设备和来源串行更新。

## 3. Control access

### 3.1 Confirmed invariant

```text
MFMS checks user permission and context
  -> optionally refreshes/displays ControlSnapshot
  -> collects force-acquire reason/confirmation when needed
  -> invokes lower machine
  -> lower machine arbitrates and returns
  -> MFMS maps result and audits
```

MFMS 不写“已持锁”数据库行，不生成锁版本，不通知调度暂停，不等待 Adapter 释放，也不恢复旧持有者。

### 3.2 Read model

`ControlSnapshot` 可规范化 `locked/owner_ip/owner_port/owner_type/owner_nick_name/owner_time/owner_description` 以及消息来源、时间和质量。`owner_time` 真实含义待下位机确认。

`FRESH` 仅表示最近收到观察；`STALE/UNKNOWN` 默认关闭风险控制并允许显式查询；任何质量值都不是授权证明。

### 3.3 External operation requirement

逻辑上需要 query/acquire/force-acquire/release，但函数名、参数、请求者标识和结果码只是需求草案。`LowerMachineControlClient` 在 SDK adapter 内吸收真实接口差异。

调用超时可能是结果未知：平台返回单一未知终态并查询下位机，不自动重发强制申请。

## 4. Realtime state

### 4.1 Producers

- 下位机：AGV、机械臂、传感器和 `AgvControl` 等状态。
- Adapter：AGV `order_id` 的运行、进度、拒绝、故障和完成实时事实。

### 4.2 Frozen normalized semantics

规范化信封包含：

```text
message_type, schema_version, factory_id, device_id,
source_type, source_instance_id, sequence, observed_at, payload
```

中台补充 `received_at` 与 opaque `stream_entry_id`。序号只在同一 factory/device/message/source/instance 范围比较；不同来源/实例不互比。时间保留 producer observation 与 platform receipt 两个维度。质量固定为 `UNKNOWN/FRESH/STALE/UNSUPPORTED`。

### 4.3 Safe merge

`AgvOrderState` 与 `SeerM4State.order_state` 可能描述同一执行单元。权威关系未确认前，缓存按来源隔离并保留冲突，不按 arrival time 静默覆盖。

Redis Stream 的数量、Key、consumer group、ACK/Pending、保留、部署和 HA 都不在中台架构内。

## 5. Order persistence

### 5.1 Confirmed responsibility

中台保存/校验订单定义；调度接管、解释/拆分并聚合持久结果；Adapter 执行运单并发布实时事实。中台不维护调度执行状态，也不实现 DAG/补偿/选车。

### 5.2 Recommended table/field ownership model

```text
mfms_order_request              MFMS writes
mfms_order_execution            Scheduler writes; MFMS reads
mfms_order_execution_binding    Scheduler writes; MFMS reads
mfms_order_change_request       MFMS request columns / Scheduler result columns
```

`order_uid` 作为内部 ID；`(factory_id, source_system, task_code)` 是建议幂等键；binding 将 AGV `order_id` 映射回 `order_uid/task_code`。所有名称/字段/约束/状态机须经 DDL/业务评审，不能因写在设计中就当成现表。

### 5.3 Multi-scheduler claim

建议以 `mfms_order_execution.order_uid` 唯一插入竞争接管。该机制归调度/数据库合同；中台不管理调度 leader、lease 或故障转移。故障接管仍待业务确认。

## 6. User views

订单页面组合三种不同事实：

```text
request definition       MFMS MySQL
persistent execution     Scheduler MySQL summary/binding
live executions          source-aware StateManager cache
control display          lower-machine ControlSnapshot
```

UI 必须标出更新时间、来源和质量；不得将控制权变化推导为运单终态，也不得将实时静默推导为持久失败/完成。

## 7. Runtime and recovery

1. 启动连接 MySQL、读取注册表和订单摘要，但所有实时/控制快照先为 `UNKNOWN`。
2. 重新消费 Stream、启动 SDK 并显式查询必要控制状态；客户端重连获取全量快照。
3. Stream 中断使状态 `STALE`，不修改真实控制权或持久订单。
4. MySQL 不可用时停止新接单；实时查看可继续。强制申请审计 fail-closed 是推荐安全基线，待评审。
5. 调度不可用时只显示执行摘要过期，中台不接管/恢复订单。
6. shutdown 停止新写请求、受限排空、关闭 SDK/Stream/DB，不持久化实时控制快照作为恢复依据。

## 8. Decision convergence

| ID | v0.3 state |
| --- | --- |
| Q-31 | 关闭为消息信封与语义合同；Redis 拓扑移出范围 |
| Q-32 | 核心设计继续；核心表单写、变更请求字段所有权拆分为推荐基线，DDL/聚合待评审 |
| Q-33 | 中台范围关闭；下位机唯一控制权权威 |
| Q-34 | Adapter 内部契约移出；仅要求规范实时消息/关联 ID |
| Q-02-rev | 基线无需 scheduler→MFMS gRPC；健康/管理 API 如需另立需求 |

## 9. Pending confirmations

- 下位机：真实控制接口、请求者身份、`AgvControl.time`、错误码、变化通知、重启、车/机械臂控制域、requestId 查询。
- 消息生产者：Payload 映射、单位、时间编码、序号 reset/wrap、source instance 生命周期、新鲜度阈值。
- 业务/调度：订单 DDL/枚举/聚合/变更/claim/failover、绑定重试、敏感结果保留。
- 上下位机：legacy `device_state/lua_state` 拆分与事件保留/确认。

## 10. Historical exclusions

【历史废弃】：中台控制权状态机、lease/fencing/control epoch/confirmation token、跨系统夺权协调、business status 决定真实控制权、Adapter 暂停协调、调度 gRPC 直连基线、由中台设计 Redis 拓扑。

## 实施计划

来源：`/Users/melene/project/mfms_Framework/.trellis/tasks/08-16-architecture-design/implement.md`

# MFMS v0.3 architecture implementation/readiness plan

> This is a planning task. The v0.3 repository documentation snapshot was produced by `08-18-mfms-docs-convergence`; external contract and business/schema gates below remain open before production implementation.

## 1. Converge the v0.3 platform boundary

- [x] Record the lower machine as the sole control-rights manager/arbiter.
- [x] Remove platform lock/lease/fencing/epoch/confirmation/takeover coordination from the active architecture.
- [x] Define `ControlSnapshot` as display/prewarning only and lower-machine response as final.
- [x] Close Q-33 within MFMS scope; move Adapter internals (Q-34) out of scope.

## 2. Freeze platform-owned contracts

- [x] Freeze the source/sequence/time/quality Stream envelope while excluding Redis topology.
- [x] Preserve multi-source execution observations until an authority rule is approved.
- [x] Document single-authority order fields (including split change-request columns) as a recommended baseline, not deployed DDL.
- [x] Retain SDK/device-service and legacy DB event contracts with corrected v0.3 boundaries.
- [x] Complete backend directory/database/error/logging/quality/runtime-recovery specifications.

## 3. Lower-machine and producer review gate

- [ ] Review exact control operation signatures, requester identity, result codes, timeout/result lookup, restart, and control-domain behavior.
- [ ] Review `AgvControl.time` and control-change publication guarantees.
- [ ] Freeze payload field/unit maps, timestamps, sequence/reset/wrap, source-instance identity, and freshness thresholds for each message family.
- [ ] Resolve Adapter order state vs `SeerM4State.order_state` authority by field/lifecycle.

Rollback point: keep platform boundary docs; mark unapproved mapping/profile documents pending and do not implement adapter guesses.

## 4. Business/database review gate

- [ ] Freeze order DDL, indexes, types, ownership-enforcing repository surface, and migrations.
- [ ] Freeze intake revision/idempotency, aggregate states, partial success/cancel/timeout, change requests, scheduler claim/failover, and binding retry rules.
- [ ] Jointly review legacy `device_state` / `lua_state` split and event retention/acknowledgement.

Rollback point: retain the single-writer principle while replacing only the reviewed recommended schema; never return to shared-row dual writing.

## 5. First vertical implementation gate

- [ ] Select/build C++ tooling and establish the recommended layer skeleton without empty speculative modules.
- [ ] Implement one vertical read-only realtime slice with exact envelope decoding, source attribution, quality, and restart-to-`UNKNOWN` tests.
- [ ] Implement one lower-machine control query/request slice with rejection vs ambiguous timeout tests and no local ownership state.
- [ ] Implement owned order intake plus read-only execution/binding query against reviewed migrations.
- [ ] Run format/lint/build/unit/contract/integration/fault tests and update specs from verified behavior.

## 6. Architecture completion gate

- [ ] All pending confirmations are linked to signed/reviewed artifacts.
- [ ] Vault diagram and current architecture notes match the reviewed external contracts and physical schema.
- [ ] No active document presents recommended fields/codes as confirmed facts.
- [ ] Task can move from planning only after user/team review; do not archive or commit automatically.

## Task JSON

来源：`/Users/melene/project/mfms_Framework/.trellis/tasks/08-16-architecture-design/task.json`

```json
{
  "id": "architecture-design",
  "name": "architecture-design",
  "title": "MFMS 数据中台新版架构设计",
  "description": "MFMS 数据中台 v0.3 架构基线：下位机统一管理控制权，Stream 消息合同与按职责分表/字段单写的订单持久化边界收敛；外部接口和业务 DDL 继续评审。",
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
  "children": [],
  "parent": null,
  "relatedFiles": [
    ".trellis/ARCHMAP.md",
    ".trellis/spec/constraints.md",
    ".trellis/spec/contracts/index.md",
    ".trellis/spec/contracts/control-access-contract.md",
    ".trellis/spec/contracts/realtime-stream-contract.md",
    ".trellis/spec/contracts/order-persistence-contract.md",
    ".trellis/spec/contracts/bus-contracts.md",
    ".trellis/spec/contracts/db-event-contracts.md",
    ".trellis/spec/backend/index.md"
  ],
  "notes": "v0.3 boundary is current; task remains planning until lower-machine signatures/codes and business order DDL/aggregation are reviewed. Vault canonical root: 14.复合mfms/MFMS新中台-资料索引与权威源.md",
  "meta": {
    "architecture_baseline": "v0.3",
    "baseline_date": "2026-08-18",
    "source_sha256": "8a5bf99998cca03837f2c7e32d438b02a382d5744d7819111a6121170e784705"
  }
}
```
