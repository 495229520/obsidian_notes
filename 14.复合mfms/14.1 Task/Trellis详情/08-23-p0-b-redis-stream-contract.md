---
generated_by: trellis-obsidian-task-detail
readonly: true
project: "mfms_Framework"
title: "P0-B Redis Stream 实时契约"
trellis_status: "planning"
cssclasses:
  - trellis-task-detail
---

# P0-B Redis Stream 实时契约

> [!info] 自动生成 · 只读详情
> 此页是 Trellis 源文档在 Obsidian 库内的展示镜像，解决库外 `file://` 链接无法打开的问题。
> 请在 Trellis 项目中修改任务；下一次同步会用权威源重新生成此页。

- 状态：规划中
- 当前阶段：Phase 1 · 规划
- 执行清单：0/13
- 优先级：P0
- 负责人：mfms-core
- 范围：realtime-contract
- 创建时间：2026-08-23
- Trellis Task：`/Users/melene/project/mfms_Framework/.trellis/tasks/08-23-p0-b-redis-stream-contract`

## 摘要

冻结 Redis Stream 规范化信封、Payload、单位、时间、序列、多源权威与运单关联语义，不设计 Redis 拓扑。

## PRD

来源：`/Users/melene/project/mfms_Framework/.trellis/tasks/08-23-p0-b-redis-stream-contract/prd.md`

# P0-B Redis Stream 实时契约

## Goal

形成下位机和 Adapter 可以联合签字的实时消息合同：冻结规范化信封、每类 Payload、字段单位、时间、序列、来源实例、质量与运单关联语义，同时保持 Redis 部署拓扑不在 MFMS 架构范围内。

## Dependency

P0-0 必须先确认消息/服务定义的权威版本。P0-B 可以先建立字段矩阵，但不得在权威版本未确认时冻结 Payload。

## Requirements

- 保留规范化信封：`message_type/schema_version/factory_id/device_id/source_type/source_instance_id/sequence/observed_at/payload`，并记录 `received_at/stream_entry_id`。
- 对每个消息家族记录字段、类型、单位、可空性、枚举、时间编码、序列 reset/wrap、来源实例生命周期和新鲜度阈值。
- 明确 Adapter 运单状态使用的关联 ID，以及 `AgvOrderState` 与 `SeerM4State.order_state` 的按字段/生命周期权威关系。
- 乱序、重复、缺口、未知 schema、多源冲突和生产者重启必须产生可解释的质量状态，不静默覆盖。
- MFMS 只规范化和消费消息；不决定 Stream 数量、消费者组、ACK/Pending、保留和高可用拓扑。

## Human Review Gate

- 下位机负责人签字其消息家族；Adapter 负责人签字运单实时事实与关联 ID。
- MFMS 负责人签字规范化、质量和展示语义。

## Acceptance Criteria

- [ ] `research/message-family-matrix.md` 覆盖全部当前消息家族和未找到项。
- [ ] `research/realtime-contract-review-packet.md` 覆盖信封、Payload、单位、时间、序列、来源和质量场景。
- [ ] Q-31、Q-45 及 Q-54 的实时关联部分有证据、状态和责任人。
- [ ] `UNKNOWN/FRESH/STALE/UNSUPPORTED` 与乱序、重复、缺口、多源冲突场景都有验收样例。
- [ ] 人工签字后才允许更新 `realtime-stream-contract.md` 的已确认内容。

## Out of Scope

- Redis 集群、Stream 数量、consumer group、ACK/Pending、保留和 HA 设计。
- 由到达先后自动决定多源权威。
- 代理自行发明生产者缺失字段。

## 设计

来源：`/Users/melene/project/mfms_Framework/.trellis/tasks/08-23-p0-b-redis-stream-contract/design.md`

# P0-B Redis Stream 实时契约设计

## Pipeline boundary

```text
Producer fact -> Stream entry -> envelope validation -> decoder registry
              -> source-aware state update -> quality-aware view/push
```

