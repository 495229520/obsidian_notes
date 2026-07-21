---
title: 存储面试问题清单 - NVMe 与 NVMe-oF
date: 2026-07-12
tags:
  - 高性能存储/存储方向专题清单/清单
roadmap_week: 求职全程（S-Week 18-22 逐周沉淀，S-Week 22 收口）
sort_order: "99.10"
status: active
---

# 存储面试问题清单 - NVMe 与 NVMe-oF

> [!info] 所属路线
> - 培养方案第十四节 NVMe / SSD / NVMe-oF 板块的沉淀版：每题给"可脱稿回答"的答案要点 + 自己的实验证据 + 追问预案
> - 排序：99.10
> - 用法：S-Week 18 起随周补充，S-Week 22 收口后面试前扫一遍。答案控制在 5 句内，**第一句永远是结论**；每句能指到 nvme-of-lab 的数据或声明。

---

## Q1 NVMe 比 SATA/AHCI 快在哪？从协议层面说三点。

**答案要点**：一是队列：AHCI 单队列 32 深，NVMe 最多 64K 队列每条 64K 深，per-CPU qpair 免锁；二是每命令开销：提交/完成各一次 doorbell 写，没有 AHCI 那一串不可缓存寄存器访问；三是中断：MSI-X 多向量定向到提交核。本质是 SSD 内部是多通道并行计算机，AHCI 的单队列是漏斗，NVMe 把管道拆成与核数和 NAND 通道匹配的并行结构。

**证据**：[[S-Week 18 - NVMe 命令模型与本地基线]] 的 QD 扫描（并行度喂进去 IOPS 才起来）；机制底稿 [[NVMe 命令模型与 SSD 内部专题]]。

**追问预案**：
- "blk-mq 和硬件多队列什么关系？" → 内核软件队列到硬件队列的映射层，S-Week 8 的 trace 能看到。
- "phase bit 干什么的？" → CQ 环形复用靠相位翻转区分新旧，免清理设计。

## Q2 画一条 NVMe 读命令的生命周期。

**答案要点**：host 写 64B SQE 进提交环 → 敲 SQ doorbell → 控制器 DMA 取命令 → 查 FTL 读 NAND、数据 DMA 直达 host buffer → 写 16B CQE（带 phase bit）进完成环 → MSI-X 中断或轮询 → host 消费后写 CQ doorbell 归还槽位。关键点：数据不过 doorbell，doorbell 只通告队列指针；除两次 MMIO 写外全是普通内存操作。

**证据**：[[S-Week 18 - 前置知识 - NVMe 命令模型与本地基线]] 八步版 + 自画的生命周期图。

**追问预案**：
- "和 io_uring、RDMA 的队列什么关系？" → 同构：提交环 + 完成环 + doorbell 是高性能 I/O 通用形态，我有三系统对照表。

## Q3 写放大是怎么产生的？怎么估、怎么降？

**答案要点**：NAND 页写块擦、不能原地改——FTL 异地更新，无效页积累后 GC 搬运有效页腾整块，搬运就是额外 NAND 写，WAF = NAND 写 ÷ 主机写。估算用 smart-log 的主机写入量对照厂商日志的 NAND 写入量（拿不到就声明不可测）。降低：加 OP、写模式顺序化/大块化、TRIM 告知可回收、ZNS 类接口把回收权交还主机。GC 与 bitcask merge、LSM compaction 是同一模式在三层的重现。

**证据**：[[S-Week 18 - NVMe 命令模型与本地基线]]；引擎侧对照 [[存储引擎专题 - bitcask 与哈希索引]]。

**追问预案**：
- "为什么随机写测试要预处理？" → 空盘 GC 不干活数字虚高；顺序写满 + 随机写到平台才是稳态。

## Q4 NVMe-oF 解决什么问题？三种 transport 的代价？

**答案要点**：盘池化与存算分离——同一套 NVMe 命令，qpair 从 PCIe 映射成网络连接，上层无感。TCP：零硬件门槛，代价是两端内核栈按字节付费、PDU 定界、单连接队头阻塞、大写 R2T 多一个往返；RDMA：命令走双边、数据单边 DMA 直达，四样开销全省，代价是无损以太网的运维（PFC/DCQCN）；FC：企业 SAN 存量。云上跨机房默认 TCP，机柜内 AI 存储上 RDMA。

**证据**：[[S-Week 19 - NVMe-oF TCP 与延迟分解]] 实测 + [[S-Week 21 - NVMe-oF RDMA 与 SPDK]] 功能级对照；底稿 [[NVMe-oF 专题 - TCP 与 RDMA transport 取舍]]。

**追问预案**：
- "NVMe/TCP 一次 4K 读几个往返？" → 约 1 RTT；大写走 R2T 约 2 RTT——大块写延迟不成比例变差的协议根源。

## Q5 你的远程盘延迟分解怎么闭环？

**答案要点**：总延迟 ≈ 网络 + 软件 + 设备三段：网络用 ping RTT 中位数、设备用同口径本地基线（bs/QD/engine 锁定）、软件是余项（两端栈合计，拆分要两侧打点，声明 TODO）。闭环靠加总对账（误差超 20% 归因）加两侧同窗口观测：target 侧 biolatency 显示设备延迟没变，多出来的就在路上和栈里。QD 高了吞吐差距收窄——"延迟差、吞吐平"是网络存储标准形态。

