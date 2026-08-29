---
title: MFMS 新中台 · Trellis 工程结构方案
date: 2026-08-16
updated: 2026-08-22
tags:
  - 复合mfms
  - MFMS
  - Trellis
  - 架构规划
version: v0.4
status: 当前项目落位方案
---

# MFMS 新中台 · Trellis 工程结构方案

> [!abstract] 本文定位
> 本文说明 v0.4 怎样进入 <code>/Users/melene/project/mfms_Framework/.trellis/</code>。vault 负责讨论、状态和图源；Trellis 只存可约束实现的红线、合同、质量规则与任务资料。当前架构入口见 [[MFMS新中台-架构构造思路]]。

> [!important] v0.4 当前落位
> 控制权与 Stream 合同沿用 v0.3；持久化合同更新为中台单写需求、调度单写订单与运单。订单逻辑字段已确认，物理 DDL、拆分规则和运单合同继续走 Q-32/Q-46/Q-47/Q-48/Q-54 评审。

## 1. 权威层级

| 层 | 负责 | 位置 |
| --- | --- | --- |
| Vault 当前层 | 架构结论、问题状态、设计工作台、可编辑图 | <code>14.复合mfms/</code> |
| Trellis 实施层 | 红线、模块地图、合同、质量与任务 | <code>mfms_Framework/.trellis/</code> |
| 外部事实层 | 下位机 SDK、legacy 源码、真实消息样例、数据库实物 | Downloads / legacy repo / 联调记录，只读引用 |
| 展示层 | Trellis Task 的 Obsidian 镜像 | <code>14.1 Task/</code>，导出器生成，禁止手改 |

结论冲突时按“较新、明确、权威方负责”的顺序处理：v0.4 已确认边界优先于旧工作台；下位机真实接口优先于中台自拟函数名；数据库联合评审优先于推荐 DDL。

## 2. v0.4 Trellis 目录

~~~text
.trellis/
├── ARCHMAP.md
├── workflow.md
├── spec/
│   ├── constraints.md
│   ├── contracts/
│   │   ├── index.md
│   │   ├── control-access-contract.md
│   │   ├── realtime-stream-contract.md
│   │   ├── order-persistence-contract.md
│   │   ├── bus-contracts.md
│   │   └── db-event-contracts.md
│   ├── backend/
│   │   ├── index.md
│   │   ├── directory-structure.md
│   │   ├── database-guidelines.md
│   │   ├── error-handling.md
│   │   ├── logging-guidelines.md
│   │   ├── quality-guidelines.md
│   │   └── runtime-recovery.md
│   ├── guides/
│   └── tooling/
├── tasks/
│   ├── 08-16-architecture-design/
│   └── 08-18-mfms-docs-convergence/
├── scripts/
└── workspace/
~~~

### 2.1 文件职责

| 文件 | 只回答 |
| --- | --- |
| <code>ARCHMAP.md</code> | 组件、依赖方向、外部系统和权威边界 |
| <code>constraints.md</code> | 不允许违反的架构红线 |
| <code>control-access-contract.md</code> | 中台怎样查询/申请/强制申请/释放与发送命令 |
| <code>realtime-stream-contract.md</code> | Stream 信封、消息来源、序号、时间、质量和多源合并 |
| <code>order-persistence-contract.md</code> | 中台需求、调度订单/运单、订单逻辑字段、血缘与只读边界 |
| <code>bus-contracts.md</code> | Cpp-Proxy-SDK、设备 ID、既有下位机总线适配 |
| <code>db-event-contracts.md</code> | legacy 双事件表及迁移；禁止承载新订单模型 |
| backend specs | 模块落位、DB、错误、审计、质量、恢复与线程规则 |

合同必须分开，避免再次把“控制锁”“实时消息”“需求/订单/运单事实”和“legacy 事件表”混成一个状态机。

## 3. vault → Trellis 映射

| Vault 章节 | Trellis | 状态 |
| --- | --- | --- |
| 架构快照 §1～§4 | ARCHMAP + constraints | 已确认边界可直接同步 |
| 工作台 §2 控制访问 | control-access-contract | 函数名/错误码仍以 Q-35～Q-44 为评审门槛 |
| 工作台 §3 实时消息 | realtime-stream-contract | 信封是推荐 P0 基线；实际 Payload 样例待收集 |
| 工作台 §4/§5 需求/订单/运单 | order-persistence-contract + database guidelines | 职责与订单逻辑字段已确认；物理表结构需调度/DBA 评审 |
| 工作台 §6 | directory-structure | 模块化单体边界已可执行 |
| 工作台 §7～§9 | error/logging/quality/runtime-recovery | 降级与 fail-closed 规则 |
| 问题清单 | architecture-design task | 只保留真正未决项 |
| drawio | Vault 图源 | 不复制进 repo；ARCHMAP 用文字可审查地表达同一边界 |

