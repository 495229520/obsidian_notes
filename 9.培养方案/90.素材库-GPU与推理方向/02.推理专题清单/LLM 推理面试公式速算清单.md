---
title: LLM 推理面试公式速算清单
date: 2026-05-24
tags:
  - infra
  - Reasoning
  - 面试
  - 速算
roadmap_week: "Week 5-8"
sort_order: "05.00"
status: active
---

# LLM 推理面试公式速算清单

> [!info] 所属路线
> - 总纲 Week：Week 5-8
> - 排序：05.00
> - 用途：服务 benchmark、KV cache、prefill/decode 和 cost 的面试口算。

> [!goal] 目标
> 把 [[AI Agent Native AI Infra GPU Performance Engineer 培养方案]] 中的 KV cache、prefill / decode、serving 指标和成本计算变成能在面试现场快速口算、口述和白板推导的模板。

这篇笔记不是完整性能模型，而是面试速算表。回答时先给公式，再说主导瓶颈，最后说工程 tradeoff。

---

## 1. 常用符号

| 符号 | 含义 |
|---|---|
| `B` | batch size / active requests 数 |
| `S` | 当前上下文长度，或 prefill prompt length |
| `T` | 输出 token 数 |
| `L` | Transformer 层数 |
| `H` | query heads 数，即 `num_attention_heads` |
| `H_kv` | KV heads 数，即 `num_key_value_heads` |
| `D` | head dim |
| `d_model` | hidden size，通常 `H * D` |
| `d_ff` | MLP intermediate size |
| `bytes` | 每个元素字节数，FP16 / BF16 通常是 2，FP32 是 4 |

---

## 2. KV cache 大小

全模型 KV cache 近似大小：

```text
KV cache bytes = B * S * L * 2 * H_kv * D * bytes
```

其中 `2` 表示 K 和 V。

口述重点：

- `B` 越大，并发越高，KV cache 越大。
- `S` 越长，长上下文显存压力越高。
- `L` 越深，每层都要存 KV。
- `H_kv` 越小，GQA / MQA 越省 KV cache。
- `bytes` 越小，量化 KV cache 越省显存，但要考虑精度和额外 dequant 开销。

面试回答模板：

```text
KV cache 本质上是在每一层缓存历史 token 的 K 和 V。大小可以粗略写成 B * S * L * 2 * num_kv_heads * head_dim * bytes。长上下文贵，是因为 S 线性放大每层 KV；GQA/MQA 能省显存，是因为 num_kv_heads 比 query heads 少。
```

---

## 3. decode 每 token 的 KV 读取量

decode 每生成一个 token，每层 attention 都要读取历史 KV：

```text
decode KV read bytes / token
≈ B * S * L * 2 * H_kv * D * bytes
```

这和 KV cache 总大小形式相同，但语义是“每生成一个 token 需要反复读多少历史 KV”。

口述重点：

- decode 的 Q 很短，通常每步只有新 token。
- 但是 K / V 是历史上下文，长度随 `S` 增长。
- 因此长上下文 decode 更容易受 KV cache 读带宽、cache layout 和调度影响。
- 小 batch decode 的 GEMM 利用率不高，也容易 latency-bound。

---

## 4. prefill 和 decode 的主导差异

| 阶段 | 输入形态 | 主要工作 | 常见瓶颈 | 主要指标 |
|---|---|---|---|---|
| prefill | 一次处理整段 prompt | QKV / MLP 大 GEMM、attention over prompt | compute、HBM、长 prompt、排队 | TTFT |
| decode | 每步生成一个 token | 每层读取历史 KV，做小 batch attention 和 MLP | KV cache bandwidth、调度、小 batch 利用率 | TPOT / ITL |

口述模板：

```text
prefill 是把 prompt 一次性喂进去，矩阵规模比较大，GEMM 和 attention 并行度更高，所以更偏 compute-heavy。decode 每次只生成一个 token，但每层都要读历史 KV，随着上下文变长，KV cache 读取量线性增长，所以更容易受显存带宽、cache layout 和调度限制。
```

