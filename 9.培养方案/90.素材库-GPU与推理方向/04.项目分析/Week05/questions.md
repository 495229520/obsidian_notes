---
tags:
  - AI-infra/素材库-GPU与推理方向/项目分析/Week05
---
# Week05 必答问题

> 配合 [[Week 5 ServingBench 服务基准测试框架 项目解析]]。这些是 plan 的 7 个学习目标 + README/阶段计划里的核心面试问题，逐题展开。

---

## 1. TTFT 和 TPOT / ITL 分别代表什么？

**答：** 

- **TTFT**（Time To First Token）：从发出请求到收到**第一个** token 的时间，主要由 **prefill**（处理整个 prompt）决定，反映"开始响应有多快"。
- **TPOT**（Time Per Output Token）：平均每个**后续** output token 的时间 ≈ `(E2E − TTFT) / (output_len − 1)`，由 **decode** 决定，反映"打字速度"。
- **ITL**（Inter-Token Latency）：相邻两 token 的实际间隔。TPOT 是 ITL 的一种平均口径，ITL 分布能暴露 decode 阶段的 tail 卡顿。

每个都记 p50/p95/p99，不只记均值。

---

## 2. request rate 和 max concurrency 有什么区别？

**答：** 

- **request rate**：到达率，每秒**发出**多少请求。开环——可以超过服务端处理能力，超出部分在服务端排队。`inf` 表示尽可能快地发完。
- **max concurrency**：同时**在飞**请求的上限。闭环——达到上限后客户端不再发新请求，完成一个才补一个。

rate 决定"来多快"，concurrency 决定"同时最多几个"。固定 rate 但 concurrency 太低会人为限制吞吐；`rate=inf` + `concurrency=N` 近似"稳态 N 路并发压测"。`burstiness` 再控制到达抖动（1.0=Poisson）。

---

## 3. 为什么不能只报告平均 TPS？

**答：** 平均 TPS 是**系统总吞吐**，掩盖**单请求体验**。高并发下典型 tradeoff 是 TPS 上升但 TPOT / p95 latency 变差——更多请求被批在一起，单请求每个 token 等得更久。所以报告必须同时写"系统总吞吐 (TPS/RPS)"和"单请求体验 (TTFT/TPOT 的 p95/p99)"。只看 p50 还会漏掉 tail，p95/p99 才反映最差用户的体验，再补 failed 数。

---

## 4. 为什么长 prompt 会推高 TTFT？

**答：** prefill 阶段必须先处理完整个 prompt 才能产出首 token，其计算量随 prompt 长度线性增长，所以 prompt 越长，首 token 之前要算的越多 → TTFT 越高。而 decode 阶段每步只在已有 KV cache 上生成一个新 token，单步成本受**初始 prompt 长度**影响小 → TPOT 基本稳定。因此长上下文是 prefill-heavy："首 token 慢、后续打字不慢"。smoke 实测：long_context（3072 prompt）TTFT p95 1478.6 ms，但 TPOT p95 仅 6.9 ms。

---

## 5. 为什么高并发下 TPS 可能上升但 p95 latency 变差？

**答：** 并发 / batch 变大时，GPU 在每个调度步能把更多请求拼成一个 batch 一起算，算力利用率更高 → 系统 output TPS 上升。但代价是每个请求的 decode 都要和 batch 里其它请求**争 GPU**，单个 token 的生成被推后 → TPOT 升高、E2E 拉长、p95/p99 尾延迟恶化。这就是延迟-吞吐权衡：吞吐和单请求体验往相反方向走，必须并列报告。

---

## 6. static batching 和 continuous batching 的区别？为什么固定 batch 不能代表真实在线服务？

**答：** 

- **continuous batching（在线）**：`vllm bench serve` 打活 server，请求按到达率陆续进来，vLLM 每个调度步**动态**把可运行请求拼成 batch，完成的请求立即让位。能同时报告 TPS **和** TTFT/TPOT/p95。
- **static batching（离线基线）**：`vllm bench throughput`，一次性给定固定数量 prompt、固定 batch、无逐请求到达，只测峰值 tokens/s，**没有** TTFT/TPOT/排队。

