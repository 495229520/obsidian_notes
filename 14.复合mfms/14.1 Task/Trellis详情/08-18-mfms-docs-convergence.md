---
generated_by: trellis-obsidian-task-detail
readonly: true
project: "mfms_Framework"
title: "MFMS 新中台 v0.3 文档与 Trellis 收敛"
trellis_status: "completed"
cssclasses:
  - trellis-task-detail
---

# MFMS 新中台 v0.3 文档与 Trellis 收敛

> [!info] 自动生成 · 只读详情
> 此页是 Trellis 源文档在 Obsidian 库内的展示镜像，解决库外 `file://` 链接无法打开的问题。
> 请在 Trellis 项目中修改任务；下一次同步会用权威源重新生成此页。

- 状态：已完成
- 当前阶段：已完成
- 执行清单：30/30
- 优先级：P0
- 负责人：mfms-core
- 范围：docs
- 创建时间：2026-08-18
- 完成时间：2026-08-18
- Trellis Task：`/Users/melene/project/mfms_Framework/.trellis/tasks/archive/2026-08/08-18-mfms-docs-convergence`

## 摘要

在既有 canonical 文档收敛基础上，同步最新版 v0.3 控制权、实时 Stream 与订单持久化边界到 vault 和 Trellis。

## PRD

来源：`/Users/melene/project/mfms_Framework/.trellis/tasks/archive/2026-08/08-18-mfms-docs-convergence/prd.md`

# MFMS 新中台 v0.3 文档与 Trellis 收敛

## Goal

在已完成的 `14.复合mfms/` canonical 路径治理基础上，把最新版《MFMS 数据中台架构方案 v0.3：下位机统一管理控制权》系统同步到：

1. Vault canonical 五篇 Markdown、唯一可编辑 drawio，以及由 drawio/同一 v0.3 语义派生的三张 SVG 展示图；
2. 本仓库 `.trellis/ARCHMAP.md`、constraints、contracts 和 backend specs；
3. `08-16-architecture-design` 与本任务的 PRD/design/implement/research/context。

本任务只更新文档和实施约束，不修改 MFMS/legacy 业务代码、不执行数据库迁移、不替外部团队冻结接口。任务保持 `in_progress`，完成综合校验和用户审阅前不 archive、不 commit。

## Source baseline

- 原始附件：`/Users/melene/.codex/attachments/6bc3b2d6-ac2c-4700-a364-92e75d3b026f/pasted-text-1.txt`
- SHA-256：`8a5bf99998cca03837f2c7e32d438b02a382d5744d7819111a6121170e784705`
- Vault v0.3 前恢复点：`/private/tmp/mfms-v03-update.npg3qb/vault-before.tar`
- 备份 SHA-256：`acbe9c64d8576ab283fd73f42d6b766d125bd9dcf556f57930a47cc5b5d8e918`

完整来源沿革与旧 canonical 收敛证据见 `research/source-inventory.md`。

## Status discipline

| Status | Required treatment |
| --- | --- |
| **【已确认】** | 作为系统职责/平台合同约束实现 |
| **【推荐基线】** | 明确保留评审空间，不写成已部署表、已发布错误码或已冻结外部 API |
| **【待下位机/业务确认】** | 保留问题和所需输出，由责任团队回答 |
| **【历史废弃】** | 保留追溯提示，确保不再指导新实现 |

## Confirmed v0.3 facts

1. 下位机是控制权唯一管理者/裁决者；中台和 Adapter 只是申请者/持有者。
2. 中台不得维护锁、lease、fencing token、control epoch、confirmation token 或跨系统夺权协调。
3. `ControlSnapshot` 仅展示/预警；真实允许与否以下位机响应为准；二次确认只是权限、知情、原因和审计。
4. 下位机与 Adapter 写 Redis Stream；中台冻结消息信封与来源/序号/时间/质量语义，不设计 Redis 拓扑。
5. Q-33 在中台范围关闭；Q-34 Adapter 内部移出范围；Q-02-rev 基线无需 gRPC 直连；Q-31 收敛为消息合同；Q-32 保持核心设计。

## Recommended v0.3 baseline

- 订单按职责分为四表并保证字段单写：`mfms_order_request`（中台）、`mfms_order_execution` + binding（调度）、`mfms_order_change_request`（中台创建请求字段、调度处理结果字段）。
- 表名、字段、唯一约束、状态枚举、调度 claim 和审计失败策略都必须标为推荐，等待业务/DDL/安全评审。
- 中台采用单进程模块化单体并用 ports/adapters 隔离 SDK、Stream、MySQL 和 endpoint 类型。

## Requirements

### Vault（主会话负责）

