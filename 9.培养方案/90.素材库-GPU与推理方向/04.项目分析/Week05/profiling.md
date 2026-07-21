# Week05 Serving Benchmark 测量与结果

> 配合 [[Week 5 ServingBench 服务基准测试框架 项目解析]]。serving 没有 Nsight 那种 kernel profiling，"profiling" 在这里指**测量方法论 + 指标该看什么 + 结果回填**。本机已用缩小版 smoke 真实跑通五类 workload 形状（2026-05-31），下文给出真实 smoke 表 + 正式矩阵的**待填模板**——正式数字请在脱离 sandbox 的真机上 `make bench-all && make report` 后回填，不要照抄 smoke 值。

---

## 1. 运行方式

```bash
make setup                                   # 装 harness 轻量依赖（pyyaml/tabulate）
make check                                   # 离线自检：每个 config 解析成真实 argv
make datasets                                # 生成 shared-prefix / long-context JSONL（可选）

# 真机：端到端跑一个场景（启动→健康检查→benchmark→聚合→关闭）
bash reproduce.sh configs/low_latency.yaml
ENGINE=sglang bash reproduce.sh configs/high_throughput.yaml

make smoke && make report                    # 6 GB 本机快速实跑五类 + 聚合
make bench-all && make report                # 正式五类矩阵 + 聚合
```

> [!note] 本机执行约束（沿用仓库 test_results.md）
> - GPU benchmark 必须脱离 Codex sandbox 执行；sandbox 内 PyTorch 访问不到 CUDA。
> - 6 GB Turing 上长上下文 / 高并发易 OOM，必要时调小 `max_model_len`、`max_concurrency`、`gpu_memory_utilization` 并在报告里记录。
> - sm_75 无 FA2，vLLM 会回退 FlashInfer，属预期。

---

## 2. 该盯哪些指标

serving 的指标分四类，核心原则是**系统吞吐和单请求体验要并列看**，且延迟必须看分位数而非均值。

| 类别 | 指标 | 看什么 | 预期方向 |
|---|---|---|---|
| Latency | TTFT p50/p95/p99 | 首 token 快慢（prefill） | 长 prompt / 高并发下显著升高 |
| Latency | TPOT / ITL p50/p95/p99 | 打字速度稳不稳（decode） | 高并发下升高；长 prompt 影响小 |
| Latency | E2E p50/p95/p99 | 用户感知总时长 | ≈ TTFT + (out-1)×TPOT |
| Throughput | output TPS | 系统级吞吐（成本核心） | 并发↑ 先升后平 |
| Throughput | RPS / total token TPS | 请求/总 token 吞吐 | 同上 |
| Reliability | completed / failed | 完成与失败 | failed 必须解释原因 |
| Resource | KV cache usage | 长上下文 / 高并发瓶颈 | 从 server 日志/metrics 读 |
| Cost | cost/1M output·total | 钱 | 吞吐越高越便宜 |

> [!warning] 不要只报平均 TPS
> 高并发下 TPS↑ 常伴随 TPOT/p95↑（更多请求批在一起，单请求每 token 等更久）。报告必须同时写"系统总吞吐 (TPS/RPS)"和"单请求体验 (TTFT/TPOT 的 p95/p99)"，并只看 p50 会漏掉 tail。

---

## 3. 必须能解释的现象（plan 验收）

1. **TTFT 低但 TPS 不高**：单请求 prefill 轻 / 并发低，首 token 快，但同时在跑的请求少，系统吞吐自然不高。低延迟 ≠ 高吞吐。
2. **TPS 上升但 TPOT / p95 变差**：并发 / batch 变大 → GPU 算得更满（TPS↑），但每个请求 decode 要和更多请求争 GPU → 单 token 间隔变长。延迟-吞吐权衡的本质。
3. **长 prompt 推高 TTFT 不推 TPOT**：prefill 计算量随 prompt 长度增长；decode 受初始 prompt 长度影响小。长上下文是 prefill-heavy。
4. **failed requests 原因**：(a) 超 `max_model_len`；(b) KV cache / 显存不足（6 GB 易 OOM）；(c) 超时 / 连接被并发压垮；(d) `rate=inf` 且无 `max_concurrency` 排队过深。

---

## 4. 真实 smoke 结果（2026-05-31，已实测）

### 环境

```text
GPU:            NVIDIA GeForce GTX 1660 SUPER, 6144 MiB, sm_75
Driver / CUDA:  595.71.05 / 13.2 (nvidia-smi)
vLLM:           0.22.0
PyTorch:        2.11.0+cu130
Triton:         3.6.0
Model / dtype:  Qwen/Qwen2.5-0.5B-Instruct / float16
Cost 假设:       1.20 USD/hour
```

