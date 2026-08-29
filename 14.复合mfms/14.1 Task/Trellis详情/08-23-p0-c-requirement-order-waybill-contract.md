---
generated_by: trellis-obsidian-task-detail
readonly: true
project: "mfms_Framework"
title: "P0-C Requirement Order Waybill 数据契约"
trellis_status: "planning"
cssclasses:
  - trellis-task-detail
---

# P0-C Requirement Order Waybill 数据契约

> [!info] 自动生成 · 只读详情
> 此页是 Trellis 源文档在 Obsidian 库内的展示镜像，解决库外 `file://` 链接无法打开的问题。
> 请在 Trellis 项目中修改任务；下一次同步会用权威源重新生成此页。

- 状态：规划中
- 当前阶段：Phase 1 · 规划
- 执行清单：0/15
- 优先级：P0
- 负责人：mfms-core
- 范围：data-contract
- 创建时间：2026-08-23
- Trellis Task：`/Users/melene/project/mfms_Framework/.trellis/tasks/08-23-p0-c-requirement-order-waybill-contract`

## 摘要

形成 Requirement、Order、Waybill 的 DDL 决策包、单写权限、血缘、拆分、幂等、变更、认领和 Adapter 交接评审基线。

## PRD

来源：`/Users/melene/project/mfms_Framework/.trellis/tasks/08-23-p0-c-requirement-order-waybill-contract/prd.md`

# P0-C Requirement Order Waybill 数据契约

## Goal

形成业务、Scheduler、DBA、Adapter 与 MFMS 可以联合签字的数据合同，冻结 Requirement、Order、Waybill 的物理 DDL、单写权限、血缘、拆分、幂等、变更、认领、生命周期和实时交接边界。

## Requirements

- 保持已确认职责：MFMS 单写 Requirement；Scheduler 单写 Order/Waybill；MFMS 对 Order/Waybill 只读。
- 冻结三类实体的 ID、列、类型、可空性、枚举、外键、索引、唯一约束、权限、迁移、保留和审计要求。
- 冻结订单 `ids[]` 元素/顺序/存储、时间戳、数量、优先级和 LOAD/UNLOAD 行规则。
- 冻结 Requirement→Order→Waybill 基数、拆分顺序、原子性、幂等、revision/change/cancel、claim 和 failover。
- 冻结 Waybill 字段、设备/路线、生命周期、终态、重试/替换和 Adapter 交接/关联 ID。
- 缺少真实 Scheduler/DBA/Adapter 输入时必须标记 `unknown` 和责任人，禁止代理发明生产 DDL。

## Human Review Gate

- 业务负责人确认字段和拆分语义。
- Scheduler 负责人确认单写、claim、幂等、状态和 failover。
- DBA 确认 DDL、索引、权限、迁移、保留和回滚。
- Adapter 负责人确认 Waybill 交接、关联和执行事实。
- MFMS 负责人确认只写 Requirement、只读派生表和组合视图。

## Acceptance Criteria

- [ ] `research/data-contract-decision-table.md` 覆盖 Q-32、Q-46～Q-48、Q-54。
- [ ] `research/ddl-review-packet.md` 包含三类实体的候选 DDL、权限矩阵、迁移与回滚清单；候选内容显式标注未批准。
- [ ] 每个写操作只有一个系统拥有者，MFMS 不存在 Order/Waybill 写 API。
- [ ] 幂等、并发 claim、失败恢复、重复拆分、变更/取消、重试/替换均有验收场景。
- [ ] 五方人工确认有姓名、版本、日期、结论和待办记录。
- [ ] 只有签字后的合同才能进入 migration 或首个纵向代码任务。

## Out of Scope

- 由 MFMS 实现拆单、选车、调度 DAG、Order/Waybill 推进或修复。
- 未经 DBA 评审执行生产迁移。
- 由代理代替业务/Scheduler/Adapter 决定未知字段或状态机。

## 设计

来源：`/Users/melene/project/mfms_Framework/.trellis/tasks/08-23-p0-c-requirement-order-waybill-contract/design.md`

# P0-C 数据契约设计

## Confirmed ownership

