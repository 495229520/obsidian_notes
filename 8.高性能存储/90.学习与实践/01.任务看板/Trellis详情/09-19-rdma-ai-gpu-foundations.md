---
generated_by: trellis-obsidian-task-detail
readonly: true
project: "obsidian_notes-main"
title: "RDMA for AI Infra：GPU 数据路径与推理通信"
trellis_status: "planning"
cssclasses:
  - trellis-task-detail
---

# RDMA for AI Infra：GPU 数据路径与推理通信

> [!info] 自动生成 · 只读详情
> 此页是 Trellis 源文档在 Obsidian 库内的展示镜像，解决库外 `file://` 链接无法打开的问题。
> 请在 Trellis 项目中修改任务；下一次同步会用权威源重新生成此页。

- 状态：规划中
- 当前阶段：Phase 1 · 规划
- 执行清单：0/13
- 优先级：P2
- 负责人：melene
- 创建时间：2026-09-19
- Trellis Task：`.trellis/tasks/09-19-rdma-ai-gpu-foundations`

## 摘要

恢复目标相关 GPU 素材；按 CUDA 数据搬运、KV/PD、测量、分布式通信推进。资料整理不等于学习完成，不沿用旧 GPU 周计划排期。

## PRD

来源：`.trellis/tasks/09-19-rdma-ai-gpu-foundations/prd.md`

# RDMA for AI Infra：GPU 数据路径与推理通信

## 目标

用户已确认从 GPU 素材库恢复与 RDMA for AI Infra 直接相关的内容。基于现有 Linux/RDMA 笔记，建立 GPU 数据搬运、KV/PD、测量与分布式通信的共同语言，为后续传输库实现提供基础。

## 范围与约束

- 恢复 8 篇，入口为 [[8.高性能存储/00.总纲]]；不整套解冻 GPU/kernel 周计划。
- 不把迁移完成、笔记 active 或已有示例当作用户已掌握。
- 当前不新建项目仓库，不预设 GPU、RNIC、多节点 Fabric 可用。
- 不变更既有 Linux/力扣任务状态，不附加未经确认的日历。
- 任务状态及验收以本目录为准，Obsidian 投影只读。

## 验收要求

- 能解释 CPU 提交、DMA、GPU 消费及 Stream/Event 的依赖。
- 能按模型假设计算 KV 容量，并区分 HBM 读取与跨节点传输。
- 能把 PD 交接画成包括容量准入、传输、可见性和消费的时间线。
- 能解释 TTFT/TPOT、请求吞吐、有效带宽的不同口径。
- 能画出 TP/EP 的典型通信，说明 NCCL 与 KV 点对点传输的区别。
- 有硬件时保存正确性和时间线证据；没有硬件时明确记录未验证项，不以模拟数据替代。

## 本次整理结果

2026-09-19：资料迁移、导航、元数据和链接修复已完成。学习未验收，任务保持 planning。执行动作见 implement.md；迁移明细见 migration.json。

## 设计

来源：`.trellis/tasks/09-19-rdma-ai-gpu-foundations/design.md`

# 资料恢复设计

采用移动原文而非复制：保留 8 篇文件名以兼容已有短链接，按 CUDA 数据路径、KV 与推理测量、分布式通信分组。合并重复 frontmatter，新增恢复范围说明，区分 active 资料和 not_started 学习状态。

当前总纲保留原路径；旧存储执行方案保存历史快照。旧周计划只作为实验素材，当前学习状态以 Trellis 为准。配套正文复用 8.高性能存储，不另写一份 RDMA/GDR/NCCL 概念解释。

迁移恢复 3 个原来失效的相对 SVG 引用，旧资源从 orphaned 移到 图片/SVG。完整路径引用随迁移更新，短链接保持不变。回退可依 migration.json 反向移动，正文变更可对照 Git 差异；不覆盖其他未提交修改。

## 实施计划

来源：`.trellis/tasks/09-19-rdma-ai-gpu-foundations/implement.md`

# 学习顺序与验收记录

资料入口：[[8.高性能存储/00.总纲]]。以下勾选表示用户学习验收，不由笔记搬迁自动勾选。跨阶段依赖按顺序推进，测量从第一个实际实验开始使用。

## A. CUDA 数据搬运与同步

阅读：[[8.高性能存储/8. AI GPU 数据路径/学习补充/3.2 CUDA Runtime 进阶]] 第 1–6、8 节；[[8.高性能存储/8. AI GPU 数据路径/学习补充/3.3 CUDA Host-Device 数据传输瓶颈与进程线程角色]] 第 3–8 节。

- [ ] 记录可用 Linux/CUDA/GPU/RNIC 环境与不可验证项；不购买或租用资源。
- [ ] 画出 CPU 提交 → Host 内存 → DMA → GPU 消费，标注所有权与可复用时点。
- [ ] 解释调用返回、Stream/Event 完成、跨设备可见性三个边界。
- [ ] 有 GPU 时做 pageable/pinned、同步/异步传输对照并保存原始数据；无 GPU 时仅完成代码与依赖分析，注明待实测。

