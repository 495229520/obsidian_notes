---
title: AI Infra 存储与 GPU 数据路径系统工程师培养方案
date: 2026-07-08
updated: 2026-07-21
tags:
  - AI-infra/素材库-GPU与推理方向/历史总纲
status: reference
---

# AI Infra 存储与 GPU 数据路径系统工程师培养方案

> [!note] 文档角色
> 这份详版保留岗位映射、项目构想、验收标准和历史取舍，供需要细节时查阅。当前执行顺序、项目上限和暑期实习倒排以 [[00.当前执行 - C++ 到 AI Infra 存储]] 为准；两份文档出现时间或项目数量冲突时，使用当前执行总纲。

> [!goal] 总定位
> **AI Infra 存储与 GPU 数据路径系统工程师。**
> 能做 Linux I/O 路径分析、NVMe / NVMe-oF / RDMA 实验、存储引擎实现，并能把存储接到 GPU / LLM 推理数据路径（GDS、KV cache offload、checkpoint I/O）上的人。
> 这套技能栈客观上覆盖存储系统、AI Infra、GPU 数据路径、RDMA / NVMe-oF、推理性能多类 JD——但这是**投递范围**，不是对外话术。

> [!warning] 对外只有一个身份（2026-07-12 叙事收口）
> 简历、自我介绍、面试开场只讲一句：**"我做数据路径——数据从 NVMe 盘到 GPU 显存要经过哪几层、每层延迟多少、卡在哪一层，我能测出来。"**
> 推理经历只作背景叙事（"从推理系统入门，发现瓶颈在数据路径"），不作并列能力主张。"能投存储、能投推理、能投 GPU"这类表述不出现在任何对外材料里——它在面试官耳中等于"什么都懂一点，什么都没深入"。

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
> 这是**求职型优先级路线**，不是学习型完整路线。所有安排服从三个硬约束：
> 1. **2026 年 9 月初之前必须有第一个可投递的项目 MVP**（春/夏实习内推窗口）。
> 2. **项目宁可少而完整，不做 5 个半成品**：必做 3 个 + 强加分 1 个。
> 3. **2026 年 12 月投递节点：手里必须是两张完成的牌（S1 完整版 + S2 v1），不是三张半成品**（2026-07-12 新增）。12 月投递不依赖 S3；没有真实数据支撑的名词（RDMA / NVMe-oF / GDS）不上简历。
> 4. **补强只做纵向插层，不开新赛道**（2026-07-15 新增）：在现有数据路径上补真实 AI 负载层、生产可靠性层、控制面层、多节点观测层；每个项目收口必须带可运维产物（`fault_injection/`、`runbook.md`、`performance_regression.md`、metrics 导出），不做纯实验室 benchmark。

> [!note] 2026-07-15 复盘判断（本次修订依据）
> 方案在 Linux I/O、NVMe、RDMA、存储引擎和 GPU 数据路径上已经足够完整，真正缺的不是更多底层名词，而是四块：**真实 AI 负载、多节点系统、生产可靠性、基础设施控制面**。对应动作：插入 S3.5（AI 存储负载 benchmark）、把 S4 拆成 S4a / S4b 并将 S4a 提前到面试季、给 S1-S3 补故障注入与 runbook、新增 Go + Kubernetes 的 benchmark operator、开源贡献前置到 12 月投递前。
> 一句话：**保留数据路径深度，加一个 AI 真实负载层、一个 Kubernetes 控制面层和一个生产可靠性层；不再横向扩展新的底层赛道。**

## 一、公司与岗位映射

### 第一梯队：主投

| 公司 | 切入岗位 | 对应本方案项目 |
|---|---|---|
| **NVIDIA** | Storage / GPUDirect Engineer、Systems Software、data path 优化 | S4a / S4b 为主，S1 / S3 / S3.5 为底 |
| **Pure Storage** | Storage Systems / Filesystem / Performance Engineer | S1 + S2 + S3 |
| **VAST Data** | Distributed Systems / Storage Engine / Performance Engineer | S1 + S2 + S3 + S3.5 + S4a |
| **WEKA** | Distributed Filesystem / RDMA / NVMe data path Engineer | S1 + S3 + S3.5 + S4b |
| **Dell / HPE / NetApp** | AI Infrastructure / Storage Systems / Performance Engineer | S1 + S2 + S3 |

### 第二梯队：稳定型补投

| 公司                                   | 切入岗位                                                                                                       |
| ------------------------------------ | ---------------------------------------------------------------------------------------------------------- |
| Solidigm / Micron / Western Digital  | SSD firmware、NVMe driver / validation、storage performance                                                  |
| Cisco / HPE Juniper / Marvell        | 网络系统软件（吃 RDMA / NVMe-oF 积累）                                                                                |


### 高成长冲刺

| 公司 | 切入岗位 |
|---|---|
| AMD / Intel | GPU / accelerator 系统软件（复用推理版 CUDA 积累） |
| CoreWeave / Lambda / Nebius 类 GPU Cloud | cloud infrastructure、storage / networking engineer（operator + 可观测性是入场券） |
| AWS Annapurna Labs（Trainium） | AI Hardware Systems / Fleet Operations（吃故障注入、runbook、自动化积累） |
| Google TPU | TPU Performance（吃多节点 profiling、benchmark 方法论积累） |

### 各家最看重的补强点（2026-07-15，按当前公开 JD 校准）

