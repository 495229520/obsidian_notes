---
title: AI Infra 存储与 GPU 数据路径系统工程师培养方案
date: 2026-07-08
updated: 2026-07-08
tags:
  - infra
  - 存储
  - 总纲
status: active
---

# AI Infra 存储与 GPU 数据路径系统工程师培养方案

> [!goal] 总定位
> **AI Infra 存储与 GPU 数据路径系统工程师。**
> 能做 Linux I/O 路径分析、NVMe / NVMe-oF / RDMA 实验、存储引擎实现，并能把存储接到 GPU / LLM 推理数据路径（GDS、KV cache offload、checkpoint I/O）上的人。
> 这个定位同时能投：存储系统岗、AI Infra 岗、GPU 数据路径岗、RDMA / NVMe-oF 岗、推理系统性能岗——不是只能投存储。

依据：[[ai_infra_report_europe_us_companies]] 赛道三（企业级 AI 存储、并行文件系统与 NVMe SSD）+ 项目二（高性能 I/O / NVMe-oF / GPUDirect Storage 实验），并保留赛道二（GPU 系统软件）作为交叉面。

核心判断来自报告：

```text
AI 不是只消耗 GPU。
长上下文推理、RAG、checkpoint、训练数据加载、KV cache offload
都会把存储推到关键路径。
"GPU 等数据"是 AI 存储公司要解决的核心问题，
也是你从推理版方案迁移过来的天然接口。
```

> [!warning] 本方案的性质
> 这是**求职型优先级路线**，不是学习型完整路线。所有安排服从两个硬约束：
> 1. **2026 年 9 月初之前必须有第一个可投递的项目 MVP**（春/夏实习内推窗口）。
> 2. **项目宁可少而完整，不做 5 个半成品**：必做 3 个 + 强加分 1 个。

## 一、公司与岗位映射

### 第一梯队：主投

| 公司 | 切入岗位 | 对应本方案项目 |
|---|---|---|
| **NVIDIA** | Storage / GPUDirect Engineer、Systems Software、data path 优化 | S4 为主，S1 / S3 为底 |
| **Pure Storage** | Storage Systems / Filesystem / Performance Engineer | S1 + S2 + S3 |
| **VAST Data** | Distributed Systems / Storage Engine / Performance Engineer | S1 + S2 + S3 + S4 |
| **WEKA** | Distributed Filesystem / RDMA / NVMe data path Engineer | S1 + S3 + S4 |
| **Dell / HPE / NetApp** | AI Infrastructure / Storage Systems / Performance Engineer | S1 + S2 + S3 |

### 第二梯队：稳定型补投

| 公司 | 切入岗位 |
|---|---|
| Solidigm / Micron / Western Digital | SSD firmware、NVMe driver / validation、storage performance |
| Cisco / HPE Juniper / Marvell | 网络系统软件（吃 RDMA / NVMe-oF 积累） |
| 国内大厂 AI Infra 团队（阿里 / 字节 / 腾讯 / 华为等） | 端侧 RDMA 性能分析、GPU 互联 / 数据路径、HPC 高性能网络——S3 的 verbs / ibv_perftest / DCQCN 证据直接对口；注意瞄准实习与校招版本，社招 JD 只用来校准技能方向 |

### 高成长冲刺

| 公司 | 切入岗位 |
|---|---|
| AMD / Intel | GPU / accelerator 系统软件（复用推理版 CUDA 积累） |
| CoreWeave / Lambda / Nebius 类 GPU Cloud | cloud infrastructure、storage / networking engineer |

## 二、时间分配

```text
存储主线：60%（新知识、新项目）
推理保温：25%（serving benchmark harness、KV cache 周任务——本来就是 S4 的输入）
面试保底：15%（C++ / Linux / 网络 八股 + 算法 + 项目问答）
```

推理保温不另起炉灶，直接沿用推理版周计划按实际进度推进：[[Week 5 - Serving Benchmark Harness]] → [[Week 6 - Observability + Metrics]] → [[Week 7 - KV Cache + Prefix Cache + Paged KV]] → [[Week 8 - Prefill Decode + Open Source Repro]]；从 [[Week 9 - GEMM Deep Dive v1]] 开始的 GEMM 深入线（Week 9-16）降级为可选，只在冲 NVIDIA / AMD 等 GPU 岗时启用。

> [!important] 为什么必须有 15% 面试保底
> 项目再硬，一面被 C++ 内存模型或一道中等 DP 卡住就全亏了。保底不是额外任务，而是求职路线的一部分。