1. 更新 canonical 五篇 Markdown：资料索引、架构构造思路、待确定问题清单、逐层设计工作台、Trellis 工程结构方案。
2. 更新唯一可编辑 `MFMS新中台-架构图.drawio`，使控制权、Stream 和订单边界与 v0.3 一致。
3. 生成并验证 `图片/SVG/14_1_1.svg`、`14_1_2.svg`、`14_1_3.svg` 三张展示图；SVG 是便于 Obsidian 阅读的派生资产，drawio 仍是唯一可编辑架构图源。
4. 保留旧笔记/跳转/自动生成任务镜像的所有权与历史追溯，不删除用户材料。
5. 使用本任务记录的受控备份作为失败恢复点。

### Repository Trellis（本轮实施代理负责）

1. 将 `ARCHMAP.md` 与 `spec/constraints.md` 从 v0.2 阻塞基线升级到 v0.3。
2. 建立 contracts index，并新增 control/realtime/order 三份合同；更新 SDK bus 与 legacy DB event 合同。
3. 补全 backend directory/database/error/logging/quality/runtime-recovery 规范。
4. 更新 `08-16` 的 task/PRD/design/implement/research/context，保持 planning 状态和外部评审门槛。
5. 更新本任务 task/PRD/design/implement/research/context，使原始路径治理和本轮 v0.3 刷新都可追溯。

## Acceptance Criteria

- [x] v0.3 原始附件与 SHA-256 已记录，Vault 更新前备份及哈希可恢复
- [x] 主会话负责的 canonical 五篇 Markdown + drawio + 三张 SVG 派生图已纳入 v0.3 更新范围并记录所有权
- [x] ARCHMAP/constraints 明确下位机唯一裁决并禁止全部中台锁/协调概念
- [x] control contract 区分已确认边界、推荐错误映射和待下位机真实签名/码表
- [x] realtime contract 冻结信封、来源、序号、时间、质量并排除 Redis 拓扑
- [x] order contract 保持核心表单写、变更请求字段拆分的推荐基线，不把表结构/状态机伪装成事实
- [x] bus/legacy DB contracts 保留有效方向并清除 v0.2 控制权/实时入口歧义
- [x] backend 六类规范系统补全，包含启动、降级、恢复和 shutdown
- [x] `08-16` 与 `08-18` task artifacts/context 统一使用 v0.3 事实层级
- [x] Vault Markdown/wikilink/drawio/SVG（结构、密度、渲染、Obsidian 嵌入）与 repo internal-link 综合校验通过
- [x] JSON/JSONL、Trellis task validate、`git diff --check` 和必要文本反向检查通过
- [x] 用户已审阅并确认项目提交；归档仍需单独执行

## Out of Scope

- 实现/修改下位机锁算法、Adapter ADK 编排或调度状态机。
- 选择 Redis 拓扑、消费者组、ACK/Pending、保留/HA。
- 执行推荐订单 DDL、修改真机/legacy 数据库或写业务代码。
- 自动提交、推送、归档或把自动生成 Obsidian task mirror 当权威源。

## Historical completion retained

本任务早期已完成 canonical 路径治理、旧工作台跳转、SDK 2026-08-10 权威来源校正和只读 task mirror 规则。v0.3 更新是在该治理结果上继续，不撤销旧恢复记录；详见 research 中的历史盘点。

## 设计

来源：`/Users/melene/project/mfms_Framework/.trellis/tasks/archive/2026-08/08-18-mfms-docs-convergence/design.md`

# MFMS 新中台 v0.3 文档收敛设计

## 1. Authority model

| Layer | Authority | Location |
| --- | --- | --- |
| Source evidence | v0.3 supplied text, SDK/legacy evidence, meeting decisions | Attachment / Downloads / legacy (read-only) |
| Vault active layer | Human architecture narrative, decision register, workbench, diagram | `14.复合mfms/` canonical set |
| Repo implementation layer | Constraints, contracts, backend guidance, task gates | `/Users/melene/project/mfms_Framework/.trellis/` |
| Projection layer | Trellis task mirrors in Obsidian | Exporter-owned, read-only |

Repo docs may encode confirmed platform boundaries and clearly labelled recommended baselines. They must not turn an external API/schema proposal into a confirmed fact. Projection files are never hand-edited.

## 2. v0.2 → v0.3 delta model

### Confirmed

- Lower machine exclusively manages/arbitrates control rights.
- MFMS/Adapter are requesters/holders only; cached control/order context is display/prewarning.
- Lower machine and Adapter write realtime facts to Redis Stream.
- MFMS owns the normalized message contract, not Redis topology.
- Q-33 closes within MFMS; Q-34 Adapter internals leave scope; Q-02-rev has no direct gRPC baseline.

### Recommended baseline

- Single-writer order request/execution/binding/change-request tables.
- Modular-monolith code placement, result mappings, audit fields, scheduler unique-insert claim.