| 目标企业 | 最应该补充 |
|---|---|
| NVIDIA | GDS / cuFile、对象路径（cuObject）、Linux kernel、GPU / PCIe 拓扑、多节点 profiling |
| VAST / WEKA / Pure / DDN | 分布式文件系统原理、RDMA / NVMe-oF、故障恢复、多租户 QoS |
| CoreWeave / Lambda / GPU Cloud | Kubernetes、Go、Operator、fleet 自动化、可观测性 |
| AWS Trainium | 系统调试、硬件验证、fleet telemetry、自动化和可靠性 |
| Google TPU | 多节点性能分析、Python / C++、ML benchmark、PyTorch / JAX |
| Solidigm / Micron / WD | SSD FTL、firmware、PCIe / NVMe、设备验证、尾延迟 |
| DPU / SmartNIC 企业 | RDMA verbs、DMA、PCIe、SR-IOV、DOCA、虚拟化数据路径 |

JD 依据（2026-07-15 核验）：CoreWeave GPU Infrastructure 岗要求 Go / Python、K8s Operator / Controller、gRPC、大规模性能测试与系统健康可见性；AWS Trainium Fleet 岗强调 dashboard 定位集群趋势、软硬件异常处置、自动化测试与"临时处置转永久修复"；Google TPU Performance 岗强调跨多个互联 host 的性能分析与 PyTorch / JAX 开源贡献。

## 二、时间分配

配比分两段（2026-07-12 重排，服务 12 月投递节点）：

```text
12 月投递前（2026-07 → 2026-12）：
  存储主线：70%（S1 深度 + S2 提前，冲两张完成牌；10 月起含开源
            贡献前置——12 月投递前 1 个有效 PR 或 2 个 issue repro）
  推理保温：10%（Week 5-8 挂接任务照做但减速；harness 达标节点
            = S4a 启动前（2027-01 初），比原 2027-03 提前；
            若届时未达标，S4a 头 1-2 周先补 harness）
  面试保底：20%（12 月投、12-1 月面，算法八股死线跟着提前）

12 月投递后（2027-01 →）恢复：
  存储主线：60%（S3 RDMA 段 → S3.5 → operator / control-plane）
  S4a kv-offload-lab：25%（推理保温升级为正式项目，2027-02 收口）
  面试保底：15%
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
| CUDA 基础、GPU 内存层次 | [[Week 1 - CUDA + Agent workflow]] → [[Week 4 - MatMul v0]]、[[CUDA 学习清单]] | 理解 GDS 为什么要绕过 CPU bounce buffer；S4b 直接用 |
| serving benchmark 方法论（warmup、p50/p95/p99、可复现、不改数据） | [[Week 5 - Serving Benchmark Harness]] | 原样迁移到 fio / io_uring / NVMe benchmark |
| observability / runbook 经验 | [[Week 6 - Observability + Metrics]] | 存储侧换成 iostat / eBPF 观测，定位思路相同 |
| KV cache / prefill / decode 理解 | [[Week 7 - KV Cache + Prefix Cache + Paged KV]]、[[Week 8 - Prefill Decode + Open Source Repro]] | 解释"为什么会出现 SSD-backed KV cache"，存储 x 推理交叉面试的杀手锏 |
| vLLM / SGLang 部署经验 | [[Week 6 - Observability + Metrics]] | S4a KV cache offload 实验的负载发生器 |
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
| 真实 AI 负载 benchmark（2026-07-15 新增） | MLPerf Storage、数据加载 / checkpoint / 权重加载三类负载、GPU utilization 归因 | 能用数据回答"存储路径换了之后，GPU 利用率、训练吞吐、checkpoint 时间改善多少" |
| 生产可靠性（2026-07-15 新增） | 故障注入（crash / 断连 / 写满 / fsync 错误）、SLO、告警、runbook、性能回归 | 能把 benchmark 项目讲成可运维系统：故障怎么注入、怎么发现、怎么恢复、怎么防回退 |
| 基础设施控制面（2026-07-15 新增） | Go 基础、Kubernetes CRD / Operator、gRPC、Prometheus | 能写一个调度存储 benchmark 的 K8s Operator，并解释 reconcile 模型与脚本编排的区别 |
| 多节点端到端观测（2026-07-15 新增） | Nsight Systems、DCGM、NCCL tests、Prometheus / Grafana、统一时间线 | 能把 GPU 空闲沿 DataLoader → 网络 → NVMe → SSD GC 归因到具体层，画在同一张时间线上 |

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
- 能解释同步 / 异步 / 分片 checkpoint 的取舍，以及 checkpoint 写入对前台负载的干扰怎么测量、怎么隔离。
- 能把一次"GPU 空闲"沿 DataLoader → 网络 → NVMe target → SSD GC 的链路归因到具体层，并有统一时间线图。
- 能写一个最小 Kubernetes Operator（CRD + reconcile），并解释它和裸脚本编排的本质区别。
- 能解释对象存储数据路径：multipart upload、range GET、小对象 vs 大对象、元数据与数据路径的分离。
- 能为自己的系统定义并实测 SLO（如 checkpoint 恢复成功率 ≥ 99.9%、故障后 30 秒内恢复数据路径）。

## 五、项目结构：一条数据路径上的 6 个节点 + 2 个挂件（2026-07-15 重排）

> [!important] 收敛原则（更新）
> 结构从"3 必做 + 1 强加分"调整为"5 必做 + 2 强加分 + 1 索引"，但这不是横向扩张：S3.5 是插层、S4a / S4b 是原 S4 的拆分、operator 和 control-plane 是挂在现有项目上的挂件。任何新项目想法仍先问：它能替代还是必须叠加？
> 每个项目收口时除 benchmark 报告外必须带可运维产物：`fault_injection/`、`runbook.md`、`performance_regression.md`、metrics 导出（或 dashboards）——这是"实验室 benchmark"和"可运维系统"的分界线，也是 AWS / CoreWeave 类 fleet 岗位 JD 的直接回应。

| 编号 | 项目 | 性质 | 完成节点 |
|---|---|---|---|
| S1 | `linux-io-lab` | **必做**，第一张牌 | MVP 2026-09 初；完整版 2026-10（含 CI / Sanitizer / 性能回归 / 故障注入底座） |
| S2 | `mini-kv-engine` | **必做**，面试主菜 | **v1 2026-11 底（12 月投递主菜）**；v1.1（崩溃矩阵补全 + metrics + runbook）2027-01；LSM v2 默认砍掉 |
| S3 | `nvme-of-lab` | **必做**，AI 存储公司核心弹药 | 2027-03，TCP 段 12-1 月、RDMA 段 1-3 月；故障切换矩阵并入；**12 月投递不依赖它** |
| S3.5 | `ai-storage-workload-lab` | **必做（2026-07-15 新增）**，把底层数字翻译成 GPU 利用率 | 2027-04（MLPerf Storage + 数据加载 / checkpoint / 权重加载 + 对象存储路径实验） |
| S4a | `kv-offload-lab` | **必做**（原 S4 前段，提前） | **2027-02**（vLLM + LMCache、CPU / NVMe backend，不依赖 GDS 硬件） |
| S4b | `gds-lab` | **强加分**（原 S4 后段） | 2027-06（cuFile + 真实 GDS 硬件，集中租用） |
| — | `storage-benchmark-operator` | **必做挂件**（Go + K8s，控制在 2-3 周） | 2027-05，CoreWeave / Lambda / AWS 类岗位的入场券 |
| — | `nvmeof-control-plane` | 强加分挂件（挂在 S3 上） | 2027-05；时间不够时**第一个砍它** |
| — | `storage-ai-infra-portfolio` | **索引，不是独立项目** | 随做随更 |

2026-07-15 结构调整的三个动机：

```text
1. S4 太晚：若 2027-03 前手里只有 S1-S3，对外形象是"Linux 高性能存储工程师"，
   不是"AI Infra 存储与 GPU 数据路径工程师"。S4a（vLLM + LMCache）成本低、
   不依赖 GDS 硬件，提前到 1-2 月就能拿到 AI 方向的项目证据。
