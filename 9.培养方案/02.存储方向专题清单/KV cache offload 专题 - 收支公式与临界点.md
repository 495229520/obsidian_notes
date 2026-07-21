---
title: KV cache offload 专题 - 收支公式与临界点
date: 2026-07-12
tags:
  - 高性能存储/存储方向专题清单
roadmap_week: 阶段 3（S4 gds-kv-offload-lab 核心实验：vLLM + LMCache benchmark）
sort_order: "06.10"
status: active
---

# KV cache offload 专题 - 收支公式与临界点

> [!info] 所属路线
> - 培养方案阶段：阶段 3 S4 的核心实验——vLLM + LMCache 构造长上下文/多轮负载，对比 KV cache 纯显存 vs offload 到 CPU 内存/SSD 的 TTFT/TPOT，直接复用 [[Week 5 - Serving Benchmark Harness]]
> - 排序：06.10
> - 用途：报告"存储被推到 AI 关键路径"论断的定量版——用一个收支公式回答"KV cache 什么时候值得 offload 到 SSD"。这是存储线和推理线合流的正式交汇点。

> [!goal] 目标
> 讲清四件事：KV cache 为什么会溢出显存（每 token 字节数口算）；offload 的收支公式与带宽临界点；存储分层（HBM/DRAM/NVMe/远端）各自的带宽数量级与角色；LMCache/Mooncake 两个系统各解决什么。

---

## 1. 问题从哪来：KV cache 的口算

每 token 的 KV 字节数（[[Week 7 - KV Cache + Prefix Cache + Paged KV]] 的公式，GQA 模型）：

$$
KV_{token} = 2 \times n_{layers} \times n_{kv} \times d_{head} \times b_{dtype}
$$

量级示意（Llama-3-8B：32 层、8 个 KV 头、head_dim 128、fp16）：

$$
KV_{token} = 2 \times 32 \times 8 \times 128 \times 2 = 131072 \text{ B} = 128 \text{ KiB}
$$

128K 上下文的一条对话 ≈ 16 GiB KV——几条长对话就吃穿一块卡的显存。而多轮对话、共享系统提示词、RAG 模板意味着**大量前缀可复用**：丢掉就要重算（prefill），留着显存装不下——offload 到更便宜的层级由此成为必选题，这就是 Mooncake、LMCache、3FS 的 KVCache 场景（[[分布式存储阅读专题 - JuiceFS 与 3FS]]）同时出现的原因。

## 2. 收支公式与带宽临界点

offload 的账只有一笔比较：**读回时间 vs 重算时间**。

$$
T_{load} = \frac{S_{kv}}{BW_{storage}}
$$

$$
T_{recompute} = \frac{F_{prefill}}{P_{gpu}}
$$

读回赢当且仅当 T_load < T_recompute。两边都随 token 数线性增长，所以约掉长度后得到**每 token 的带宽临界点**：

$$
BW_{min} = \frac{KV_{token}}{t_{prefill}}
$$

量级口算（标注为示意，实测归 S4）：8B 模型 prefill 每 token 约 2 × 8×10⁹ = 16 GFLOP；A100 bf16 峰值 312 TFLOPS，按四到五成 MFU 算有效 130-150 TFLOP/s → 每 token 约 0.1 ms。临界带宽 ≈ 128 KiB ÷ 0.1 ms ≈ **1.3 GB/s**——单块 PCIe 4.0 NVMe 的顺序读（5-7 GB/s）已经跨过临界几倍。**结论的方向**：对这个量级的模型，SSD offload 在纸面上是赢的；模型越大每 token 计算越贵、临界带宽越低，offload 越划算。

但纸面结论到实测之间隔着四件事（S4 实验就是去量它们）：命中率（复用不发生，一切白搭——按负载形态测）、读回与计算能否流水线重叠（分块加载边算边取）、并发下多请求抢带宽、以及调度/索引开销。**公式给方向，harness 给结论**——TTFT 对比 + 命中率两条曲线是 S4 报告的主图。

## 3. 存储分层：每层的带宽数量级与角色

| 层 | 带宽量级 | 角色 |
|---|---|---|
| HBM（显存） | TB/s 级 | 活跃 KV，解码每步都要读 |
| CPU DRAM（经 PCIe） | PCIe 4.0 x16 ≈ 30 GB/s 上下 | 第一级 offload：容量 ×10，读回最快 |
| 本地 NVMe | GB/s 级（单盘 5-7，多盘可聚合） | 第二级：容量 ×100，跨过临界即可用 |
| 远端（NVMe-oF / 3FS / 对象存储） | 受网络与协议（S3 的延迟分解直接适用） | 池化共享：跨实例复用前缀、容灾 |

