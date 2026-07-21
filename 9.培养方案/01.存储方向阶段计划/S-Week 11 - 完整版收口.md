---
title: S-Week 11 - 完整版收口
date: 2026-07-11
tags:
  - 高性能存储/存储方向阶段计划/总结
status: active
---

# S-Week 11 - 完整版收口

> [!goal] 本周目标
> 补上读路径最后一组实验（readahead / fadvise 定量化），把整个 linux-io-lab 的知识收敛成一张"全路径图 + 每层证据"的 `io_path_notes.md`，README 定稿，然后对照总纲阶段 1 验收标准复盘，更新 gap-analysis 和简历。本周结束，linux-io-lab 从 MVP 变成"有深度、有观测、有事故定位案例"的完整项目。

## 学习目标

1. **readahead 的收益和误伤怎么定量？** 窗口大小 × 负载模式的矩阵：顺序负载吃红利，随机负载被污染 cache、浪费带宽。
2. **fadvise 四个常用提示分别改变什么？** SEQUENTIAL 加大预读窗口、RANDOM 关预读、WILLNEED 主动预取、DONTNEED 提示丢弃。
3. **全路径图的标准讲法是什么？** 每层三元组：延迟量级 + 哪次实验测的它 + 哪个工具观测它。有这三元组才叫"自己的图"，不是背的图。
4. **项目局限怎么声明才加分？** 云主机单盘、无 RAID/多盘、文件系统只测了一种——主动写出来，比被面试官问出来强得多。

## 1. readahead / fadvise 定量实验（Day 1-2）

S-Week 4 做过 fadvise 的初版对比，这次用块层证据把故事补完整：

- `read_ahead_kb` 扫描：0 / 128（默认） / 1024，各跑冷顺序读与冷随机读。
- fadvise 对照：同一负载分别加 SEQUENTIAL / RANDOM / 不加提示。
- 每组同窗口采 blktrace + iostat：用平均请求大小（rareq-sz）和 M 事件数证明"预读批量化"——把 S-Week 1 的推断升级成 trace 证据。

必须回答：

- read_ahead_kb 调到 0，冷顺序读退化多少？请求大小变成什么形态？
- 调到 1024，随机负载多读了多少无用数据（rkB/s 与程序实际消费量之差）？
- POSIX_FADV_RANDOM 和 read_ahead_kb=0 效果一样吗？作用域差在哪？（前者 per-fd，后者全设备）

## 2. io_path_notes.md：一张图收全程（Day 3）

手画 read 全路径：用户态 → VFS → page cache → 块层（blk-mq） → NVMe 驱动 → 设备，每层标注三元组：

| 层 | 延迟量级（本机实测） | 测它的实验 | 观测它的工具 |
|---|---|---|---|
| syscall + VFS | 百 ns 级 | S-Week 1 热读 | strace / perf |
| page cache 命中 | ~1 µs | S-Week 1 | free、iostat 零流量 |
| 块层排队 | QD 相关 | S-Week 8 Q2D | blktrace / btt |
| 设备执行 | ~20-100 µs | S-Week 2 O_DIRECT、S-Week 8 D2C | biolatency、iostat |

写路径补一条分支：write → dirty page → writeback / fsync（S-Week 2 数据）。旁注两条路径变体：mmap 缺页（S-Week 4）、io_uring 异步提交（S-Week 5/10）。图存 SVG 进 `图片/9.培养方案/`，笔记里嵌入并配三分钟口述稿。

## 3. README 定稿与统一复现（Day 4）

- README：项目动机、结构、全部实验索引（每个实验一行：问题 → 方法 → 结论链接）、复现指引、**局限声明**、Agent 使用声明。
- `reproduce.sh` 全量重跑一遍，确认阶段 1 新增实验（blktrace、eBPF、消融、readahead）都进了复现脚本。
- 局限声明至少包含：云主机虚拟化层影响、单盘单文件系统、毛刺注入是人为构造（真实生产干扰更复杂）。

## 4. 阶段 1 复盘与求职材料更新（Day 5）

- 对照总纲阶段 1 验收标准逐条核对（见下节验收清单前三条）。
- `gap-analysis.md` 更新：把投递以来见过的 JD 要求逐条映射到"已覆盖周次 / 阶段 2 哪周补"，排出阶段 2 优先级。
- 简历 bullet 升级，加入阶段 1 增量：

