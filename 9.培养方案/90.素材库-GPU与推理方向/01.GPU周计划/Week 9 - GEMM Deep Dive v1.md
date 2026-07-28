---
title: Week 9 - GEMM Deep Dive v1
date: 2026-05-16
tags:
  - AI-infra
  - 素材库-GPU与推理方向
  - GPU周计划
  - 计划
aliases:
  - 阶段二 Week 9
  - GEMM Deep Dive v1
  - matmul-lab v1
status: active
---

# Week 9 - GEMM Deep Dive v1

> [!goal] 本周目标
> 从 [[Week 4 - MatMul v0]] 的学习型 matmul，升级到阶段二 `matmul-lab-cuda-triton-cutlass` 的工程骨架。重点是先把 FP32 GEMM 的 reference、CUDA naive、shared memory tiled、register blocking v1、cuBLAS SGEMM baseline 和统一 benchmark 协议打牢，不急着进入 CUTLASS / Tensor Core 复杂实现。

## 学习目标

完成这一周后，应该能回答七个问题：

1. **阶段二 GEMM 项目和 Week 4 MatMul v0 有什么区别？**
2. **为什么 GEMM comparison report 必须先统一 correctness 和 benchmark 协议？**
3. **naive CUDA matmul 的主要浪费在哪里？**
4. **shared memory tiled matmul 复用了哪些 A/B 数据？**
5. **register blocking v1 的收益和代价是什么？**
6. **cuBLAS SGEMM baseline 应该如何公平放进同一张表？**
7. **Nsight 指标如何支持“慢在哪里”的判断？**

## 1. 为什么本周做这个

阶段一的 [[Week 4 - MatMul v0]] 已经让你知道：

- 每个 `C[row, col]` 是一个 K 维内积。
- naive matmul 会重复读取 global memory。
- shared memory tile 能提高 block 内的数据复用。
- register blocking 会带来 register reuse，也可能带来 register pressure。
- [[3.8.1 cuBLAS GEMM Baseline]] 是工业库 baseline，不是必须战胜的目标。

阶段二的目标不是再写一个玩具 kernel，而是把这些版本放进同一个可复现项目：

```text
reference
-> CUDA kernels
-> cuBLAS baseline
-> benchmark runner
-> profiling report
-> README comparison
```

这一周先把 FP32 路线打稳。后续 Week 10 再把 Triton matmul 接进同一套协议，后面再考虑 CUTLASS、Tensor Core、FP16 / BF16 和更复杂 tiling。

## 2. 必做实现 / 实验场景

| 模块 | 目标 | 验收方式 |
|---|---|---|
| 工程骨架 | `matmul-lab-cuda-triton-cutlass` 初版目录 | 能一键 build / run |
| CPU / PyTorch reference | 统一正确性基准 | 多 shape 对拍 |
| naive CUDA | 每个线程计算一个 C 元素 | correctness pass |
| shared memory tiled | block 级 A/B tile 复用 | 比 naive 有可解释提升 |
| register blocking v1 | 一个线程计算多个 C 元素 | 记录 register count / occupancy |
| cuBLAS SGEMM | 同 shape 工业 baseline | benchmark 表中成对记录 |
| benchmark 协议 | warmup、repeat、timing scope 固定 | `benchmark_results.csv` 可追溯 |

建议 shape：

| M | N | K | 用途 |
|---:|---:|---:|---|
| 16 | 16 | 16 | debug / correctness |
| 127 | 129 | 131 | 非整除边界 |
| 512 | 512 | 512 | 小 benchmark |
| 1024 | 1024 | 1024 | 常规 GEMM |
| 1024 | 4096 | 4096 | LLM-like projection |
| 4096 | 4096 | 4096 | 大规模压力测试 |

## 3. 实现顺序

### Day 1：项目骨架和 reference

- 建立 `src/`、`include/`、`benchmarks/`、`docs/`、`scripts/`。
- 写 CPU 或 PyTorch reference。
- 固定 row-major 数据布局。
- 写 shape 配置和随机输入生成。

验收：

- 小 shape 能打印少量结果辅助 debug。
- 每个测试都能输出 max abs error / RMS error。
- README 写清楚本项目比较哪些实现。

### Day 2：naive CUDA matmul

- 一个线程计算一个 `C[row, col]`。
- K 维循环从 global memory 读取 A/B。
- 加边界检查，覆盖非整除 shape。

验收：

- naive CUDA 和 reference 对拍通过。
- 能解释 A/B 元素被重复读取的问题。
- benchmark 表里有 naive 的 time、TFLOPS、correctness。

### Day 3：shared memory tiled matmul

- 一个 block 计算一个 C tile。
- A tile / B tile 加载进 shared memory。
- 每个 K tile 后使用 `__syncthreads()`。
- 支持非整除 M/N/K。

验收：

- tiled 版本 correctness pass。
- README 能说明 tile 存什么、为什么增加数据复用。
- benchmark 至少覆盖 3 个 shape。

### Day 4：register blocking v1

- 从简单的 1x2、2x1 或 2x2 thread tile 开始。
- 记录每个线程计算多个输出时复用哪些 A/B 值。
- 观察 register count、occupancy 和性能变化。

验收：

- register blocking v1 correctness pass。
- `profiling.md` 记录 register count 和 occupancy。
- 能说明 register blocking 不是免费午餐。