```text
MFMS writes Requirement
Scheduler reads Requirement and writes Order + Waybill
Adapter executes Waybill and emits live facts
MFMS reads Order + Waybill and composes RequirementView
```

所有候选 DDL 都必须保留这个单写者边界。数据库权限应在账号层与 Repository API 层同时表达。

## Decision table

每个实体/字段记录：business meaning、physical name、type、nullability、default、owner、writer、readers、identity、lineage、constraint/index、lifecycle、migration、retention、evidence、status、reviewer。

## Scenario groups

- Requirement：新建、重复提交、revision、change、cancel、校验失败。
- Scheduler：多实例 claim、重复拆分、部分失败、原子落地、failover。
- Order：LOAD/UNLOAD、`ids[]`、quantity、priority、business time、1..N 拆分。
- Waybill：assignment、handoff、running、terminal、retry/replacement、correlation。
- MFMS view：只读延迟、数据缺口、实时与持久状态不一致、来源质量。

## Outputs and writeback

代理只写本任务 `research/`。五方签字后，由单一集成者更新 order persistence contract、database guidelines、父任务和 Vault。migration 仍需另建执行任务。

## Rollback

评审否决物理名称或类型时，只撤销候选 DDL；已确认职责边界与订单最小逻辑字段继续保留。

## 实施计划

来源：`/Users/melene/project/mfms_Framework/.trellis/tasks/08-23-p0-c-requirement-order-waybill-contract/implement.md`

# P0-C 执行计划

## 1. Build the decision packet

- [ ] 将 Q-32、Q-46～Q-48、Q-54 拆成字段、权限、生命周期和场景决策表。
- [ ] 汇总已确认订单逻辑字段和当前推荐表/血缘方案。
- [ ] 登记缺失的业务、Scheduler、DBA、Adapter 输入及责任人。

## 2. Draft candidate contracts

- [ ] 起草 Requirement/Order/Waybill 候选 DDL 和 Repository 权限矩阵。
- [ ] 起草 ID、外键、索引、唯一约束、迁移、保留和回滚问题包。
- [ ] 起草拆分、claim、幂等、revision/change/cancel、failover 场景。
- [ ] 起草 Waybill 生命周期、重试/替换、Adapter 交接和关联场景。

## 3. Cross-contract review

- [ ] 检查 MFMS 只写 Requirement、只读 Order/Waybill。
- [ ] 检查实时事实没有越权推进持久状态，持久状态也不授权控制权。
- [ ] 检查未知字段仍为 unknown，没有被推荐名称掩盖。

## 4. Human gate and integration

- [ ] 业务/Scheduler/DBA/Adapter/MFMS 联合评审并记录签字、版本和日期。
- [ ] 单一集成者更新 contracts、database guidelines、Q 状态和 Vault。
- [ ] 另建 migration/纵向实现任务；本任务不执行生产迁移。

## Validation

- [ ] 五类评审角色均有结论或明确阻塞项。
- [ ] `python3 .trellis/scripts/task.py validate 08-23-p0-c-requirement-order-waybill-contract` 通过。

## Rollback point

保留职责边界和订单逻辑字段；只撤销未批准的物理 DDL、状态和算法候选。

## Task JSON

来源：`/Users/melene/project/mfms_Framework/.trellis/tasks/08-23-p0-c-requirement-order-waybill-contract/task.json`

```json
{
  "id": "p0-c-requirement-order-waybill-contract",
  "name": "p0-c-requirement-order-waybill-contract",
  "title": "P0-C Requirement Order Waybill 数据契约",
  "description": "形成 Requirement、Order、Waybill 的 DDL 决策包、单写权限、血缘、拆分、幂等、变更、认领和 Adapter 交接评审基线。",
  "status": "planning",
  "dev_type": null,
  "scope": "data-contract",
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
    "review_owners": "business,scheduler,dba,adapter,mfms-core",
    "execution_mode": "orca-review-only",
    "human_gate": "业务+Scheduler+DBA+Adapter+MFMS 联合签字",
    "orca_run_id": "run_709bea11811c",
    "orca_task_id": "task_68c6fd0c30fa"
  }
}
```
