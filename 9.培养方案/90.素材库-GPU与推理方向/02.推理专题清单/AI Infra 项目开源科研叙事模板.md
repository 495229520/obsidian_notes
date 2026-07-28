---
title: AI Infra 项目开源科研叙事模板
date: 2026-05-24
tags:
  - AI-infra
  - 素材库-GPU与推理方向
  - 推理专题清单
  - 模板
roadmap_week: Week 8, Week 16, Week 17+
sort_order: "08.30"
status: active
---

# AI Infra 项目开源科研叙事模板

> [!info] 所属路线
> - 总纲 Week：Week 8，Week 16，Week 17+
> - 排序：08.30
> - 用途：把 benchmark report、issue reproduction、开源 PR 和科研项目转成面试叙事。

> [!goal] 目标
> 把 AI Infra 项目、开源贡献、科研经历和实习经历整理成面试官能追问、自己能用证据支撑的叙事结构，服务于 [[AI Agent Native AI Infra GPU Performance Engineer 培养方案]] 的作品集和面试表达。

---

## 1. 统一叙事骨架

任何项目都按这条线讲：

```text
问题背景
-> baseline / 现状
-> 我的方法
-> correctness 验证
-> benchmark / profiling 证据
-> 结果
-> tradeoff
-> 复盘和下一步
```

这比“我做了 X，用了 Y 技术”更像工程师。

---

## 2. 30 秒版本

用于自我介绍或简历 bullet 后追问：

```text
我做的是 [问题]。原来的 baseline 是 [baseline]，主要瓶颈在 [bottleneck]。我实现/复现了 [method]，用 [correctness test] 保证正确性，再用 [benchmark/profiling] 验证效果。最后在 [workload] 下观察到 [result]，同时也发现 [tradeoff]。
```

示例占位：

```text
我做的是 vLLM / SGLang serving benchmark。baseline 是默认 benchmark 配置，但它没有覆盖 shared prefix 和 long context。我构造了 random prompt、shared system prompt 和长上下文 workload，记录 TTFT、TPOT、p95 latency、KV cache usage 和 failed requests，最后说明 prefix cache 在 shared prefix 场景下降低 TTFT，但在 random prompt 下收益不明显。
```

---

## 3. 开源贡献讲法

不要只说：

```text
给某项目提过 PR。
```

要说：

```text
Issue / gap -> root cause -> reproduction -> fix / report -> maintainer feedback -> impact
```

模板：

```text
我当时发现 [issue / docs gap / benchmark gap]。先用 [版本、命令、配置] 做了最小复现，确认问题来自 [root cause]。然后我提交了 [PR / issue reproduction / benchmark report]，包含 [test / logs / benchmark table]。这个贡献的价值是 [帮助 maintainer 复现 / 修正文档 / 补充 benchmark case]。
```

简历 bullet 模板：

```text
- Reproduced and minimized [project] [issue type] under [environment], provided [script/log/benchmark] and proposed [fix/docs/report], clarifying [root cause or metric impact].
```

中文版本：

```text
- 复现并最小化 [项目] 中的 [问题类型]，补充 [脚本/日志/benchmark 表]，定位 [原因]，提交 [PR/issue/report] 帮助维护者验证。
```

---

## 4. 科研项目讲法

科研项目不要只讲“用了两个 LLM、启发式搜索、差分测试”，要补证据链：

```text
研究问题 -> 为什么难 -> 方法设计 -> 对照实验 -> 失败 case -> 局限性
```

模板：

```text
这个项目解决的是 [research problem]。难点在于 [search space / correctness / evaluation]。我的方法是 [method]，核心创新是 [one sentence]。为了验证它，我设计了 [baseline / ablation / metric]。最终结果是 [result]，但它在 [limitation] 上仍有限制，所以我后续会 [next step]。
```

面试官通常会追：

- 贡献到底是不是你的？
- baseline 是什么？
- 失败 case 是什么？
- 结果有没有统计意义？
- 有没有开源或可复现？

---

## 5. Kernel 项目讲法

模板：

```text
我实现了 [kernel] 的 CUDA / Triton 版本。正确性上用 [PyTorch reference]，覆盖 [shape / dtype / edge cases]，按 dtype 设置 [atol / rtol]。性能上用 [CUDA event / Nsight Compute]，和 [baseline] 比较。这个 kernel 的瓶颈是 [memory-bound / compute-bound]，证据是 [throughput / occupancy / stall reason]。优化后 [result]，但代价是 [tradeoff]。
```

必须准备的追问：

- 为什么这个 kernel 是 memory-bound / compute-bound？
- benchmark 有没有 warmup？
- 为什么不直接用 PyTorch / cuBLAS / Triton official？
- shape 换了还快吗？
- FP16 / BF16 / FP32 误差怎么设？

---

## 6. Serving benchmark 项目讲法

模板：

```text
我构造了 [workload]，比较 [engine/config] 在 [request rate / concurrency / prompt length / output length] 下的表现。记录指标包括 TTFT、TPOT / ITL、TPS、RPS、p95 latency、queue time、KV cache usage 和 failed requests。结论不是只看平均 TPS，而是解释 [latency-throughput tradeoff]。
```

必须准备的追问：

- workload 是否贴近真实 online serving？
- random prompt 和 shared prefix 有什么差异？
- max concurrency 增大为什么 p95 变差？
- chunked prefill 改善什么，牺牲什么？
- prefix cache 为什么不改变模型输出？

---

## 7. 简历 bullet 写法

弱表达：

```text
熟悉 FlashAttention、PagedAttention、vLLM、SGLang。
```

强表达：

```text
- Built a vLLM/SGLang benchmark harness covering short/long prompt, shared-prefix and decode-heavy workloads; reported TTFT/TPOT p95, KV cache usage and failed requests to analyze latency-throughput tradeoffs.
```

弱表达：

```text
参与开源贡献。
```

强表达：

```text
- Minimized a Triton lowering/codegen issue with reproducible inputs and reference outputs; documented root cause hypothesis and submitted a PR / issue report with correctness evidence.
```

---

## 8. 反面清单

> [!warning] 不要堆关键词
> “PagedAttention、CUDA Graph、TP、chunked prefill 都了解”不如讲清一个 benchmark 的 workload、指标和结论。

> [!warning] 不要夸大贡献
> 如果只是复现 issue，就说复现和分析；如果没有 merged PR，不要暗示已经贡献核心代码。

> [!warning] 不要只有结果没有方法
> “提升 20%”必须接 benchmark 条件、baseline、warmup、重复次数和指标，否则可信度不足。

---

## 9. 自测问题

1. 你的项目 baseline 是什么？
2. correctness 怎么验证？
3. benchmark 怎么保证公平？
4. profiling 证据是什么？
5. 如果面试官质疑结果，你能拿出什么原始数据？
6. 这个项目最失败或最不确定的地方是什么？
