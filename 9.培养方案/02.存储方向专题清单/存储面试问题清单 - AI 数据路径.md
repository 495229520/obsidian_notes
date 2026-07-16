---
title: 存储面试问题清单 - AI 数据路径
date: 2026-07-12
tags:
  - infra
  - 存储
  - GPU
  - 面试
roadmap_week: "求职全程（阶段 3 S4 逐题补实测证据；机制与口算答案现在就能用）"
sort_order: "99.40"
status: active
---

# 存储面试问题清单 - AI 数据路径

> [!info] 所属路线
> - 培养方案第十四节 AI / GPU 数据路径板块（交叉杀手锏）的沉淀版：存储 × 推理两条线合流后的问题域——别的存储候选人不懂推理、推理候选人不懂存储，这个板块是差异化所在
> - 排序：99.40
> - 用法：机制与口算答案现在就能脱稿；标注"S4 实测"的证据随 gds-kv-offload-lab 推进逐题补上。答案 5 句内，第一句结论。

---

## Q1 为什么说"存储被推到了 AI 的关键路径上"？

**答案要点**：训练侧和推理侧各有一半。训练侧：数据加载要持续喂饱 GPU、checkpoint 是周期性 TB 级 burst 写加恢复时的全集群读风暴——存储慢直接换算成 GPU 空转的钱。推理侧：KV cache 溢出显存后走"DRAM→NVMe→远端"分层，读回带宽决定 TTFT；模型加载决定冷启动与弹性扩缩速度。GPU 时薪远高于存储成本，**存储的每一毫秒都在给最贵的资源计费**——这是所有 AI 存储公司（VAST/WEKA/DDN）商业故事的物理基础。

**证据**：收支口算见 [[KV cache offload 专题 - 收支公式与临界点]]；checkpoint 量级见 [[checkpoint I O 专题 - burst write 隔离]]；S4 实测补 TTFT 曲线。

**追问预案**：
- "存储厂商说自己能提高 GPU 利用率，怎么验证？" → 测数据加载/读回是否还在拖 GPU 空转：GPU 利用率时间线对齐存储带宽时间线——harness 就能做。

## Q2 SSD 到显存的传统路径和 GDS 路径差在哪？什么时候 GDS 收益大？

**答案要点**：传统两跳——NVMe DMA 进主机内存、cudaMemcpy 再进显存：两份总线带宽、主机内存穿透两次、CPU 参与编排。GDS 用 nvidia-fs + cuFile 让 NVMe DMA 经 PCIe P2P 一跳直达显存。收益来源是"少一跳、解除主机瓶颈"，所以在多盘聚合、大块顺序、主机 CPU/内存带宽吃紧时收益大；单盘小块时和 pinned bounce buffer 差距不大。条件：O_DIRECT + 对齐、PCIe 拓扑、nvidia-fs——最大陷阱是兼容模式静默回落，每轮实验用 gds_stats 验证真走了 P2P。

**证据**：[[GPUDirect Storage 专题 - cuFile 与 bounce buffer]] 三组对照设计；S4 实测补带宽/CPU 数据。

**追问预案**：
- "和 io_uring registered buffer、RDMA MR 什么关系？" → 同一模式三次出现：预注册 + pin 换 DMA 直达，GDS 只是把终点换成显存。

## Q3 KV cache 什么时候值得 offload 到 SSD？

**答案要点**：账只有一笔：读回时间 vs 重算时间，两边都随长度线性，约掉得每 token 带宽临界点 = KV 字节数 ÷ prefill 每 token 耗时。口算示意：8B 模型每 token 128 KiB、A100 上 prefill 约 0.1 ms/token → 临界约 1.3 GB/s，单块 NVMe 顺序读跨过几倍——纸面是赢的，模型越大越划算。但实测要过四关：命中率、读回与计算重叠、并发抢带宽、调度开销——我的 S4 实验就是拿 vLLM+LMCache 三档对照与公式对账。

**证据**：[[KV cache offload 专题 - 收支公式与临界点]]；[[Week 7 - KV Cache + Prefix Cache + Paged KV]] 的口算基本功；S4 实测补 TTFT/命中率曲线。

**追问预案**：
- "GQA 对这笔账的影响？" → KV 头数减少直接压 KV 字节数，临界带宽同比例降——offload 更容易划算。

## Q4 offload 改善的是 TTFT 还是 TPOT？为什么？

**答案要点**：主要是 TTFT——命中的前缀免重算 prefill，首 token 提前；TPOT 设计上不该变（解码读的是显存内活跃 KV）。若实测 TPOT 变差，归因是读回流量抢了 PCIe/内存带宽或调度干扰——这本身就是要测的干扰项，和我 S-Week 9 的"后台流量伤前台 p99"是同一类问题，工具与方法直接复用。

**证据**：[[Week 5 - Serving Benchmark Harness]] 的指标定义；S4 三档对照实测。

## Q5 PD 分离里 KV cache 怎么从 prefill 节点到 decode 节点？

