---
generated_by: trellis-obsidian-task-detail
readonly: true
project: "mfms_Framework"
title: "P0-A 下位机控制契约"
trellis_status: "planning"
cssclasses:
  - trellis-task-detail
---

# P0-A 下位机控制契约

> [!info] 自动生成 · 只读详情
> 此页是 Trellis 源文档在 Obsidian 库内的展示镜像，解决库外 `file://` 链接无法打开的问题。
> 请在 Trellis 项目中修改任务；下一次同步会用权威源重新生成此页。

- 状态：规划中
- 当前阶段：Phase 1 · 规划
- 执行清单：0/13
- 优先级：P0
- 负责人：mfms-core
- 范围：control-contract
- 创建时间：2026-08-23
- Trellis Task：`/Users/melene/project/mfms_Framework/.trellis/tasks/08-23-p0-a-lower-machine-control-contract`

## 摘要

基于已确认的下位机唯一控制权红线，核验 Q-35 至 Q-44 的真实接口、时间、错误码、重启与安全语义。

## PRD

来源：`/Users/melene/project/mfms_Framework/.trellis/tasks/08-23-p0-a-lower-machine-control-contract/prd.md`

# P0-A 下位机控制契约

## Goal

在“下位机是控制权唯一管理者和唯一裁决者”的已确认红线上，形成可供下位机、Adapter 与 MFMS 联合签字的控制接口合同，关闭 Q-35～Q-44 中有充分证据的部分。

## Dependency

P0-0 必须先确认当前权威 SDK。P0-A 可以在此之前整理候选差异和问题包，但不得冻结签名、码表或时间语义。

## Requirements

- 记录普通申请、强制申请、释放、查询和设备命令的真实函数签名及请求者身份。
- 明确 `AgvControl.time` 的单位、来源、单调性和重启语义。
- 明确结果码、拒绝原因、超时后的结果查询、是否允许重试及重复请求语义。
- 明确控制权变化通知、下位机重启、通信中断、控制域和多客户端行为。
- 中台只做权限、风险提示、调用、结果映射、展示和审计；不得引入本地锁、lease、fencing token 或跨系统夺权协调。
- 缓存和 Stream 快照都不是授权依据；下位机响应始终是最终结果。

## Human Review Gate

- 必须由下位机负责人签字；涉及 Adapter 身份或转发时由 Adapter 负责人共同签字。
- MFMS 负责人确认错误映射、审计与 UI 语义没有扩大控制权。

## Acceptance Criteria

- [ ] `research/control-interface-matrix.md` 覆盖 Q-35～Q-44，逐项给出证据和状态。
- [ ] `research/control-review-packet.md` 给出拟冻结签名、码表、时序、超时和重启案例。
- [ ] 所有 UNKNOWN/冲突项有明确责任人，未被默认值掩盖。
- [ ] 正常、拒绝、超时未知、断联、重启、重复请求和强制申请场景均有验收样例。
- [ ] 人工签字后才允许更新 `control-access-contract.md` 的已确认部分。

## Out of Scope

- 下位机内部锁算法。
- 中台实现控制权状态机或恢复控制权。
- 未经下位机确认的自动重试和超时推断。

## 设计

来源：`/Users/melene/project/mfms_Framework/.trellis/tasks/08-23-p0-a-lower-machine-control-contract/design.md`

# P0-A 下位机控制契约设计

## Contract layers

1. **Wire/API fact**：真实 SDK 签名、消息字段和码表。
2. **Platform mapping**：MFMS DTO、错误分类、审计字段和 UI 展示。
3. **Authority rule**：下位机最终裁决；任何缓存只用于展示。

三层必须分别记录，避免把中台建议反写成下位机事实。

## Review matrix

每个操作记录：operation、requester、request fields、response fields、result codes、timeout meaning、query-after-timeout、retry rule、idempotency key、audit fields、source evidence、status、owner。

## Safety invariants

- 强制申请是显式高风险操作，必须携带权限、原因和审计上下文。
- 超时表示结果未知，除非合同明确支持基于 request ID 查询，否则禁止自动重发。
- Stream 中的控制快照不能代替命令响应，也不能授权后续设备命令。
- 重启或断联后，MFMS 必须重新查询，不从本地持久状态恢复控制权。

## Outputs and writeback

代理只写本任务 `research/`。人工签字后，由单一集成者更新 `.trellis/spec/contracts/control-access-contract.md`、父任务 Q 状态和 Vault 未决项。

## Rollback

若责任团队否决候选字段，仅回退候选映射；下位机唯一裁决红线不回退。

## 实施计划

来源：`/Users/melene/project/mfms_Framework/.trellis/tasks/08-23-p0-a-lower-machine-control-contract/implement.md`

# P0-A 执行计划

## 1. Evidence and delta

- [ ] 读取 P0-0 的已确认证据清单；未确认时只标记 candidate。
- [ ] 提取真实控制相关 `.srv`、`.msg` 和 legacy 调用点。
- [ ] 建立 Q-35～Q-44 的接口差异矩阵。

## 2. Contract draft

- [ ] 起草普通申请、强制申请、释放、查询和设备命令合同。
- [ ] 起草结果码、超时未知、查询、重试和幂等规则。
- [ ] 起草变化通知、断联、重启和控制域场景表。

## 3. Adversarial review

- [ ] 检查是否出现中台锁、lease、缓存授权或自动重发。
- [ ] 检查强制申请是否有权限、原因、风险提示和审计。
- [ ] 检查每个建议是否与真实下位机事实分层。

## 4. Human gate and integration

- [ ] 下位机/Adapter/MFMS 联合评审并记录签字、版本和日期。
- [ ] 单一集成者更新 contract、Q 状态和 Vault；代理不得直接关闭问题。

## Validation

- [ ] Q-35～Q-44 全部有状态、证据和责任人。
- [ ] `python3 .trellis/scripts/task.py validate 08-23-p0-a-lower-machine-control-contract` 通过。

## Rollback point

保留下位机唯一裁决红线；仅撤销未获签字的签名、码表和映射建议。

## Task JSON

来源：`/Users/melene/project/mfms_Framework/.trellis/tasks/08-23-p0-a-lower-machine-control-contract/task.json`

```json
{
  "id": "p0-a-lower-machine-control-contract",
  "name": "p0-a-lower-machine-control-contract",
  "title": "P0-A 下位机控制契约",
  "description": "基于已确认的下位机唯一控制权红线，核验 Q-35 至 Q-44 的真实接口、时间、错误码、重启与安全语义。",
  "status": "planning",
  "dev_type": null,
  "scope": "control-contract",
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
    "orca_task_id": "task_1a15bdf628e3"
  }
}
```
