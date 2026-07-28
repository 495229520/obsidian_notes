---
title: S-Week 18 - NVMe 命令模型与本地基线
date: 2026-07-12
tags:
  - 高性能存储
  - 存储方向阶段计划
  - 计划
status: active
---

# S-Week 18 - NVMe 命令模型与本地基线

> [!goal] 本周目标
> 开启 S3 nvme-of-lab：用 nvme-cli 把 NVMe 命令模型（admin / IO queue pair、SQ/CQ、doorbell）摸到能画图，理解 SSD 内部机制（FTL / GC / 写放大 / OP），并复用 S1 方法论打一版本地 fio 基线——它是后面所有 NVMe-oF 延迟分解的"设备延迟"分母。

## 学习目标

1. **NVMe 为什么比 SATA/AHCI 快？** 多且深的队列（最多 64K 个队列 × 64K 深度 vs AHCI 单队列 32 深度）、精简命令集、per-CPU 队列免锁、MSI-X 中断——协议为并行闪存而生。
2. **一次读命令的生命周期？** host 填 SQE → 写 SQ doorbell → 控制器取命令 → DMA 数据到主机内存 → 写 CQE（带 phase bit）→ MSI-X 中断 → host 消费 CQ → 写 CQ doorbell。
3. **phase bit 是干什么的？** CQ 是环形数组，host 靠 phase bit 翻转判断哪些 CQE 是新的，避免读到旧轮次的残留项。
4. **admin 队列和 IO 队列怎么分工？** admin：identify / set-features / 创建 IO 队列；IO：读写数据。qpair 概念记牢——S-Week 20 的 RDMA QP、S-Week 5 的 io_uring SQ/CQ 会拼成同一张对照表。
5. **为什么空盘性能是假的？** OP 未耗尽、GC 未触发；稳态测试要预写满盘再持续随机写到性能平台（fio 预处理）。写放大定义：

$$
WAF = \frac{W_{nand}}{W_{host}}
$$

## 1. nvme-cli 实操（Day 1-2）

```bash
sudo apt install -y nvme-cli
sudo nvme list
sudo nvme id-ctrl /dev/nvme0 | head -40   # 队列数上限、MDTS
sudo nvme id-ns /dev/nvme0n1              # LBA 格式、容量
sudo nvme smart-log /dev/nvme0            # 温度、data_units_written、磨损
sudo nvme get-feature /dev/nvme0 -f 0x07  # 队列数量 feature
```

- 逐字段记笔记：哪些字段说明了队列模型、哪些说明了介质状态。
- 画一次读命令的生命周期图（SVG 存 `图片/9.培养方案/`，嵌入笔记）。
- 云主机的 NVMe 多是虚拟化设备：记录哪些字段可信、哪些失真（进 `env.md`，沿用三类实验声明的纪律）。

## 2. SSD 内部机制（Day 3）

- FTL（逻辑块 → 物理页映射）、NAND 页写/块擦不对称 → GC 搬运 → 写放大、over-provisioning、wear leveling，整理进 `docs/nvme_notes.md`。
- 用 smart-log 的 data_units_written 与主机侧写入量对账估 WAF；云盘拿不到真实值就写边界声明，不硬凑。

## 3. 本地 fio 基线（Day 4-5）

- 复用 S-Week 3 的 benchmark matrix 纪律：4K randread / randwrite / 顺序读，QD 1-32 扫描，每组 3 次。
- 随机写：空盘 vs 预处理后（能测则测；云盘虚拟化下 GC 行为不可见就声明边界）。
- 产出 `results/nvme_baseline/`——这组数字是 S-Week 19 延迟分解的分母，格式必须和 S1 对齐，方便直接引用。

## 4. 推理保温（约 25%）

- 维护态：KV cache 口算保手感；harness 冒烟一次。

## 5. 面试保底（约 15%）

> 阶段 2 八股按章清账第 7 讲。

- 算法（5-8 题）：最短路。参考 [[5.2.28 最短路径算法专题]]，配 [[CodeTop 高频题 Top300]] 图类高频题。
- 八股（1 章）：压测与线上排障。过 [[15.2 压测、指标与线上排障]]、[[8.14 高性能发送与线上排障]]。验收：给出一个"线上 p99 涨了"的排查框架——正好复用 [[S-Week 9 - eBPF 观测]] 的 runbook 方法。
- 项目问答：10 个 Q&A（本周素材：NVMe 队列模型、phase bit、写放大与稳态）。

## 6. 本周产出文件

| 文件 | 内容 | 验收方式 |
|---|---|---|
| `docs/nvme_notes.md` | 命令模型 + SSD 内部机制 | 生命周期图能脱稿画 |
| 生命周期 SVG | doorbell → DMA → CQE → 中断 | 嵌入笔记并有口述稿 |
| `results/nvme_baseline/*` | QD 扫描 + 读写 + 顺序随机 | 格式与 S1 对齐 |
| `env.md`（更新） | 云主机 NVMe 虚拟化边界 | 可信/失真字段分列 |

## 7. 验收标准

- [ ] 能脱稿画一次 NVMe 读命令生命周期并解释每一步。
- [ ] nvme-cli 关键输出能逐字段解释。
- [ ] FTL / GC / 写放大能讲清，WAF 有估算方法或边界声明。
- [ ] 本地基线数据齐全，QD 扫描曲线与 S1 的 io_uring 数据能互相印证。
- [ ] 空盘 vs 稳态差异有数据或有明确边界声明。
- [ ] 推理保温和面试保底完成本周额度。

## 面试问题

- NVMe 比 SATA 快在哪？从协议层面说三点。
- phase bit 是干什么的？没有它会怎样？
- 写放大是怎么产生的？怎么降低？
- 为什么 SSD 测试要预处理？你的数据是空盘还是稳态？
- 你的云主机 NVMe 数据哪些可信、哪些不可信？

## 关联知识

- [[S-Week 17 - benchmark 与 design_note 收口]]
- [[S-Week 19 - NVMe-oF TCP 与延迟分解]]
- [[S-Week 18 - 前置知识 - NVMe 命令模型与本地基线]]
- [[NVMe 命令模型与 SSD 内部专题]]
- [[存储面试问题清单 - NVMe 与 NVMe-oF]]（本周起沉淀）
- [[S-Week 3 - fio 对照与 Benchmark Matrix]]（基线方法论）
- [[S-Week 8 - 块层与 blktrace]]（块层到驱动的衔接）
- nvme-cli 文档；NVMe Base Specification 概览章
