---
title: Week 5 - Serving Benchmark Harness
date: 2026-05-14
tags:
  - AI-infra/素材库-GPU与推理方向/GPU周计划/计划
status: active
---

# Week 5 - Serving Benchmark Harness

> [!goal] 本周目标
> 把 Week 1-4 的 CUDA / profiling 基础接到真实 LLM serving 指标上：不再只记录一个 tokens/s，而是能设计低延迟、高吞吐、长上下文、共享 prefix、batching 对比五类负载，并解释 TTFT、TPOT / ITL、TPS、RPS、p95 latency 和 failed requests 为什么变化。

## 学习目标

完成这一周后，应该能回答七个问题：

1. **什么是可信 serving benchmark？** 能固定模型、版本、GPU、prompt / output 分布、request rate、max concurrency 和 warmup。
2. **TTFT / TPOT / ITL / TPS / RPS 分别看什么？** 能区分首 token、连续 token、总吞吐和请求吞吐。
3. **request rate 和 max concurrency 有什么区别？** 能解释到达率、排队、并发上限和服务端调度的关系。
4. **为什么平均 TPS 不够？** 能补充 p50 / p95 / p99 latency、failed requests 和 tail latency。
5. **为什么真实负载不是固定 prompt？** 能构造 prompt length distribution、output length distribution、shared prefix ratio 和 long context ratio。
6. **benchmark 结果怎样复现？** 能留下 `benchmark_config.yaml`、原始 CSV / JSON、命令和环境。
7. **Agent 在这里做什么？** Agent 可以生成脚本和表格，但不能改 benchmark 数据，也不能替你下性能结论。

## 1. 为什么 Week 5 改成 Serving Benchmark

前四周已经完成 CUDA 基础 kernel、reduction / profiling、transpose / memory coalescing 和 matmul v0。现在继续横向刷更多 toy kernel，收益会下降；更重要的是把 kernel 能力接到推理系统问题上。

这周的重点不是“vLLM 跑起来了”，而是：

```text
负载设计 -> 运行 benchmark -> 保存原始数据 -> 解释指标变化 -> 写可复现报告
```

CUDA / Triton 在这一周仍然是支撑能力：你需要用它们理解 GPU busy、memory bandwidth、batching 和 kernel launch，但主线已经切到 LLM serving。

## 2. 必做实验场景

| 场景 | 负载特征 | 主要观察 |
|---|---|---|
| 低延迟 | 低 request rate、低 max concurrency、短 prompt、短 output | TTFT / TPOT 是否稳定 |
| 高吞吐 | 高 request rate、高 max concurrency、中等 prompt / output | TPS 上升时 p95 latency 如何恶化 |
| 长上下文 | 长 prompt、短 output 或长 prompt、长 output | TTFT、GPU memory、KV cache 压力 |
| 共享 prefix | 相同 system prompt / tools schema / 长文档 prefix，不同用户问题 | prefix cache 对 TTFT 和成本的影响 |
| batching 对比 | 固定 batch baseline vs online request arrival | static batching 与 continuous batching 对 TPS、TPOT、tail latency 的影响 |

可选扩展：

- streaming vs non-streaming。
- quantization on / off。
- prefix caching on / off。
- chunked prefill on / off。
- batching mode：static / continuous。
- request arrival pattern：steady / bursty / mixed。
- vLLM vs SGLang。

> [!note] 与后续成本专项的边界
> Week 5 的主线是先把 serving benchmark harness 跑可信，所以 quantization on / off 作为可选扩展；到后续 `llm-serving-cost-benchmark` 或 quantization 成本专项时，FP16 / BF16 vs INT8 / FP8 / INT4 才是必做对比。

## 3. benchmark 配置模板

每次实验都从配置文件开始，不从临时命令开始。

```yaml
model:
  name: Qwen2.5-1.5B-Instruct
  dtype: bfloat16
  max_model_len: 4096

engine:
  name: vllm
  version: TBD
  gpu_memory_utilization: 0.90
  prefix_caching: false
  chunked_prefill: TBD
  batching_mode: continuous

environment:
  gpu: TBD
  gpu_count: 1
  cuda: TBD
  driver: TBD
  pytorch: TBD
  triton: TBD

workload:
  total_requests: 512
  request_rate: 4
  max_concurrency: 32
  burstiness: 1.0
  request_arrival_pattern: steady
  prompt_length_distribution: fixed_512
  output_length_distribution: fixed_128
  shared_prefix_ratio: 0.0
  long_context_ratio: 0.0
  streaming: false

measurement:
  warmup_runs: 1
  repeats: 3
  raw_result: benchmark_results.csv
```

## 4. 必须记录指标

| 类别 | 指标 |
|---|---|
| Latency | TTFT p50 / p95 / p99、TPOT / ITL p50 / p95 / p99、E2E latency p50 / p95 / p99 |
| Throughput | output TPS、RPS、goodput，如果工具支持 |
| Resource | GPU memory、GPU utilization、KV cache usage |
| Reliability | total requests、failed requests、error message |
| Cost | GPU price assumption、cost / 1M output tokens、cost / 1M total tokens |

> [!warning] 不要只看平均 TPS
> 高并发下 TPS 上升但 TPOT / p95 latency 变差，是 serving 系统里非常常见的 tradeoff。报告必须同时写“系统总吞吐”和“单请求体验”。

