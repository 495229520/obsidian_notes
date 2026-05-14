---
title: Week 7 - KV Cache + Prefix Cache + Paged KV
date: 2026-05-14
tags:
  - AIInfra
  - LLMServing
  - KVCache
  - PrefixCache
  - PagedAttention
status: active
---

# Week 7 - KV Cache + Prefix Cache + Paged KV

> [!goal] 本周目标
> 把 Week 5-6 的 benchmark / observability 进一步推进到 KV cache、prefix cache 和 paged KV：能解释长上下文为什么贵、共享 prefix 为什么能省 prefill、paged KV cache 解决什么问题，以及这些机制如何影响 TTFT、TPOT、GPU memory 和 serving 成本。

## 学习目标

完成这一周后，应该能回答八个问题：

1. **KV cache 到底缓存什么？** 能区分 prefill 产生的 K / V 和 decode 阶段复用的 K / V。
2. **为什么长上下文会让 KV cache 成本变成瓶颈？**
3. **prefix cache 为什么能降低重复 prefill 成本？**
4. **random prompt benchmark 为什么可能低估 Agent / RAG / coding assistant workload 的优化空间？**
5. **paged KV cache 解决的是显存碎片、调度灵活性，还是 attention 计算复杂度？**
6. **page table / block table 为什么会影响 kernel 访存？**
7. **FlashInfer / vLLM 的 paged KV 思想如何作为 baseline，而不是从零硬写工业 attention？**
8. **指标上如何判断 prefix cache 或 paged KV 是否真的有收益？**

## 1. 为什么 Week 7 学 KV / Prefix / Paged KV

推理系统的核心瓶颈不是单个 RMSNorm。真实 serving 中更容易遇到：

```text
长 prompt -> prefill 贵 -> TTFT 变高
长 context -> KV cache 大 -> 显存压力变高
variable-length requests -> cache 管理复杂
共享 system prompt / tools schema -> prefix cache 有收益
decode attention -> 反复读 KV cache -> memory bandwidth 关键
```

这一周不要求写工业级 FlashAttention 或完整 vLLM。最低目标是做出 toy layout、benchmark 对比和设计说明。

## 2. 必做实验场景

| 场景 | 配置 | 观察重点 |
|---|---|---|
| random prompt | 每个请求完全不同 | prefix cache 基线 |
| shared system prompt | 相同 system prompt，不同用户问题 | TTFT 是否下降 |
| shared tools schema | system prompt + tools schema 相同 | Agent workload 收益 |
| shared long document | 长文档 prefix 相同 | 长 prefix 下 prefill 成本变化 |
| multi-turn context | 多轮对话重复历史上下文 | cache 命中与 memory 压力 |
| long context | 长 prompt、高并发 | KV cache usage、failed requests |
| variable-length batch | 不同 request 长度混排 | cache fragmentation / scheduling |

## 3. prefix cache benchmark

对比配置：

- prefix cache off。
- prefix cache on。
- 不同 shared prefix length。
- 不同 request rate。
- 不同 max concurrency。
- 不同 prompt / output ratio。

必须记录：

- TTFT。
- TPOT / ITL。
- TPS / RPS。
- GPU memory。
- KV cache usage。
- prefix cache hit rate，如果框架暴露。
- failed requests。
- cost / 1M tokens。

验收问题：

- prefix cache 为什么不改变模型输出？
- 什么场景下 prefix cache 收益最大？
- prefix cache 和 chunked prefill 同时存在时，可能带来什么调度问题？

## 4. toy paged KV cache

先做极简模型，不追求完整 kernel：

```text
request_id
  -> logical token positions
  -> block table
  -> physical KV pages
  -> batch decode attention reads pages
```

实现内容：

- toy contiguous KV cache。
- toy paged KV cache。
- page table / block table。
- variable-length sequence batch。
- batch decode attention toy version。
- 可选：调用 FlashInfer paged KV cache attention wrapper 做 baseline。

对比实验：

| 对比 | 目的 |
|---|---|
| contiguous KV vs paged KV | 观察碎片和调度灵活性 |
| fixed length vs variable length | 观察不同长度 request 的 cache 管理 |
| small batch decode vs large batch decode | 观察 decode 阶段 memory read 压力 |
| short context vs long context | 观察 KV cache footprint |
| 不同 page size | 观察 page size 对 memory 和 latency 的影响 |

## 5. CUDA / Triton 支撑任务

这一周可以补少量 kernel 视角，但不要把主线改回 toy kernel：