两条来自前面积累的直接连线：读回路径要不要经 CPU 中转——**GDS 直读显存**（[[GPUDirect Storage 专题 - cuFile 与 bounce buffer]]）省掉 bounce 一跳；跨节点搬 KV——**RDMA 单边 write**（[[RDMA verbs 专题 - QP WQE CQ 状态机]]），对端 GPU/CPU 零参与。S1-S3 学的每一层，在这张表里都有位置。

## 4. LMCache 与 Mooncake：两个系统各解决什么

- **LMCache**：vLLM 的 KV cache 分层与共享层——KV 按 chunk 组织，命中的前缀从 CPU 内存/磁盘/远端后端读回而不重算；解决的是**单实例/集群内的 KV 复用与容量扩展**。S4 的实验载体。
- **Mooncake**（月之暗面）：以 KV cache 为中心的 PD 分离架构——prefill 集群算完的 KV 经 RDMA 池化存储转移给 decode 集群，KV cache 成为集群的一等公民资源；解决的是**跨节点的 KV 流转与算力解耦**（[[Week 8 - Prefill Decode + Open Source Repro]] 的 KV transfer 就是这一步）。
- 一句话分工：LMCache 管"存与取"，Mooncake 管"在谁那算、往谁那搬"——两者叠加就是"KV cache 的存储系统"这个新物种，也是 JD 里"AI 存储"岗位真正在招的东西。

## 5. S4 实验骨架（harness 复用）

- 负载：长上下文多轮对话（同前缀多轮追问）+ 控制组（无复用的一次性请求），用 [[Week 5 - Serving Benchmark Harness]] 的发压与指标管线。
- 对照：纯显存 / +CPU 内存 offload / +本地 NVMe offload 三档；指标：TTFT（offload 主要改善它——省 prefill）、TPOT（应基本不变，若变差说明读回抢了解码资源）、命中率、存储侧带宽（iostat——S1 工具直接上）。
- 判据回到收支公式：实测 TTFT 改善量 vs 公式预测，偏差归因（命中率、重叠度、带宽争抢）——这份对账就是 `ai_data_path_report.md` 的核心章节。

## 6. 面试口述模板

```text
KV cache 每 token 字节数是 2 乘层数乘 KV 头数乘 head_dim 乘字节宽，
8B 模型约 128 KiB，128K 上下文一条对话就是 16 GiB——显存装不下而
前缀又高度可复用，所以 offload 是必选题。账只有一笔：读回时间对
重算时间，两边都随长度线性，约掉后得到每 token 的带宽临界点——
KV 字节数除以 prefill 每 token 耗时，8B 模型在 A100 上口算约 1 点
几 GB/s，单块 NVMe 顺序读就跨过去几倍，模型越大越划算。但纸面到
实测隔着命中率、读回与计算的重叠、并发抢带宽，所以我用 vLLM 加
LMCache 三档对照实测 TTFT 和命中率，和公式对账。分层上 DRAM 是
第一级、NVMe 第二级、远端做池化共享——读回可以走 GDS 直达显存省
bounce，跨节点走 RDMA 单边写，这正好是我 S1 到 S3 攒下的全部路径。
LMCache 管存取、Mooncake 管流转，合起来就是 KV cache 的存储系统。
```

追问预案：

- "offload 会不会伤 TPOT？" → 设计上不该（解码读的是显存里的活跃 KV）；实测若变差，归因读回流量抢 PCIe/内存带宽——这正是要测的干扰项。
- "为什么大模型更划算？" → 每 token 计算量随参数线性涨，KV 字节数随层数×头数涨但 GQA 压着——重算变贵更快，临界带宽反而下降。
- "命中率由什么决定？" → 负载形态：多轮对话/共享系统提示词/RAG 模板命中高；一次性长文摘要命中低——所以报告按负载分开给结论，不给单一数字。
- "3FS 为什么把 KVCache 列为目标场景？" → 池化的远端 KV 层需要高吞吐随机读 + RDMA 数据面——正是它的设计点；我的 S3 积累能解释它每一层的选型。

## 关联知识

- [[Week 7 - KV Cache + Prefix Cache + Paged KV]]（公式与 paged KV 地基）
- [[Week 8 - Prefill Decode + Open Source Repro]]（PD 分离与 KV transfer）
- [[Week 5 - Serving Benchmark Harness]]（S4 的实验载体）
- [[GPUDirect Storage 专题 - cuFile 与 bounce buffer]]（读回路径的直达选项）
- [[RDMA verbs 专题 - QP WQE CQ 状态机]]（跨节点搬运的机制）
- [[分布式存储阅读专题 - JuiceFS 与 3FS]]（远端池化层的对标系统）
- [[LLM 推理面试公式速算清单]]（口算的推理侧总表）
- [[00.存储方向专题清单索引]]
- LMCache / Mooncake 文档与设计笔记