2. 缺真实 AI 负载：fio 的 IOPS 回答不了"存储路径换了之后，GPU 利用率、
   训练吞吐、checkpoint 时间改善多少"。S3.5 用 MLPerf Storage + 自建负载
   补这一层，把故事从"我测过 fio 的 IOPS"升级为"我能解释某个存储路径
   为什么让 GPU 利用率下降、换路径后端到端改善多少"。
3. 缺控制面与可靠性证据：CoreWeave / AWS 类 JD 明确要求 K8s、Go、自动化、
   故障处置。operator + 故障注入矩阵 + runbook 是对这类 JD 的直接回应。
```

> [!warning] 明确不做清单（2026-07-15）
> 完整 Ceph 源码、自研分布式文件系统、Raft / Paxos 深度实现、完整 CSI Driver、大量 CUDA 算子优化、AI 编译器、复杂 LSM v2、多个互不关联的小 Demo。所有新增都是纵向插在现有数据路径上，不开新赛道。

`storage-ai-infra-portfolio` 只是一个总入口仓库，不写新代码：

```text
storage-ai-infra-portfolio/
  README.md             # 故事线 + 各项目一句话结论 + 关键图表
  project-index.md      # 各仓库链接与状态
  benchmark-template.md # 统一 benchmark 报告模板
  runbook-template.md   # 统一故障处置模板（2026-07-15 新增）
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

目标：把 MVP 补成有深度的完整项目。详细周计划已落成：[[S-Week 8 - 块层与 blktrace]] → [[S-Week 9 - eBPF 观测]] → [[S-Week 10 - io_uring 深入]] → [[S-Week 11 - 完整版收口]]，总览见 [[00.存储方向阶段计划索引]]。

新增内容：

- 块层与观测：blktrace / blkparse、eBPF（biolatency / biosnoop）、`iostat -x` 的 util / await / aqu-sz 怎么读，把一笔 I/O 的块层生命周期讲清楚。
- io_uring 深入：polling 模式、registered buffers、和 epoll 的模型对比（关联 [[13.4 epoll模型]]）。
- readahead / `posix_fadvise` 效果实验。
- 一次真实 p99 毛刺的定位记录（用 biolatency 直方图 + blktrace 交叉验证）。
- 产出 `io_path_notes.md`：手画 read 全路径图（用户态 → VFS → page cache → 块层 → NVMe driver → 设备），标注每层延迟量级。
- 工程化底座（2026-07-15 新增）：GitHub Actions CI、ASan / UBSan、`performance_regression.md`（基线数字 + 允许波动范围 + 回退判定规则）、第一批故障注入脚本归入 `fault_injection/`。这套产物是后续所有项目的复用模板。

配套学习：《Systems Performance》第 8-9 章（File Systems / Disks）；OSTEP SSD 章节补完。

### 阶段 1 验收标准（10 月底）

- `linux-io-lab` 完整版定稿，含 p99 毛刺定位案例。
- fio / iostat / biolatency 成为肌肉记忆。
- 推理保温：serving benchmark harness 按 10% 配比继续推进即可，达标节点为 S4a 启动前（2027-01 初），**不阻塞本阶段验收**。

## 八、阶段 2：2026 年 10 月 → 2027 年 3 月（mini-kv-engine + nvme-of-lab + S4a 提前）

目标：从"懂 I/O 路径"升级为"能写存储核心、能做 NVMe / NVMe-oF 实验"，覆盖 2027 春/夏实习面试季，并在面试季内用 S4a 把 AI 叙事立住。

### 必做项目 S2：mini-kv-engine

定位：证明"能写有 crash consistency 的存储核心逻辑"，Modern C++，面试主菜。目标 **2026 年 11 月底完成 v1**（12 月投递的第二张牌）；S-Week 12-17 周计划与自然周一一对应、不摊期，这段是 12 月节点的关键路径。