## B. KV 数据模型与 PD 交接

阅读：[[8.高性能存储/8. AI GPU 数据路径/学习补充/3.11 LLM Serving Week 7 前置知识 - KV Cache + Prefix Cache + Paged KV]]、[[8.高性能存储/8. AI GPU 数据路径/学习补充/LLM 推理面试公式速算清单]]、[[8.高性能存储/8. AI GPU 数据路径/学习补充/3.12 LLM Serving Week 8 前置知识 - Prefill Decode + Open Source Repro]]；接 [[8.16 PD 分离（Prefill-Decode Disaggregation）下的 KV Cache 网络传输架构]]。

- [ ] 给定模型层数、KV head、维度、dtype 和序列长度，计算逻辑 KV 字节量并注明分片假设。
- [ ] 分开描述 Prefix 取回、PD handoff、Decode HBM 读取及各自计量口径。
- [ ] 画出目标容量准入 → 分块传输 → 就绪发布 → GPU 消费；说明超时与部分完成后的处理。

## C. 测量与性能归因

阅读：[[8.高性能存储/8. AI GPU 数据路径/学习补充/3.9 LLM Serving Week 5 前置知识 - Benchmark Harness + Metrics]]、[[8.高性能存储/8. AI GPU 数据路径/学习补充/3.10 LLM Serving Week 6 前置知识 - Observability + Metrics + Runbook]]；Nsight Systems 参考 [[8.21 NCCL 异步执行与性能：CUDA Stream、Overlap、nccl-tests 与故障诊断]]。

- [ ] 固定版本、负载、到达率/并发、warm-up 与测量窗口，列出成功和失败的统计口径。
- [ ] 用一条时间线区分排队、计算、传输与同步；没有 trace 时标注为设计图。
- [ ] 保存实验命令、原始结果和复现说明；区分 smoke、合成负载、真实推理及未完成项。

## D. 分布式通信接口

阅读：[[8.高性能存储/8. AI GPU 数据路径/学习补充/分布式推理通信与 TP 切分速记]]；配合 8.高性能存储的 NCCL 8.18–8.22。

- [ ] 画出 TP 的规约和 EP 的 All-to-All，解释与 KV handoff 的不同通信模式。
- [ ] 说明 PCIe、NVLink、RDMA 的适用路径，以及通信库与底层 verbs 的边界。
- [ ] 形成一份差距记录，再选择 Mooncake 或 NIXL 的一个传输路径作为后续任务，不同时开多个库的实现项目。

## 结果记录

目前没有学习验收或新增硬件实验结果。资料恢复日期：2026-09-19。后续每项完成时记录证据路径、环境与仍待验证的限制。

## Task JSON

来源：`.trellis/tasks/09-19-rdma-ai-gpu-foundations/task.json`

```json
{
  "id": "rdma-ai-gpu-foundations",
  "name": "rdma-ai-gpu-foundations",
  "title": "RDMA for AI Infra：GPU 数据路径与推理通信",
  "description": "恢复目标相关 GPU 素材；按 CUDA 数据搬运、KV/PD、测量、分布式通信推进。资料整理不等于学习完成，不沿用旧 GPU 周计划排期。",
  "status": "planning",
  "dev_type": null,
  "scope": null,
  "package": null,
  "priority": "P2",
  "creator": "melene",
  "assignee": "melene",
  "createdAt": "2026-09-19",
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
    "8.高性能存储/00.总纲.md",
    "8.高性能存储/00.总纲.md",
    "8.高性能存储/8. AI GPU 数据路径/学习补充/LLM 推理面试公式速算清单.md",
    "8.高性能存储/8. AI GPU 数据路径/学习补充/分布式推理通信与 TP 切分速记.md",
    "8.高性能存储/8. AI GPU 数据路径/学习补充/3.12 LLM Serving Week 8 前置知识 - Prefill Decode + Open Source Repro.md",
    "8.高性能存储/8. AI GPU 数据路径/学习补充/3.3 CUDA Host-Device 数据传输瓶颈与进程线程角色.md",
    "8.高性能存储/8. AI GPU 数据路径/学习补充/3.9 LLM Serving Week 5 前置知识 - Benchmark Harness + Metrics.md",
    "8.高性能存储/8. AI GPU 数据路径/学习补充/3.10 LLM Serving Week 6 前置知识 - Observability + Metrics + Runbook.md",
    "8.高性能存储/8. AI GPU 数据路径/学习补充/3.2 CUDA Runtime 进阶.md",
    "8.高性能存储/8. AI GPU 数据路径/学习补充/3.11 LLM Serving Week 7 前置知识 - KV Cache + Prefix Cache + Paged KV.md"
  ],
  "notes": "资料恢复完成；学习尚未验收，保持 planning；不切换既有学习任务。",
  "meta": {
    "domain": "study"
  }
}
```
