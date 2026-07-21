---
title: JAX TPU Dynamic Shape 认知边界
date: 2026-05-24
tags:
  - AI-infra/素材库-GPU与推理方向/推理专题清单
roadmap_week: Week 8, Week 17+
sort_order: "08.20"
status: active
---

# JAX TPU Dynamic Shape 认知边界

> [!info] 所属路线
> - 总纲 Week：Week 8，Week 17+
> - 排序：08.20
> - 用途：作为 GPU Infra 主线之外的跨框架、跨硬件面试补盲。

> [!goal] 目标
> 这篇笔记是 [[AI Agent Native AI Infra GPU Performance Engineer 培养方案]] 的跨框架补盲：主线仍然是 GPU Infra，但需要能在面试中解释 PyTorch / Triton / torch.compile 与 JAX / XLA / TPU、dynamic shape、variable-length sequence 的基本边界。

---

## 1. 为什么需要这块认知

AI Infra 面试不一定只问 CUDA。框架研发、编译器、TPU 或模型迁移相关岗位可能追问：

- eager mode 和 compiler mode 的区别。
- PyTorch / Triton 和 JAX / XLA 的执行模型差异。
- TPU 和 GPU 的硬件编程直觉差异。
- dynamic shape / variable-length sequence 为什么麻烦。
- 算子从 PyTorch 对齐到 JAX 时怎么验证精度。

这部分不用抢主线，但不能完全空白。

---

## 2. PyTorch eager / torch.compile / Triton / JAX-XLA

| 路线 | 直觉 | 优点 | 风险 |
|---|---|---|---|
| PyTorch eager | Python 逐步执行 op | 调试简单、动态灵活 | kernel launch 多，图优化弱 |
| torch.compile | 捕获图并编译优化 | 减少 overhead、做 fusion / lowering | graph break、动态 shape 复杂 |
| Triton | 手写 GPU kernel DSL | 控制 kernel layout 和 tiling | 需要自己负责 correctness / benchmark |
| JAX / XLA | 函数式风格 + XLA 编译 | 编译优化强，适合大图和 TPU | static shape 假设更强，调试和动态控制流更难 |

口述模板：

```text
PyTorch eager 更灵活，适合调试；torch.compile 和 JAX/XLA 更强调图捕获、优化和 lowering。Triton 是手写 GPU kernel 的 DSL，控制力更强，但 correctness 和性能证据要自己负责。
```

---

## 3. TPU vs GPU 的粗粒度差异

| 维度 | GPU | TPU |
|---|---|---|
| 设计重点 | 通用并行计算 + Tensor Core | 大规模矩阵计算和 XLA 图编译生态 |
| 编程入口 | CUDA、Triton、cuBLAS、PyTorch | JAX/XLA、TensorFlow/XLA |
| 优化直觉 | kernel、memory coalescing、shared memory、occupancy | shape、layout、XLA fusion、systolic array 利用 |
| 动态性 | 更容易容纳动态控制和自定义 kernel | 更偏静态图和编译期优化 |

面试回答不要把 TPU 说成“另一种 GPU”。更稳的说法：

```text
GPU 优化更常从 kernel、memory hierarchy 和 occupancy 出发；TPU 更依赖 XLA 编译、静态 shape、layout 和大矩阵计算的高效映射。
```

---

## 4. dynamic shape 为什么难

编译器喜欢稳定 shape，因为它可以提前决定：

- memory layout。
- kernel launch 参数。
- tiling 策略。
- buffer 分配。
- fusion 边界。

LLM serving 里 request 长度不同，会带来：

```text
variable prompt length
variable output length
variable batch composition
variable KV cache usage
```

这会让编译器和 runtime 都更难：

- 静态编译版本可能需要 padding。
- padding 浪费计算。
- 动态 shape 可能触发重新编译或 graph break。
- variable-length batch 需要更复杂的 KV cache / page table 管理。

---

## 5. variable-length sequence 的工程问题

| 问题 | 影响 |
|---|---|
| padding 到同一长度 | 浪费 attention / MLP 计算 |
| 每个请求长度不同 | batch 内负载不均 |
| KV cache 长度不同 | cache 分配、碎片、page table 更复杂 |
| decode 进度不同 | scheduler 需要 continuous batching |
| prefix 共享程度不同 | prefix cache / RadixAttention 命中率变化 |

这部分和 [[Week 7 - KV Cache + Prefix Cache + Paged KV]]、[[Week 8 - Prefill Decode + Open Source Repro]] 是同一条 serving 主线。

---

## 6. JAX / PyTorch 算子精度对齐

跨框架对齐不要只说“结果一样”，而要说清楚验证闭环：

```text
reference implementation
shape coverage
dtype coverage
atol / rtol
edge cases
random seed
极端值 / 非对齐 shape / 长序列
```

口述模板：

```text
我会先定义 PyTorch reference 和 JAX implementation 的 API 对齐规则，再覆盖 shape、dtype 和边界输入。因为不同框架的 lowering、累加顺序和低精度路径可能不同，所以用 allclose 的 atol / rtol，而不是要求 bitwise 相等。
```

---

## 7. 面试诚实边界

如果没有深入做 TPU，不要假装写过底层 TPU kernel。可以这样说：

```text
我的主线是 GPU Infra / CUDA / Triton / serving benchmark。JAX / TPU 我把它作为跨框架认知补充，重点理解 XLA 编译、static shape、layout、dynamic shape 和精度对齐问题。如果岗位需要，我会从模型实现、XLA HLO 和 benchmark 对齐开始切入。
```

---

## 8. 自测问题

1. PyTorch eager 和 compiler mode 的区别是什么？
2. torch.compile 为什么会遇到 graph break？
3. JAX / XLA 为什么更偏 static shape？
4. TPU 和 GPU 的优化直觉有什么不同？
5. variable-length sequence 为什么影响 serving runtime？
6. 跨框架算子精度对齐应该怎么做？