### Pending external confirmation

- Lower-machine signatures, identity, codes, control timestamp/change/restart/domain semantics.
- Payload/unit/time/sequence/freshness profiles and multi-source authority.
- Physical order DDL, aggregate lifecycle, change/claim/failover and legacy DB migration.

### Deprecated history

- MFMS lock/lease/epoch/confirmation/takeover coordinator.
- Scheduler business status as real control authorization.
- MFMS pause/release coordination with scheduler/Adapter.
- MFMS-owned Redis topology or required scheduler gRPC direct access.

## 3. Repository information architecture

```text
.trellis/
├── ARCHMAP.md                      v0.3 overview and Q status
├── spec/
│   ├── constraints.md              binding redlines
│   ├── contracts/
│   │   ├── index.md
│   │   ├── control-access-contract.md
│   │   ├── realtime-stream-contract.md
│   │   ├── order-persistence-contract.md
│   │   ├── bus-contracts.md
│   │   └── db-event-contracts.md
│   └── backend/
│       ├── index.md
│       ├── directory-structure.md
│       ├── database-guidelines.md
│       ├── error-handling.md
│       ├── logging-guidelines.md
│       ├── quality-guidelines.md
│       └── runtime-recovery.md
└── tasks/
    ├── 08-16-architecture-design/  long-lived architecture planning/review gates
    └── 08-18-mfms-docs-convergence/ current synchronization/audit trail
```

The contracts index is the routing point. ARCHMAP summarizes; constraints state non-negotiable boundaries; detailed contracts own semantics; backend specs translate them into implementation and failure behavior.

## 4. Vault synchronization

The main session updates the canonical five Markdown files and drawio from the same v0.3 source. It also owns the derived Obsidian SVG assets `图片/SVG/14_1_1.svg`, `14_1_2.svg`, and `14_1_3.svg`, including structural, density, rendered-image, and in-app embed verification. The drawio remains the single editable architecture-diagram source; the SVGs are reading assets. This implementation/check-agent scope is intentionally restricted to `.trellis/`.

Required consistency:

- control authority and prohibited concepts match `control-access-contract.md`;
- message/source/sequence/time/quality semantics match `realtime-stream-contract.md` without drawing a Redis topology as MFMS-owned;
- order four-table design is visibly a recommendation;
- decision register shows Q-31/Q-33/Q-34/Q-02-rev convergence and Q-32 ongoing schema/business work;
- old documents remain historical, not competing authority.

## 5. Safeguards and rollback

- Vault backup before v0.3 writes: `/private/tmp/mfms-v03-update.npg3qb/vault-before.tar`.
- Backup SHA-256: `acbe9c64d8576ab283fd73f42d6b766d125bd9dcf556f57930a47cc5b5d8e918`.
- Manual v0.3 Vault writes are limited to the canonical five Markdown files, drawio, and the three explicitly named SVG reading assets, using explicit paths; exporter-owned task projections may change only through the exporter.
- Repo changes remain an uncommitted git diff; unrelated work is not overwritten.
- No workflow/tooling scripts, legacy business code, SDK binaries, or database files are modified.

Rollback uses the vault archive for the six pre-existing vault targets, removes only the three newly created SVG assets if a full v0.3 rollback is requested, and uses the version-control diff for repo docs. Do not use destructive broad reset commands.

## 6. Validation design

### Repository

- Parse every `task.json` with JSON tooling and every JSONL line as one object.
- Run `task.py validate` for both tasks.
- Resolve relative Markdown links inside `.trellis` and flag missing files.
- Search active docs for obsolete v0.2 assertions: MFMS lock/epoch/lease, business status granting control, unresolved Q-33, or Redis KV as the current realtime contract.
- Search proposed fields/codes for nearby recommended/pending labels.
- Run `git diff --check` and inspect scoped status/diff.

### Vault (main session/integration)

- Parse/inspect canonical Markdown links and drawio XML.
- Validate the three SVGs structurally and for density, inspect rendered images, and verify their embeds in Obsidian reading view.
- Confirm canonical names remain unique and historical materials are not deleted.
- Confirm the generated task projection is not manually edited; refresh only through exporter if required.
- Cross-check Vault Q states, table-status labels, and control boundary against Repo contracts.

## 7. Deferred implementation decisions

This documentation task does not close lower-machine API/profile questions, order physical schema/business aggregation, scheduler failover, Redis operations, or legacy DB table migration. `08-16-architecture-design` remains planning and lists those review gates.

## 实施计划

来源：`/Users/melene/project/mfms_Framework/.trellis/tasks/archive/2026-08/08-18-mfms-docs-convergence/implement.md`

# MFMS 新中台 v0.3 文档收敛实施记录

## 1. Historical canonical-path convergence

