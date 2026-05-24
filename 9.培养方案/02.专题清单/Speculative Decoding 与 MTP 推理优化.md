---
title: Speculative Decoding 与 MTP 推理优化
date: 2026-05-24
tags:
  - infra
  - Reasoning
  - serving
  - 面试
roadmap_week: "Week 8, Week 17+"
sort_order: "08.10"
status: active
---

# Speculative Decoding 与 MTP 推理优化

> [!info] 所属路线
> - 总纲 Week：Week 8，Week 17+
> - 排序：08.10
> - 用途：服务 prefill/decode、SGLang、推理优化和高级面试追问。

> [!goal] 目标
> 把 speculative decoding 从“知道它在干什么”升级为能解释收益条件、失败条件、acceptance rate、verifier cost 和 serving 调度影响的面试能力。

---

## 1. 为什么需要 speculative decoding

自回归 decode 的瓶颈是：

```text
每生成一个 token -> 跑一次大模型 forward -> 更新 KV cache -> 再生成下一个 token
```

这导致输出 token 串行依赖强，TPOT / ITL 容易成为用户体验瓶颈。Speculative decoding 的目标是：用更便宜的方式先猜多个 token，再让目标模型一次验证多个 token，减少目标模型串行 forward 次数。

---

## 2. 基本流程

```text
draft model 生成 k 个 candidate tokens
        ↓
target / verifier model 并行验证这些 tokens
        ↓
接受前若干个 token，遇到不匹配则回退或重采样
        ↓
继续下一轮
```

关键对象：

| 对象 | 作用 |
|---|---|
| draft model | 便宜地产生候选 token |
| verifier / target model | 保证最终分布或输出质量 |
| candidate length `k` | 每轮猜几个 token |
| acceptance rate | 候选 token 被接受的比例 |
| average accepted length | 每轮平均推进多少 token |

---

## 3. speedup 取决于什么

粗略判断：

```text
一轮收益 ≈ 接受 token 数 * 原始目标模型单 token 成本
一轮代价 ≈ draft 生成 k 个 token 成本 + verifier 验证成本 + 调度开销
```

只有当：

```text
接受 token 带来的节省 > draft + verify + scheduling overhead
```

才真的加速。

口述模板：

```text
speculative decoding 不是一定更快。它依赖 draft 足够便宜、候选质量足够高、acceptance rate 足够好，而且 verifier 能高效并行验证多个 token。如果 draft 太慢或接受率太低，额外调度和 KV 管理反而会拖慢。
```

---

## 4. acceptance rate 为什么关键

如果平均每轮只接受 1 个 token，那 speculative decoding 基本没有减少串行步数，还多了 draft 和验证开销。

如果平均每轮接受多个 token，目标模型一次验证推动多个输出位置，才可能降低 TPOT / ITL。

影响 acceptance rate 的因素：

- draft model 和 target model 分布是否接近。
- 任务难度和采样温度。
- candidate length 是否过长。
- prompt domain 是否和 draft 训练分布匹配。
- 多 token 预测头是否足够准确。

---

## 5. MTP 的定位

MTP，即 Multi-Token Prediction，通常指模型在一个 forward 中预测多个未来 token 或辅助头。它和 speculative decoding 的关系可以这样讲：

```text
MTP 可以看作提高多 token candidate 质量的一类方法；speculative decoding 是使用 candidate + verifier 来减少串行 decode 步数的系统机制。
```

面试不要把二者完全等同：

- MTP 更偏模型结构 / 训练目标 / candidate 生成。
- speculative decoding 更偏推理算法和 serving 系统。

---

## 6. 常见路线对比

| 路线 | 直觉 | 优点 | 风险 |
|---|---|---|---|
| 小 draft model | 小模型先猜，大模型验证 | 实现直观，draft 便宜 | 分布差异大时 acceptance 低 |
| Medusa 类多头 | 在目标模型上接多个预测头 | 避免独立 draft 模型 | 训练和集成复杂 |
| EAGLE 类方法 | 利用特征预测后续 token | candidate 质量更好 | 实现复杂度更高 |
| MTP | 多 token 预测辅助能力 | 可能提高平均接受长度 | 需要模型侧支持 |
| n-gram speculative | 从历史 n-gram 猜测 | 很便宜，适合重复文本 | 泛化能力有限 |

---

## 7. 和 KV cache 的关系

Speculative decoding 会改变 KV cache 管理：

- draft 阶段可能维护自己的 KV cache。
- verifier 一次验证多个候选位置。
- 被拒绝的 token 对应 KV 不能直接保留。
- 接受 token 后要更新 target KV cache。
- 多请求 batch 中，各请求接受长度不同，调度更复杂。

所以 serving 系统里不能只看单请求 speedup，还要看：

- batching 效率。
- variable accepted length。
- KV cache 分配 / 回收。
- tail latency。
- failed requests / memory pressure。

---

## 8. 和 SGLang / serving 调度的关系

如果团队做 SGLang + speculative inference，面试重点通常不是只问算法，而是问系统收益：

- speculative decoding 如何进入 scheduler。
- candidate verification 如何 batching。
- 不同请求接受长度不同，如何避免 batch 内浪费。
- 和 prefix cache / RadixAttention 是否冲突或互补。
- 长上下文下 verifier 读 KV 的代价是否抵消收益。
- 指标上看 TTFT、TPOT / ITL、TPS、p95 latency 还是 GPU utilization。

---

## 9. 什么时候不划算

| 情况 | 原因 |
|---|---|
| acceptance rate 低 | 每轮推进 token 少，draft 成本浪费 |
| draft model 太慢 | 额外 forward 抵消目标模型节省 |
| candidate length 过长 | 后半段更容易被拒绝，验证浪费 |
| batch 调度复杂 | accepted length 不一致，降低 serving 效率 |
| 长上下文 KV read 太重 | verifier 仍要读大量 KV |
| 低延迟小请求 | 额外流程可能增加 TTFT |

---

## 10. 面试口述模板

```text
Speculative decoding 的核心是用便宜的 draft 先猜多个 token，再用 target model 一次验证，从而减少自回归 decode 的串行步数。它的收益不是固定的，主要取决于 draft cost、verifier cost、candidate length 和 acceptance rate。如果平均每轮能接受多个 token，TPOT 可能下降；如果接受率低或 draft 太慢，就会因为额外 forward、KV cache 管理和 scheduler overhead 反而变慢。MTP 可以理解为提高多 token candidate 质量的一类模型侧方法，而 SGLang 这类 serving 系统还要考虑 batching、prefix cache、KV cache 和 tail latency。
```

---

## 11. 3 分钟口述验收

1. speculative decoding 为什么能降低 decode 串行开销？
2. acceptance rate 低为什么会失败？
3. draft model 和 verifier model 分别承担什么？
4. MTP 和 speculative decoding 是什么关系？
5. speculative decoding 如何影响 KV cache 管理？
6. 在 serving benchmark 中应该看哪些指标判断它是否有效？
