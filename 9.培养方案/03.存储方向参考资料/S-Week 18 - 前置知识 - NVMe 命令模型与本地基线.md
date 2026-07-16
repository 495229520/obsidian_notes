---
title: S-Week 18 - 前置知识 - NVMe 命令模型与本地基线
date: 2026-07-12
tags:
  - infra
  - 存储
  - 参考资料
aliases:
  - 存储 Week 18 前置知识
  - NVMe 前置知识
status: active
---

# S-Week 18 - 前置知识 - NVMe 命令模型与本地基线

## 索引

- [[#0. 先建立直觉：盘的另一头是一台并行计算机]]
- [[#1. NVMe 为什么替代 AHCI：协议层面的三板斧]]
- [[#2. 一条读命令的完整生命周期]]
- [[#3. admin 队列与 IO 队列：控制面和数据面分开]]
- [[#4. nvme-cli 实操地图：每个命令看什么]]
- [[#5. SSD 内部：FTL、GC、写放大、OP]]
- [[#6. 稳态 vs 空盘：为什么要预处理]]
- [[#7. 本地基线：作为 S-Week 19 分母的三条纪律]]
- [[#8. 常见错误]]
- [[#9. 学习检查清单]]
- [[#10. 关键要点总结]]
- [[#关联知识]]
- [[#参考]]

## 阅读说明

这篇是 [[S-Week 18 - NVMe 命令模型与本地基线]] 的总前置知识：动 nvme-cli 之前通读 1-3 节建立命令模型，跑 smart-log 前对照 4 节逐字段，设计基线实验前读 5-7 节。深挖版见 [[NVMe 命令模型与 SSD 内部专题]]。

---

> S-Week 8 你从块层往下看到了 NVMe 驱动的门口；本周推门进去。视角要换一次：SSD 不是"一块会存数据的板子"，而是**一台有自己 CPU、DRAM 和几十条 NAND 通道的并行计算机**——NVMe 协议的全部设计、SSD 性能的全部怪脾气（稳态掉速、GC 毛刺、写放大），都从这个事实出发。

---

## 0. 先建立直觉：盘的另一头是一台并行计算机

一块 NVMe SSD 内部：主控（多核 ARM 级别的处理器）+ DRAM（放 FTL 映射表）+ 多通道 NAND 闪存（每通道可独立并行读写）。它能同时服务几十个请求——但 SATA/AHCI 协议只给它**一条深度 32 的命令队列**，就像给一座八车道大桥只开一个收费亭。

NVMe 是为这台并行计算机重新设计的协议：多队列、深队列、低开销，让主机把并行度真正喂进设备。S-Week 5 你实测过"QD1 到 QD32 IOPS 翻了一个数量级"——本周从协议层面解释为什么这件事是可能的。

> [!important] 第一性原理
> NVMe 的设计目标只有一句话：**主机侧的提交/完成路径不能成为闪存并行度的瓶颈**。理解每个机制（多队列、doorbell、MSI-X、phase bit）时都问一句"它消掉了哪个瓶颈"，就不会陷进名词海。

## 1. NVMe 为什么替代 AHCI：协议层面的三板斧

| 维度 | AHCI（为机械盘设计） | NVMe（为闪存设计） |
|---|---|---|
| 队列 | 1 条 × 32 深 | 最多 64K 条 × 每条最多 64K 深 |
| 队列与 CPU | 全局一条，多核抢锁 | per-CPU 各一条，天然无锁 |
| 中断 | 单一中断 | MSI-X 多向量，中断可以定向到提交它的核 |
| 每命令协议开销 | 多次不可缓存的寄存器读写 | 提交/完成各一次 doorbell 写（MMIO 写，便宜） |
| 命令集 | 背着 ATA 历史包袱 | 精简：必选管理命令 + 读/写/flush 等少数 IO 命令 |

三板斧总结：**多且深的队列**（喂饱 NAND 并行度）、**per-CPU 队列 + 定向中断**（多核扩展性）、**极简命令路径**（单命令固定成本低）。S-Week 8 学的 blk-mq（软件多队列）就是内核为对接 NVMe 硬件多队列而生的——两层在这里合上了。

## 2. 一条读命令的完整生命周期

八步，每步问"谁在动、动的是哪块内存"：

```text
1. host 在 SQ（提交队列，主机内存里的环形数组）写入一条 64 B 的 SQE
   —— 命令码、LBA、长度、数据 buffer 的物理地址（PRP/SGL）
2. host 写 SQ doorbell 寄存器（MMIO 写）：告诉控制器"队尾到 N 了"
3. 控制器 DMA 取走 SQE
4. 控制器执行：查 FTL → 读 NAND → 数据 DMA 直达 host buffer
5. 控制器往 CQ（完成队列，也在主机内存）写一条 16 B 的 CQE，
   其中带 phase bit
6. 控制器发 MSI-X 中断（或 host 在轮询，见 S-Week 10 的 IOPOLL）
7. host 消费 CQE，按 phase bit 判断哪些是新条目
8. host 写 CQ doorbell：告诉控制器"我消费到 M 了"，槽位可复用
```

两个必考细节：

- **doorbell**：设备暴露的寄存器，主机写它来通告队列头尾指针的移动。它是"主机内存里的队列"和"设备"之间唯一的同步点——提交路径上除这一次 MMIO 写外全是普通内存操作。
- **phase bit**：CQ 是环形复用的，host 怎么知道一个槽位是"这一圈的新 CQE"还是"上一圈的旧数据"？控制器每绕一圈翻转一次 phase bit，host 比对期望相位即可——不需要额外的 valid 标志清除动作。

看完这八步再回看 [[io_uring 异步 IO 专题]]：io_uring 的 SQ/CQ 就是把同一套"共享内存环 + 通知"思想搬到了 syscall 层。S-Week 20 的 RDMA QP/CQ 是第三次遇见它——三队列对照表在那周收口。

## 3. admin 队列与 IO 队列：控制面和数据面分开

- **admin qpair（固定一对）**：identify（问设备"你是谁"）、创建/删除 IO 队列、get/set features、拿日志页。控制器初始化时最先建立。
- **IO qpair（数量协商）**：只跑读/写/flush 等数据命令。驱动启动时通过 admin 命令 set features（Number of Queues）协商出每核一对。
- qpair = SQ + CQ 的配对（多个 SQ 也可共享一个 CQ）。**记住 qpair 这个词**：NVMe-oF 里它被映射成网络连接（S-Week 19）、NVMe/RDMA 里映射成 RDMA QP（S-Week 21）——它是贯穿 S3 的主线概念。

## 4. nvme-cli 实操地图：每个命令看什么

| 命令 | 重点字段 | 它告诉你什么 |
|---|---|---|
| `nvme list` | node、model、namespace | 有哪些控制器/命名空间 |
| `nvme id-ctrl /dev/nvme0` | `mdts`（最大传输大小，2 的幂 × 页大小）、`sqes/cqes`、`nn`（命名空间数）、`oncs` | 协议能力边界；mdts 决定单命令最大 I/O，超过的请求会被驱动拆分 |
| `nvme id-ns /dev/nvme0n1` | `nsze/ncap`、`lbaf`（LBA 格式表：512 vs 4096）、`flbas`（当前用哪个格式） | 容量与扇区格式；4Kn 盘对齐语义在这里确认 |
| `nvme smart-log /dev/nvme0` | `data_units_written/read`（单位 = 1000 × 512 B）、`percentage_used`、`media_errors`、温度 | 介质磨损与主机写入量——估算 WAF 的原料 |
| `nvme get-feature /dev/nvme0 -f 0x07` | Number of Queues | 协商到的 IO 队列数 |

云主机纪律：虚拟化 NVMe（如各家云的 virtio/NVMe 模拟层）里 model 是虚拟设备名、smart-log 可能全零或无意义、mdts/队列数是虚拟层的值。**逐字段标注"可信/失真"写进 env.md**——这是三类实验声明纪律在设备信息层的延伸。

## 5. SSD 内部：FTL、GC、写放大、OP

NAND 的物理约束：**读写以页为单位（4-16 KiB），擦除以块为单位（数 MiB，几百个页）**，且页不能原地改写——必须先擦后写，擦除还有寿命次数限制。

FTL（Flash Translation Layer）用一张"逻辑块 → 物理页"映射表把这个约束藏起来：

- 主机"覆盖写 LBA 100" → FTL 把新数据写进**别处的空闲页**，改映射，旧页标记无效——**SSD 内部天然就是 append-only**，和你的 bitcask 是同构的！
- 无效页积累 → 空闲块不够 → **GC**：挑一个块，把里面还有效的页搬走，整块擦除。搬运产生额外的 NAND 写。
- 写放大因子：

$$
WAF = \frac{W_{nand}}{W_{host}}
$$

- **OP（over-provisioning）**：物理容量预留一部分不暴露给主机（企业盘常见 7%-28%），给 GC 留腾挪空间——OP 越大，GC 效率越高，稳态 WAF 越低。
- wear leveling：FTL 顺手把写入摊匀到所有块，避免热点块先写穿。

估算 WAF：smart-log 的 `data_units_written` 是**主机写入量**；部分盘通过厂商扩展日志暴露 NAND 写入量，拿不到就只能声明"WAF 不可测"。bitcask 的 merge、LSM 的 compaction、FTL 的 GC——同一个"append + 垃圾回收"模式在三个层次上重复出现，面试里把这条线串出来非常加分。

## 6. 稳态 vs 空盘：为什么要预处理

空盘（FOB, fresh out of box）状态下：空闲块充足、GC 不需要干活、随机写性能虚高——**这是厂商标称值最爱的状态，也是最不真实的状态**。

持续随机写之后：空闲块耗尽 → 每次主机写都可能触发 GC 搬运 → 随机写 IOPS 掉到稳态值（可能只有空盘的几分之一），且出现周期性毛刺（GC 批量干活）。

预处理（preconditioning）的标准做法（SNIA PTS 思路的简化版）：

1. 顺序写满盘 1-2 遍（让所有逻辑块都有映射）；
2. 用目标块大小持续随机写，直到性能落入稳定窗口（例如滑动窗口内波动 < 10%）；
3. 之后才开始正式测量。

云主机边界：虚拟盘背后是分布式存储池，"稳态"概念不成立——能测则测，测不出形态就写边界声明，不硬凑。

## 7. 本地基线：作为 S-Week 19 分母的三条纪律

本周 fio 基线的特殊使命：S-Week 19 要做"远程延迟 = RTT + 软件 + **设备**"的减法分账，设备项就是本周的数字。三条纪律：

1. **参数锁定**：ioengine/bs/QD/direct 的组合写进脚本，S-Week 19 跨网络测试必须用**完全相同**的参数——分母和分子不同口径，减法就是错的。
2. **冷热分开**：O_DIRECT 测设备（这是分母要的），buffered 热读只做对照。
3. **格式对齐 S-Week 3**：results 目录结构、CSV 列、每组 3 次的惯例全部沿用——S3 的报告要能直接引用 S1 的表格模板。

## 8. 常见错误

- **把 doorbell 理解成"数据通道"**：数据走 DMA 直达主机内存，doorbell 只是队列指针的通告——一次 MMIO 写，不搬数据。
- **忘了 phase bit 的存在**：以为 CQ 靠"清零已消费条目"工作；实际靠相位翻转，这是免清理设计的关键。
- **把 mdts 当成盘的最大 I/O 能力**：它是单命令上限，驱动会自动拆分大请求——iostat 里看到的请求大小上限往往是它的体现。
- **smart-log 的 data_units 直接当字节数**：单位是 1000 × 512 B，差三个数量级。
- **空盘随机写数据直接进报告**：不做预处理声明，数字虚高且不可复现。
- **基线和 S-Week 19 的 fio 参数不一致**：延迟分解的减法失效，整周白测。
- **在云主机上强行解读 GC 行为**：虚拟盘看不到真实 FTL，硬凑解释不如老实声明。

## 9. 学习检查清单

- [ ] 能按八步脱稿讲一条读命令的生命周期，说清 doorbell 和 phase bit 各解决什么。
- [ ] 能从三个维度对比 NVMe 和 AHCI（队列、中断、每命令开销）。
- [ ] 能解释 admin/IO qpair 的分工，并记住 qpair 在 S3 后续两周的映射关系。
- [ ] nvme-cli 五个命令的重点字段能逐个解释，云主机失真字段已标注。
- [ ] 能画 FTL 的"覆盖写 → 异地写 + 改映射 → GC"链条，并写出 WAF 定义。
- [ ] 能说出预处理三步和它背后的空盘/稳态机制。
- [ ] 本地基线参数已锁定并脚本化，格式与 S-Week 3 对齐。

## 10. 关键要点总结

- SSD 是并行计算机，NVMe 是为它设计的低开销多队列协议——一切机制都为"别挡住 NAND 的并行度"服务。
- 提交/完成路径 = 共享内存环 + doorbell + phase bit；与 io_uring、RDMA 的队列对同构。
- FTL 让 SSD 内部天然 append-only：GC 与 bitcask merge / LSM compaction 是同一模式的三次重现。
- 空盘性能是假的，稳态才是真的；预处理是随机写测试的前置义务。
- 本周基线是 S-Week 19 延迟分解的分母：参数锁定、口径一致高于一切。

## 关联知识

- [[S-Week 18 - NVMe 命令模型与本地基线]]（本篇服务的周计划）
- [[NVMe 命令模型与 SSD 内部专题]]（深挖版与面试口述）
- [[S-Week 8 - 块层与 blktrace]]（blk-mq 与硬件多队列的衔接）
- [[io_uring 异步 IO 专题]]（SQ/CQ 同构的第二次出现）
- [[S-Week 3 - fio 对照与 Benchmark Matrix]]（基线方法论）
- [[存储面试问题清单 - NVMe 与 NVMe-oF]]（本周起逐周沉淀）

## 参考

- NVMe Base Specification（读 1-4 章概览：队列模型、命令集、doorbell 语义）
- nvme-cli 文档与 `man nvme-id-ctrl` 等子命令手册
- Systems Performance（Brendan Gregg）ch 9 Disks（SSD 内部机制一节）
- SNIA Solid State Storage Performance Test Specification（预处理与稳态的正规定义，泛读）