```text
深入 Linux 块层与观测：使用 blktrace/btt 分解 I/O 延迟为排队与设备
执行两段，基于 eBPF（biolatency/biosnoop）定位并复现一次 p99 毛刺，
量化 io_uring SQPOLL/IOPOLL/registered buffers 的收益与 CPU 代价。
```

- 投递节奏保持：本周继续发出至少 5 个投递/内推，interview_qa.md 增量并入。

## 5. 推理保温（约 25%）

- 对账总纲阶段 1 验收：serving benchmark harness 达到 [[Week 5 - Serving Benchmark Harness]] 原定验收标准（S4 的输入）。未达标的项列出，排进阶段 2 的维护额度。

## 6. 面试保底（约 15%）

> 阶段 1 收口讲：文件系统补上，网络四周串成一条线。

- 算法（5-8 题）：CodeTop 综合。按频率补 [[CodeTop 高频题 Top300]] 前 100 中未刷的题，并限时模拟一场笔试（90 分钟 3 题，用 [[AI Infra 岗算法笔试保底清单]] 的组卷方式）。
- 八股（1 章）：文件系统 + 串讲收口。文件系统过 inode / 目录项 / journaling 概念（OSTEP Crash Consistency 章 + [[4.1 打开、读取、写入、关闭]]、[[4.2 重定向、同步]]），mmap 复面用 [[S-Week 4 - mmap 与读路径对比]] 的实验数据回答。网络收口：把三次握手 → 状态机 → 内核队列 → 拥塞控制 → epoll → io_uring 串成 10 分钟脱稿故事线（[[13.5 串讲]] 的方法扩到全网络栈）。
- 项目问答：阶段 1 全部素材并入 `interview_qa.md`，随机抽 10 题能脱稿答。

## 7. 本周产出文件

| 文件 | 内容 | 验收方式 |
|---|---|---|
| `results/readahead/*` | 扫描与 fadvise 对照原始数据 + trace | 不手动修改 |
| `docs/readahead_report.md` | 收益/误伤定量结论 | 有块层证据 |
| `io_path_notes.md` | 全路径图 + 每层三元组 + 口述稿 | 3 分钟脱稿讲完 |
| `README.md`（定稿） | 完整版项目门面 | 新读者 15 分钟建立全貌 |
| `gap-analysis.md`（更新） | JD 缺口 → 阶段 2 周次映射 | 阶段 2 优先级明确 |
| 阶段 1 复盘笔记 | 验收核对 + 配比复盘 | 未达项有去处 |

## 8. 验收标准（对照总纲阶段 1）

- [ ] linux-io-lab 完整版定稿，含 p99 毛刺定位案例（`p99_hunt.md`）。
- [ ] fio / iostat / biolatency 成为肌肉记忆：随机抽指标能当场解释。
- [ ] 推理保温：harness 达到 Week 5 原定验收标准。
- [ ] `io_path_notes.md` 完成，全路径每层有三元组。
- [ ] readahead / fadvise 实验有定量结论和 trace 证据。
- [ ] README 定稿，reproduce.sh 全量通过。
- [ ] 网络八股四讲串成完整故事线，能 10 分钟脱稿。
- [ ] gap-analysis 更新完毕，阶段 2 优先级排定。

## 面试问题

- 画出一次 read 的完整路径，并说出每层的延迟量级和你怎么测的。
- readahead 什么时候有害？你的数据里浪费了多少带宽？
- 这个项目最大的测量局限是什么？如果给你一台物理机你会先补哪个实验？
- 你的 p99 毛刺案例里，证据链是怎么闭环的？
- 三次握手到 epoll，把网络栈的故事讲 5 分钟。

## 关联知识

- [[S-Week 10 - io_uring 深入]]
- [[S-Week 12 - mini-kv-engine v0 与 bitcask 模型]]（阶段 2 开局）
- [[S-Week 11 - 前置知识 - 完整版收口]]
- [[S-Week 1 - Linux I O 路径专题 - VFS 到 NVMe 全路径]]（全路径图的底稿）
- [[S-Week 1 - Page Cache 与 readahead 专题]]（预读机制）
- [[S-Week 7 - 简历化与投递启动]]（简历与投递方法）
- [[AI Infra 存储与 GPU 数据路径系统工程师培养方案]]（阶段 1 验收标准）