里程碑：

1. v0：append-only log + 内存 hash index（bitcask 模型）、crash recovery、checksum。
2. v1：WAL + group commit、fsync 策略可配置、崩溃注入测试——除随机 kill -9 外，崩溃矩阵至少覆盖 WAL 截断、部分写入、checksum 错误、恢复期间再次崩溃四类（12 月投递前的底线）。
3. v1.1（2026-12 → 2027-01，面试季并行的低强度收尾，2026-07-15 新增）：崩溃矩阵补全（磁盘写满、fsync 返回错误、文件权限变化、并发写入期间崩溃）、Prometheus metrics 导出、`runbook.md`（每类故障的现象 / 定位 / 恢复步骤）。
4. v2（**默认砍掉**，秋招后有富余再做）：简化 LSM——memtable + SSTable flush + 一层 compaction + bloom filter。LSM 三角与 compaction 取舍改走 [[LSM-tree 与 B+ tree 专题]] 口述级备考，不写代码。

技术点：RAII 文件句柄、错误处理、多线程写入（复用 [[17.2 原子性、可见性与内存序]]、[[17.6 并发队列：有界队列与无锁队列]]）、io_uring 写路径（连接 S1）。

配套学习：DDIA 第 3 章（存储与检索）、RocksDB 文档与关键路径源码（作为工业参照，不通读）。

验收标准：

- 崩溃注入测试 1000 次无数据丢失。
- 崩溃矩阵可用 `fault_injection/` 脚本一键重放，每类故障在 `runbook.md` 有对应条目（v1.1 收口）。
- benchmark：写吞吐 / 读延迟 / 恢复时间，对比不同 fsync 策略。
- `design_note.md` 写清每个决策的备选方案和取舍。
- 能对照 RocksDB 说出自己实现和工业实现差在哪。

面试问题：

- 崩溃发生在 WAL 写完但 index 没更新时怎么办？
- group commit 提升吞吐的代价是什么？
- LSM 为什么写快读慢？bloom filter 补救了什么？
- 你的 checksum 防的是哪类故障？防不了哪类？
- fsync 返回 EIO 之后重试是安全的吗？你的引擎怎么处理？（fsyncgate 问题）
- 磁盘写满时，你的写路径行为是什么？怎么保证不破坏已有数据？

### 必做项目 S3：nvme-of-lab

定位：报告"项目二"的直接落地，Pure / VAST / WEKA / Solidigm 简历的核心弹药；RDMA 部分同时是国内端侧 RDMA 性能分析 / GPU 互联岗的直接证据（2026-07-09 依据国内 JD 校准加深）。目标 2027 年 3 月完成，**整体在 12 月投递之后，分两段执行**（2026-07-12 重排）：

- **TCP 段（2026-12 → 2027-01，S-Week 18-19）**：nvme-cli、SSD 内部、NVMe-oF over TCP 延迟分解。两台带本地 NVMe 的云主机即可，**数据是真的**，12-1 月面试季随做随讲，作为增量弹药。
- **RDMA 段（2027-01 → 2027-03，S-Week 20-22）**：verbs / soft-RoCE 功能级实验 + **收口前集中租一次真实 RDMA 环境补测**（支持 RoCE 的裸金属或 eRDMA 类云实例），用 ib_send_lat / ib_send_bw 拿到真数据后，"RDMA / NVMe-oF" 才允许挂回简历主标题。

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
- 故障切换矩阵（2026-07-15 新增）：target 重启、TCP / RDMA 连接断开、tc netem 注入延迟与丢包、一条 path 失效与恢复、远端设备延迟突增；记录 I/O 卡顿时长与恢复时间，产出 `failover_notes.md` 并入 runbook。
- 输出：`nvmeof_latency_breakdown.md` + 系统图（host / target / transport / 设备四层延迟标注）。

> [!warning] soft-RoCE 的边界（必须写进报告）
> soft-RoCE 只用于理解 RDMA verbs / NVMe-oF RDMA 的**功能路径**，不用于得出真实性能结论——它在内核里用软件模拟 RDMA，延迟数据没有代表性。真实 RDMA 性能必须使用支持 RoCE / InfiniBand 的 NIC 或云裸金属实例（可在收口前集中租一次补测）。面试被问"你的 RDMA 数据真实吗"时，这个边界就是标准答案。

验收标准：

- 能解释 NVMe-oF 解决什么问题（存算分离、盘池化），三种 transport 各自的代价。
- 能解释 RDMA 数据路径和 TCP 的本质差异（zero-copy、kernel bypass、CPU 占用）。
- 能画出 QP / WQE / CQ 状态机，讲清一次 RDMA send/recv 从 post 到 completion 的完整生命周期，并有自己的 rc_pingpong 程序和 `ibv_perftest` 数据。
- 能口述 PFC / ECN / DCQCN 分别解决什么、副作用是什么（如 PFC 的队头阻塞与 pause 风暴）。
- 能回答报告面试题 5 / 6 / 7（RDMA vs TCP、RoCE 的 PFC/ECN、NVMe-oF transport 代价）。
- 能口述 NVMe multipath 在一条路径失效时的行为，并有自己的断连 / 恢复测量数据。
- **报告中明确区分三类实验**：

```text
功能级实验：soft-RoCE / network namespace —— 只讲路径和机制
性能级实验：真实 RDMA NIC / 本地 NVMe / 裸金属 —— 才能写性能结论
云盘实验：只用于流程验证，不得写成设备性能结论
```

### 必做项目 S4a：kv-offload-lab（原 S4 前段，2026-07-15 提前）

