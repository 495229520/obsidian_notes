---
title: S-Week 21 - NVMe-oF RDMA 与 SPDK
date: 2026-07-12
tags:
  - 高性能存储/存储方向阶段计划/计划
status: active
---

# S-Week 21 - NVMe-oF RDMA 与 SPDK

> [!goal] 本周目标
> 把 S3 的最后两块拼图拼上：nvmet 换 RDMA transport（soft-RoCE，功能级）与 TCP 对比路径；PFC / ECN / DCQCN 无损网络概念过到口述级；SPDK perf 对比内核驱动，理解用户态轮询"快在哪、贵在哪"。配菜收口：JuiceFS / 3FS 两篇对比笔记落库。

## 学习目标

1. **NVMe-oF RDMA 比 TCP 少了哪些开销？** 内核协议栈的拷贝与中断、字节流定界；qpair 直接映射到 RDMA QP，数据面走单边 read/write——S-Week 20 学的所有东西在这里组装成产品形态。
2. **RoCE 为什么需要无损以太网？** RoCEv2 的重传机制弱（go-back-N），丢一个包代价极大——PFC 按优先级暂停上游发送来保不丢。
3. **PFC 的副作用是什么？** 队头阻塞、pause 帧级联传播（pause 风暴）、极端时死锁——所以要 ECN/DCQCN 在真丢包之前主动降速，PFC 只当最后防线。
4. **DCQCN 是什么？** 基于 ECN 标记的端到端速率控制（发送端降速/恢复的一套算法），参数敏感、调优复杂。口述级即可，不做调优实战。
5. **SPDK 为什么快、什么时候不值得？** 用户态驱动 + 轮询 + 无锁，消掉中断/系统调用/上下文切换；代价是核独占（CPU 100% 是工作状态不是故障）、生态隔离（文件系统与常规工具链不可用）。与 S-Week 10 的 IOPOLL、S-Week 20 的 poll CQ 是同一个哲学：用 CPU 换延迟。

## 1. NVMe-oF RDMA 功能级（Day 1-2）

- target 端 `modprobe nvmet-rdma`，port transport 换 rdma（跑在 soft-RoCE 上）；host 端 `nvme connect -t rdma`。
- fio 功能级跑通；与 TCP transport 的路径差异画一张对比图（哪层消失了、哪层换成了什么）。
- 不写性能结论——soft-RoCE 是软件模拟，数值无意义，边界声明沿用 S-Week 20。

## 2. 无损网络概念（Day 2-3）

- PFC / ECN / DCQCN 各解决什么、副作用是什么，整理进 `docs/lossless_network_notes.md` + 3 分钟口述稿。
- 素材：DCQCN 论文（SIGCOMM 2015）读摘要与设计章 + 厂商文档。
- 不做实验，声明认知边界："概念与取舍能讲，没调过真实交换机"。

## 3. SPDK 对比（Day 3-5）

- 环境：hugepages 配置 + `scripts/setup.sh` 把 NVMe 绑到 vfio/uio（云主机不一定支持 vfio，跑不通就记录并声明，不硬凑）。
- `hello_bdev` 跑通理解 bdev 抽象；`spdk_nvme_perf` 与内核 fio 同负载对比：4K randread，QD1 / QD32，记录延迟、IOPS、CPU 占用。
- 结论表算一列"每千 IOPS 的 CPU 代价"——SPDK 的收益必须和烧掉的核放在一起看（方法论同 S-Week 10 的 SQPOLL 分析）。

## 4. 配菜收口：分布式存储阅读线

- JuiceFS / 3FS 两篇对比笔记落库（总纲最低产出）：元数据路径对比、副本 vs EC、与传统 NAS 的差异。3FS 重点看它为什么用 RDMA + NVMe SSD——和本仓库路线的对标点。
- Ceph 只读架构文档（RADOS / CRUSH 概念），能画出写路径即止。
- 可选：issue reproduction \#2（JuiceFS / LMCache 任一，时间盒 1 天）。

## 5. 推理保温（约 25%）

- LMCache / Mooncake 架构速记 30 分钟：KV offload 的数据面在哪里用了 RDMA 和 NVMe——S4 叙事的证据收集。

## 6. 面试保底（约 15%）

> 阶段 2 八股按章清账第 10 讲。

- 算法（5-8 题）：[[CodeTop 高频题 Top300]] 前 150 冲刺第二轮。
- 八股（1 章）：Modern C++ 清账。过 [[15.3 atomic]]、[[15.10 chrono]]、[[15.11 default、delete、初始化列表]]、[[15.12 回调函数]]（或对照章节索引换成自己的弱章）。
- 项目问答：10 个 Q&A（本周素材：transport 对比、PFC 副作用、SPDK 取舍）。

## 7. 本周产出文件

| 文件 | 内容 | 验收方式 |
|---|---|---|
| `results/nvmeof_rdma/*` | RDMA transport 功能级记录 | 标签"功能级"写死 |
| `docs/lossless_network_notes.md` | PFC/ECN/DCQCN + 口述稿 | 3 分钟脱稿过关 |
| `results/spdk/*` + `docs/spdk_vs_kernel.md` | 对比数据 + CPU 代价列 | 不手动修改 |
| JuiceFS / 3FS 对比笔记 × 2 | 落在本仓库 | 元数据路径能画图 |

## 8. 验收标准

- [ ] NVMe-oF RDMA 功能级跑通，与 TCP 的路径差异能画图讲清。
- [ ] PFC / ECN / DCQCN 三分钟口述过关，副作用必须讲到。
- [ ] SPDK vs 内核对比有数据表，"每千 IOPS 的 CPU 代价"算出来了。
- [ ] JuiceFS / 3FS 对比笔记两篇落库。
- [ ] 报告面试题 5 / 6 / 7（RDMA vs TCP、PFC/ECN、transport 代价）能答。
- [ ] 推理保温和面试保底完成本周额度。

## 面试问题

- NVMe-oF 三种 transport 的代价对比？
- PFC 解决什么、引入什么新问题？DCQCN 和它是什么关系？
- SPDK 快在哪？什么场景下不值得用？
- 你的 RDMA 数据真实吗？（标准答案 = soft-RoCE 边界声明）
- JuiceFS 和 3FS 的元数据路径差在哪？

## 关联知识

- [[S-Week 20 - RDMA verbs 入门]]
- [[S-Week 22 - nvme-of-lab 收口与阶段 2 复盘]]
- [[S-Week 21 - 前置知识 - NVMe-oF RDMA 与 SPDK]]
- [[RoCE 拥塞控制专题 - PFC ECN DCQCN]]（口述稿底稿）
- [[NVMe-oF 专题 - TCP 与 RDMA transport 取舍]]
- [[副本与 EC 取舍专题]]（配菜阅读线）
- [[S-Week 10 - io_uring 深入]]（IOPOLL：同一个"CPU 换延迟"哲学）
- [[13.6 Reactor模式与EventLoop]]（事件驱动 vs 轮询的对照背景）
- SPDK 文档；DCQCN 论文（SIGCOMM 2015）；JuiceFS / 3FS 架构文档