**答案要点**：RDMA 单边 write：prefill 节点把 KV 块直接写进 decode 节点预注册的内存/显存区，对端 CPU/GPU 零参与——这正是单边语义存在的理由。完成感知用 IMM 或元数据通知。跨到显存就是 GPUDirect RDMA（网卡 DMA 直达显存，与 GDS 同族）。Mooncake 这类系统再加一层池化：KV 先进共享存储层，decode 按需拉取——KV cache 成为集群一等资源。

**证据**：[[RDMA verbs 专题 - QP WQE CQ 状态机]] 手写单边 write 实验；[[Week 8 - Prefill Decode + Open Source Repro]]。

**追问预案**：
- "为什么不用 TCP 搬 KV？" → 每 token 128 KiB 级、按字节付 CPU 费的路径在这个吞吐下烧不起——S3 的 transport 对照直接回答。

## Q6 checkpoint 保存怎么做到不打死前台？

**答案要点**：四层缓解。应用层异步 checkpoint：先 D2H 快照进 pinned 内存（秒级），训练立即继续、后台刷盘；写路径 O_DIRECT 大块写，从根上不堆脏页、不触发 writeback 风暴；调度层 cgroup io.max 限速；架构层独立存储池 + 分片并行写。落盘原子性走"临时文件 + fsync + rename + 目录 fsync"。这条伤害链我在自己的实验里复现并定位过——buffered 突发写让前台 p99 起飞，biosnoop 抓到肇事者。

**证据**：[[checkpoint I O 专题 - burst write 隔离]]；[[S-Week 9 - eBPF 观测]] 的 p99_hunt 定位记录（已有实测）。

## Q7 大模型冷启动加载怎么加速？

**答案要点**：本质是"几百 GB 顺序读进显存"的路径优化，三招：大块顺序读吃满盘/网带宽（S1 的 readahead 与块大小结论直接适用）；GDS 直读省 bounce 一跳；多节点场景避免 N 个节点拉同一份——分片 + 节点间 P2P/广播分发，把一对多改成多对多。安全带宽预估用存储分层表口算：单盘 NVMe GB/s 级 → 百 GB 模型分钟级，聚合或直读把它压进十秒级。

**证据**：[[GPUDirect Storage 专题 - cuFile 与 bounce buffer]]；S1 顺序读基线（已有实测）。

## Q8 存储分层的带宽数量级，口算一遍。

**答案要点**：HBM TB/s 级；CPU DRAM 经 PCIe 4.0 x16 约 30 GB/s（PCIe 5.0 翻倍）；单盘 NVMe 顺序读 GB/s 级（5-7），多盘可聚合；跨网络受链路与协议——25/100/400 GbE 折算 3/12/50 GB/s 再扣协议开销，RDMA 逼近线速、TCP 打折。用法：KV 读回、模型加载、checkpoint 各自的时间预算都从这张表出发——**先口算后实测**是我全部项目的习惯。

**证据**：[[LLM 推理面试公式速算清单]] + [[GPU 存储层次与算子融合口述]]（推理侧对照表）。

## Q9 用你的三个项目支撑"GPU 数据路径"这条故事线。

**答案要点**：S1 给了测量与归因能力——I/O 路径逐层延迟、blktrace/eBPF 定位毛刺，AI 场景的 checkpoint 干扰、读回争抢全是它的应用；S2 给了存储语义能力——WAL/崩溃一致性/群提交，checkpoint 原子落盘和 KV 存储层的可靠性同源；S3 给了数据面能力——NVMe-oF 延迟分解、RDMA 单边写、SPDK 用户态路径，正是 KV transfer 与 AI 存储系统的数据面。S4 把三者合流到 GPU：GDS 直读 + KV offload 收支实测。**一条线：会测 → 会写 → 会连 → 直达 GPU**——每一段都有可复现的仓库和报告。

**证据**：三项目 README 与报告；[[S-Week 22 - nvme-of-lab 收口与阶段 2 复盘]] 的故事线结构。

**追问预案**：
- "如果 S4 没做完就来面试？" → 机制、口算、实验设计全部现成，缺的只是那几条曲线——我能白板推完收支公式并说清实验怎么跑，这就是"计划中"和"空谈"的区别。

## 关联知识

- [[GPUDirect Storage 专题 - cuFile 与 bounce buffer]] / [[KV cache offload 专题 - 收支公式与临界点]] / [[checkpoint I O 专题 - burst write 隔离]]（本板块三篇机制底稿）
- [[存储面试问题清单 - Linux I O]] / [[存储面试问题清单 - 存储引擎]] / [[存储面试问题清单 - NVMe 与 NVMe-oF]] / [[存储面试问题清单 - 分布式存储]]（前四个板块）
- [[Week 5 - Serving Benchmark Harness]] / [[Week 7 - KV Cache + Prefix Cache + Paged KV]] / [[Week 8 - Prefill Decode + Open Source Repro]]（推理侧地基）
- [[AI Infra 存储与 GPU 数据路径系统工程师培养方案]]（S4 与作品集收口）
- [[00.存储方向专题清单索引]]