**证据**：[[S-Week 19 - NVMe-oF TCP 与延迟分解]] 分解表；[[S-Week 22 - nvme-of-lab 收口与阶段 2 复盘]] 四层图。

**追问预案**：
- "为什么不跨机对时间戳？" → 时钟不同步；分解全部单侧计时。

## Q6 RDMA 和 TCP 的本质差异？零拷贝零在哪一步？

**答案要点**：把 OS 请出数据路径：kernel bypass（数据面零 syscall）、zero-copy（网卡 DMA 直达注册内存——省的是两端"用户 buffer ↔ 内核 buffer"那两次拷贝）、传输层卸载到网卡（重传保序不烧 CPU）。代价是自己补 OS 的活：MR 补地址翻译与钉页、状态机加带外交换补连接协商、rkey 补权限。CPU 从按字节付费变成只下命令收完成。

**证据**：[[S-Week 20 - RDMA verbs 入门]] 手写 pingpong；底稿 [[RDMA verbs 专题 - QP WQE CQ 状态机]]。

**追问预案**：
- "MR 和 io_uring registered buffer 什么关系？" → 同一逻辑：pin + 预翻译换每次操作免翻译，注册都很贵所以都池化复用。

## Q7 QP 从 RESET 到 RTS 每步在配什么？

**答案要点**：INIT 配本端（端口、pkey、访问权限）；RTR 配对端（QPN、GID、起始 PSN、path MTU）——所以这步之前必须带外交换，RDMA 自己不解决初识问题；RTS 配发送与重传（timeout、retry_cnt、本端 PSN）。数据面铁律：RC 下对端必须先 post_recv 否则 RNR NAK；CQE status 逐个检查——错误不抛异常只躺在 CQE 里。

**证据**：[[S-Week 20 - RDMA verbs 入门]] 状态机图 + 三个故意错误现场记录。

**追问预案**：
- "单边 write 对端怎么知道数据到了？" → WRITE_WITH_IMM / 轮询尾字节 / 补一个小 send，三种都能说。

## Q8 PFC、ECN、DCQCN 分别解决什么？副作用？

**答案要点**：RoCEv2 硬件重传是 go-back-N，丢一个包重发整窗，所以要无损网络。PFC 逐跳背压保不丢，副作用三连：优先级粒度的队头阻塞、pause 逐跳传播成风暴、缓冲依赖成环死锁。ECN+DCQCN 做常态调速：交换机标记、接收网卡回 CNP、发送网卡降速再三阶段爬升——跑在网卡里、显式信号、调速率，三点都不同于 TCP。分工：DCQCN 把队列压在 PFC 水线下，PFC 只当最后防线。边界：概念能推，真交换机没调过。

**证据**：[[RoCE 拥塞控制专题 - PFC ECN DCQCN]] 口述稿；DCQCN 论文摘要笔记。

## Q9 SPDK 为什么快？什么场景不值得用？

**答案要点**：三支柱各消一项内核服务：用户态驱动省 syscall 和内核块层、轮询省中断和上下文切换、无锁 per-core 加 hugepages 省锁竞争和 TLB miss。代价：设备独占、整核 100% 常驻、生态隔离（文件系统工具链全不可用）。划算的临界条件是软件固定成本可比于硬件延迟——微秒级 NVMe 成立；单盘低负载、需要文件系统语义、核比 IOPS 贵的场景不值得。我的对比算了"每千 IOPS 的 CPU 代价"。

**证据**：[[S-Week 21 - NVMe-oF RDMA 与 SPDK]] spdk perf vs fio 对比表。

**追问预案**：
- "CPU 100% 是不是坏了？" → 轮询常态；要看的是每核换来的 IOPS 密度。

## Q10 你的 RDMA 数据真实吗？

**答案要点**：主动分层答：我的 RDMA 实验在 soft-RoCE 上——内核软件实现的 RoCEv2，verbs API 完整所以功能级结论全部成立（编程模型、状态机、语义、NVMe-oF RDMA 路径），但它本身就是真 RDMA 绕过的那层软件栈，性能数字结构性失真，一个都没进结论。报告用三分法：功能级讲机制、性能级才有数字带环境三元组、云环境降级看待。补测清单和脚本现成，租两天 RDMA 裸金属就能补上关键数字。

**证据**：[[soft-RoCE 与实验真实性边界专题]]；报告的三类实验声明。

## 关联知识

- [[存储面试问题清单 - Linux I O]] / [[存储面试问题清单 - 存储引擎]]（前两个板块）
- [[NVMe 命令模型与 SSD 内部专题]] / [[NVMe-oF 专题 - TCP 与 RDMA transport 取舍]] / [[RDMA verbs 专题 - QP WQE CQ 状态机]] / [[RoCE 拥塞控制专题 - PFC ECN DCQCN]] / [[soft-RoCE 与实验真实性边界专题]]（机制底稿五件套）
- [[S-Week 22 - nvme-of-lab 收口与阶段 2 复盘]]（收口周）
- [[00.存储方向专题清单索引]]