面试保底内容（每周约 5-6 小时，建议拆成 3 次）：

| 板块 | 内容 | 复用本仓库 |
|---|---|---|
| C++ | 并发、RAII、move、智能指针、内存模型、虚函数 | `1.C++基础` / `2.C++高级` 各章题目汇总 |
| Linux / 网络 | 进程线程、epoll、mmap、文件系统、TCP 状态机与调优 | `3.Linux` 7-13 章、17 章并发同步 |
| 算法 | 数组、哈希、二分、栈队列、树、图、DP 基础 | `5.算法` + [[CodeTop 高频题 Top300]] + [[AI Infra 岗算法笔试保底清单]]，每周 5-8 题 |
| 项目问答 | 每周整理 10 个 Q&A 进 `interview_qa.md` | 各项目 README |

面试口算与专题速记直接复用推理版沉淀：[[LLM 推理面试公式速算清单]]、[[00.专题清单索引]]。

## 三、与推理版培养方案的关系

前置方案：[[AI Agent Native AI Infra GPU Performance Engineer 培养方案]]。已投入约 5-6 周（CUDA 基础 kernel、benchmark 方法论、serving benchmark / observability 起步），这些**不作废，全部成为存储线的输入**：

| 推理版已有积累 | 入口 | 在存储线中的复用方式 |
|---|---|---|
| CUDA 基础、GPU 内存层次 | [[Week 1 - CUDA + Agent workflow]] → [[Week 4 - MatMul v0]]、[[CUDA 学习清单]] | 理解 GDS 为什么要绕过 CPU bounce buffer；S4 直接用 |
| serving benchmark 方法论（warmup、p50/p95/p99、可复现、不改数据） | [[Week 5 - Serving Benchmark Harness]] | 原样迁移到 fio / io_uring / NVMe benchmark |
| observability / runbook 经验 | [[Week 6 - Observability + Metrics]] | 存储侧换成 iostat / eBPF 观测，定位思路相同 |
| KV cache / prefill / decode 理解 | [[Week 7 - KV Cache + Prefix Cache + Paged KV]]、[[Week 8 - Prefill Decode + Open Source Repro]] | 解释"为什么会出现 SSD-backed KV cache"，存储 x 推理交叉面试的杀手锏 |
| vLLM / SGLang 部署经验 | [[Week 6 - Observability + Metrics]] | S4 KV cache offload 实验的负载发生器 |
| AI Agent 工程流（correctness gate、人工验证边界） | 推理版执行原则章节 | 所有存储项目沿用同一套规范 |
| Nsight / profiling 思维 | [[3.4 CUDA Nsight Compute 指标速查]] | 换成 perf / iostat / blktrace / eBPF，方法论相同 |
| 本仓库 Linux 笔记（epoll、TCP、并发同步） | [[13.4 epoll模型]]、[[8.13 TCP内核队列与参数调优]] | NVMe-oF over TCP、RDMA、存储引擎并发控制的直接前置知识 |

## 四、能力主线

| 主线 | 要练什么 | 最终简历表达 |
|---|---|---|
| Linux I/O 路径 | VFS、page cache、buffered vs direct I/O、mmap、fsync、io_uring、blk-mq | 能画出一次 read/write 从系统调用到 NVMe 设备的完整路径，并解释每层的延迟来源 |
| 存储介质与协议 | NVMe 命令模型、queue pair、SSD FTL / GC / 写放大、NVMe-oF（TCP / RDMA）、SPDK | 能解释 NVMe 为什么快、NVMe-oF 三种 transport 的代价、内核态 vs 用户态驱动的取舍 |
| 存储引擎 | WAL、crash consistency、LSM-tree vs B+ tree、compaction、RocksDB 参照 | 能用 Modern C++ 实现带 WAL 和恢复测试的 KV 引擎，并解释每个设计决策 |
| 分布式存储（阅读线） | JuiceFS / 3FS 主读，Ceph 架构泛读，副本 vs EC、元数据路径 | 能对比 2-3 个真实分布式存储系统的数据路径和元数据设计 |
| 存储性能分析 | fio、iostat、blktrace、perf、eBPF（biolatency / biosnoop） | 能设计公平可复现的存储 benchmark，把 p99 毛刺定位到具体层 |
| AI / GPU 数据路径 | GDS / cuFile、KV cache offload（LMCache / Mooncake）、checkpoint burst write | 能解释存储如何出现在 LLM 推理/训练关键路径上，并有实验数据 |

最低可验证能力（贯穿全程）：