## 5. 实现顺序

### Day 1：指标和负载设计

- 阅读 vLLM `bench serve` 参数和 NVIDIA GenAI-Perf 指标解释。
- 写第一版 `benchmark_config.yaml`。
- 固定模型、GPU、dtype、prompt / output 长度、request rate、max concurrency。

验收：

- 能解释 TTFT、TPOT / ITL、E2E latency、TPS、RPS 的区别。
- 能说明本周 benchmark 想模拟哪种真实 workload。

### Day 2：跑通单引擎 baseline

- 启动 vLLM OpenAI-compatible server。
- 用最小请求数跑通 `vllm bench serve` 或等价脚本。
- 保存原始 JSON / CSV，不手动改数据。

验收：

- `reproduce.sh` 能从启动 server 到跑 benchmark。
- `benchmark_results.csv` 至少有一组 baseline。

### Day 3：request rate / max concurrency 矩阵

- 固定 prompt / output 长度。
- 扫 request rate：低、中、高。
- 扫 max concurrency：低、中、高。
- 如果工具支持，记录 static batching baseline 和 continuous batching serving 结果的差异。
- 观察 p95 latency 开始恶化的拐点。

验收：

- 能解释为什么 max concurrency 增大后 TPS 可能上升，但 TPOT / p95 latency 变差。
- 能解释固定 batch benchmark 为什么不等于真实 online serving。

### Day 4：prompt / output length 矩阵

- 短 prompt + 短 output。
- 长 prompt + 短 output。
- 短 prompt + 长 output。
- 长 prompt + 长 output。

验收：

- 能区分 prefill-heavy 和 decode-heavy workload。
- 能说明长 prompt 为什么推高 TTFT。

### Day 5：共享 prefix 初版

- 构造完全随机 prompt。
- 构造共享 system prompt。
- 构造共享 system prompt + tools schema。
- 如果框架支持，比较 prefix cache off / on。
- 如果框架支持，比较 chunked prefill off / on。

验收：

- 能解释 shared prefix 为什么接近 Agent / RAG / coding assistant 场景。
- 能说明 prefix cache 和 chunked prefill 分别影响 TTFT、TPOT 还是 throughput。

### Day 6：报告

写 `benchmark_report.md`：

- 实验环境。
- benchmark 命令。
- workload 配置。
- 原始结果路径。
- 关键表格。
- 异常数据解释。
- 不确定性和限制。

### Day 7：复盘 + 面试表达

形成一句项目表达：

```text
构建 vLLM / SGLang serving benchmark harness，覆盖 request rate、max concurrency、prompt / output length、shared prefix workload，统计 TTFT、TPOT / ITL、TPS、RPS、p95 latency、KV cache usage 和 cost / 1M tokens，并分析延迟-吞吐-成本权衡。
```

## 6. 本周产出文件

| 文件 | 内容 | 验收方式 |
|---|---|---|
| `benchmark_config.yaml` | 模型、环境、engine、负载、测量规则 | 不看代码也能复现实验设计 |
| `benchmark_results.csv` | 原始结果 | 不手动修改 |
| `benchmark_report.md` | 表格、图、结论、限制 | 结论能追溯到原始数据 |
| `reproduce.sh` | 启动 server 和运行 benchmark | 新环境可复跑 |
| `serving_metrics.md` | 指标定义和观察记录 | 能解释 TTFT / TPOT / TPS / RPS |

## 7. 验收标准

- 至少覆盖低延迟、高吞吐、长上下文、共享 prefix、batching 对比五类场景。
- 至少补充一次 static batching vs continuous batching 的解释或对比。
- 每个实验都有环境、命令、配置和原始结果。
- 配置中显式记录 `shared_prefix_ratio` 和 `long_context_ratio`，避免把长上下文场景只写成文字描述。
- Week 5 可以不做 quantization 对比，但如果启用 quantization，必须记录 dtype / quantization 配置和成本口径；后续成本专项必须补齐量化对比。
- 能解释 TTFT 低但 TPS 不高的情况。
- 能解释 TPS 上升但 TPOT / p95 latency 变差的情况。
- 能解释 failed requests 的原因。
- 能说明 benchmark 是否公平、可复现、可解释。

## 面试问题

- TTFT 和 TPOT / ITL 分别代表什么？
- request rate 和 max concurrency 有什么区别？
- 为什么不能只报告平均 TPS？
- 为什么长 prompt 会推高 TTFT？
- 为什么高并发下 TPS 可能上升但 p95 latency 变差？
- static batching 和 continuous batching 的区别是什么？
- 为什么固定 batch benchmark 不能代表真实 online serving？
- chunked prefill 可能改善什么，又可能伤害什么？
- random prompt benchmark 和 Agent / RAG workload 有什么差异？
- 你怎么证明这个 benchmark 可复现？
- Agent 生成 benchmark 脚本时你如何防止它改数据？

## 关联知识

- [[AI Agent Native AI Infra GPU Performance Engineer 培养方案]]
- [[Week 4 - MatMul v0]]
- [[Week 6 - Observability + Metrics]]
- [vLLM bench serve](https://docs.vllm.ai/en/latest/api/vllm/benchmarks/serve/)
- [NVIDIA GenAI-Perf guide](https://developer.nvidia.com/blog/llm-performance-benchmarking-measuring-nvidia-nim-performance-with-genai-perf/)