## 4. 当前 gap

| 交付物 | 当前状态 | 下一步 |
| --- | --- | --- |
| 控制权职责 | v0.4 继承确认 | 进入 constraints；删净本地锁/协调器表述 |
| 下位机控制接口 | 目标调用面已写 | 与下位机关闭 Q-35～Q-44，附真实头文件/响应样例 |
| Stream 消息目录 | 来源类型已确认 | 收集每种 payload 样例，冻结 schema_version 和质量阈值 |
| 需求/订单/运单持久化 | 中台需求、调度订单/运单职责已确认 | 产出三类记录 DDL、订单字段精确定义、拆分与调度恢复评审 |
| 模块目录 | 设计已定，业务代码尚未创建 | P1 任务按 endpoint/application/domain/capability/persistence/infrastructure 建骨架 |
| 运行恢复 | v0.4 行为已成文 | 用断流、重启、超时、MySQL/调度不可用测试固化 |
| 审计 | 字段和 fail-closed 原则已成文 | 确认保留期、身份绑定和查询权限 |
| 工厂定制 | SiteProtocolAdapter 边界已定 | 等 Q-14/Q-15 后创建 site spec，不提前猜工厂配置 |

## 5. 任务拆分

当前架构任务 <code>08-16-architecture-design</code> 继续作为 v0.4 规划总任务，不把所有代码塞进同一任务。建议后续建立可独立验收的子任务：

1. **P0-A 下位机控制合同**：接口签名、身份、错误码、超时确认与控制域；
2. **P0-B 实时 Stream 合同**：消息目录、信封、版本、乱序/新鲜度和录制样例；
3. **P0-C 需求/订单/运单合同**：DDL、订单字段、血缘、拆分、幂等、多调度实例与变更请求；
4. **P1 能力骨架**：Ingestor、DecoderRegistry、StateManager、SDK 薄适配；
5. **P2 需求竖切**：中台需求接入、订单/运单只读查询与血缘；
6. **P3 调试竖切**：控制访问、调试命令、权限、审计和结果未知恢复；
7. **P4 统一视图与推送**：RequirementView、状态质量、全量快照/增量和断流恢复。

父任务只维护跨任务一致性；子任务各自具备 PRD、design、implement、context 和可运行验收。

## 6. 实施门槛

### 6.1 可以立即执行

- 用 v0.4 更新 ARCHMAP、红线、合同和 backend 规范；
- 编写契约测试样例与 mock 接口；
- 建稳定 DTO 和目录骨架；
- 设计 UNKNOWN/FRESH/STALE/UNSUPPORTED 状态质量；
- 设计需求/订单/运单 DDL 草案与评审材料，但不把推荐物理字段冒充已确认事实。

### 6.2 不能越过

- 未拿到真实接口就把建议函数名当 SDK API；
- 在中台实现任何真实锁状态；
- 用调度/运单状态代替下位机控制权；
- 让中台创建或更新调度拥有的订单/运单；
- 无来源合并 Stream 消息；
- 将 Adapter 内部恢复或调度故障转移塞进中台；
- 把 legacy 双事件表改名后冒充需求/订单/运单模型；
- 审计不可用时仍允许强制申请。

## 7. 评审与验证

每个 v0.4 实现任务至少检查：

1. 依赖方向是否与 ARCHMAP 一致；
2. 是否出现 lock/lease/epoch/token/coordinator 等越界概念；
3. 所有控制动作是否以下位机终态为准；
4. 所有状态是否保留来源、时间、序号和质量；
5. 中台是否只写需求、订单/运单 Repository 是否只读；
6. 超时是否区分“失败”和“结果未知”；
7. 强制申请是否具备权限、reason 和成功/失败审计；
8. 重启、断流、MySQL/调度不可用测试是否覆盖；
9. SDK/Redis/MySQL 类型是否被 infrastructure 隔离；
10. 文档、任务 context、生成镜像是否同步。

## 8. Trellis 工具边界

现有 workflow、task 生命周期、Obsidian dashboard 导出器和 workspace journal 继续沿用。<code>where.py / ctx.py / map_check.py / intake/</code> 等早期工具设想不是 v0.4 架构前置条件；如确有需求另开 tooling 任务，不再和业务合同冻结绑在一起。

## 9. 关联

- [[MFMS新中台-架构构造思路]] —— 当前架构结论
- [[MFMS新中台-逐层设计工作台]] —— 字段、流程与实施切片
- [[MFMS新中台-待确定问题清单]] —— P0 联合评审问题
- [[MFMS新中台-资料索引与权威源]] —— repo、legacy、SDK 与版本边界