### Day 5：cuBLAS SGEMM baseline

- 用同一组 M/N/K shape 跑 cuBLAS SGEMM。
- 确认 row-major wrapper 或参数交换逻辑。
- 计时范围只包含 GEMM 本体。

验收：

- benchmark 表中 custom kernel 和 cuBLAS 同 shape 相邻。
- README 说明 cuBLAS 是 baseline，不把打不过解释成失败。
- 记录 GPU、CUDA、driver、cuBLAS 版本。

### Day 6：统一 benchmark runner

- 固定 warmup、repeat、CUDA event timing。
- 输出 `benchmark_results.csv`。
- 记录 dtype、layout、implementation、shape、time、TFLOPS、ratio vs cuBLAS。

验收：

- 一条命令能跑完整 benchmark matrix。
- 原始结果不手工修改。
- 异常数据保留并在报告里解释。

### Day 7：profiling + README

- 选择 1 到 2 个代表 shape 跑 Nsight Compute。
- 记录 achieved occupancy、SM utilization、memory throughput、stall reason、register count。
- 写 README 初稿和 `profiling.md`。

验收：

- 每个实现有“为什么快/慢”的解释。
- 至少一个结论能被 Nsight 指标支撑。
- 给 Week 10 的 Triton 对比留出接口和 benchmark 协议。

## 4. Benchmark / Profiling 指标

GEMM FLOPs：

```text
FLOPs = 2 * M * N * K
TFLOPS = FLOPs / time_seconds / 1e12
```

最小结果表：

| Impl | M | N | K | Dtype | Layout | Time(ms) | TFLOPS | Ratio vs cuBLAS | Correct |
|---|---:|---:|---:|---|---|---:|---:|---:|---|
| naive CUDA | 1024 | 1024 | 1024 | FP32 | row-major | TBD | TBD | TBD | pass |
| tiled CUDA | 1024 | 1024 | 1024 | FP32 | row-major | TBD | TBD | TBD | pass |
| register blocking v1 | 1024 | 1024 | 1024 | FP32 | row-major | TBD | TBD | TBD | pass |
| cuBLAS SGEMM | 1024 | 1024 | 1024 | FP32 | wrapper | TBD | TBD | 1.00 | pass |

Nsight 最少关注：

- achieved occupancy。
- SM utilization。
- memory throughput。
- stall reason。
- register count。
- shared memory usage。

## 5. 常见坑

> [!warning] 不要先追性能，后补 correctness
> GEMM 的性能数字只有在 reference 对拍通过后才有意义。非整除 shape、非方阵和不同 K 值都要覆盖。

> [!warning] 不要混入 H2D / D2H 时间
> benchmark 表里计时窗口必须只包含 GEMM 本体。数据拷贝时间可以单独记录，但不能混进 kernel / cuBLAS 对比。

> [!warning] 不要把 cuBLAS 当成普通 kernel
> cuBLAS 是高度优化的工业库 baseline。学习型 FP32 kernel 和 cuBLAS 的差距，应该解释为 tiling 层级、指令调度、寄存器复用、库工程优化等差异。

> [!warning] 不要只测 1024 方阵
> LLM projection shape 经常不是方阵。至少要有非方阵和非 tile 对齐尺寸。

## 6. 本周交付物

| 交付物 | 内容 | 验收 |
|---|---|---|
| `matmul-lab-cuda-triton-cutlass` 初版 | reference、CUDA kernels、cuBLAS baseline、benchmark runner | 能 build / run |
| `benchmark_results.csv` | naive、tiled、register blocking、cuBLAS | 同 shape 可比较 |
| `profiling.md` | Nsight 指标和瓶颈解释 | 结论能被指标支持 |
| `README.md` 初稿 | 项目目标、运行方式、结果表、差距解释 | 第三个人能复跑 |

## 验收标准

- [ ] CPU / PyTorch reference 可用。
- [ ] naive CUDA、tiled CUDA、register blocking v1 都 correctness pass。
- [ ] cuBLAS SGEMM baseline 跑通。
- [ ] benchmark runner 固定 warmup、repeat 和 timing scope。
- [ ] `benchmark_results.csv` 包含至少 4 个 shape。
- [ ] `profiling.md` 至少分析 1 个 shape 的 Nsight 指标。
- [ ] README 明确说明“打不过 cuBLAS 不是失败”。

## 面试问题

- 阶段二 GEMM 项目和 Week 4 MatMul v0 有什么区别？
- naive matmul 为什么慢？
- shared memory tiled matmul 复用了什么？
- register blocking 有什么用，为什么可能降低 occupancy？
- cuBLAS baseline 怎么做才公平？
- 为什么 GEMM 通常比 vector add 更偏 compute-bound？
- 你怎么证明 benchmark 可信？

## 关联知识

- [[AI Agent Native AI Infra GPU Performance Engineer 培养方案]]
- [[Week 4 - MatMul v0]]
- [[Week 10 - GEMM Triton + Benchmark Protocol]]
- [[3.8 CUDA Week 4 前置知识 - MatMul + Tiling + Roofline]]
- [[3.8.1 cuBLAS GEMM Baseline]]
- [[3.6 CUDA 生态工具清单]]
- [[CUDA 学习清单]]
