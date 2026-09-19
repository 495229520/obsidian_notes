---
generated_by: trellis-obsidian-task-detail
readonly: true
project: "obsidian_notes-main"
title: "合并存储知识与培养方案，统一总纲并保留全部笔记"
trellis_status: "completed"
cssclasses:
  - trellis-task-detail
---

# 合并存储知识与培养方案，统一总纲并保留全部笔记

> [!info] 自动生成 · 只读详情
> 此页是 Trellis 源文档在 Obsidian 库内的展示镜像，解决库外 `file://` 链接无法打开的问题。
> 请在 Trellis 项目中修改任务；下一次同步会用权威源重新生成此页。

- 状态：已完成
- 当前阶段：已完成
- 执行清单：5/5
- 优先级：P2
- 负责人：melene
- 创建时间：2026-09-19
- 完成时间：2026-09-19
- Trellis Task：`.trellis/tasks/09-19-unify-storage-study`

## 摘要

按用户确认的目录结构合并 8.高性能存储 与 9.培养方案；所有笔记迁移保留，旧总纲历史归档，更新链接与 Trellis 看板配置，逐文件核验。

## PRD

来源：`.trellis/tasks/09-19-unify-storage-study/prd.md`

# 合并存储知识与培养方案

## 用户要求

实施已确认的合并方案，不能删除笔记。保留 8.高性能存储 作为统一主题目录，9.培养方案 内容按知识正文、学习实践、历史方案归位；统一总纲；更新链接及 Trellis 看板输出位置。

## 验收

- 迁移前每篇笔记都有唯一的新位置；不覆盖同名文件，不丢失正文。
- 旧总纲和培养方案保留为历史资料，只有 00.总纲.md 决定当前目标与优先级。
- 8 篇恢复的 GPU 资料进入 AI GPU 数据路径；技术前置知识进入对应章节，项目交付类资料归学习实践。
- 当前待办仍以 .trellis/tasks 为权威，迁移不改变学习任务完成状态。
- 迁移涉及的 wiki、Markdown、相对资源链接与运行配置同步更新，无新增断链。
- 自动生成看板从新位置产出，旧目录无遗留文件。
- 提交逐文件清单、原文备份和验证报告；不自动提交或推送 Git。

## 设计

来源：`.trellis/tasks/09-19-unify-storage-study/design.md`

# 迁移设计

先记录两目录全部 Markdown 的路径与哈希，并保存迁移前快照。每个旧路径映射到唯一新路径；正文移动保留。旧知识总纲、执行总纲和 RDMA 入口全部保留为历史文档，新总纲集中导航和目标。相近正文不删除、不强行揉成单篇，以主文与学习补充区分。

9.培养方案 的资料按含义分配：技术前置知识到对应章节的学习补充；8 篇 GPU 资料到 AI GPU 数据路径的学习补充；阶段计划和工程收口资料到 90.学习与实践；冻结路线到 99.历史方案。没有碰撞才执行迁移。

对路径限定的 wikilink、Markdown URL 和相对路径做映射；保留短文件名兼容。旧总纲短链接统一指向新总纲，通过归档文档的新名称避免多个当前入口。更新 Trellis 本地输出配置及仓库指南，随后运行生成器。

备份存于本任务目录。验证比较预期内容与现存内容，链接验证比较迁移前后可解析性；对原有断链独立报告，避免把已有问题归为迁移成功。用户原有修改以迁移前实际工作树为准，不从 Git HEAD 覆盖。

## 实施计划

来源：`.trellis/tasks/09-19-unify-storage-study/implement.md`

# 执行记录

- [x] 保存原文快照和文件清单，确定无冲突的映射。
- [x] 移动全部文件，建立知识/实践/历史结构。
- [x] 写统一总纲并归档旧入口，更新链接和看板配置。
- [x] 重新生成看板，验证逐文件保留、链接和结构。
- [x] 记录迁移结果和可追溯证据。

## Task JSON

来源：`.trellis/tasks/09-19-unify-storage-study/task.json`

```json
{
  "id": "unify-storage-study",
  "name": "unify-storage-study",
  "title": "合并存储知识与培养方案，统一总纲并保留全部笔记",
  "description": "按用户确认的目录结构合并 8.高性能存储 与 9.培养方案；所有笔记迁移保留，旧总纲历史归档，更新链接与 Trellis 看板配置，逐文件核验。",
  "status": "completed",
  "dev_type": null,
  "scope": null,
  "package": null,
  "priority": "P2",
  "creator": "melene",
  "assignee": "melene",
  "createdAt": "2026-09-19",
  "completedAt": "2026-09-19",
  "branch": null,
  "base_branch": "main",
  "worktree_path": null,
  "commit": null,
  "pr_url": null,
  "subtasks": [],
  "children": [],
  "parent": null,
  "relatedFiles": [
    "8.高性能存储/00.总纲.md",
    "8.高性能存储/90.学习与实践/00.资料导航.md",
    "8.高性能存储/99.历史方案/00.历史资料索引.md"
  ],
  "notes": "合并完成；403 篇原笔记保留，191 篇移动，原有效链接无新增断链。证据见 migration-report.md 与 verification.json。",
  "meta": {
    "domain": "notes"
  }
}
```