定位：把"AI Infra"三个字提前半年放进作品集。若 2027-03 前手里只有 S1-S3，对外形象仍是"Linux 高性能存储工程师"；S4a 成本低（不依赖 GDS 硬件），提前到面试季就能拿出 GPU 数据路径的项目证据。时间：2027-01 → 2027-02，用恢复到 25% 的推理线时段 + 周六集中实验推进，与 S3 RDMA 功能级实验并行。

实验内容：

- vLLM + LMCache：GPU KV cache → CPU 内存 offload → 本地 NVMe offload、跨进程 KV cache 复用、基础 prefill / decode 分离。LMCache 已支持把 KV cache 做成可持久化、跨 serving engine 复用、可观测的缓存层（GPU / CPU 内存 / 外部后端），vLLM 官方有 LMCache offload、PD 分离和 KV sharing 示例。
- 实验变量：prompt 长度、命中率、并发度、KV cache 大小、SSD 带宽、CPU 内存容量、cache 读回延迟。
- 输出指标：TTFT、TPOT、吞吐、GPU 显存节省量、cache 命中收益 vs 读回成本 vs 重算成本。
- 故障侧（并入可靠性层）：KV cache 后端不可用、SSD cache miss 风暴、远端 KV cache 超时、cache 数据损坏、存储降速时自动回退到重算——每项都要有观察记录和降级行为描述。
- 直接复用推理版 harness（[[Week 5 - Serving Benchmark Harness]]）与 KV cache 前置理解（[[Week 7 - KV Cache + Prefix Cache + Paged KV]]、[[Week 8 - Prefill Decode + Open Source Repro]]）。

验收标准：

- 能用数据回答"KV cache 什么时候值得 offload 到 SSD"（收支公式 + 临界点实测）。
- 简历解锁 KV offload bullet（见第十三节），比原计划提前约 4 个月；此后对外叙事升级为"AI Infra 存储与 GPU 数据路径"。

### 开源贡献前置（2026-07-15 调整）

原计划把开源贡献放到秋招前，现在提前：**2026-12 投递前至少完成 1 个有效 PR，或 2 个高质量 issue reproduction**（10 月起投入，计入存储主线 70%）。优先级：

```text
LMCache > fio / liburing > JuiceFS > SPDK > RocksDB
```

现实的切入点：benchmark 脚本修复、指标导出、文档与示例修复、错误处理、可复现的性能回退报告、LMCache storage backend benchmark、fio job 配置问题、liburing 边界条件测试。**不一开始就挑战 Ceph 或 SPDK 核心代码。**对基础设施岗位，开源证据的价值通常高于再写一个孤立的小 Demo。

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

- S2 v1 + v1.1 完成、S3 完成、S4a 完成，各自有完整报告和面试 Q&A。
- JuiceFS / 3FS 对比笔记完成。
- 开源贡献：12 月投递前的底线（1 个有效 PR 或 2 个高质量 issue reproduction，LMCache 优先）已完成，本阶段结束前争取第二个。
- 面试保底：CodeTop 高频前 150 题与 C++ / Linux 八股第一轮收口**提前到 12 月投递前**，1-3 月做第二轮查漏。
- 投递分两波：**第一波 2026-12，用 S1 + S2 投**（简历不写 RDMA / NVMe-oF）；第二波 2027 春/夏实习正式批，S3 + S4a 完成后用 S1 + S2 + S3 + S4a 投——此时对外形象已是"AI Infra 存储与 GPU 数据路径"，不再只是"Linux 存储"。

## 九、阶段 3：2027 年 3 月 → 9 月（S3.5 + 控制面 + S4b + 作品集收口）

前段（1-2 月）的 S4a 已经把 AI 叙事立住，本阶段做三件事：用真实 AI 负载验证底层优化的端到端价值（S3.5）、补控制面证据（operator / control-plane）、上真实 GDS 硬件收口（S4b）。若春/夏实习占用时间，砍序：先砍 `nvmeof-control-plane`，再压缩 S4b 为单实验，**S3.5 与 operator 保住**。

### 必做项目 S3.5：ai-storage-workload-lab（2026-07-15 新增）

定位：回答面试官最致命的追问——"你测的 IOPS 提升，对训练和推理到底有什么用？"把 S1-S3 的底层数字翻译成 GPU 利用率、训练吞吐、checkpoint 时间。时间 2027-03 → 2027-04，双节点（1 台 GPU 实例 + 1 台带本地 NVMe 的存储节点）集中实验。

工具：MLPerf Storage（MLCommons 官方 AI 存储基准，目标就是测量存储系统向模型训练供数的能力，v2.0 起含面向大模型训练的 checkpoint benchmark）+ 自建 PyTorch DataLoader 负载。

实验矩阵：

```text
负载 1：训练数据加载
- 单节点 vs 双节点
- cold cache vs hot cache
- local NVMe vs NVMe-oF/TCP vs NVMe-oF/RDMA
- 1 / 2 / 4 / 8 个 DataLoader worker

负载 2：checkpoint
- 同步 vs 异步 vs 分片 checkpoint
- 写入期间前台训练/推理的延迟抖动

负载 3：模型权重加载
- page cache vs O_DIRECT vs local NVMe vs 远端存储
- S4b 完成后补 GDS 对照
```

每组实验统一记录：

```text
训练 samples/s、数据加载等待时间
checkpoint 保存时间、恢复时间
GPU utilization、CPU utilization
网络吞吐、存储带宽、p95 / p99 I/O 延迟
```

附带一次对象存储路径实验（中优先级，控制在 1 周内）：MinIO / S3 → CPU 内存 → GPU 显存，对比 S3 over TCP、本地文件系统、NVMe-oF 路径；理解 multipart upload、range GET、小对象 vs 大对象、请求并发、元数据路径与数据路径的分离。**不实现对象存储本身**。GDS 体系同时有文件方向的 cuFile 和对象方向的 cuObject，对象路径是本线的合理补充而非跑题。

