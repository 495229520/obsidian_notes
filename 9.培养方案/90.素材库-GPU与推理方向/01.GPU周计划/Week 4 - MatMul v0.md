---
title: Week 4 - MatMul v0
date: 2026-05-06
tags:
  - AI-infra/素材库-GPU与推理方向/GPU周计划/计划
aliases:
  - CUDA Week 4
  - MatMul Lab v0
status: active
---

# Week 4 - MatMul v0

> 第四周目标：从 naive matmul 走到 shared memory tiled matmul，并用 cuBLAS 做 baseline。重点不是打赢 cuBLAS，而是能解释每个版本为什么快或慢。

---

## 学习目标

1. 能写 CPU reference matmul。
2. 能写 naive CUDA matmul。
3. 能写 shared memory tiled matmul。
4. 能理解 register blocking 的基本动机。
5. 能用 cuBLAS 做 baseline。
6. 能用 Nsight Compute 分析 memory throughput、occupancy、stall reason。
7. 能用 roofline thinking 判断 compute-bound / memory-bound。
8. 能理解 Triton `program id`、`mask load / store`、`BLOCK_SIZE` 的最小用法。
9. 能为 `torch-triton-rmsnorm` 建立起步任务：PyTorch reference、shape / dtype matrix、后续实现 TODO。

---

## 1. 为什么 matmul 是关键项目

MatMul / GEMM 是深度学习和 LLM 推理的核心算子之一。它和前几周的 kernel 不同：

- `vector add` 更偏 memory-bound。
- `transpose` 主要训练访存模式。
- `reduction` 训练线程协作。
- `matmul` 同时考验 tiling、shared memory、register reuse、计算密度和库 baseline。

早期目标不是写出工业级 GEMM，而是建立正确的性能分析框架。

---

## 2. 必做实现

| 版本 | 目标 | 重点观察 |
|---|---|---|
| CPU reference | 正确性基准 | 小 shape 验证 |
| naive CUDA matmul | 每个线程算一个 C 元素 | global memory 重复读取 |
| shared memory tiled matmul | tile A/B 到 shared memory | 数据复用 |
| register blocking toy | 一个线程算多个输出 | register reuse |
| cuBLAS baseline | 工业库对比 | 差距解释 |
| Triton 入门练习 | 观察 program id、mask load-store、BLOCK_SIZE | 建立 Triton kernel 心智模型 |
| RMSNorm 起步任务 | 准备 reference、测试 shape、项目任务拆解 | 为 `torch-triton-rmsnorm` 铺路 |

---

## 3. 实现顺序

### Day 1：CPU reference + shape 设计

实现：

- `C[M, N] = A[M, K] * B[K, N]`
- row-major 输入输出。
- FP32 起步。

测试 shape：

| M | N | K | 用途 |
|---:|---:|---:|---|
| 16 | 16 | 16 | debug |
| 128 | 128 | 128 | 小规模 |
| 1024 | 1024 | 1024 | 常规 benchmark |
| 1024 | 4096 | 4096 | LLM-like shape |
| 4096 | 4096 | 4096 | 大规模 |

### Day 2：naive CUDA matmul

- 一个线程计算一个 `C[row, col]`。
- 循环 K 维。
- 每次从 global memory 读取 A 和 B。

必须回答：

- 每个 A 元素被重复读了多少次？
- 每个 B 元素被重复读了多少次？
- naive matmul 为什么慢？

### Day 3：shared memory tiled matmul

- 一个 block 计算一个 C tile。
- A tile 和 B tile 先加载到 shared memory。
- block 内同步。
- 循环 K 维 tile。

必须回答：

- shared memory tile 存什么？
- tile size 如何影响 shared memory 使用量？
- 为什么 tile 可以增加数据复用？

### Day 4：register blocking toy

目标只做入门版：

- 一个线程计算 2 个或 4 个 C 元素。
- 尽量复用 A/B 数据。
- 观察 register count 和 occupancy 变化。