---

## 5. Attention FLOPs 粗算

对一层 attention，忽略常数细节：

```text
QK^T FLOPs ≈ 2 * B * H * S_q * S_k * D
P V FLOPs  ≈ 2 * B * H * S_q * S_k * D
Attention FLOPs ≈ 4 * B * H * S_q * S_k * D
```

prefill 自注意力中 `S_q = S_k = S`：

```text
Attention FLOPs ≈ 4 * B * H * S^2 * D
```

decode 单 token 中 `S_q = 1, S_k = S`：

```text
Attention FLOPs / token ≈ 4 * B * H * S * D
```

注意：causal attention 的理论有效计算约为三角区域，但实际 kernel 和分块实现会影响精确计算量。面试中先用粗算解释趋势即可。

---

## 6. Linear / MLP FLOPs 粗算

一个 Linear：

```text
Linear FLOPs ≈ 2 * tokens * in_dim * out_dim
```

QKV projection：

```text
QKV FLOPs ≈ 2 * B * S * d_model * (H * D + 2 * H_kv * D)
```

普通 MLP 两层：

```text
MLP FLOPs ≈ 2 * B * S * d_model * d_ff
          + 2 * B * S * d_ff * d_model
```

SwiGLU 常有 gate / up / down 三个投影：

```text
SwiGLU FLOPs ≈ 2 * B * S * d_model * d_ff * 2
            + 2 * B * S * d_ff * d_model
```

口述重点：

- prefill 阶段 `tokens = B * S`，大 GEMM 更容易吃满 GPU。
- decode 阶段 `tokens = B`，batch 小时 GEMM 变瘦，利用率下降。

---

## 7. Serving 指标换算

| 指标 | 含义 | 口述重点 |
|---|---|---|
| TTFT | Time To First Token | 排队、prefill、网络、调度共同影响 |
| TPOT / ITL | 每个输出 token 间隔 | 更贴近 decode 速度和流式体验 |
| TPS | tokens per second | 吞吐指标，不能单独代表体验 |
| RPS | requests per second | 请求吞吐，受 prompt / output 分布影响 |
| p95 / p99 | 尾延迟 | online serving 比平均值更重要 |
| failed requests | 失败请求 | 可能来自 OOM、timeout、队列过长、KV cache 不足 |

batch 变大时常见现象：

```text
batch / concurrency ↑
  -> GPU utilization ↑
  -> output TPS 可能 ↑
  -> queueing / TPOT / p95 latency 也可能 ↑
```

所以不能只说“吞吐更高”，要说明 latency-throughput tradeoff。

---

## 8. 成本速算

每百万输出 token 成本：

```text
cost per 1M output tokens
= GPU 每小时价格 / (output TPS * 3600) * 1,000,000
```

如果要算总 token 成本，需要区分 input token 和 output token：

```text
总 token TPS = input TPS + output TPS
```

但 serving 面试里通常更关注 output token，因为 decode 阶段常决定持续生成吞吐。

---

## 9. 3 分钟口述验收

任选一个问题，不看稿讲清公式、瓶颈和 tradeoff：

1. KV cache 大小怎么算？GQA 为什么省显存？
2. prefill 和 decode 的瓶颈为什么不同？
3. 为什么长 prompt 推高 TTFT，长 output 放大 TPOT / ITL？
4. batch size 增大为什么 TPS 可能上升但 p95 latency 变差？
5. cost per 1M tokens 怎么估算？
6. 为什么 decode attention 更容易被 KV cache 读带宽限制？

---

## 10. 常见误区

> [!warning] 不要只背结论
> “prefill 计算密集，decode 访存密集”只是结论。面试要补上公式：prefill 有大矩阵并行度，decode 每 token 要跨层读取历史 KV，读取量随 `B * S * L * H_kv * D * bytes` 增长。

> [!warning] 不要只看 TPS
> AI Infra 面试会追问 TTFT、TPOT、p95、queueing、KV cache usage 和 failed requests。只报平均 TPS 会显得不像 serving engineer。