验收标准：

- 能用数据讲出至少一条完整因果链——某个存储路径为什么让 GPU 利用率下降、换路径后端到端改善多少。
- 两个 SLO 有实测值（如 checkpoint 恢复成功率 ≥ 99.9%、故障后 30 秒内恢复数据路径）。

### 必做挂件：storage-benchmark-operator（Go + Kubernetes，2-3 周）

定位：只投 NVIDIA / WEKA / VAST 的底层 C++ 数据路径岗可以没有它；要覆盖 CoreWeave / Lambda / AWS AI Infra / GPU Cloud，Kubernetes + Go 是明确缺口。**不做完整 CSI Driver**——工作量过大且偏离数据路径主线。时间 2027-04 → 2027-05，严格控制在 2-3 周，小而完整。

功能清单：

```text
1. 定义 StorageBenchmark CRD
2. 自动调度 fio / MLPerf Storage benchmark Pod
3. 识别节点上的 NVMe、GPU、NIC 拓扑
4. 设置 CPU affinity、NUMA affinity
5. 收集 fio、iostat、GPU、网络指标
6. 结果写入对象存储或数据库
7. 自动判断 benchmark 是否性能回退
8. 失败后清理和重试
```

示例资源：

```yaml
apiVersion: infra.example.com/v1
kind: StorageBenchmark
metadata:
  name: nvmeof-rdma-test
spec:
  clientNodes: 2
  transport: rdma
  blockSize: 128k
  queueDepth: 32
  duration: 300
  workload: llm-checkpoint
```

一个小项目同时证明：Go、Kubernetes、controller 模式、分布式任务编排、benchmark 自动化、可观测性。Go 基础从 2027-02 起用每周 2-3 小时碎片时间预热，不挤占 S3 收口。

### 强加分挂件：nvmeof-control-plane（时间不够第一个砍）

定位：轻量分布式系统切片，证明"高性能数据面之外，系统还需要可扩展、可恢复、可观测的控制面"。**不实现 Raft，不做副本存储。**

```text
C++ 数据面（复用 S3）：NVMe-oF I/O、RDMA 通信、性能关键路径
Go 控制面（新写）  ：target 注册、健康检查、路径选择、故障摘除、
                     配置下发、Prometheus metrics、gRPC API
```

顺带覆盖分布式系统概念清单（口述级）：节点发现、RPC、超时与重试、幂等性、租约、健康检查、负载均衡、多路径、元数据一致性。

### 强加分项目 S4b：gds-lab（原 S4 后段）

定位：真实 GDS 硬件上的收口实验，与 S4a 的结论合并成完整的 SSD → GPU 数据路径报告。时间 2027-05 → 2027-06，租支持 GDS 的云 GPU 机型集中实验。

实验内容：

- cuFile / GDS：SSD → GPU memory 直读，对比传统 POSIX + CPU bounce buffer 路径（带宽、CPU 占用）。
- 同步 vs 异步 / 批量 cuFile 路径（GDS 文档已覆盖 async 与 batch API，核心仍是存储与 GPU 显存之间的直接 DMA 数据路径）。
- local NVMe vs 支持 GDS 的远端文件系统。
- checkpoint I/O 干扰实验 S3.5 已做，这里只补 GDS 对照组。
- 输出：`ai_data_path_report.md`——合并 S4a 数据，回答"KV cache 什么时候值得 offload 到 SSD"与"GDS 什么时候值得上"。

必须回答（即报告面试题 4 / 8）：

- KV cache 为什么会成为显存瓶颈？为什么会出现 SSD-backed KV cache？
- offload 的收支公式：省下的 prefill 重算时间 vs KV 读回延迟，临界点在哪？
- GDS 为什么能降低 CPU 参与？它需要文件系统和驱动满足什么条件？

### 多节点端到端 profiling（贯穿 S3.5 → S4b，2026-07-15 新增）

观测工具从存储侧（fio / iostat / blktrace / eBPF / perf）补齐 GPU 与多节点侧：

```text
Nsight Systems、DCGM、nvidia-smi dmon、NCCL tests
ib_send_lat / ib_send_bw、sar / mpstat
Prometheus、Grafana、OpenTelemetry
```

最终产出不是一堆分散的图，而是一张统一时间线：

```text
时间
 ├── GPU utilization
 ├── GPU kernel activity
 ├── DataLoader wait
 ├── CPU utilization
 ├── RDMA throughput
 ├── NVMe queue depth
 ├── block I/O latency
 └── application TTFT / throughput
```

能沿着它讲完一条归因链：

```text
GPU 空闲 → DataLoader 阻塞 → 远端读取延迟升高
→ RDMA CQ completion 延后 → NVMe target queue depth 过高
→ SSD GC 导致 p99 毛刺
```

这就是对外那句话——"数据卡在哪一层，我能测出来"——的最终形态。

### 作品集收口：storage-ai-infra-portfolio

不写新代码，只做整合：统一报告模板、统一环境记录、统一 runbook 模板，README 汇总关键结论图表和故事线。

```text
秋招故事线：
"我从 LLM 推理系统入门，发现推理的下一个瓶颈在数据路径，
于是系统性补齐了 Linux I/O、NVMe、RDMA 和存储引擎，
再用真实 AI 负载验证了每层优化对 GPU 利用率的影响。
现在我能同时讲清 GPU 在等什么数据、数据卡在哪一层、坏了怎么恢复。"
```

### 开源贡献目标（前置后的加码）

12 月投递前的底线（1 个有效 PR 或 2 个高质量 issue reproduction）已在阶段 2 完成。秋招前在此基础上加码，沿用推理版 Reproduce → Minimize → Analyze → Contribute 四步法：