- [x] Create the `14.复合mfms/` authority index and move the active workbench into the canonical set.
- [x] Convert the old workbench to a historical jump page and retain archived notes.
- [x] Correct SDK authority to `/Users/melene/Downloads/HyRMS_export_0810` and preserve hash evidence.
- [x] Establish exporter-owned read-only Trellis task projections.

These items are retained for traceability from the task's first phase; v0.3 work below does not redo or delete them.

## 2. v0.3 preflight and recovery

- [x] Read the supplied v0.3 text completely and record SHA-256 `8a5bf99998cca03837f2c7e32d438b02a382d5744d7819111a6121170e784705`.
- [x] Classify confirmed, recommended, pending, and deprecated content before editing.
- [x] Create vault-before archive `/private/tmp/mfms-v03-update.npg3qb/vault-before.tar`.
- [x] Verify archive SHA-256 `acbe9c64d8576ab283fd73f42d6b766d125bd9dcf556f57930a47cc5b5d8e918`.
- [x] Keep implementation/check-agent scope limited to `.trellis/`; main session owns the five canonical Markdown files, drawio, and three SVG reading assets plus their verification.

Rollback point: restore only the six pre-existing vault targets from the archive; if a complete v0.3 rollback is requested, remove only the three explicitly named new SVG assets; revert only the scoped repo documentation diff without broad/destructive reset.

## 3. Vault v0.3 synchronization (main session)

- [x] Update the canonical five Markdown files: authority index, architecture, decision register, workbench, and Trellis structure plan.
- [x] Update the single editable drawio with the v0.3 control, Stream, and order boundaries.
- [x] Add `图片/SVG/14_1_1.svg`, `14_1_2.svg`, and `14_1_3.svg` as Obsidian reading assets while retaining drawio as the editable source.
- [x] Validate Markdown/wikilinks, drawio XML, SVG structure/density/render/embed, canonical uniqueness, and historical-file preservation.

## 4. Trellis v0.3 architecture/spec synchronization

- [x] Upgrade `ARCHMAP.md` and `spec/constraints.md` from the v0.2/Q-blocked baseline.
- [x] Add contracts index plus control-access, realtime-stream, and order-persistence contracts.
- [x] Update SDK/device-bus and legacy DB-event contracts without erasing compatible history.
- [x] Replace backend placeholders with directory, database, error, logging, quality, and runtime-recovery rules.
- [x] Keep proposed lower-machine codes/signatures and order schema visibly recommended/pending.

## 5. Trellis task traceability

- [x] Update `08-16` task/PRD/design and add implementation/readiness plan plus v0.3 source research.
- [x] Replace `08-16` placeholder implement/check contexts with real spec/research entries.
- [x] Update `08-18` task/PRD/design/implement/research and contexts for the second convergence phase.
- [x] Keep `08-16` planning and `08-18` in progress; do not archive or commit automatically.

## 6. Repository validation

- [x] Run JSON and JSONL parsing checks.
- [x] Run both Trellis task validations.
- [x] Check all relative Markdown links in changed `.trellis` documents.
- [x] Run text assertions for control authority, forbidden lock concepts, Q status, Stream topology exclusion, and order recommendation labels.
- [x] Run `git diff --check` and review scoped diff/status for unintended files.

## 7. Integration and handoff

- [x] Cross-check Vault canonical statements/drawio against Repo ARCHMAP/contracts.
- [x] Record final validation evidence and remaining external confirmations.
- [x] Present uncommitted changes to the user; wait for review before any commit/archive action.

## Task JSON

来源：`/Users/melene/project/mfms_Framework/.trellis/tasks/archive/2026-08/08-18-mfms-docs-convergence/task.json`

```json
{
  "id": "mfms-docs-convergence",
  "name": "mfms-docs-convergence",
  "title": "MFMS 新中台 v0.3 文档与 Trellis 收敛",
  "description": "在既有 canonical 文档收敛基础上，同步最新版 v0.3 控制权、实时 Stream 与订单持久化边界到 vault 和 Trellis。",
  "status": "completed",
  "dev_type": null,
  "scope": "docs",
  "package": null,
  "priority": "P0",
  "creator": "mfms-core",
  "assignee": "mfms-core",
  "createdAt": "2026-08-18",
  "completedAt": "2026-08-18",
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
    ".trellis/spec/backend/index.md",
    ".trellis/tasks/08-16-architecture-design"
  ],
  "notes": "Original canonical-path convergence is complete. Current in-progress phase applies the v0.3 source dated 2026-08-18; do not archive or commit automatically.",
  "meta": {
    "architecture_baseline": "v0.3",
    "source_sha256": "8a5bf99998cca03837f2c7e32d438b02a382d5744d7819111a6121170e784705"
  }
}
```
