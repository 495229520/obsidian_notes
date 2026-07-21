---
title: Week 13 - CUDA GEMM Warp Tiling + Pipeline
date: 2026-05-17
tags:
  - AI-infra/素材库-GPU与推理方向/GPU周计划/计划
aliases:
  - 阶段二 Week 13
  - CUDA GEMM Warp Tiling
  - GEMM Pipeline v2
status: active
---

# Week 13 - CUDA GEMM Warp Tiling + Pipeline

> [!goal] 本周目标
> 在 [[Week 12 - Tensor Core + Mixed Precision GEMM]] 之后，回到手写 CUDA GEMM，把 Week 9 的 naive / tiled / register blocking v1 升级成更接近工业 GEMM 思路的 v2：理解 threadblock tile、warp tile、thread tile、vectorized load、shared memory layout 和简单 pipeline。重点不是手写完整 Tensor Core MMA，而是能解释手写 CUDA GEMM 距离 CUTLASS / cuBLAS 还差哪些层级。

## 学习目标

完成这一周后，应该能回答八个问题：

1. **threadblock tile、warp tile、thread tile 的层级关系是什么？**
2. **为什么 Week 9 的 register blocking v1 还不够接近工业 GEMM？**
3. **vectorized load 对 global memory 访问有什么要求？**
4. **shared memory layout 为什么会影响 bank conflict？**
5. **simple pipeline / double buffering 试图隐藏什么延迟？**
6. **为什么手写 CUDA GEMM 很难追上 CUTLASS / cuBLAS？**
7. **Nsight 中哪些指标能说明 v2 是否真的变好？**
8. **哪些优化值得保留进最终 GEMM comparison report？**

## 1. 为什么本周做这个

Week 9 到 Week 12 已经建立了四条线：

```text
CUDA v1
Triton v1 / autotune
CUTLASS baseline
cuBLAS / GemmEx baseline
```

但如果只停留在 Week 9 的 tiled kernel，会很难解释工业 GEMM 的层级差距。本周要补一层“手写 CUDA 优化视角”：

```text
每个 block 算一个 C tile
  -> 每个 warp 负责 C tile 的一部分
  -> 每个 thread 负责多个 C 元素
  -> A/B 数据在 global、shared、register 中分层复用
```

这周的价值不是追求最优性能，而是让你能读懂 CUTLASS / cuBLAS 差距来自哪些工程层级。

## 2. 必做实现 / 实验场景

| 模块 | 目标 | 验收方式 |
|---|---|---|
| CUDA GEMM v2 | 在 v1 基础上加入 warp/thread tile 设计 | correctness pass |
| vectorized load | 尝试 `float4` 或等价向量化读取 | 记录 alignment 条件 |
| shared memory layout | 观察 bank conflict / padding 影响 | `profiling.md` 有解释 |
| simple pipeline | 尝试预取下一段 K tile | 记录收益或无收益 |
| Nsight 对比 | v1 vs v2 | 至少一个指标支撑结论 |
| report 更新 | 写清手写 CUDA 的优化边界 | 不夸大性能 |

建议保留 Week 9/10 的核心 shape：

| M | N | K | 用途 |
|---:|---:|---:|---|
| 512 | 512 | 512 | 快速迭代 |
| 1024 | 1024 | 1024 | 主对比 |
| 1024 | 4096 | 4096 | LLM-like projection |
| 4096 | 4096 | 4096 | 大规模吞吐 |
| 127 | 129 | 131 | 边界正确性 |

## 3. 实现顺序

### Day 1：设计 CUDA GEMM v2 层级

- 画出 threadblock tile、warp tile、thread tile。
- 明确一个 block 负责多大的 C tile。
- 明确每个 thread 累加几个 C 元素。
- 写 `cuda_gemm_v2_design.md`。

验收：

- 能说明 v2 和 Week 9 tiled v1 的区别。
- 设计里包含 tile size、thread layout、register accumulator 数量。
- 不急着写代码，先说明访存和复用路径。

### Day 2：实现 thread tile / register accumulator

- 一个 thread 负责多个 C 输出。
- accumulator 放在 register 中。
- 先只支持 FP32。

验收：

- correctness pass。
- 和 v1 共用 reference。
- 记录 register count 和 occupancy。

### Day 3：vectorized load 和 alignment

- 尝试向量化读取 A/B。
- 记录 shape / pointer alignment 条件。
- 对不满足条件的 shape 保留 fallback 或说明限制。

验收：

- benchmark 表中能区分 scalar load / vectorized load。
- README 写清 vectorized load 的适用条件。
- 非整除 shape 不被静默破坏。

### Day 4：shared memory layout / padding

- 观察 shared memory tile 的布局。
- 尝试 padding 或调整访问方式。
- 对比 bank conflict 相关指标。

验收：

- `profiling.md` 有 shared memory 段落。
- 能说明 padding 是否带来收益。
- 如果收益不明显，也要保留指标和解释。

### Day 5：simple pipeline / double buffering