- 能解释 page cache 命中和不命中时 read 延迟差多少个数量级，并有自己的测量数据。
- 能解释 `O_DIRECT`、`fsync`、`fdatasync`、`sync_file_range` 分别保证什么、不保证什么。
- 能写 io_uring 程序并和 read、mmap 路径做延迟/吞吐对比。
- 能用 fio 设计 benchmark matrix：随机/顺序、读/写、块大小、队列深度、numjobs。
- 能口算：4K 随机读 IOPS x 块大小 = 带宽；给定 p99 目标反推队列深度上限。
- 能解释 NVMe queue pair 模型为什么适合多核，和 SATA/AHCI 的本质区别。
- 能解释 SSD 写放大、GC、over-provisioning 对稳态性能和 p99 的影响。
- 能解释 NVMe-oF over TCP / RDMA 的路径差异，以及 RDMA 为什么能降延迟。
- 能实现 WAL + 崩溃恢复，并用 kill -9 测试证明不丢已提交数据。
- 能解释 LSM-tree 的写放大/读放大/空间放大三角，以及 compaction 策略取舍。
- 能画出 JuiceFS 或 3FS 的一次写入数据流（client → 元数据 → 数据节点）。
- 能解释 GDS 的"存储到 GPU memory 直接路径"绕过了什么、需要什么条件。
- 能解释 KV cache offload 到 SSD 的收益公式：省下的重算时间 vs 读回延迟。

## 五、项目结构：3 必做 + 1 强加分 + 1 索引

> [!important] 收敛原则
> 宁可 3 个项目每个都有完整的 correctness / benchmark / 报告 / 面试 Q&A，也不要 5 个半成品。任何新项目想法先问：它能替代还是必须叠加？

| 编号 | 项目 | 性质 | 完成节点 |
|---|---|---|---|
| S1 | `linux-io-lab` | **必做**，第一张牌 | MVP 2026-09 初，完整版 2026-10 |
| S2 | `mini-kv-engine` | **必做**，面试主菜 | 2027-01 |
| S3 | `nvme-of-lab` | **必做**，AI 存储公司核心弹药 | 2027-03 |
| S4 | `gds-kv-offload-lab` | **强加分**，两条线交汇点 | 2027-06 |
| — | `storage-ai-infra-portfolio` | **索引，不是独立项目** | 随做随更 |

`storage-ai-infra-portfolio` 只是一个总入口仓库，不写新代码：

```text
storage-ai-infra-portfolio/
  README.md             # 故事线 + 各项目一句话结论 + 关键图表
  project-index.md      # 各仓库链接与状态
  benchmark-template.md # 统一 benchmark 报告模板
  env-template.md       # 统一环境记录模板
  interview_qa.md       # 累积的项目问答
  figures/
```

## 六、阶段 0：2026 年 7 月中旬 → 9 月初（linux-io-lab MVP）

目标：**先能投**。9 月初之前产出可展示的 MVP，用于提前投递/内推 2027 春/夏实习。

MVP 只做 5 件事，不多做：

```text
1. buffered vs O_DIRECT
2. cold cache vs hot cache（drop_caches 前后）
3. read vs mmap vs io_uring（iodepth 1 / 8 / 32）
4. write vs write+fsync vs write+fdatasync 延迟对比
5. fio 对照实验 + benchmark_report.md
```

每组实验记录 IOPS、带宽、p50 / p95 / p99、CPU 占用；全部可用 `reproduce.sh` 一键复现。

Agent 负责：CMake、fio job file、结果解析脚本、图表脚本、README 框架。
你自己负责：io_uring 核心代码、异常数据判断、每张图的结论。

配套学习（只学 MVP 需要的）：

- OSTEP Persistence：I/O 设备、磁盘、文件系统实现、journaling（SSD 章节可后置）。
- fio 的 ioengine / iodepth / numjobs / direct / bs 参数。
- 关联已有笔记：[[4.1 打开、读取、写入、关闭]]、[[4.2 重定向、同步]]。

### 阶段 0 周安排（约 7 周）

每周详细任务见 [[00.存储方向阶段计划索引]]（`9.培养方案/01.存储方向阶段计划/`），每篇周计划都内嵌当周的推理保温与面试保底任务：