必须回答：

- register blocking 有什么用？
- register pressure 为什么可能让性能下降？

### Day 5：cuBLAS baseline

- 用 cuBLAS 跑同样 shape。
- benchmark 表中把 cuBLAS 作为 baseline。
- 不把打不过 cuBLAS 解释成失败。

必须记录：

- 自己最快版本与 cuBLAS 的差距。
- 差距可能来自 Tensor Core、tiling 层级、指令调度、库工程优化。

### Day 6-7：Nsight + Triton 入门 + README

Nsight 关注：

- achieved occupancy。
- SM utilization。
- memory throughput。
- stall reason。
- register count。
- shared memory usage。

Triton 入门关注：

- 一个 Triton program 对应什么 tile。
- mask 如何处理非整除边界。
- `BLOCK_SIZE` / `num_warps` 为什么影响性能。
- 本周只做入门阅读或最小实验，不做 Triton matmul autotune。

README 必须解释：

- naive 为什么慢。
- tiled 为什么更快。
- register blocking 带来的收益和代价。
- cuBLAS 为什么是 baseline。
- 当前版本下一步怎么优化。

---

## 4. Benchmark 指标

MatMul 主要记录 TFLOPS：

```text
FLOPs = 2 * M * N * K
TFLOPS = FLOPs / time_seconds / 1e12
```

表格：

| Version | M | N | K | Dtype | Time(ms) | TFLOPS | Correct | Note |
|---|---:|---:|---:|---|---:|---:|---|---|
| naive | 1024 | 1024 | 1024 | FP32 | TBD | TBD | pass | baseline |
| tiled | 1024 | 1024 | 1024 | FP32 | TBD | TBD | pass | shared memory |
| cuBLAS | 1024 | 1024 | 1024 | FP32 | TBD | TBD | pass | library baseline |

---

## 5. 常见坑

> [!warning] row-major / column-major 搞反
> cuBLAS 默认接口习惯和 C++ row-major 不完全一致，baseline 对比前必须确认矩阵布局。

> [!warning] 只测 square shape
> LLM 里常见 shape 不一定是方阵，必须覆盖多个 M/N/K。

> [!warning] 只看 TFLOPS
> TFLOPS 是结果指标。解释性能还要看 Nsight 指标。

> [!warning] 急着上 Tensor Core
> 先把 FP32 naive/tiled 的数据复用讲清楚，再进入 Tensor Core 和 CUTLASS。

---

## 6. 本周交付物

| 交付物 | 内容 | 验收 |
|---|---|---|
| matmul kernels | naive、tiled、register blocking toy | correctness pass |
| cuBLAS baseline | 同 shape benchmark | README 有差距解释 |
| benchmark.md | 多 M/N/K、多版本 | TFLOPS 表格 |
| profiling.md | Nsight 指标和瓶颈判断 | 结论能被指标支持 |
| triton_intro.md | program id、mask load-store、BLOCK_SIZE | 能解释 Triton 最小 kernel 的执行模型 |
| rmsnorm_kickoff.md 或 tasks.md | RMSNorm reference、shape / dtype matrix、下一步任务 | `torch-triton-rmsnorm` 起步清晰 |

---

## 面试问题

- 为什么 naive matmul 慢？
- shared memory tile 存什么？
- register blocking 有什么用？
- Tensor Core 为什么快？
- 为什么你的实现打不过 cuBLAS？
- matmul 为什么更偏 compute-bound？
- roofline thinking 怎么判断瓶颈？

---

## 关联知识

- [[CUDA 学习清单]]
- [[Week 3 - Transpose + Memory Coalescing]]
- [[3.4 CUDA Nsight Compute 指标速查|CUDA Nsight Compute 指标速查]]
- [[3.6 CUDA 生态工具清单|CUDA 生态工具清单]]
- [[Week 5 - Serving Benchmark Harness]]