### smoke 汇总（缩小版，每场景 repeats=1）

| scenario | completed | failed | TTFT p95(ms) | TPOT p95(ms) | E2E p95(ms) | output TPS | RPS | cost/1M out |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| smoke_low_latency | 8 | 0 | 131.300 | 7.300 | 356.820 | 30.66 | 0.958 | 10.8719 |
| smoke_high_throughput | 16 | 0 | 1046.883 | 133.161 | 4774.937 | 27.09 | 0.846 | 12.3047 |
| smoke_long_context | 4 | 0 | 1478.647 | 6.939 | 1693.723 | 17.01 | 0.532 | 19.5963 |
| smoke_shared_prefix | 8 | 0 | 551.779 | 126.172 | 4463.039 | 15.19 | 0.475 | 21.9443 |
| smoke_batching_compare | 16 | 0 | n/a | n/a | n/a | 73.15 | 2.286 | 4.5568 |

原始文件：`results/raw/smoke_*_vllm_rep0.json`；聚合：`results/benchmark_results.csv`。

### smoke 观察

- **low_latency**：TTFT/TPOT 低且稳（131 / 7.3 ms），但并发=4，output TPS 仅 30.66 → "TTFT 低但 TPS 不高"。
- **high_throughput**：`rate=inf` / `max_concurrency=4` 把 TTFT p95 推到 1046.9 ms、TPOT p95 到 133.2 ms，tail 明显恶化 → 延迟-吞吐权衡。
- **long_context**：3072-token prompt 把 TTFT p95 推到 1478.6 ms，但 TPOT p95 仍 6.9 ms → 干净的 prefill-heavy。
- **shared_prefix**：256/800-token 共享前缀、cache-off，TTFT p95 551.8 ms；cache-on 待补跑对比。
- **batching_compare**：static/offline output TPS 73.15、cost/1M 最低 4.56，但**无** TTFT/TPOT/tail → 不能单独代表线上。
- 五个 smoke 全部 0 failed。

---

## 5. 正式五类矩阵（待回填）

> 用全量 config（非 smoke）`make bench-all REPEATS=3 && make report` 后回填。每格取 repeats 的代表值。

| scenario | engine | request_rate | max_concurrency | completed | failed | TTFT p95 | TPOT p95 | E2E p95 | output TPS | RPS | cost/1M out |
|---|---|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| low_latency | vllm |  |  |  |  |  |  |  |  |  |  |
| high_throughput | vllm |  |  |  |  |  |  |  |  |  |  |
| long_context | vllm |  |  |  |  |  |  |  |  |  |  |
| shared_prefix (nocache) | vllm |  |  |  |  |  |  |  |  |  |  |
| shared_prefix (cache) | vllm |  |  |  |  |  |  |  |  |  |  |
| batching_compare (static) | vllm | — | — |  | 0 | n/a | n/a | n/a |  |  |  |

### request rate / max concurrency 扫描（找 p95 拐点）

> 固定 prompt/output，复制 `high_throughput.yaml` 改两个值。

| request_rate | max_concurrency | TTFT p95 | TPOT p95 | output TPS | 备注 |
|---|---|---:|---:|---:|---|
| 4 | 16 |  |  |  |  |
| 8 | 64 |  |  |  |  |
| inf | 64 |  |  |  |  |
| inf | 256 |  |  |  |  |

### prefix cache on/off 对比（shared_prefix）

| prefix_caching | TTFT p95 | TPOT p95 | output TPS | cost/1M out |
|---|---:|---:|---:|---:|
| false (nocache) |  |  |  |  |
| true (cache) |  |  |  |  |

---

## 6. 分析（实测后填写）

### Observation


### Latency-Throughput Tradeoff


### Failed Requests（原因归类）


### Cost


### Fair / Reproducible / Explainable 自评


---

## 7. 速记

```bash
# 离线验证配置→命令（无 GPU）
python -m harness.config --config configs/shared_prefix.yaml --print-args
python -m harness.run_benchmark --config configs/batching_compare.yaml --dry-run

# 单独聚合 / 覆盖 cost 假设
python -m harness.summarize --result-dir results/raw --csv results/benchmark_results.csv
python -m harness.summarize --gpu-hourly-usd 1.20
```

指标定义见仓库 `docs/serving_metrics.md`；负载与 flag 映射见 `docs/workload_design.md`。