| 周次 | 主题 | 产出 |
|---|---|---|
| [[S-Week 1 - 环境搭建 + Page Cache 基线\|Week 1]] | 环境 + page cache | 云主机（本地 NVMe）就绪、冷/热 cache 读延迟第一组数据 |
| [[S-Week 2 - O_DIRECT + 持久化语义\|Week 2]] | O_DIRECT + 持久化语义 | buffered vs O_DIRECT、write / fsync / fdatasync 对比表 |
| [[S-Week 3 - fio 对照与 Benchmark Matrix\|Week 3]] | fio 对照 | fio matrix、与自写程序结果互相验证 |
| [[S-Week 4 - mmap 与读路径对比\|Week 4]] | mmap | read vs mmap 对比报告 |
| [[S-Week 5 - io_uring 异步 IO\|Week 5]] | io_uring | io_uring 读程序、iodepth 扫描曲线 |
| [[S-Week 6 - MVP 收口与报告\|Week 6]] | 收口 | `benchmark_report.md`、`reproduce.sh`、README 定稿 |
| [[S-Week 7 - 简历化与投递启动\|Week 7]] | 简历化 | 简历 bullet、10 个面试 Q&A、开始内推投递 |

### 阶段 0 验收标准（9 月初）

- MVP 上 GitHub，报告可复现。
- 简历能写：

```text
构建 Linux I/O benchmark lab，对比 buffered I/O、O_DIRECT、mmap、
io_uring 与 fsync/fdatasync 路径，使用 fio / iostat 分析 IOPS、
带宽与 p95/p99 延迟，并输出可复现实验报告。
```

- 能脱稿回答：write 返回后数据在哪、O_DIRECT 对齐要求、io_uring 相比 epoll+read 解决了什么。
- 面试保底同步启动：算法每周 5-8 题、C++ 八股按章过。

## 七、阶段 1：2026 年 9 月 → 10 月（linux-io-lab 完整版）

目标：把 MVP 补成有深度的完整项目。周任务纲要（S-Week 8 → S-Week 11）见 [[00.存储方向阶段计划索引]]，进入阶段 1 时再落成详细周计划。

新增内容：

- 块层与观测：blktrace / blkparse、eBPF（biolatency / biosnoop）、`iostat -x` 的 util / await / aqu-sz 怎么读，把一笔 I/O 的块层生命周期讲清楚。
- io_uring 深入：polling 模式、registered buffers、和 epoll 的模型对比（关联 [[13.4 epoll模型]]）。
- readahead / `posix_fadvise` 效果实验。
- 一次真实 p99 毛刺的定位记录（用 biolatency 直方图 + blktrace 交叉验证）。
- 产出 `io_path_notes.md`：手画 read 全路径图（用户态 → VFS → page cache → 块层 → NVMe driver → 设备），标注每层延迟量级。

配套学习：《Systems Performance》第 8-9 章（File Systems / Disks）；OSTEP SSD 章节补完。

### 阶段 1 验收标准（10 月底）

- `linux-io-lab` 完整版定稿，含 p99 毛刺定位案例。
- fio / iostat / biolatency 成为肌肉记忆。
- 推理保温：serving benchmark harness 达到 [[Week 5 - Serving Benchmark Harness]] 原定验收标准（S4 的输入）。

## 八、阶段 2：2026 年 10 月 → 2027 年 3 月（mini-kv-engine + nvme-of-lab）

目标：从"懂 I/O 路径"升级为"能写存储核心、能做 NVMe / NVMe-oF 实验"，覆盖 2027 春/夏实习面试季。

### 必做项目 S2：mini-kv-engine

定位：证明"能写有 crash consistency 的存储核心逻辑"，Modern C++，面试主菜。目标 2027 年 1 月完成 v1。

里程碑：

1. v0：append-only log + 内存 hash index（bitcask 模型）、crash recovery、checksum。
2. v1：WAL + group commit、fsync 策略可配置、崩溃注入测试（随机 kill -9 后验证已提交数据不丢）。
3. v2（可选，时间不够就砍）：简化 LSM——memtable + SSTable flush + 一层 compaction + bloom filter。

技术点：RAII 文件句柄、错误处理、多线程写入（复用 [[17.2 原子性、可见性与内存序]]、[[17.6 并发队列：有界队列与无锁队列]]）、io_uring 写路径（连接 S1）。

配套学习：DDIA 第 3 章（存储与检索）、RocksDB 文档与关键路径源码（作为工业参照，不通读）。

验收标准：

- 崩溃注入测试 1000 次无数据丢失。
- benchmark：写吞吐 / 读延迟 / 恢复时间，对比不同 fsync 策略。
- `design_note.md` 写清每个决策的备选方案和取舍。
- 能对照 RocksDB 说出自己实现和工业实现差在哪。

面试问题：

