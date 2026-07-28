---
title: S-Week 7 - 简历化与投递启动
date: 2026-07-08
tags:
  - 高性能存储
  - 存储方向阶段计划
  - 计划
status: active
---

# S-Week 7 - 简历化与投递启动

> [!goal] 本周目标
> 把 `linux-io-lab` MVP 变成投递动作：简历 bullet 定稿、30+ 面试 Q&A 汇总、投递 tracker 建立、第一批内推/申请发出。阶段 0 在本周结束——9 月初能不能投出去，就看这一周。

## 学习目标

1. **项目怎么讲才像工程师？** 问题 → 方法 → 数据 → 结论 → 限制，而不是"我学了 io_uring"。
2. **JD 关键词怎么对表？** 把目标公司 JD 里的词和自己已有证据一一对应，缺口进阶段 1 计划。
3. **投递怎么管理？** 公司 / 岗位 / 渠道 / 状态 / 跟进日期，一张表管到底。

## 1. 简历与作品集定稿（Day 1-2）

### 1.1 简历 bullet（中英各一版）

```text
构建 Linux I/O benchmark lab，对比 buffered I/O、O_DIRECT、mmap、io_uring
与 fsync/fdatasync 路径，使用 fio / iostat 分析 IOPS、带宽与 p95/p99 延迟，
并输出可复现实验报告（reproduce.sh 在全新环境验证通过）。
```

```text
Built a Linux I/O benchmark lab comparing buffered I/O, O_DIRECT, mmap,
io_uring and fsync/fdatasync paths; analyzed IOPS, bandwidth and p95/p99
latency with fio/iostat, and published a fully reproducible report.
```

主标题按主方案：`AI Infrastructure Systems Engineer — C++ · Linux · Storage Systems · LLM Serving`（RDMA/NVMe-oF 等阶段 2 完成后再加，简历只写有证据的词）。

### 1.2 portfolio 更新

`storage-ai-infra-portfolio/README.md`：故事线第一段上线——"从 LLM 推理入门，正在向数据路径下钻"；`project-index.md` 标注 `linux-io-lab: done (MVP)`、后续项目 `planned`。

## 2. 面试弹药（Day 3-4）

### 2.1 `interview_qa.md` 汇总

把 S-Week 1-6 每周 10 个 Q&A 合并去重（目标 30+），按主题分组：page cache / O_DIRECT / 持久化 / mmap / io_uring / benchmark 方法论。每题答案控制在 5 句内，第一句是结论。

### 2.2 口述自测

- 20 分钟脱稿讲一遍 `linux-io-lab`（录音回听：有没有讲成流水账？数据有没有说出具体数字？）。
- 随机抽 10 道 Q&A 脱稿回答。
- 让 Agent 扮演面试官追问两轮（"为什么 p99 在 QD32 起飞？""换 XFS 会怎样？"），卡住的题回炉。

## 3. 投递启动（Day 5-7）

### 3.1 渠道排查

按主方案公司梯队过一遍 careers 页与实习计划（NVIDIA university recruiting、Pure / VAST / WEKA 的 intern 岗、国内存储团队暑期实习），把 JD 关键词抄进 tracker。

### 3.2 投递 tracker（进 portfolio 仓库）

| 公司 | 岗位 | JD 关键词 | 我的证据 | 渠道 | 状态 | 跟进 |
|---|---|---|---|---|---|---|
| （示例）WEKA | Storage Perf Intern | Linux I/O, fio, p99 | linux-io-lab | 内推/官网 | 已投 | +7d |

### 3.3 缺口分析

JD 里出现而你还没有证据的词（NVMe-oF、RDMA、分布式、kernel），逐个映射到阶段 1 / 2 的哪一周会补上——这张缺口表就是阶段 1 的优先级依据。

## 4. 阶段复盘（Day 7）

- 60 / 25 / 15 配比实际执行如何？哪一线经常被挤掉？
- MVP 是否达到 [[AI Infra 存储与 GPU 数据路径系统工程师培养方案]] 阶段 0 验收标准？未达项列入阶段 1 第一周。
- 决定阶段 1 起点（S-Week 8 块层与 blktrace，纲要见 [[00.存储方向阶段计划索引]]）。

## 5. 推理保温（约 25%）

- [[Week 8 - Prefill Decode + Open Source Repro]]：prefill/decode workload 对比 + 一个 issue reproduction。
- 推理线简历 bullet 同步定稿（serving benchmark harness 一条），与存储 bullet 并列进简历——这就是"两条线"的第一次合体亮相。

## 6. 面试保底（约 15%）

- 算法（5-8 题）：DP 起步。参考 [[5.2.19 动态规划]]，做 [[53. 最大子数组和]] 及 [[CodeTop 高频题 Top300]] DP 基础题。
- 八股（1 章）：IO 复用总串讲。过 [[13.5 串讲]]，把 select / poll / epoll / io_uring 串成一个演进故事（正好接本项目）。
- 项目问答：并入 `interview_qa.md` 汇总。

## 7. 本周产出文件

| 文件 | 内容 | 验收方式 |
|---|---|---|
| 简历（更新版） | 存储 + 推理两条项目 bullet | 每个词都有证据支撑 |
| `interview_qa.md` | 30+ Q&A 分主题 | 随机抽 10 题能脱稿答 |
| `apply-tracker.md` | 投递记录表 | 第一批 ≥ 5 个投递/内推发出 |
| `gap-analysis.md` | JD 缺口 → 阶段 1/2 周次映射 | 阶段 1 优先级依据 |
| 阶段 0 复盘笔记 | 配比复盘 + 验收核对 | 未达项有去处 |

## 8. 验收标准

- [ ] 简历定稿，中英 bullet 各一版。
- [ ] `interview_qa.md` ≥ 30 题且能脱稿抽答。
- [ ] 20 分钟项目口述完成且回听过。
- [ ] 第一批投递/内推发出（≥ 5 个）。
- [ ] 缺口分析完成，阶段 1 优先级确定。
- [ ] 阶段 0 验收清单核对完毕。

## 面试问题（元问题）

- 用 3 分钟介绍 linux-io-lab：问题、方法、最重要的三个数字。
- 这个项目和你要投的岗位有什么关系？
- 项目里最意外的一个实验结果是什么？
- 接下来两个月你打算补什么？为什么是它？

## 关联知识

- [[S-Week 6 - MVP 收口与报告]]
- [[S-Week 7 - 前置知识 - 简历化与投递启动]]
- [[存储面试问题清单 - Linux I O]]（必答题与答案范式）
- [[00.存储方向阶段计划索引]]（阶段 1 纲要）
- [[AI Infra 存储与 GPU 数据路径系统工程师培养方案]]
- [[AI Infra 项目开源科研叙事模板]]（项目叙事方法）
- [[ai_infra_report_europe_us_companies]]（公司与岗位清单）
