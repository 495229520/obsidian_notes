---
generated_by: trellis-obsidian-task-detail
readonly: true
project: "mfms_Framework"
title: "P0-0 证据与版本核验"
trellis_status: "planning"
cssclasses:
  - trellis-task-detail
---

# P0-0 证据与版本核验

> [!info] 自动生成 · 只读详情
> 此页是 Trellis 源文档在 Obsidian 库内的展示镜像，解决库外 `file://` 链接无法打开的问题。
> 请在 Trellis 项目中修改任务；下一次同步会用权威源重新生成此页。

- 状态：规划中
- 当前阶段：Phase 1 · 规划
- 执行清单：0/12
- 优先级：P0
- 负责人：mfms-core
- 范围：evidence-baseline
- 创建时间：2026-08-23
- Trellis Task：`/Users/melene/project/mfms_Framework/.trellis/tasks/08-23-p0-0-evidence-baseline`

## 摘要

核验下位机 SDK、消息/服务定义、哈希与权威路径，形成 P0 契约评审可追溯证据基线。

## PRD

来源：`/Users/melene/project/mfms_Framework/.trellis/tasks/08-23-p0-0-evidence-baseline/prd.md`

# P0-0 证据与版本核验

## Goal

为 P0-A 和 P0-B 建立可追溯的下位机接口证据基线，确认当前可用 SDK、消息/服务定义及其版本身份。代理可以整理和比较证据，但不得自行宣布某个候选包为生产权威版本。

## Requirements

- 将 Vault 记录的 SDK 来源、当前磁盘状态和候选 SDK 包分开记录。
- 核对共享库、消息和服务定义的路径、文件数、大小、修改时间及 SHA-256；当前已知候选包包含 15 个 `.msg` 和 9 个 `.srv` 文件。
- 将每项证据标记为 `confirmed`、`candidate`、`legacy`、`missing` 或 `conflict`，并记录判断来源。
- 建立 Q-31、Q-35～Q-45、Q-54 与消息/服务文件的映射，注明哪些问题仍需要责任团队回答。
- 所有 SDK、legacy 仓库和 Vault 架构资料均为只读输入；本任务只写入自身 `research/`。

## Human Review Gate

- MFMS 负责人和下位机负责人共同确认“当前权威 SDK 包、版本和固定路径”。
- 未通过人工确认前，P0-A/P0-B 可以调查和起草，但不得关闭外部契约问题。

## Acceptance Criteria

- [ ] `research/evidence-manifest.md` 给出来源、版本、哈希、文件清单和状态分类。
- [ ] `research/evidence-delta.md` 记录 Vault 声明与磁盘候选之间的缺失、相同和差异。
- [ ] 每个契约问题都能追溯到文件、行号、哈希或明确的“未找到”。
- [ ] 人工确认结果记录到 Obsidian 人工确认台，并回填最终权威路径。
- [ ] 未修改 SDK、legacy 仓库、Vault canonical 文档或任何生产配置。

## Out of Scope

- 解释或改造下位机内部实现。
- 用哈希相同替代责任团队的版本确认。
- 修改 P0-A/P0-B 的最终契约状态。

## 设计

来源：`/Users/melene/project/mfms_Framework/.trellis/tasks/08-23-p0-0-evidence-baseline/design.md`

# P0-0 证据与版本核验设计

## Boundary

本任务建立证据目录，不定义控制或实时契约。证据身份与契约语义分离：文件存在、哈希相同可以由代理证明；“这是当前生产权威版本”必须由责任人确认。

## Read-only sources

| Source | Purpose | Initial state |
| --- | --- | --- |
| Vault `MFMS新中台-资料索引与权威源.md` | 已声明来源与优先级 | 声明的下载目录当前缺失 |
| `/Users/melene/project/mfms/HyRMS_export_202601251449_bszydxh-HP` | 候选 SDK/消息/服务包 | 与记录中的共享库哈希相同，仍待确认 |
| `/Users/melene/project/mfms` | legacy 行为证据 | 冻结只读，不代表当前合同 |
| 父任务 research/spec | v0.3/v0.4 已确认边界 | 实施约束来源 |

## Evidence model

每条证据至少包含：`evidence_id`、来源路径、相对路径、文件类型、大小、mtime、SHA-256、可定位行号、关联 Q 项、状态、责任人、备注。

状态只允许：

- `confirmed`：责任人已经确认。
- `candidate`：文件与现有证据一致，但未获得责任人确认。
- `legacy`：只用于解释历史行为。
- `missing`：声明存在但当前未找到。
- `conflict`：多个来源对同一事实不一致。

## Outputs

- `research/evidence-manifest.md`：证据清单和 Q 项映射。
- `research/evidence-delta.md`：声明来源、候选来源和 legacy 来源的差异。
- Obsidian 人工确认台中的一条决策记录：只由集成者写入。

## Rollback

若候选 SDK 被否决，保留清单和哈希记录，将其降级为 `legacy`，重新登记新的候选来源；不得删除历史证据。

## 实施计划

来源：`/Users/melene/project/mfms_Framework/.trellis/tasks/08-23-p0-0-evidence-baseline/implement.md`

# P0-0 执行计划

## 1. Collect

- [ ] 核对 Vault 声明的 SDK 路径是否存在。
- [ ] 盘点候选 SDK 的共享库、15 个消息和 9 个服务文件。
- [ ] 为二进制和接口文件生成 SHA-256，并记录稳定的相对路径。

## 2. Compare

- [ ] 对照父任务 v0.3/v0.4 research、Vault 权威索引和 legacy 调用点。
- [ ] 建立 Q-31、Q-35～Q-45、Q-54 到具体证据的映射。
- [ ] 将“相同”“缺失”“冲突”“无法判断”分开，不用推断填空。

## 3. Review gate

- [ ] 形成 `evidence-manifest.md` 与 `evidence-delta.md`。
- [ ] MFMS 与下位机负责人确认当前权威包、版本和长期固定路径。
- [ ] 将确认结果写入 Obsidian 人工确认台；未确认项保持 open。

## Validation

- [ ] 所有文件引用可在当前机器重新定位。
- [ ] 所有哈希可重复计算。
- [ ] `python3 .trellis/scripts/task.py validate 08-23-p0-0-evidence-baseline` 通过。

## Rollback point

任何人工否决只改变证据状态，不覆盖或删除既有记录。

## Task JSON

来源：`/Users/melene/project/mfms_Framework/.trellis/tasks/08-23-p0-0-evidence-baseline/task.json`

```json
{
  "id": "p0-0-evidence-baseline",
  "name": "p0-0-evidence-baseline",
  "title": "P0-0 证据与版本核验",
  "description": "核验下位机 SDK、消息/服务定义、哈希与权威路径，形成 P0 契约评审可追溯证据基线。",
  "status": "planning",
  "dev_type": null,
  "scope": "evidence-baseline",
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
    "review_owners": "mfms-core,lower-machine",
    "execution_mode": "orca-review-only",
    "human_gate": "MFMS+下位机确认 SDK 权威版本",
    "orca_run_id": "run_709bea11811c",
    "orca_task_id": "task_a79d1be152b2"
  }
}
```
