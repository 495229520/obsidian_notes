---
title: Week 8 - Prefill Decode + Open Source Repro
date: 2026-05-14
tags:
  - AIInfra
  - LLMServing
  - PrefillDecode
  - OpenSource
  - benchmark
status: active
---

# Week 8 - Prefill Decode + Open Source Repro

> [!goal] 本周目标
> 用 Week 5-7 的 benchmark、observability 和 KV cache 基础，专门分析 prefill-heavy / decode-heavy workload，并完成一个高质量开源 issue reproduction 或 benchmark report。重点不是“投递包装”，而是拿到一份可复现、可解释、能被 maintainer 看懂的工程证据。

## 学习目标

完成这一周后，应该能回答八个问题：

1. **prefill 和 decode 的瓶颈为什么不同？**
2. **什么 workload 是 prefill-heavy，什么 workload 是 decode-heavy？**
3. **为什么长 prompt 会推高 TTFT，长 output 会放大 TPOT / ITL 的影响？**
4. **prefill / decode disaggregation 适合什么场景，不适合什么场景？**
5. **KV cache transfer 的开销在哪里？**
6. **prefill worker 和 decode worker 的比例如何估算？**
7. **开源 issue reproduction 怎样写才像工程师？**
8. **Agent 可以帮你整理报告，但为什么不能替你判断 issue 是否成立？**

## 1. 为什么 Week 8 做 Prefill / Decode + Repro

经过 Week 5-7，你已经有：

- benchmark harness。
- metrics dashboard。
- KV cache / prefix cache / paged KV mental model。

接下来要把这些能力汇成一个外部证据：

```text
构造 workload
  -> 观察 TTFT / TPOT / queue / KV cache
  -> 判断 prefill-bound 或 decode-bound
  -> 最小化复现
  -> 写 issue / benchmark report
```

这比单纯包装简历更强，因为它能证明你真的会定位推理系统问题。

## 2. 必做实验场景

| 场景 | 配置 | 观察重点 |
|---|---|---|
| prefill-heavy | 长 prompt、短 output | TTFT、prefill time、GPU memory |
| decode-heavy | 短 prompt、长 output | TPOT / ITL、output TPS、KV cache read |
| mixed workload | prompt / output 长度混合 | queueing、tail latency、公平性 |
| high concurrency prefill | 长 prompt + 高 request rate | backlog、failed requests |
| shared prefix prefill | 长 shared prefix + 不同问题 | prefix cache 是否降低 TTFT |
| KV cache pressure | 长 context + 高并发 | KV cache usage、OOM / rejection |

## 3. 轻量实验路径

不真正部署 disaggregated serving，也能完成本周最低目标：

- 用 vLLM / SGLang benchmark 构造长 prompt / 短 output。
- 构造短 prompt / 长 output。
- 构造混合负载。
- 对比 TTFT、TPOT、TPS、RPS、queue time、GPU memory、KV cache usage。
- 写出 prefill-heavy 与 decode-heavy 的判断标准。

最低产出：

- `prefill_decode_report.md`
- `benchmark_results.csv`
- `workload_config.yaml`
- `reproduce.sh`

## 4. 进阶实验路径

如果 GPU、网络和时间允许，再尝试：

- vLLM disaggregated prefilling example。
- SGLang PD disaggregation。
- 画出 prefill worker、decode worker、KV transfer、request router 的系统图。
- 测试拆分前后 TTFT、TPOT、TPS、GPU memory、网络 / NVLink 传输压力。

> [!warning] 不要硬上复杂部署
> disaggregation 的价值是理解资源隔离、KV transfer 和调度 tradeoff。如果部署成本过高，先用轻量 workload 分析写出清楚报告，比半成品集群更有价值。

## 5. 开源贡献冲刺

本周至少选择一个目标：

1. vLLM / SGLang benchmark issue reproduction。
2. vLLM / SGLang docs 中 benchmark 参数或 metrics 说明的改进建议。
3. FlashInfer example reproduction。
4. TensorRT-LLM benchmark report。
5. Triton benchmark / tutorial 小 PR。

四步法：

### Step 1：Reproduce

- 找一个 issue、discussion 或 benchmark gap。
- 在自己的环境复现。
- 记录版本、命令、GPU、日志和结果。
- 判断是否稳定复现。

### Step 2：Minimize

- 缩小到最小模型。
- 缩小到最小配置。
- 缩小到最小 prompt / output。
- 去掉无关变量。
- 写 `reproduce.sh`。

### Step 3：Analyze

- 如果是性能问题，补 benchmark 表。
- 如果是 kernel 问题，补 Nsight Compute / Systems 证据。
- 如果是 serving 问题，补 TTFT / TPOT / queue / KV cache 指标。
- 如果是文档问题，补正确命令和解释。

