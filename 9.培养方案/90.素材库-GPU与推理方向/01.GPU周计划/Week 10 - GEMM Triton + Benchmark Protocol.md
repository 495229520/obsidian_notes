---
title: Week 10 - GEMM Triton + Benchmark Protocol
date: 2026-05-16
tags:
  - infra
  - CUDA
  - 阶段计划
aliases:
  - 阶段二 Week 10
  - GEMM Triton
  - Triton MatMul Benchmark
status: active
---

# Week 10 - GEMM Triton + Benchmark Protocol

> [!goal] 本周目标
> 把 [[Week 9 - GEMM Deep Dive v1]] 的 CUDA GEMM v1 接到 Triton matmul，并统一 correctness、benchmark、profiling 和 README 表达。重点不是一周内写出最强 Triton GEMM，而是让 CUDA / Triton / cuBLAS 能在同一套 shape、dtype、timing scope 和结果表里公平比较。

## 学习目标

完成这一周后，应该能回答八个问题：

1. **Triton matmul 的 program id 如何映射到 C tile？**
2. **mask load / store 为什么是处理边界 shape 的关键？**
3. **Triton 的 BLOCK_M / BLOCK_N / BLOCK_K 会影响什么？**
4. **Triton matmul v1 和 CUDA tiled v1 的数据复用有什么相同和不同？**
5. **为什么 CUDA / Triton / cuBLAS 必须使用同一套 benchmark 协议？**
6. **ratio vs cuBLAS 应该怎么解释？**
7. **什么指标能说明当前实现是 memory-bound、compute-bound，还是调度 / occupancy 问题？**
8. **GEMM comparison report 应该如何避免只堆数字？**

## 1. 为什么本周做这个

Week 9 已经把 FP32 GEMM 的工程骨架搭起来：

```text
reference
-> CUDA naive / tiled / register blocking
-> cuBLAS SGEMM
-> benchmark_results.csv
-> profiling.md
```

Week 10 要做的是把 Triton 放进这个体系，而不是另起一套散乱脚本。

Triton 的价值在于：

- 用更高层的 program/block 抽象表达 tile。
- 更方便写 mask load / store。
- 更容易快速尝试 BLOCK size、num_warps、num_stages。
- 适合和 PyTorch / serving 项目连接。

但 Triton 不是免 benchmark 的捷径。任何 Triton kernel 都必须和 CUDA、cuBLAS 在同一组 shape、同一 dtype、同一 timing scope 下比较。

## 2. 必做实现 / 实验场景

| 模块 | 目标 | 验收方式 |
|---|---|---|
| Triton matmul v1 | FP32 matmul，支持非整除 shape | correctness pass |
| block mapping | program id -> C tile | README 能画清楚 |
| mask load / store | 处理 M/N/K 边界 | 非整除 shape pass |
| benchmark adapter | CUDA / Triton / cuBLAS 同表 | `benchmark_results.csv` 有三类实现 |
| parameter sweep | BLOCK_M/N/K、num_warps | 至少 2 组配置对比 |
| profiling note | Triton vs CUDA 指标解释 | 结论不只看 TFLOPS |
| report skeleton | GEMM comparison report 初稿 | 能继续扩展 CUTLASS / Tensor Core |

最小 benchmark matrix：

| Shape | 用途 |
|---|---|
| 512 x 512 x 512 | 小规模 sanity benchmark |
| 1024 x 1024 x 1024 | 常规对比 |
| 1024 x 4096 x 4096 | LLM-like projection |
| 4096 x 4096 x 4096 | 大规模吞吐 |
| 127 x 129 x 131 | mask / 边界测试 |

## 3. 实现顺序

### Day 1：Triton matmul 最小实现

- 写 FP32 Triton matmul v1。
- 使用 `pid_m` / `pid_n` 映射 C tile。
- 用 `BLOCK_M`、`BLOCK_N`、`BLOCK_K` 控制 tile。
- 和 PyTorch reference 对拍。

验收：

- 规则 shape correctness pass。
- README 能说明 program id 如何定位 C tile。
- 输出 max abs error / RMS error。

### Day 2：mask load / store 和边界 shape

- 支持 M/N/K 不能被 block size 整除。
- 对 A/B load 使用 mask。
- 对 C store 使用 mask。
- 跑 `127 x 129 x 131` 这类故意不整除 shape。

验收：

- 非整除 shape correctness pass。
- 能解释为什么方阵通过不代表边界正确。
- 记录边界测试结果。

### Day 3：接入统一 benchmark runner

- 把 Triton matmul 加进 Week 9 的 benchmark runner。
- 输出同一份 `benchmark_results.csv`。
- 保持 warmup、repeat、dtype、shape、timing scope 一致。

验收：

- 同一 shape 下 CUDA / Triton / cuBLAS 相邻记录。
- ratio vs cuBLAS 自动或半自动生成。
- 原始结果不手动修改。

### Day 4：Triton 参数小扫

- 扫至少 2 到 3 组 BLOCK_M / BLOCK_N / BLOCK_K。
- 尝试不同 `num_warps`。
- 记录性能变化和 correctness。

验收：

- 能说明 block size 影响数据复用、并行度和 occupancy。
- 不把单个最佳数字当成通用结论。
- README 写清楚测试环境。

### Day 5：CUDA vs Triton vs cuBLAS 对比

