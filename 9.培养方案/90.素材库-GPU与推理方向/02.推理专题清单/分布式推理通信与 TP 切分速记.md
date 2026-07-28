---
title: 分布式推理通信与 TP 切分速记
date: 2026-05-24
tags:
  - AI-infra
  - 素材库-GPU与推理方向
  - 推理专题清单
roadmap_week: Week 5-8, Week 17+
sort_order: "05.20"
status: active
---

# 分布式推理通信与 TP 切分速记

> [!info] 所属路线
> - 总纲 Week：Week 5-8，Week 17+
> - 排序：05.20
> - 用途：支撑 serving benchmark、TP/DP/PP/EP、PD disaggregation 和多 GPU 推理口述。

> [!goal] 目标
> 把 [[AI Agent Native AI Infra GPU Performance Engineer 培养方案]] 中的 TP / DP / PP / EP、NCCL 和 RDMA 从概念词变成能在面试中画数据流、解释 collective 和分析 latency 风险的口述能力。

---

## 1. 四种并行先分清

| 并行方式 | 切分对象 | 推理关注点 |
|---|---|---|
| TP | 单层内的 tensor / hidden / weight 维度 | 每层通信频繁，影响 TPOT |
| DP | 复制多份模型，分摊请求 | router、负载均衡、成本 |
| PP | 按层切分 pipeline stage | bubble、micro-batch、跨 stage latency |
| EP | MoE experts 分散到不同设备 | expert dispatch、load balance、All-to-All |

口述顺序：

```text
DP 是复制模型分请求，PP 是按层切，TP 是层内切矩阵，EP 是 MoE 专家切分。推理优化里 TP 和 EP 的通信最容易进入单 token latency 路径。
```

---

## 2. 常见 collective

| Collective | 做什么 | 常见场景 |
|---|---|---|
| AllReduce | 所有卡先 reduce，再每张卡拿到完整结果 | Row Parallel Linear partial output 求和 |
| AllGather | 每张卡拿一片，最后拼成完整 tensor | 需要完整 hidden / logits 时 |
| ReduceScatter | 先 reduce，再把结果切片分给各卡 | AllReduce 的一种拆分阶段，也可减少峰值通信 |
| All-to-All | 每张卡给每张卡发送不同数据 | MoE expert dispatch / combine |
| Broadcast | 一张卡发给多张卡 | 参数、metadata、调度信息 |

AllReduce 可以理解为：

```text
AllReduce = ReduceScatter + AllGather
```

但面试里不要只背等式，还要说明为什么拆开有意义：可能减少中间峰值、配合并行布局、改善通信调度。

---

## 3. Column Parallel Linear

Linear：

```text
Y = X W
X: [tokens, d_in]
W: [d_in, d_out]
Y: [tokens, d_out]
```

Column Parallel 把 `W` 的输出列切开：

```text
W = [W_0, W_1, ..., W_{n-1}]
Y_i = X W_i
Y = concat(Y_i)
```

特点：

- 每张卡得到输出的一部分 hidden。
- 如果下一层能接受分片输出，可以不立刻 AllGather。
- 常用于 QKV projection、MLP gate / up projection。

口述模板：

```text
Column parallel 是按输出维度切权重。每张卡用完整输入 X 乘自己的 W_i，得到一段输出。它减少单卡权重和计算，但如果后续需要完整输出，就要 AllGather；如果后续 row parallel 能接住分片，就可以延迟通信。
```

---

## 4. Row Parallel Linear

Row Parallel 把 `W` 的输入维度切开：

```text
X = [X_0, X_1, ..., X_{n-1}]
W = [W_0; W_1; ...; W_{n-1}]
Y_partial_i = X_i W_i
Y = sum_i(Y_partial_i)
```

特点：

- 每张卡只算部分输入维度对应的 partial output。
- 最后需要 AllReduce 把 partial output 求和。
- 常用于 attention output projection、MLP down projection。

口述模板：