生产者事实、传输元数据和平台补充字段必须分层保存。`received_at` 与 `stream_entry_id` 是平台观察数据，不能冒充生产者业务时间。

## Message family matrix

每个家族至少记录：message type、schema version、producer、device key、payload fields、unit、observed time、sequence、source instance、correlation ID、freshness、authority by field、unknown handling、evidence、status、owner。

## Conflict rules

- schema 未知：保留原始证据并标记 `UNSUPPORTED`，不按旧 schema 猜测。
- 乱序/重复/缺口：按来源实例和序列记录质量，不跨来源简单比较 sequence。
- 生产者重启：source instance 或明确 reset 规则必须可区分。
- 多源冲突：按评审后的字段/生命周期权威处理；未确认时来源隔离展示。

## Outputs and writeback

代理只写本任务 `research/`。签字后由单一集成者更新 realtime contract、父任务 Q 状态和 Vault。

## Rollback

拒绝某个 Payload 映射时，退回来源隔离和 `UNSUPPORTED/UNKNOWN`，不以旧字段静默兼容。

## 实施计划

来源：`/Users/melene/project/mfms_Framework/.trellis/tasks/08-23-p0-b-redis-stream-contract/implement.md`

# P0-B 执行计划

## 1. Inventory

- [ ] 读取 P0-0 的权威消息版本；未确认时标记 candidate。
- [ ] 盘点全部消息家族、生产者、设备键和运单关联字段。
- [ ] 建立 Q-31、Q-45、Q-54 实时部分的证据映射。

## 2. Contract draft

- [ ] 填写信封和每类 Payload 字段/单位/时间矩阵。
- [ ] 定义 sequence/reset/wrap、source instance 和 freshness 候选规则。
- [ ] 定义未知 schema、乱序、重复、缺口、重启和多源冲突样例。

## 3. Cross-source review

- [ ] 比较下位机与 Adapter 对同一运单/设备字段的来源权威。
- [ ] 检查 `AgvOrderState` 与 `SeerM4State.order_state` 未被按到达时间覆盖。
- [ ] 检查没有混入 Redis 运维拓扑设计。

## 4. Human gate and integration

- [ ] 下位机/Adapter/MFMS 联合评审并记录签字、版本和日期。
- [ ] 单一集成者更新 contract、Q 状态和 Vault。

## Validation

- [ ] 每个消息家族有状态、证据、责任人和失败场景。
- [ ] `python3 .trellis/scripts/task.py validate 08-23-p0-b-redis-stream-contract` 通过。

## Rollback point

保留已确认信封和来源感知原则；撤销未签字的 Payload 映射并回到来源隔离。

## Task JSON

来源：`/Users/melene/project/mfms_Framework/.trellis/tasks/08-23-p0-b-redis-stream-contract/task.json`

```json
{
  "id": "p0-b-redis-stream-contract",
  "name": "p0-b-redis-stream-contract",
  "title": "P0-B Redis Stream 实时契约",
  "description": "冻结 Redis Stream 规范化信封、Payload、单位、时间、序列、多源权威与运单关联语义，不设计 Redis 拓扑。",
  "status": "planning",
  "dev_type": null,
  "scope": "realtime-contract",
  "package": null,
  "priority": "P0",
  "creator": "mfms-core",
  "assignee": "mfms-core",
  "createdAt": "2026-08-23",
  "completedAt": null,
  "branch": null,
  "base_branch": "main",
  "worktree_path": null,
  "commit": null,
  "pr_url": null,
  "subtasks": [],
  "children": [],
  "parent": "08-16-architecture-design",
  "relatedFiles": [],
  "notes": "",
  "meta": {
    "review_wave": "p0-contract-review",
    "review_owners": "lower-machine,adapter,mfms-core",
    "execution_mode": "orca-review-only",
    "human_gate": "下位机+Adapter+MFMS 联合签字",
    "orca_run_id": "run_709bea11811c",
    "orca_task_id": "task_b643891f7a89"
  }
}
```