固定 batch 假设所有请求同时就绪、长度一致、没有排队和到达抖动，因此 tokens/s 偏乐观；真实在线请求陆续到达、长短不一，只有 continuous batching 才反映真实的延迟-吞吐权衡。smoke 里 static 73 TPS 远高于在线 smoke，正说明它不能单独下结论。

---

## 7. chunked prefill 可能改善什么，又可能伤害什么？

**答：** chunked prefill 把长 prompt 的 prefill 切成小块，与其它请求的 decode **交错调度**。

- **改善**：高并发下，长 prompt 的 prefill 不再一次性独占 GPU，decode 请求不会被一个大 prefill 卡住 → decode 的 **TPOT / ITL 更平滑**。
- **伤害**：单条长 prompt 的 prefill 被切块、穿插执行，它自己的 **TTFT 可能略微升高**。

属可选扩展，需用 server 启动 flag 开关并对比。

---

## 8. random prompt benchmark 和 Agent / RAG workload 有什么差异？

**答：** `--dataset-name random` 生成均匀随机 token，每个请求独立、无共享结构。真实 Agent / RAG / 编码助手流量则是**长共享前缀（system prompt + tools schema / 检索到的长文档）+ 各异的用户问题**——大量 token 在请求间重复，正好能命中 prefix cache。`datasets.py` 的 `gen_shared_prefix` / `gen_long_context` 就是为模拟这种结构而生：固定前缀 + 每请求 `(request #i, vary=...)` 微扰。所以纯 random 会低估 prefix cache 在真实场景的收益。

---

## 9. 你怎么证明这个 benchmark 可复现？

**答：** 三件事落到位：

1. **配置即真相**：固定模型、版本（vLLM/PyTorch/Triton）、GPU/driver、dtype、seed、prompt/output 分布、request rate、max concurrency、warmup，全写进一个 `benchmark_config.yaml` + 场景文件，连 `environment` 和 `cost` 假设都记。
2. **原始数据 + 命令**：`run_benchmark.py` 跑前先打印完整命令（可复制粘贴），原始 JSON 原样存 `results/raw/`，不手改。
3. **一键重跑**：`reproduce.sh` 从启动 server → 健康检查 → benchmark → 聚合 → 关闭全自动，新环境照跑。

可复现性归约成"仅凭 YAML + reproduce.sh 能重建"，而不依赖记忆里的命令行。

---

## 10. Agent 生成 benchmark 脚本时，你如何防止它改数据？

**答：** 靠职责边界把"测量"和"解释"分开：

- harness **不自己算指标**，全交给官方 `vllm bench`（可信指标数学）。
- `results/raw/*.json` 是 vllm bench 原样输出，summarize 只读不写它；聚合结果单独落 CSV。
- Agent 可以生成脚本、生成表格，但**不改 benchmark 数据，也不替你下性能结论**——下结论的是人。

这样"benchmark 是否公平可信"就不依赖一段易错/可能被 Agent 篡改的自研统计代码，而只取决于"配置是否固定、命令是否记录"。

---

## 11. 什么是可信的 serving benchmark？

**答：** 能**固定**全部混淆变量并**可复现**：固定模型、版本、GPU、prompt/output 分布、request rate、max concurrency、warmup、seed；保存原始数据和命令；指标计算交给可信的官方实现而非自研。再加上"系统吞吐和单请求体验并列、延迟看分位数、failed 必须解释原因"，才算公平、可复现、可解释。这正是本项目五层架构 + 三条铁律（配置单一真相 / 包装可信指标 / 原始数据不改）要保证的。

---

## 12. failed requests 通常是什么原因？（结合 6 GB 本机）

**答：** 常见四类：(a) prompt+output 超出 `max_model_len`；(b) KV cache / 显存不足——6 GB Turing 上长上下文 + 高并发尤其容易 OOM 或被 server 拒绝；(c) 超时 / 连接被并发压垮；(d) `request_rate=inf` 且无 `max_concurrency` 时排队过深。报告要写清是哪一类。本次 smoke 五类全部 0 failed，是因为缩小了请求数/token 数并把长上下文并发压到 2。