- 选择 3 到 5 个 shape 做完整对比。
- 分析 naive CUDA、tiled CUDA、register blocking v1、Triton v1、cuBLAS SGEMM。
- 为每个实现写一句解释。

验收：

- 表格里每个数字都有 shape、dtype、版本、time、TFLOPS、correctness。
- 能解释 Triton v1 为什么可能快于 CUDA v1，也可能仍远慢于 cuBLAS。
- 不把 cuBLAS 当成普通手写 kernel。

### Day 6：Profiling / Nsight / Triton profiler 记录

- 对代表 shape 做 profiler 记录。
- CUDA 侧继续关注 Nsight Compute。
- Triton 侧记录可获得的 profiler 信息、kernel 配置和运行时间。

验收：

- `profiling.md` 有 CUDA / Triton 对比段落。
- 至少一个性能判断能被指标支持。
- 写清楚不能确定的部分，避免过度解释。

### Day 7：GEMM comparison report 骨架

写阶段二 GEMM report 初稿：

```text
目的
实验环境
实现版本
correctness 方法
benchmark 协议
结果表
profiling 观察
差距解释
限制
下一步：CUTLASS / Tensor Core / FP16
```

验收：

- 第三个人能按 README 复跑。
- 结论都能追溯到 CSV / profiler / 环境记录。
- 明确 Week 11 以后再进入 CUTLASS / Tensor Core。

## 4. Benchmark / Profiling 指标

统一 benchmark 字段：

| 字段 | 含义 |
|---|---|
| implementation | naive CUDA / tiled CUDA / register blocking / Triton / cuBLAS |
| M / N / K | GEMM shape |
| dtype | FP32 起步 |
| layout | row-major / wrapper |
| warmup / repeat | 测量协议 |
| time_ms | GEMM 本体时间 |
| TFLOPS | `2 * M * N * K / time` |
| ratio_vs_cublas | 同 shape 相对 cuBLAS |
| max_abs_error / rms_error | correctness 证据 |

报告里至少有一张主表：

| Impl | Shape | Time(ms) | TFLOPS | Ratio vs cuBLAS | Correct | Note |
|---|---|---:|---:|---:|---|---|
| CUDA tiled | 1024x1024x1024 | TBD | TBD | TBD | pass | shared memory |
| Triton v1 | 1024x1024x1024 | TBD | TBD | TBD | pass | block matmul |
| cuBLAS SGEMM | 1024x1024x1024 | TBD | TBD | 1.00 | pass | baseline |

## 5. 常见坑

> [!warning] 不要让 Triton 独立使用另一套输入和 shape
> CUDA、Triton、cuBLAS 的对比必须共享 shape、dtype、初始化、reference 和 timing scope。

> [!warning] 不要只测整除 shape
> Triton mask load / store 的价值，必须用非整除 M/N/K 验证。

> [!warning] 不要把 BLOCK size sweep 当成 autotune
> Week 10 只做小规模参数观察，不宣称找到最优配置。真正 autotune 后续再展开。

> [!warning] 不要只写 Triton 比 CUDA 简洁
> 代码简洁不是性能结论。需要用 benchmark 和 profiler 支撑。

## 6. 本周交付物

| 交付物 | 内容 | 验收 |
|---|---|---|
| Triton matmul v1 | FP32 matmul，支持 mask 边界 | correctness pass |
| 统一 benchmark runner | CUDA / Triton / cuBLAS 同表 | `benchmark_results.csv` 可追溯 |
| 参数 sweep 记录 | BLOCK size / num_warps 小对比 | 不夸大结论 |
| `profiling.md` 更新 | CUDA vs Triton 指标解释 | 至少一个指标支撑结论 |
| GEMM comparison report 骨架 | 环境、协议、表格、限制、下一步 | 可扩展到 CUTLASS |

## 验收标准

- [ ] Triton matmul v1 在规则 shape 和非整除 shape 上 correctness pass。
- [ ] CUDA / Triton / cuBLAS 使用同一 benchmark matrix。
- [ ] `benchmark_results.csv` 包含至少 5 个 shape。
- [ ] 同 shape 下有 ratio vs cuBLAS。
- [ ] 至少完成 2 组 Triton BLOCK 参数对比。
- [ ] `profiling.md` 有 CUDA / Triton 对比解释。
- [ ] report 骨架明确下一步是 CUTLASS / Tensor Core / FP16，而不是本周硬塞。

## 面试问题

- Triton matmul 的 program id 如何映射到 C tile？
- mask load / store 解决什么问题？
- BLOCK_M / BLOCK_N / BLOCK_K 会影响什么？
- Triton 和 CUDA tiled matmul 的共同点是什么？
- 为什么 Triton v1 仍可能远慢于 cuBLAS？
- ratio vs cuBLAS 应该怎么解释？
- 你怎么保证 CUDA / Triton / cuBLAS benchmark 公平？
- GEMM comparison report 为什么不能只放 TFLOPS？

## 关联知识

- [[AI Agent Native AI Infra GPU Performance Engineer 培养方案]]
- [[Week 9 - GEMM Deep Dive v1]]
- [[Week 4 - MatMul v0]]
- [[3.8 CUDA Week 4 前置知识 - MatMul + Tiling + Roofline]]
- [[3.8.1 cuBLAS GEMM Baseline]]
- [[3.6 CUDA 生态工具清单]]
- [[CUDA 学习清单]]