```text
Row parallel 是按输入维度切。每张卡只看到 X 的一片和 W 的一片，算出完整输出形状的 partial result，最后必须 AllReduce 求和，否则每张卡只有部分贡献。
```

---

## 5. 一层 Transformer 里的 TP 通信

典型 Megatron 风格直觉：

```text
QKV projection: Column Parallel
Attention per head / per shard compute
Output projection: Row Parallel -> AllReduce

MLP gate/up: Column Parallel
Activation / SwiGLU local
MLP down: Row Parallel -> AllReduce
```

所以 TP 对 decode latency 的影响很直接：每一层都可能有 collective，层数越多，跨卡同步越频繁。

面试可说：

```text
TP 能降低单卡显存和计算压力，但代价是每层引入通信。prefill 阶段大 GEMM 可能更容易 amortize 通信，decode 阶段每 token 都要经过多层 collective，通信 latency 对 TPOT 更敏感。
```

---

## 6. LM Head 和 vocab parallel

LM Head 常见做法是按 vocab 维度切：

```text
logits_i = hidden @ W_vocab_i
```

如果只是取局部 top-k，需要跨卡合并候选；如果需要完整 logits，则需要 AllGather 或等价通信。

口述重点：

- vocab 很大，切 vocab 可以省单卡显存和计算。
- 采样 / top-k 需要跨卡协调。
- 不同 serving engine 可能实现细节不同，面试回答先讲数据依赖。

---

## 7. EP / MoE 为什么关注 All-to-All

MoE 推理中，每个 token 会被 router 分到 top-k experts。不同 expert 可能在不同 GPU 上：

```text
tokens -> router -> dispatch to expert GPUs -> expert compute -> combine
```

这不是简单求和，而是不同 token 发往不同设备，所以 All-to-All 更关键。

风险：

- expert load imbalance。
- token dispatch 带来 tail latency。
- batch token 分布不均会让部分 GPU 等待。
- 跨机 EP 受 RDMA / network latency 影响更大。

---

## 8. NVLink / RDMA / PCIe 的口述边界

| 通信边界 | 口述重点 |
|---|---|
| 同卡 | HBM / SM 内部，最快 |
| 同机 NVLink | 多 GPU 高带宽互联，适合频繁 collective |
| PCIe | 带宽和延迟通常弱于 NVLink |
| 跨机 RDMA | 跨节点通信，影响 TP 扩展、EP dispatch、KV transfer、PD disaggregation |

面试不要只说“RDMA 很快”，要说明它解决的是跨机 CPU 参与和拷贝开销问题，但仍然比单机片上/卡间通信贵。

---

## 9. 和 serving 指标的关系

| 机制 | 可能改善 | 可能恶化 |
|---|---|---|
| TP | 单卡显存、单卡计算压力 | 每层 collective latency |
| DP | RPS、吞吐、隔离 | 成本、负载均衡 |
| PP | 超大模型部署 | bubble、跨 stage latency |
| EP | MoE 参数规模和 expert 扩展 | All-to-All、负载不均、tail latency |
| PD disaggregation | 资源隔离、长 prompt TTFT | KV transfer、网络开销 |

---

## 10. 3 分钟口述验收

1. Column parallel 和 row parallel 的区别是什么？
2. 为什么 row parallel 后面通常要 AllReduce？
3. AllReduce 和 ReduceScatter + AllGather 是什么关系？
4. TP 为什么可能让 decode TPOT 变差？
5. MoE / EP 为什么更关注 All-to-All？
6. PD disaggregation 为什么要关心 NVLink / RDMA？

---

## 11. 常见误区

> [!warning] 不要把 all-reduce 说成 all-gather
> AllReduce 是对 partial result 做规约并让每张卡拿到规约结果；AllGather 是拼接各卡分片。二者语义不同。

> [!warning] 不要只说 TP 能加速
> TP 降低单卡压力，但引入层内通信。decode 阶段每 token 都要跨很多层，collective latency 可能直接体现在 TPOT / ITL 上。
