---
title: LLM Kernel 专题清单
date: 2026-05-06
tags:
  - AI-infra
  - 素材库-GPU与推理方向
  - 推理专题清单
  - 清单
aliases:
  - LLM CUDA Kernel
  - LLM 算子清单
roadmap_week: Week 4, Week 9-16
sort_order: "04.10"
status: active
---

# LLM Kernel 专题清单

> [!info] 所属路线
> - 总纲 Week：Week 4，Week 9-16
> - 排序：04.10
> - 用途：从基础 CUDA 过渡到 RMSNorm、Softmax、RoPE、dequant、toy attention 等 LLM kernel。

> 这份笔记把 CUDA 基础推进到 LLM 推理常见算子。目标不是一开始就写 FlashAttention，而是先把 RMSNorm、Softmax、RoPE、activation、dequant 和 fusion 做扎实。

---

## 1. 学习前置

进入 LLM kernel 前，应先完成：

- [[Week 2 - Reduction + Profiling]]
- [[Week 3 - Transpose + Memory Coalescing]]
- [[Week 4 - MatMul v0]]
- [[3.4 CUDA Nsight Compute 指标速查|CUDA Nsight Compute 指标速查]]

最低能力：

- 能写 row-wise reduction。
- 能做 CUDA event benchmark。
- 能用 PyTorch reference 验证。
- 能解释 memory-bound / compute-bound。
- 能看 Nsight Compute 的关键指标。

---

## 2. 必做算子

| 算子 | 重点 | 依赖 |
|---|---|---|
| RMSNorm | row-wise reduction、rsqrt、scale | reduction |
| Softmax | max reduce、exp、sum reduce、数值稳定 | reduction |
| RoPE | position-dependent rotation | elementwise + vectorized load |
| SwiGLU / GELU | elementwise activation | elementwise kernel |
| top-k sampling | partial selection | reduction / sort |
| INT8 dequant | scale + type conversion | vectorized load |
| fused activation | 减少读写 global memory | fusion |
| toy attention | QK、mask、softmax、V、Online Softmax 与 FlashAttention 口述 | matmul + softmax + [[FlashAttention 版本演进与面试口述]] |

---

## 3. RMSNorm

公式直觉：

```text
y = x / sqrt(mean(x^2) + eps) * weight
```

学习点：

- [ ] 每行一个 block 的基础版本。
- [ ] row-wise sum of squares。
- [ ] shared memory / warp reduce。
- [ ] FP32 accumulation。
- [ ] FP16 输入、FP32 累加、FP16 输出。
- [ ] PyTorch reference。
- [ ] CUDA vs Triton 对比。

必须回答：

- RMSNorm 和 LayerNorm 区别是什么？
- 为什么 RMSNorm 需要 reduction？
- 这个 kernel 是 memory-bound 还是 compute-bound？

---

## 4. Softmax

数值稳定版本：

```text
softmax(x_i) = exp(x_i - max(x)) / sum(exp(x_j - max(x)))
```

这部分需要和 [[FlashAttention 版本演进与面试口述]] 联动：先把 row-wise stable softmax 讲清楚，再升级到 Online Softmax、分块 attention 和 FlashAttention 版本演进。

学习点：

- [ ] row-wise max reduce。
- [ ] row-wise exp + sum reduce。
- [ ] normalize 写回。
- [ ] mask 处理。
- [ ] FP32 accumulation。
- [ ] CUDA event benchmark。
- [ ] Triton fused softmax 对比。
- [ ] Online Softmax 口述。

必须回答：

- 为什么要减去 max？
- softmax 有几次 pass？
- fused softmax 减少了什么开销？

---

## 5. RoPE

RoPE 是对 query/key 做位置相关旋转。

学习点：

- [ ] 理解 pair-wise rotation。
- [ ] 支持不同 head dimension。
- [ ] 处理 sin/cos table。
- [ ] vectorized load。
- [ ] 尝试和 attention 前处理融合。

必须回答：

- RoPE 改的是 Q/K 还是 V？
- RoPE 为什么适合 elementwise kernel？
- RoPE 可以和哪些操作融合？

---

## 6. Activation 与 Fusion

必做：

- [ ] GELU。
- [ ] SwiGLU。
- [ ] bias + activation。
- [ ] activation + multiply。
- [ ] 比较 fused 和 unfused 的 global memory 读写量。

判断方式：

- 如果两个 kernel 中间结果写回 global memory，再被下一个 kernel 读出，fusion 可能减少一次写和一次读。
- fusion 后 register pressure 可能增加，不能只凭直觉判断。

---

## 7. INT8 Dequant Toy Kernel

目标：

```text
fp16_value = int8_value * scale
```

学习点：

- [ ] per-tensor scale。
- [ ] per-channel scale。
- [ ] vectorized load。
- [ ] dequant + matmul 前处理。
- [ ] 为什么 dequant 常和 matmul 融合。

必须回答：

- INT8 dequant 为什么常和 matmul 融合？
- dequant 单独做有什么 global memory 开销？

---

## 8. 每个算子的固定交付

| 交付物 | 内容 |
|---|---|
| PyTorch reference | 正确性基准 |
| CUDA version | 手写 kernel |
| Triton version | 至少 3 个算子需要 |
| correctness test | 多 shape、多 dtype |
| benchmark.md | CUDA event 表格 |
| profiling.md | Nsight 指标 |
| README | 解释瓶颈和优化 |

---

## 9. 推荐实现顺序

1. RMSNorm：最适合从 reduction 过渡到 LLM kernel。
2. Softmax：练数值稳定和多次 row-wise reduce。
3. RoPE：练 elementwise、布局和 vectorized load。
4. GELU / SwiGLU：练 activation fusion。
5. INT8 dequant：进入量化推理。
6. toy attention：把 matmul、softmax 和 memory layout 串起来。

---

## 面试问题

- RMSNorm 和 LayerNorm 区别是什么？
- Softmax 如何保证数值稳定？
- RoPE 可以和 attention 前处理融合吗？
- INT8 dequant 为什么常和 matmul 融合？
- kernel fusion 为什么可能更快？
- fusion 会带来什么副作用？
- 如何为 LLM kernel 设计 correctness test？
- FlashAttention V1 / V2 / V3 的优化主线分别是什么？
- 为什么 FlashAttention 解决的是 HBM traffic，而 PagedAttention 更关注 KV cache 管理？

---

## 关联知识

- [[CUDA 学习清单]]
- [[3.6 CUDA 生态工具清单|CUDA 生态工具清单]]
- [[3.4 CUDA Nsight Compute 指标速查|CUDA Nsight Compute 指标速查]]