- 尝试在 K tile 循环里预取下一块 A/B。
- 不要求实现复杂异步拷贝。
- 对比开启 / 关闭 pipeline 的结果。

验收：

- 记录实现限制。
- 能解释 pipeline 想隐藏 global memory latency。
- 不把一次 shape 的收益推广到所有 shape。

### Day 6：v1 / v2 / Triton / CUTLASS / cuBLAS 对比

- 用统一 benchmark runner 跑代表 shape。
- 将 CUDA v2 加入 comparison table。
- 对每个实现写一句定位。

验收：

- 表格包含 CUDA v1、CUDA v2、Triton、CUTLASS、cuBLAS。
- ratio vs cuBLAS 限定同 dtype / same shape。
- 能说明 CUDA v2 仍和工业库有差距。

### Day 7：写优化边界总结

更新 GEMM comparison report：

```text
CUDA v2 design
v1 vs v2 benchmark
Nsight evidence
what improved
what did not improve
why CUTLASS / cuBLAS still win
```

验收：

- 至少一个优化被保留。
- 至少一个优化被明确放弃或标记为后续。
- 报告能解释手写 CUDA GEMM 的学习价值。

## 4. Benchmark / Profiling 指标

重点对比：

| 指标 | 看什么 |
|---|---|
| TFLOPS | 结果性能 |
| register count | thread tile 的寄存器压力 |
| achieved occupancy | register / shared memory 是否限制并发 |
| shared memory bank conflict | layout 是否有问题 |
| global load efficiency | vectorized load 是否有效 |
| stall reason | 是否仍受 memory / dependency / instruction 调度影响 |

最小表格：

| Impl | Shape | Dtype | Time(ms) | TFLOPS | Ratio vs cuBLAS | Note |
|---|---|---|---:|---:|---:|---|
| CUDA tiled v1 | 1024x1024x1024 | FP32 | TBD | TBD | TBD | baseline |
| CUDA GEMM v2 | 1024x1024x1024 | FP32 | TBD | TBD | TBD | warp/thread tile |
| Triton v1 | 1024x1024x1024 | FP32 | TBD | TBD | TBD | DSL |
| CUTLASS | 1024x1024x1024 | FP32 | TBD | TBD | TBD | template baseline |
| cuBLAS SGEMM | 1024x1024x1024 | FP32 | TBD | TBD | 1.00 | vendor baseline |

## 5. 常见坑

> [!warning] 不要为优化破坏 correctness
> vectorized load、padding、thread tile 都容易引入边界 bug。每次优化都要先跑 correctness。

> [!warning] 不要只看一个 shape
> 某个 tile size 在 1024 方阵上表现好，不代表 LLM-like shape 也好。

> [!warning] 不要把简单 pipeline 说成工业 double buffering
> 本周只做入门版 pipeline 思路，不能夸大为完整工业实现。

> [!warning] 不要忽略 register pressure
> thread tile 变大可能提升复用，也可能降低 occupancy。

## 6. 本周交付物

| 交付物 | 内容 | 验收 |
|---|---|---|
| CUDA GEMM v2 | warp/thread tile、register accumulator | correctness pass |
| `cuda_gemm_v2_design.md` | tile 层级和访存路径 | 能解释设计 |
| `benchmark_results.csv` 更新 | v1 / v2 / Triton / CUTLASS / cuBLAS | 同 shape 对比 |
| `profiling.md` 更新 | register、occupancy、shared memory、stall | 结论有指标 |
| report CUDA v2 章节 | 优化收益和边界 | 不夸大 |

## 验收标准

- [ ] CUDA GEMM v2 correctness pass。
- [ ] 至少 3 个 shape 有 v1 vs v2 对比。
- [ ] 记录 register count 和 occupancy。
- [ ] 至少尝试 vectorized load 或 shared memory padding 之一。
- [ ] `profiling.md` 能解释一个优化是否有效。
- [ ] report 写清手写 CUDA 与 CUTLASS / cuBLAS 差距。

## 面试问题

- threadblock tile、warp tile、thread tile 有什么区别？
- register blocking 为什么可能让 occupancy 下降？
- vectorized load 需要什么前提？
- shared memory bank conflict 如何影响 GEMM？
- pipeline 想隐藏什么延迟？
- 为什么手写 CUDA GEMM 追 cuBLAS 很难？

## 关联知识

- [[AI Agent Native AI Infra GPU Performance Engineer 培养方案]]
- [[Week 9 - GEMM Deep Dive v1]]
- [[Week 10 - GEMM Triton + Benchmark Protocol]]
- [[Week 11 - CUTLASS GEMM + Industrial Baseline]]
- [[Week 12 - Tensor Core + Mixed Precision GEMM]]
- [[3.8 CUDA Week 4 前置知识 - MatMul + Tiling + Roofline]]
- [[3.8.1 cuBLAS GEMM Baseline]]
- [[3.6 CUDA 生态工具清单]]
- [[CUDA 学习清单]]