- 崩溃发生在 WAL 写完但 index 没更新时怎么办？
- group commit 提升吞吐的代价是什么？
- LSM 为什么写快读慢？bloom filter 补救了什么？
- 你的 checksum 防的是哪类故障？防不了哪类？

### 必做项目 S3：nvme-of-lab

定位：报告"项目二"的直接落地，Pure / VAST / WEKA / Solidigm 简历的核心弹药；RDMA 部分同时是国内端侧 RDMA 性能分析 / GPU 互联岗的直接证据（2026-07-09 依据国内 JD 校准加深）。目标 2027 年 3 月完成。

学习重点：

- NVMe 命令模型：admin / I/O queue pair、completion queue、doorbell；`nvme-cli` 实操。
- SSD 内部：FTL、GC、写放大、over-provisioning、稳态 vs 空盘性能。
- NVMe-oF 架构：host / target、transport 抽象；用 Linux `nvmet` 搭 over TCP soft target。
- RDMA 实操（加深）：verbs 编程模型、**QP / WQE / CQ 状态机**、内存注册、zero-copy、kernel bypass；写一个 rc_pingpong 级最小 verbs 程序（跑在 soft-RoCE 上），用 `ibv_perftest`（ib_send_lat / ib_send_bw）做基准。
- RoCE 拥塞控制概念：PFC、ECN、**DCQCN**（lossless Ethernet 为什么需要它们、各自的副作用），概念 + 面试口述级，不要求调优实战。
- SPDK：`hello_bdev` / `spdk perf`，理解用户态轮询的收益和代价（CPU 独占、生态隔离）。
- 关联已有笔记：[[8.13 TCP内核队列与参数调优]]、[[13.6 Reactor模式与EventLoop]]。

实验路径：

- 本地 NVMe fio 基线（复用 S1 方法论）。
- NVMe-oF/TCP：跨网络 fio，延迟分解（网络 RTT + 协议开销 + 设备延迟）。
- RDMA verbs 入门：soft-RoCE 上跑通 rc_pingpong 级程序 + `ibv_perftest` 基准，产出 `rdma_verbs_notes.md`（QP/WQE/CQ 状态机图 + 一次 send/recv 的完整生命周期）。
- NVMe-oF/RDMA：无 RDMA 网卡时用 soft-RoCE（rxe）做**功能级**实验。
- SPDK perf 对比内核 NVMe 驱动。
- 输出：`nvmeof_latency_breakdown.md` + 系统图（host / target / transport / 设备四层延迟标注）。

> [!warning] soft-RoCE 的边界（必须写进报告）
> soft-RoCE 只用于理解 RDMA verbs / NVMe-oF RDMA 的**功能路径**，不用于得出真实性能结论——它在内核里用软件模拟 RDMA，延迟数据没有代表性。真实 RDMA 性能必须使用支持 RoCE / InfiniBand 的 NIC 或云裸金属实例（可在收口前集中租一次补测）。面试被问"你的 RDMA 数据真实吗"时，这个边界就是标准答案。

验收标准：

- 能解释 NVMe-oF 解决什么问题（存算分离、盘池化），三种 transport 各自的代价。
- 能解释 RDMA 数据路径和 TCP 的本质差异（zero-copy、kernel bypass、CPU 占用）。
- 能画出 QP / WQE / CQ 状态机，讲清一次 RDMA send/recv 从 post 到 completion 的完整生命周期，并有自己的 rc_pingpong 程序和 `ibv_perftest` 数据。
- 能口述 PFC / ECN / DCQCN 分别解决什么、副作用是什么（如 PFC 的队头阻塞与 pause 风暴）。
- 能回答报告面试题 5 / 6 / 7（RDMA vs TCP、RoCE 的 PFC/ECN、NVMe-oF transport 代价）。
- **报告中明确区分三类实验**：

```text
功能级实验：soft-RoCE / network namespace —— 只讲路径和机制
性能级实验：真实 RDMA NIC / 本地 NVMe / 裸金属 —— 才能写性能结论
云盘实验：只用于流程验证，不得写成设备性能结论
```

### 分布式存储阅读线（降级为配菜，不做项目）

```text
主读：JuiceFS（架构清楚、代码好读，适合做简历笔记）
     3FS（AI 负载、RDMA + SSD，最贴本路线，VAST / WEKA 的对标学习对象）
泛读：Ceph 架构文档（RADOS / CRUSH 概念即可）
可选：MinIO 的 EC 机制
```

> [!warning] 不要过早读 Ceph 核心源码
> Ceph 工业价值高，但源码体量对实习准备是陷阱。阶段 2 只读架构文档、能画出写路径即可；深入留到入职后或秋招后。