- 写一个简化版 batch decode attention，只处理小 shape。
- 用 PyTorch reference 验证 correctness。
- 用 CUDA event 或 PyTorch benchmark 记录 latency。
- 写 `profiling.md` 解释为什么 decode attention 主要受 KV cache 读带宽影响。

> [!important] 重点
> 这里的 kernel 只是为了理解 serving engine 的 cache layout，不是为了从零复刻 vLLM / FlashInfer。

## 6. 实现顺序

### Day 1：KV cache mental model

- 画出 prefill 产生 K / V、decode 复用 K / V 的流程。
- 记录每层 KV cache 的内存量估算公式。
- 写 `kv_cache_notes.md`。

验收：

- 能说明为什么 batch、seq length、num layers、num heads、head dim、dtype 都会影响 KV cache memory。

### Day 2：prefix cache workload

- 生成 random prompt 和 shared prefix 两类请求。
- 对比 prefix cache off / on。
- 保存 `prefix_cache_benchmark.md`。

验收：

- 能解释 shared system prompt / tools schema 为什么贴近 Agent 场景。

### Day 3：长上下文 + cache pressure

- 增大 prompt length。
- 增大 max concurrency。
- 观察 GPU memory、KV cache usage、failed requests。

验收：

- 能说明 max model len、concurrency、GPU memory utilization 之间的关系。

### Day 4：toy contiguous KV

- 写 contiguous KV layout 说明。
- 用固定长度 batch 做 baseline。

验收：

- 能画出连续内存布局。

### Day 5：toy paged KV

- 写 page table / block table。
- 模拟 variable-length sequence batch。
- 记录 page size 变化。

验收：

- 能说明 paged KV cache 解决显存碎片和调度灵活性，不是直接降低 attention 复杂度。

### Day 6：FlashInfer / vLLM baseline 阅读

- 阅读 FlashInfer paged KV wrapper。
- 阅读 vLLM PagedAttention / prefix caching 设计文档。
- 写 `design_note.md`。

### Day 7：报告 + 面试表达

形成一句项目表达：

```text
构建 KV cache / prefix cache / paged KV lab，设计 random prompt、shared system prompt、tools schema、长文档 prefix 和 long-context workload，对比 prefix cache on/off 的 TTFT、TPOT、KV cache usage 与 cost，并用 toy paged KV cache 解释 block table、variable-length request 和 batch decode attention 的 serving 价值。
```

## 7. 本周产出文件

| 文件 | 内容 | 验收方式 |
|---|---|---|
| `kv_cache_notes.md` | KV cache memory model 和 prefill / decode 流程 | 能解释显存成本 |
| `prefix_cache_benchmark.md` | prefix cache on/off 对比 | 有原始数据和结论 |
| `paged_kv_layout.md` | contiguous KV vs paged KV 图和说明 | 能解释 block table |
| `attention_decode_benchmark.csv` | toy batch decode 对比 | 数据可追溯 |
| `design_note.md` | FlashInfer / vLLM reference 阅读总结 | 不从零硬写工业 kernel |

## 8. 验收标准

- 能解释 KV cache memory 如何随层数、head、head dim、seq length、dtype 增长。
- 至少完成 random prompt vs shared prefix 的 benchmark。
- 至少完成 prefix cache off / on 的对比。
- 能解释 paged KV 解决的问题和不能解决的问题。
- 能画出 request -> block table -> physical pages 的流程。
- 能说明长上下文下 TTFT、TPOT、GPU memory、failed requests 的关系。

## 面试问题

- KV cache 缓存的是什么？
- 为什么 decode attention 主要受 KV cache 读带宽影响？
- prefix cache 为什么能降低重复 prefill 成本？
- prefix cache 会不会改变模型输出？
- random prompt benchmark 为什么可能不代表 Agent / RAG workload？
- paged KV cache 解决什么问题？
- page size 太大或太小分别有什么风险？
- 为什么 variable-length requests 会带来 cache 管理问题？
- FlashInfer / vLLM 可以作为 baseline，但为什么不能只说“调库就行”？

## 关联知识

- [[Week 5 - Serving Benchmark Harness]]
- [[Week 6 - Observability + Metrics]]
- [[Week 8 - Prefill Decode + Open Source Repro]]
- [[AI Agent Native AI Infra GPU Performance Engineer 培养方案]]
- [vLLM prefix caching](https://docs.vllm.ai/en/stable/design/prefix_caching/)
- [FlashInfer cascade wrappers](https://docs.flashinfer.ai/api/cascade.html)