- 再拿 1 个 merged PR（benchmark 脚本、指标导出、文档示例、错误处理、边界条件测试都算）。
- 或 2 篇 benchmark report 被社区回复认可（LMCache storage backend benchmark、可复现的性能回退报告是好切入点）。

优先级不变：

```text
LMCache（新项目、贴 AI 存储叙事、贡献门槛低）
> fio / liburing（社区友好、与 S1 直接相关）
> JuiceFS
> SPDK / RocksDB（认可度高但门槛高）
> Ceph（只读架构，不贡献核心）
```

## 十、项目真实性核验

核验日期：2026-07-08，2026-07-15 增补。开源项目为真实学习对象；S1-S4b 与两个挂件是拟建个人仓库，不能说成已有开源项目。

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
| MLPerf Storage | 真实开源基准 | MLCommons 官方 AI 存储 benchmark（v2.0 含 checkpoint 场景），S3.5 主工具 | [GitHub](https://github.com/mlcommons/storage) |
| DCGM | 真实开源工具 | GPU 指标采集（多节点观测层） | [GitHub](https://github.com/NVIDIA/DCGM) |
| Nsight Systems | 官方工具 | 多节点时间线 profiling | [Docs](https://developer.nvidia.com/nsight-systems) |
| kubebuilder | 真实开源项目 | K8s Operator 脚手架（storage-benchmark-operator 用） | [GitHub](https://github.com/kubernetes-sigs/kubebuilder) |
| NVIDIA GPUDirect Storage | 官方文档 | SSD → GPU 直接路径 | [Docs](https://docs.nvidia.com/gpudirect-storage/) |
| OSTEP | 免费教材 | Persistence 部分是主教材 | [网站](https://pages.cs.wisc.edu/~remzi/OSTEP/) |
| linux-io-lab 等 S1-S4b 及 operator / control-plane 挂件 | 拟建作品集项目 | 见各阶段 | 本人未来创建 |

## 十一、环境与成本策略

存储实验比 GPU 实验便宜得多，这是这条线的红利：

| 任务 | 环境 | 数据可信度 |
|---|---|---|
| I/O 路径 / io_uring / fio / mini-kv-engine | 按小时租带本地 NVMe 的云主机；本地 VM 只用于开发调试 | 本地 NVMe 数据可写进报告；VM 虚拟盘不行 |
| NVMe 真实性能数据 | 裸金属或 local NVMe 实例 | **云盘数据不能写进报告，只能做流程验证** |
| NVMe-oF / soft-RoCE | 两台云主机或一台机器 + 网络命名空间 | **soft-RoCE 只做功能级结论，性能结论需真实 RDMA NIC** |
| SPDK | 带 local NVMe 的云主机，集中实验 | 同上 |
| GDS / KV offload | 支持 GDS 的 GPU 实例，集中租用（沿用推理版"每周一次、4-6 小时"节奏）；S4a 只需普通 GPU 实例 | 记录机型与驱动版本 |
| AI 负载实验（S3.5） | 1 台 GPU 实例 + 1 台带本地 NVMe 的存储节点，双节点集中租用 | 记录机型、互联带宽、模型与负载参数；GPU utilization 结论必须注明负载 |
| K8s Operator 开发 | 本地 kind / k3s 开发，收口时租 2-3 节点小集群验证一次 | 本地集群只验证功能，不出性能结论 |
| 日常开发 | 本地 Mac 写代码 + Agent 生成框架，Linux 环境统一用远程 | — |

所有 benchmark 必须记录：机器型号、内核版本、文件系统与挂载参数、盘型号（`nvme id-ctrl`）、是否云盘/本地盘、fio 版本、job file、drop_caches 与否、重复次数（至少 3 次、含 warmup、不只报最好结果）。

## 十二、每周执行模板

沿用推理版周一到周日模板（见 [[AI Agent Native AI Infra GPU Performance Engineer 培养方案#每周执行模板|推理版每周执行模板]]），改动三处：

- 周四 Profiling：Nsight → `iostat` / `blktrace` / `biolatency` / `perf`。
- 周六集中实验：租 GPU → 租带 NVMe 的机器（GDS 周除外）。
- **新增面试保底档**：每周 3 次、每次约 1 小时（建议周二/周四/周日各一次）——算法 2 题 + 一章八股；周日复盘时把本周项目问答补进 `interview_qa.md`。

Agent 边界不变：可以生成脚手架、fio job、解析脚本、README 初稿；不可以决定结论、改 benchmark 数据、绕过 correctness / 崩溃注入测试。

## 十三、简历表达

主标题按"有真数据才解锁"分两版：

12 月投递版（只挂 S1 + S2 能支撑的词）：

```text
AI Infrastructure Systems Engineer
C++ · Linux I/O · Storage Engine · Performance Analysis
```

S3 / S4a 真数据落地后的完整版：

```text
AI Infrastructure Systems Engineer
C++ · Linux · Storage Systems · RDMA/NVMe-oF · LLM Serving · GPU Data Path
```

投 CoreWeave / Lambda / GPU Cloud 类岗位时（operator 完成后解锁），第二行替换为：

```text
C++ / Go · Linux · Storage Systems · RDMA/NVMe-oF · Kubernetes · GPU Data Path
```

> [!warning] 名词解锁纪律
> 简历上每一个名词都是面试官的提问许可。RDMA / NVMe-oF / GDS 在拿到真实环境数据之前不上标题、不进 bullet；soft-RoCE 和云盘的数字**永远**不进简历——一个数字被拆穿，整份简历的数据可信度归零。

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
与可配置 fsync 策略，崩溃注入测试 1000 次零数据丢失。      ← 2026-11 底解锁，12 月投递主力
```

```text
搭建 NVMe-oF over TCP / RDMA 实验环境，输出本地 NVMe 与远端访问的
延迟分解报告，并用 SPDK 对比内核态与用户态 NVMe 驱动路径。 ← S3 完成解锁（2027-03，12 月投递不写）
```

```text
基于 vLLM + LMCache 构造长上下文负载，量化 KV cache offload 到
CPU 内存 / NVMe SSD 对 TTFT / TPOT 的影响与收支临界点。    ← S4a 解锁（2027-02，较原计划提前 4 个月）
```

```text
使用 MLPerf Storage 与自建训练负载，量化 local NVMe / NVMe-oF TCP / RDMA
三种数据路径下的 GPU 利用率、训练吞吐与 checkpoint 保存/恢复时间，
并输出端到端归因时间线。                                  ← S3.5 解锁（2027-04）
```

```text
为存储引擎与 NVMe-oF 路径构建故障注入矩阵（crash / 断连 / 磁盘写满 /
fsync 错误），配套 runbook 与性能回归基线，实测 checkpoint 恢复成功率
与故障恢复时间 SLO。                                      ← 随 S2 v1.1 / S3 逐步解锁
```

```text
用 Go 开发 Kubernetes Operator（CRD + controller），自动调度 fio /
MLPerf Storage 基准任务、采集 NVMe / GPU / 网络指标并自动检测
性能回退。                                                ← operator 解锁（2027-05，GPU Cloud 岗专用）
```

```text
使用 cuFile / GPUDirect Storage 对比 SSD → GPU 直读与 CPU bounce buffer
路径的带宽与 CPU 占用差异。                               ← S4b 解锁（2027-06）
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
- multipart upload / range GET 各解决什么问题？小对象为什么难做？
- 对象直读 GPU（cuObject 类路径）与文件路径 GDS 的区别？

### AI / GPU 数据路径（交叉杀手锏）

- KV cache 为什么会成为显存瓶颈？为什么出现 SSD-backed KV cache？
- KV offload 的收支公式和临界点？
- GPUDirect Storage 为什么能降低 CPU 参与？需要什么条件？
- checkpoint burst write 对在线负载的干扰怎么隔离？
- 训练"GPU 等数据"时，你会按什么顺序排查？

### AI 存储负载与 checkpoint（2026-07-15 新增）

- 同步 / 异步 / 分片 checkpoint 各自的取舍？写入期间怎么隔离对前台负载的干扰？
- DataLoader worker 从 4 加到 8，训练吞吐没变，你按什么顺序排查？
- MLPerf Storage 测的是什么？它和 fio 的本质区别在哪？
- 存储带宽翻倍，训练吞吐只涨 10%，可能的原因链是什么？

### 生产可靠性与故障处置（2026-07-15 新增）

- fsync 返回 EIO 之后重试是安全的吗？正确的处理是什么？
- NVMe-oF 一条路径断了，正在飞的 I/O 会发生什么？multipath 怎么切换？
- "checkpoint 恢复成功率 ≥ 99.9%"这类 SLO 怎么测量、怎么报警？
- 一次性能回退怎么被自动发现？基线数字怎么维护？
- KV cache 后端不可用时，推理服务的降级策略是什么？降级的代价怎么量化？

### 控制面与 Kubernetes（GPU Cloud 岗专用）

- Operator 的 reconcile 模型和"写个脚本轮询"有什么本质区别？
- 你的 StorageBenchmark CRD 的状态机怎么设计？失败重试怎么保证幂等？
- 在 K8s 里怎么做 CPU / NUMA affinity？为什么 benchmark 需要它？
- 控制面挂了，数据面应该发生什么？

## 十五、近期行动清单

- [ ] 创建 `linux-io-lab` 仓库（沿用推理版 `CLAUDE.md` / `AGENTS.md` 模板）和 `storage-ai-infra-portfolio` 索引仓库。
- [ ] 按 [[S-Week 1 - 环境搭建 + Page Cache 基线]] 租一台带本地 NVMe 的云主机，跑通第一个冷/热 page cache 实验。
- [ ] 开始 OSTEP Persistence（I/O 设备 → 磁盘 → 文件系统 → journaling）。
- [ ] 装好 fio / iostat 工具链，写第一篇 `benchmark.md`。
- [ ] 面试保底启动：本周 2 道算法 + C++ 智能指针一章八股。
- [ ] 推理线保温：按原计划推进 serving benchmark harness 周任务（达标死线已提前到 S4a 启动前，2027-01 初）。
- [ ] 开源贡献前置启动：2026-10 前在 LMCache / fio 里挑定 1 个可复现 issue 开始跟踪，目标 12 月投递前 1 个 PR 或 2 个 issue reproduction。
- [ ] S4a 排期落位：12 月底核对推理 harness 达标情况，2027-01 启动 kv-offload-lab。
- [ ] Go 基础排期：2027-02 起每周 2-3 小时碎片时间过 Go + kubebuilder 教程，为 operator 做准备，不挤占 S3 收口。
- [ ] 三周后回顾：70 / 10 / 20 配比是否可持续、MVP 是否在 9 月初轨道上。
- [ ] 11 月底核对 12 月投递节点：S1 完整版 + S2 v1 两张牌齐、简历只含真数据名词、CodeTop 前 150 第一轮收口、开源贡献底线（1 PR 或 2 个 issue repro）到位。

## 一句话定位

```text
我要成为能指挥 AI Agent 快速开发，但自己能测量 Linux I/O 路径、
写有 crash consistency 的存储引擎、做 NVMe-oF / RDMA / GDS 实验、
能用真实 AI 负载证明每层优化对 GPU 利用率的影响、
并能让数据路径在故障后可观测、可恢复的
AI Infra 存储与 GPU 数据路径系统工程师。
```