### Step 4：Contribute

- 发 issue reproduction。
- 发 benchmark report。
- 发 docs PR。
- 发小 bugfix PR。
- 等 maintainer 回复后再尝试更深入修改。

## 6. 实现顺序

### Day 1：prefill / decode 指标拆解

- 复习 TTFT、TPOT / ITL、TPS、RPS。
- 写 `workload_config.yaml`。
- 定义 prefill-heavy / decode-heavy / mixed 三类 workload。

验收：

- 能说明 TTFT 主要受 queueing + prefill + 网络影响，TPOT / ITL 更贴近 decode 阶段体验。

### Day 2：prefill-heavy benchmark

- 长 prompt、短 output。
- 扫 request rate 和 max concurrency。
- 观察 TTFT、queue time、GPU memory、KV cache usage。

验收：

- 能说明为什么长 prompt 会推高 TTFT。

### Day 3：decode-heavy benchmark

- 短 prompt、长 output。
- 扫 max concurrency 和 output length。
- 观察 TPOT / ITL、output TPS、p95 latency。

验收：

- 能说明为什么 decode 阶段更关注连续 token 间隔和 KV cache read。

### Day 4：mixed workload

- 混合短 prompt、长 prompt、短 output、长 output。
- 观察 tail latency 和 failed requests。

验收：

- 能说明混合负载为什么比固定 prompt 更接近真实 serving。

### Day 5：选择开源 reproduction 目标

- 找 vLLM / SGLang / FlashInfer / TensorRT-LLM 的 issue、discussion 或 docs gap。
- 确认目标不需要改核心代码也能贡献。
- 写 reproduction plan。

验收：

- 有明确链接、版本、命令、预期现象和实际现象。

### Day 6：最小化和报告

- 写 `reproduce.sh`。
- 保存日志和原始 benchmark 数据。
- 写 `issue_reproduction.md` 或 `benchmark_report.md`。

验收：

- 第三个人可以按你的命令复跑。

### Day 7：发布前审查 + 面试表达

- 检查是否夸大结论。
- 检查所有图表是否追溯到 CSV / JSON。
- 检查 issue wording 是否礼貌、清晰、可复现。

形成一句项目表达：

```text
构建 prefill / decode workload analysis，比较长 prompt、长 output、mixed workload 下 TTFT、TPOT / ITL、queue time、KV cache usage 和 failed requests，并基于最小复现脚本产出 vLLM / SGLang issue reproduction 或 benchmark report。
```

## 7. 本周产出文件

| 文件 | 内容 | 验收方式 |
|---|---|---|
| `workload_config.yaml` | prefill-heavy、decode-heavy、mixed workload | 配置清楚 |
| `benchmark_results.csv` | 原始 benchmark 数据 | 不手动修改 |
| `prefill_decode_report.md` | 指标分析和判断标准 | 结论能被数据支持 |
| `reproduce.sh` | 最小复现命令 | 可复跑 |
| `issue_reproduction.md` 或 `benchmark_report.md` | 开源贡献材料 | maintainer 能看懂 |

## 8. 验收标准

- 至少完成 prefill-heavy、decode-heavy、mixed 三类 workload。
- 能解释 TTFT、TPOT / ITL、TPS、RPS、queue time、KV cache usage 的变化。
- 能说明什么场景适合 prefill / decode disaggregation，什么场景不适合。
- 能说明 KV cache transfer 的收益和代价。
- 至少完成一个高质量 issue reproduction 或 benchmark report 草稿。
- Agent 生成的报告必须人工核对指标和结论。

## 面试问题

- prefill 和 decode 的瓶颈有什么不同？
- 为什么长 prompt 推高 TTFT？
- 为什么长 output 更容易暴露 TPOT / ITL 问题？
- 什么 workload 适合 prefill / decode disaggregation？
- 什么 workload 不适合 disaggregation？
- KV cache transfer 的开销在哪里？
- prefill worker 和 decode worker 比例如何估算？
- issue reproduction 为什么要最小化？
- benchmark report 怎样避免“看起来很漂亮但不可复现”？
- Agent 在开源贡献中可以帮什么，不能替你做什么？

## 关联知识

- [[Week 5 - Serving Benchmark Harness]]
- [[Week 6 - Observability + Metrics]]
- [[Week 7 - KV Cache + Prefix Cache + Paged KV]]
- [[AI Agent Native AI Infra GPU Performance Engineer 培养方案]]
- [vLLM disaggregated prefilling](https://docs.vllm.ai/en/v0.14.0/features/disagg_prefill/)
- [SGLang PD Disaggregation](https://docs.sglang.io/docs/advanced_features/pd_disaggregation)
