---
title: checkpoint I O 专题 - burst write 隔离
date: 2026-07-12
tags:
  - 高性能存储
  - 存储方向专题清单
roadmap_week: 阶段 3（S4 可选实验；S-Week 9 干扰注入方法论的 AI 场景版）
sort_order: "06.20"
status: active
---

# checkpoint I O 专题 - burst write 隔离

> [!info] 所属路线
> - 培养方案阶段：阶段 3 S4 的可选实验（时间不够可只做口述级）——模拟大 checkpoint 保存/加载，测量顺序大块写对前台负载的干扰
> - 排序：06.20
> - 用途：把 [[S-Week 9 - eBPF 观测]] 的"writeback 突发干扰前台 p99"实验重新讲一遍——那次实验就是 checkpoint 干扰的显微镜版本，本篇把它放大到 AI 训练的真实场景。

> [!goal] 目标
> 讲清三件事：checkpoint 为什么是暴力负载（体量口算 + burst 形态）；它怎么伤前台、怎么定位（S-Week 9 方法直接复用）；工程上的四层缓解（异步化、O_DIRECT、限速、隔离）。

---

## 1. checkpoint 是什么量级的负载

训练态 checkpoint 不只存权重——混合精度 + Adam 优化器状态下，每参数的完整训练状态约 12-16 字节（fp16 权重 + fp32 主权重 + 一阶/二阶动量），量级口算：

$$
S_{ckpt} \approx N_{params} \times 16
$$

8B 模型 ≈ 百 GB 级，70B ≈ TB 级。按训练组惯例每小时级存一次、故障恢复全量读一次：**写侧是周期性的顺序大块 burst，读侧是重启时全集群同时拉取的读风暴**。它不是背景噪声，是能把共享存储打穿的一等负载——这就是"存储被推到 AI 关键路径"论断在训练侧的那一半（推理侧是 KV cache）。

## 2. 它怎么伤前台：S-Week 9 的放大版

复盘 [[S-Week 9 - eBPF 观测]] 的干扰实验——当时注入的"大量 buffered 写 + 周期 sync"就是 checkpoint 的微缩模型，伤害链条一模一样：

1. checkpoint 进程 buffered 写 → 脏页海量堆积 → 触到 `dirty_ratio` 阈值 → **全系统 writeback 风暴**，前台的读请求在块层排队（Q2D 暴涨，S-Week 8 的分解直接适用）。
2. 大块顺序写抢占设备带宽与队列深度 → 前台 4K 随机读的 p99 起飞（biolatency 直方图长出第二个峰）。
3. 共享存储（NVMe-oF / 分布式 FS）场景下，伤害从单机扩散到整个存储集群——别的租户的训练作业跟着抖。

定位工具链零新增：biosnoop 抓肇事者、biolatency 看形态、blktrace 分解 Q2D/D2C——**S1 的观测能力在 AI 场景原样变现**，这是面试里"你的旧项目怎么支撑新场景"的现成答案。

## 3. 四层缓解：从应用到设备

| 层 | 手段 | 原理 | 代价 |
|---|---|---|---|
| 应用层 | **异步 checkpoint**：先 D2H 快照进 pinned 主机内存，训练立即继续，后台线程慢慢刷盘 | GPU 停顿从"写盘时间"缩到"D2H 时间"（PCIe 几十 GB/s，秒级） | 多占一份主机内存；崩溃窗口内最后一份 ckpt 可能不完整——要配原子落盘 |
| 写路径 | O_DIRECT + 大块对齐写 | 绕过 page cache，不制造脏页海啸——writeback 风暴从根上消失 | 自己管对齐与缓冲（S-Week 2 的纪律） |
| 调度层 | cgroup v2 `io.max` 限速 / ionice | 把 checkpoint 流量压到前台可容忍的水位 | 写得更久，崩溃恢复点变稀 |
| 架构层 | 独立盘/独立存储池 + 分片并行写 + 读侧 P2P 分发 | 物理隔离故障域与带宽域；恢复时避免全集群拉同一份（广播树/对等分发） | 成本与复杂度 |

原子性细节（和 mini-kv-engine 的语义完全同源）：checkpoint 落盘必须"写临时文件 → fsync → rename → fsync 目录"，否则崩溃恢复时读到半个 checkpoint——[[存储引擎专题 - WAL 与 crash consistency]] 的原子切换模式第三次出现。

## 4. 实验骨架（S4 可选，做不了就口述级）

- 前台：io_uring 4K 随机读 QD4 持续记录 p99（S-Week 9 的程序原样复用）。
- 注入：模拟 checkpoint——数十 GB 顺序写，四种姿势各一轮：buffered / buffered+限速 / O_DIRECT / O_DIRECT+限速。
- 观测：前台 p99 时间线 + biosnoop 肇事记录 + 脏页水位（`/proc/meminfo` 的 Dirty）。
- 预期：buffered 组复现 S-Week 9 的 p99 起飞；O_DIRECT 组风暴消失但带宽争抢仍在；限速组换来"写得久但前台稳"——四组数据合成一张"缓解手段收益表"。

## 5. 面试口述模板

```text
checkpoint 是训练侧的暴力负载：混合精度加 Adam 状态每参数约 16 字节，
70B 就是 TB 级，每小时级一次 burst 顺序写，恢复时还有全集群读风暴。
它伤前台的链条我在自己的实验里完整复现过：buffered 写堆脏页、触发
writeback 风暴、前台随机读在块层排队 p99 起飞——biosnoop 抓肇事者、
blktrace 分解排队和设备两段，就是我 p99 毛刺定位那套。缓解分四层：
应用层异步 checkpoint，先快照进 pinned 内存训练立即继续、后台刷盘；
写路径 O_DIRECT 大块写，从根上不制造脏页；调度层 cgroup io.max 限速；
架构层独立存储池加分片并行、读侧对等分发。落盘还要原子：临时文件加
fsync 加 rename 加目录 fsync——和我 KV 引擎的切换语义同一个模式。
```

追问预案：

- "异步 checkpoint 的崩溃语义？" → 快照一致性由 D2H 时刻保证（对训练状态做一致性切面），落盘原子性由 rename 模式保证；崩溃丢的是"最近一次未完成的"，恢复用上一份——和 everysec 的丢失窗口是同构的权衡。
- "为什么不干脆全用 O_DIRECT？" → 要自己做对齐、缓冲和并发流水线，工程量换稳定性；很多框架先走 buffered+限速的低垂果实。
- "读风暴怎么解？" → 避免 N 节点拉同一份：分片存储 + 节点间 P2P/广播树分发，或存储侧多副本摊读——本质是把"一对多"改成"多对多"。
- "这和你的 S-Week 9 实验什么关系？" → 同一个机制的两个尺度：那次是人为注入的显微镜版，这里是生产尺度——工具链和定位方法完全复用。

## 关联知识

- [[S-Week 9 - eBPF 观测]]（干扰注入与定位方法的来源）
- [[S-Week 8 - 块层与 blktrace]]（Q2D/D2C 分解）
- [[S-Week 2 - O_DIRECT + 持久化语义]]（O_DIRECT 写与脏页机制）
- [[存储引擎专题 - WAL 与 crash consistency]]（原子落盘的同源模式）
- [[GPUDirect Storage 专题 - cuFile 与 bounce buffer]]（checkpoint 加载侧的直读选项）
- [[AI Infra 存储与 GPU 数据路径系统工程师培养方案]]（S4 实验定义）
- [[00.存储方向专题清单索引]]