最低产出：2 篇架构对比笔记（元数据路径、副本 vs EC、和传统 NAS 的区别），落在本仓库。

### 阶段 2 验收标准（2027 年 3 月）

- S2 v1 完成、S3 完成，两者都有完整报告和面试 Q&A。
- JuiceFS / 3FS 对比笔记完成。
- 有 1-2 个存储相关 issue reproduction（fio / RocksDB / JuiceFS / LMCache 任一）。
- 面试保底：CodeTop 高频前 150 题过完一遍，C++ / Linux 八股按章过完。
- 用 S1 + S2 + S3 投 2027 春/夏实习。

## 九、阶段 3：2027 年 3 月 → 9 月（gds-kv-offload-lab + 作品集收口）

### 强加分项目 S4：gds-kv-offload-lab

定位：两条线的交汇点，报告"存储被推到 AI 关键路径"论断的亲手验证。若春/夏实习占用时间，可压缩为"KV offload benchmark + GDS 单实验"的精简版。

实验内容：

- GPUDirect Storage：用 cuFile API 做 SSD → GPU memory 直读，对比走 CPU bounce buffer 的路径（带宽、CPU 占用）。租支持 GDS 的云 GPU 机型集中实验。
- KV cache offload：vLLM + LMCache（或调研 Mooncake 架构）构造长上下文 / 多轮对话负载，对比 KV cache 纯显存 vs offload 到 CPU 内存 / SSD 的 TTFT、TPOT 变化——**直接复用推理版 serving benchmark harness（[[Week 5 - Serving Benchmark Harness]]）**；KV cache / paged KV 前置理解见 [[Week 7 - KV Cache + Prefix Cache + Paged KV]]，PD 分离与 KV transfer 见 [[Week 8 - Prefill Decode + Open Source Repro]]。
- checkpoint I/O（可选）：模拟大 checkpoint 保存/加载，测量顺序大块写对前台负载的干扰。
- 输出：`ai_data_path_report.md`——用数据回答"KV cache 什么时候值得 offload 到 SSD"。

必须回答（即报告面试题 4 / 8）：

- KV cache 为什么会成为显存瓶颈？为什么会出现 SSD-backed KV cache？
- offload 的收支公式：省下的 prefill 重算时间 vs KV 读回延迟，临界点在哪？
- GDS 为什么能降低 CPU 参与？它需要文件系统和驱动满足什么条件？

### 作品集收口：storage-ai-infra-portfolio

不写新代码，只做整合：统一报告模板、统一环境记录、README 汇总关键结论图表和故事线。

```text
秋招故事线：
"我从 LLM 推理系统入门，发现推理的下一个瓶颈在数据路径，
于是系统性补齐了 Linux I/O、NVMe、RDMA 和存储引擎，
现在我能同时讲清 GPU 在等什么数据、数据卡在哪一层。"
```

### 开源贡献目标

秋招前至少做到以下之一（沿用推理版 Reproduce → Minimize → Analyze → Contribute 四步法）：

- fio / liburing / RocksDB / JuiceFS / LMCache 的 1 个 merged PR（文档、benchmark、小修复）。
- 3 个高质量 issue reproduction。
- 2 篇 benchmark report 被社区回复认可。

优先级：

```text
LMCache（新项目、贴 AI 存储叙事、贡献门槛低）
> JuiceFS / fio（社区友好）
> RocksDB / SPDK（认可度高但门槛高）
> Ceph（只读架构，不贡献核心）
```

## 十、项目真实性核验

核验日期：2026-07-08。开源项目为真实学习对象；S1-S4 是拟建个人仓库，不能说成已有开源项目。

| 名称 | 类型 | 用途 | 链接 |
|---|---|---|---|
| fio | 真实开源项目 | 存储 benchmark 主力工具 | [GitHub](https://github.com/axboe/fio) |
| liburing | 真实开源项目 | io_uring 用户态库，作者即 io_uring 作者 | [GitHub](https://github.com/axboe/liburing) |
| SPDK | 真实开源项目 | 用户态 NVMe 驱动、NVMe-oF target | [GitHub](https://github.com/spdk/spdk) |
| RocksDB | 真实开源项目 | 工业级 LSM 引擎参照 | [GitHub](https://github.com/facebook/rocksdb) |
| JuiceFS | 真实开源项目 | 主读分布式文件系统 | [GitHub](https://github.com/juicedata/juicefs) |
| DeepSeek 3FS | 真实开源项目 | AI 负载并行文件系统（RDMA + NVMe） | [GitHub](https://github.com/deepseek-ai/3FS) |
| Ceph | 真实开源项目 | 架构泛读 | [GitHub](https://github.com/ceph/ceph) |
| MinIO | 真实开源项目 | 对象存储 + EC（可选） | [GitHub](https://github.com/minio/minio) |
| LMCache | 真实开源项目 | vLLM KV cache offload / 共享 | [GitHub](https://github.com/LMCache/LMCache) |
| Mooncake | 真实开源项目 | KV cache 分离式推理架构（Moonshot） | [GitHub](https://github.com/kvcache-ai/Mooncake) |
| nvme-cli | 真实开源工具 | NVMe 设备管理与信息查询 | [GitHub](https://github.com/linux-nvme/nvme-cli) |
| rdma-core | 真实开源项目 | verbs 库与示例（含 rc_pingpong） | [GitHub](https://github.com/linux-rdma/rdma-core) |
| perftest | 真实开源工具 | ib_send_lat / ib_send_bw 等 RDMA 基准 | [GitHub](https://github.com/linux-rdma/perftest) |
| bcc / bpftrace | 真实开源工具 | biolatency / biosnoop 等存储观测 | [GitHub](https://github.com/iovisor/bcc) |
| NVIDIA GPUDirect Storage | 官方文档 | SSD → GPU 直接路径 | [Docs](https://docs.nvidia.com/gpudirect-storage/) |
| OSTEP | 免费教材 | Persistence 部分是主教材 | [网站](https://pages.cs.wisc.edu/~remzi/OSTEP/) |
| linux-io-lab 等 S1-S4 | 拟建作品集项目 | 见各阶段 | 本人未来创建 |

## 十一、环境与成本策略

存储实验比 GPU 实验便宜得多，这是这条线的红利：

| 任务 | 环境 | 数据可信度 |
|---|---|---|
| I/O 路径 / io_uring / fio / mini-kv-engine | 按小时租带本地 NVMe 的云主机；本地 VM 只用于开发调试 | 本地 NVMe 数据可写进报告；VM 虚拟盘不行 |
| NVMe 真实性能数据 | 裸金属或 local NVMe 实例 | **云盘数据不能写进报告，只能做流程验证** |
| NVMe-oF / soft-RoCE | 两台云主机或一台机器 + 网络命名空间 | **soft-RoCE 只做功能级结论，性能结论需真实 RDMA NIC** |
| SPDK | 带 local NVMe 的云主机，集中实验 | 同上 |
| GDS / KV offload | 支持 GDS 的 GPU 实例，集中租用（沿用推理版"每周一次、4-6 小时"节奏） | 记录机型与驱动版本 |
| 日常开发 | 本地 Mac 写代码 + Agent 生成框架，Linux 环境统一用远程 | — |

所有 benchmark 必须记录：机器型号、内核版本、文件系统与挂载参数、盘型号（`nvme id-ctrl`）、是否云盘/本地盘、fio 版本、job file、drop_caches 与否、重复次数（至少 3 次、含 warmup、不只报最好结果）。

## 十二、每周执行模板

沿用推理版周一到周日模板（见 [[AI Agent Native AI Infra GPU Performance Engineer 培养方案#每周执行模板|推理版每周执行模板]]），改动三处：

- 周四 Profiling：Nsight → `iostat` / `blktrace` / `biolatency` / `perf`。
- 周六集中实验：租 GPU → 租带 NVMe 的机器（GDS 周除外）。
- **新增面试保底档**：每周 3 次、每次约 1 小时（建议周二/周四/周日各一次）——算法 2 题 + 一章八股；周日复盘时把本周项目问答补进 `interview_qa.md`。

Agent 边界不变：可以生成脚手架、fio job、解析脚本、README 初稿；不可以决定结论、改 benchmark 数据、绕过 correctness / 崩溃注入测试。

## 十三、简历表达

主标题：

```text
AI Infrastructure Systems Engineer
C++ · Linux · Storage Systems · RDMA/NVMe-oF · LLM Serving · GPU Data Path
```

一句话定位（中文场合）：

```text
AI Infra 存储与 GPU 数据路径系统工程师
```

不要写：

```text
熟悉 Linux I/O，了解 NVMe 和分布式存储。
```

应该写（按项目进度逐条解锁）：

```text
构建 Linux I/O benchmark lab，对比 buffered I/O、O_DIRECT、mmap、io_uring
与 fsync/fdatasync 路径，使用 fio / iostat / eBPF（biolatency）分析 IOPS、
带宽与 p95/p99 延迟构成，并输出可复现实验报告。          ← 阶段 0/1 解锁
```

```text
实现带 WAL 与崩溃恢复的 KV 存储引擎（Modern C++），支持 group commit
与可配置 fsync 策略，崩溃注入测试 1000 次零数据丢失。      ← 阶段 2 解锁
```

```text
搭建 NVMe-oF over TCP / RDMA 实验环境，输出本地 NVMe 与远端访问的
延迟分解报告，并用 SPDK 对比内核态与用户态 NVMe 驱动路径。 ← 阶段 2 解锁
```

```text
基于 vLLM + LMCache 构造长上下文负载，量化 KV cache offload 到
CPU 内存 / NVMe SSD 对 TTFT / TPOT 的影响，并结合 GPUDirect Storage
实验分析 SSD → GPU 数据路径的带宽与 CPU 占用差异。        ← 阶段 3 解锁
```

## 十四、面试问题清单

### Linux I/O

- 一次 read 的完整路径是什么？page cache 命中和不命中差在哪？
- write 返回后数据一定落盘了吗？fsync / fdatasync / O_SYNC 区别？
- O_DIRECT 的对齐要求和适用场景？
- io_uring 的 SQ / CQ 模型是什么？相比 epoll + read、AIO 解决了什么？
- mmap 读文件什么时候比 read 快、什么时候更慢？
- p99 延迟毛刺怎么定位到具体层？

### NVMe / SSD / NVMe-oF

- NVMe queue pair 模型为什么适合多核？
- SSD 写放大是怎么产生的？GC 如何影响稳态 p99？
- 空盘 benchmark 为什么不可信？
- NVMe-oF 解决什么问题？TCP / RDMA / InfiniBand transport 各有什么代价？
- RDMA 和 TCP 的数据路径差异？RoCE 为什么要关注 PFC / ECN？
- QP / WQE / CQ 分别是什么？一次 RDMA send 从 post 到 completion 经历了什么？
- DCQCN 解决什么问题？它和 PFC / ECN 是什么关系？PFC 有什么副作用？
- RDMA 的内存注册为什么必要、为什么贵？
- 你的 RDMA 实验数据是怎么来的？soft-RoCE 的边界是什么？（主动交代）
- SPDK 用户态轮询的收益和代价？

### 存储引擎

- WAL 为什么能保证 crash consistency？恢复流程是什么？
- group commit 的吞吐/延迟取舍？
- LSM-tree 三种放大的三角关系？leveled vs tiered compaction？
- B+ tree 和 LSM 分别适合什么负载？
- torn write 是什么？checksum 能防什么、不能防什么？

### 分布式存储

- 副本和 EC 的空间/修复/延迟取舍？
- JuiceFS 元数据/数据分离的设计动机？
- 3FS / WEKA 这类 AI 存储和传统 NAS 的本质区别是什么？
- 元数据路径为什么常是并行文件系统的瓶颈？

### AI / GPU 数据路径（交叉杀手锏）

- KV cache 为什么会成为显存瓶颈？为什么出现 SSD-backed KV cache？
- KV offload 的收支公式和临界点？
- GPUDirect Storage 为什么能降低 CPU 参与？需要什么条件？
- checkpoint burst write 对在线负载的干扰怎么隔离？
- 训练"GPU 等数据"时，你会按什么顺序排查？

## 十五、近期行动清单

- [ ] 创建 `linux-io-lab` 仓库（沿用推理版 `CLAUDE.md` / `AGENTS.md` 模板）和 `storage-ai-infra-portfolio` 索引仓库。
- [ ] 按 [[S-Week 1 - 环境搭建 + Page Cache 基线]] 租一台带本地 NVMe 的云主机，跑通第一个冷/热 page cache 实验。
- [ ] 开始 OSTEP Persistence（I/O 设备 → 磁盘 → 文件系统 → journaling）。
- [ ] 装好 fio / iostat 工具链，写第一篇 `benchmark.md`。
- [ ] 面试保底启动：本周 2 道算法 + C++ 智能指针一章八股。
- [ ] 推理线保温：按原计划推进 serving benchmark harness 周任务。
- [ ] 三周后回顾：60 / 25 / 15 配比是否可持续、MVP 是否在 9 月初轨道上。

## 一句话定位

```text
我要成为能指挥 AI Agent 快速开发，但自己能测量 Linux I/O 路径、
写有 crash consistency 的存储引擎、做 NVMe-oF / RDMA / GDS 实验、
并能解释 GPU 在等什么数据的 AI Infra 存储与 GPU 数据路径系统工程师。
```
