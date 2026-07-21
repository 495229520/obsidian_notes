---
title: Week 11 - CUTLASS GEMM + Industrial Baseline
date: 2026-05-17
tags:
  - infra
  - CUDA
  - 阶段计划
aliases:
  - 阶段二 Week 11
  - CUTLASS GEMM
  - Industrial GEMM Baseline
status: active
---

# Week 11 - CUTLASS GEMM + Industrial Baseline

> [!goal] 本周目标
> 在 [[Week 10 - GEMM Triton + Benchmark Protocol]] 的 CUDA / Triton / cuBLAS 统一 benchmark 基础上，引入 CUTLASS 作为工业级 GEMM hierarchy 学习对象。重点是跑通 CUTLASS GEMM examples / profiler，把 CUTLASS benchmark 行放进同一张 comparison 表，并能解释 threadblock tile、warp tile、instruction tile、Tensor Core 在 CUTLASS 中的大致位置，而不是急着改 CUTLASS 核心模板。

## 学习目标

完成这一周后，应该能回答八个问题：

1. **CUTLASS 和手写 CUDA / Triton / cuBLAS 的定位有什么不同？**
2. **CUTLASS GEMM hierarchy 里 threadblock tile、warp tile、instruction tile 分别是什么层次？**
3. **为什么本周目标是跑通和解释，而不是修改 CUTLASS 核心模板？**
4. **CUTLASS example / profiler 的结果如何放进统一 benchmark 表？**
5. **CUTLASS 与 cuBLAS 的比较应该怎么写才不夸大？**
6. **CUTLASS 的 Tensor Core 路径应该如何谨慎表述？**
7. **哪些 shape 适合用来做 CUTLASS baseline 对比？**
8. **GEMM comparison report 的 CUTLASS 章节应该包含哪些限制？**

## 1. 为什么本周做这个

经过 Week 9 和 Week 10，你已经有：

```text
CUDA naive / tiled / register blocking
Triton matmul v1
cuBLAS SGEMM baseline
统一 correctness + benchmark 协议
```

但这些还不能完整回答工业 GEMM 为什么强。CUTLASS 的价值在这里：

- 它展示了工业级 GEMM 如何分层组织 tile。
- 它把 threadblock、warp、instruction、epilogue 等概念放进模板体系。
- 它可以作为比手写学习 kernel 更接近工业实现的参考。
- 它能帮助你理解 Tensor Core 路径和 GEMM hierarchy，但不要求你从零写出同等级实现。

本周的关键边界：

```text
跑通 CUTLASS -> 复现实验 -> 放进同表对比 -> 写清 hierarchy 和限制
```

不是：

```text
深入修改 CUTLASS 内部模板并宣称掌握工业 GEMM
```

## 2. 必做实现 / 实验场景

| 模块 | 目标 | 验收方式 |
|---|---|---|
| CUTLASS 环境 | clone / build / 运行 example 或 profiler | 能记录版本和命令 |
| GEMM example 复现 | 跑 FP32 或默认 GEMM example | 有日志和结果 |
| CUTLASS profiler | 选择若干 M/N/K shape | 能输出 benchmark 行 |
| 统一表格接入 | CUDA / Triton / CUTLASS / cuBLAS 同表 | `benchmark_results.csv` 可扩展 |
| hierarchy notes | threadblock / warp / instruction tile 直觉 | `cutlass_notes.md` |
| report 章节 | comparison report 加 CUTLASS 段落 | 有差距解释和限制 |

建议 shape 继续沿用 Week 9/10：

| M | N | K | 用途 |
|---:|---:|---:|---|
| 512 | 512 | 512 | 小规模 sanity |
| 1024 | 1024 | 1024 | 常规 GEMM |
| 1024 | 4096 | 4096 | LLM-like projection |
| 4096 | 4096 | 4096 | 大规模吞吐 |
| 127 | 129 | 131 | 边界对比，不一定所有 CUTLASS 配置都适合 |

## 3. 实现顺序

### Day 1：CUTLASS 定位和环境记录

- 阅读 [[3.6 CUDA 生态工具清单]] 中 CUTLASS 部分。
- 确认 CUTLASS repo、版本、CUDA、GPU、compiler。
- 跑通一个最小 example 或 profiler help。
- 记录安装和构建命令。

验收：

- `cutlass_notes.md` 写清 CUTLASS 是学习 GEMM hierarchy 和工业 baseline 的参考。
- README 记录 CUTLASS 版本和构建方式。
- 不把 CUTLASS 当成自己手写 kernel 的替代解释。

### Day 2：GEMM example reproduction

- 跑一个 CUTLASS GEMM example。
- 固定 dtype、layout、M/N/K。
- 保存命令、输出、日志。

验收：

- 至少一个 CUTLASS GEMM example 能稳定复现。
- 记录 input dtype、output dtype、accumulate type。
- 说明这个 example 对应的是哪类 GEMM。

### Day 3：CUTLASS profiler 进入 benchmark matrix

- 用 CUTLASS profiler 或 example 跑 Week 10 的核心 shape。
- 将 CUTLASS 结果转换到统一字段：
  - implementation
  - M/N/K
  - dtype
  - layout
  - time_ms
  - TFLOPS
  - correctness / verification
  - note

验收：

- `benchmark_results.csv` 可以容纳 CUTLASS 行。
- 同一 shape 下 CUDA / Triton / CUTLASS / cuBLAS 相邻。
- README 写清 CUTLASS 结果来自 example / profiler，而不是自己的 kernel。

### Day 4：GEMM hierarchy 笔记

写 `cutlass_notes.md`：

- threadblock tile。
- warp tile。
- instruction tile。
- mainloop。
- epilogue。
- Tensor Core 在低精度 GEMM 中的位置。

验收：

- 能用自己的话解释 CUTLASS 分层。
- 不需要展开复杂模板源码。
- 能说明这些层次和 Week 9/10 的 tile 概念如何连接。

### Day 5：CUTLASS vs cuBLAS vs Triton vs CUDA

- 选择 3 个代表 shape 做主表。
- 每个实现写一句定位：
  - CUDA：学习型手写 kernel。
  - Triton：DSL 快速实现和调参。
  - CUTLASS：工业模板库参考。
  - cuBLAS：vendor library baseline。

验收：

- comparison 表不只写 TFLOPS，还写可维护性、可调空间、开发成本、正确性风险。
- 不宣称 CUTLASS 一定快于 cuBLAS。
- 不把 CUTLASS example 结果和手写 kernel 混成同类贡献。

### Day 6：profiling 和差距解释

- 对代表 shape 记录 CUTLASS / cuBLAS / Triton / CUDA 的可获得 profiler 信息。
- 如果没有完整 profiler，也要记录限制。
- 分析差距来自 tile 层级、Tensor Core、library selection、调度或数据类型。

验收：

- `profiling.md` 增加 CUTLASS 段落。
- 至少一个结论有指标或文档依据。
- 对不能确认的 Tensor Core 路径保持谨慎表述。

### Day 7：GEMM report 的 CUTLASS 章节

在 GEMM comparison report 中补：

```text
CUTLASS setup
CUTLASS command
GEMM shape table
hierarchy explanation
CUTLASS vs CUDA/Triton/cuBLAS
limitations
next step: mixed precision / Tensor Core / autotune
```

验收：

- 第三个人能复现 CUTLASS 命令。
- 报告能说明 CUTLASS 的学习价值。
- 明确 Week 12 进入 mixed precision 和 Tensor Core 解释。

## 4. Benchmark / Profiling 指标

统一表格继续沿用 Week 10：

| Impl | Shape | Dtype | Time(ms) | TFLOPS | Ratio vs cuBLAS | Correct | Note |
|---|---|---|---:|---:|---:|---|---|
| CUDA tiled | 1024x1024x1024 | FP32 | TBD | TBD | TBD | pass | hand-written |
| Triton v1 | 1024x1024x1024 | FP32 | TBD | TBD | TBD | pass | DSL kernel |
| CUTLASS | 1024x1024x1024 | FP32 | TBD | TBD | TBD | pass | template library |
| cuBLAS SGEMM | 1024x1024x1024 | FP32 | TBD | TBD | 1.00 | pass | vendor baseline |

除了 TFLOPS，还要记录：

- CUTLASS commit / release。
- build type。
- CUDA / driver / GPU。
- profiler command。
- kernel / operation 名称。
- dtype、layout、accumulate type。

## 5. 常见坑

> [!warning] 不要把 CUTLASS 写成自己的手写实现
> CUTLASS 是工业模板库和学习参考。报告里要明确区分“我复现并分析 CUTLASS example”和“我手写了 CUDA / Triton kernel”。

> [!warning] 不要急着改 CUTLASS 核心模板
> 本周的价值是理解 hierarchy 和建立 baseline。修改模板源码属于后续深入，不是 Week 11 最低目标。

> [!warning] 不要用不同 shape 做横向比较
> CUTLASS、CUDA、Triton、cuBLAS 必须按同 shape 对比，否则 ratio 没意义。

> [!warning] 不要断言 Tensor Core 路径
> 如果没有 profiler 或日志证据，不要写“这次一定用了 Tensor Core”。可以写“该配置可能使用 Tensor Core，具体需结合 profiler 确认”。

## 6. 本周交付物

| 交付物 | 内容 | 验收 |
|---|---|---|
| CUTLASS example 复现记录 | 命令、环境、日志、结果 | 第三个人可复跑 |
| CUTLASS benchmark 行 | 接入统一 GEMM comparison 表 | 同 shape 对比 |
| `cutlass_notes.md` | GEMM hierarchy、使用边界、限制 | 能解释层级 |
| `profiling.md` 更新 | CUTLASS 和其他实现差距解释 | 至少一个指标支撑 |
| GEMM report CUTLASS 章节 | setup、结果、解释、限制 | 不夸大贡献 |

## 验收标准

- [ ] CUTLASS example 或 profiler 能运行。
- [ ] 记录 CUTLASS 版本、CUDA、GPU、命令。
- [ ] 至少 3 个 shape 有 CUTLASS benchmark 行。
- [ ] CUTLASS 结果能进入 CUDA / Triton / cuBLAS 同表。
- [ ] `cutlass_notes.md` 能解释 threadblock / warp / instruction tile。
- [ ] report 明确区分 CUTLASS 复现和手写 kernel。
- [ ] 不修改 CUTLASS 核心模板作为本周必要目标。

## 面试问题

- CUTLASS 和 cuBLAS 的区别是什么？
- CUTLASS 和手写 CUDA / Triton 的区别是什么？
- threadblock tile、warp tile、instruction tile 分别是什么层次？
- 为什么学习 CUTLASS 能帮助理解工业 GEMM？
- CUTLASS benchmark 怎么和 cuBLAS 公平比较？
- 为什么不能把 CUTLASS example 说成自己的 kernel？
- Tensor Core 在 CUTLASS 中处在什么位置？

## 关联知识

- [[AI Agent Native AI Infra GPU Performance Engineer 培养方案]]
- [[Week 9 - GEMM Deep Dive v1]]
- [[Week 10 - GEMM Triton + Benchmark Protocol]]
- [[Week 12 - Tensor Core + Mixed Precision GEMM]]
- [[3.8.1 cuBLAS GEMM Baseline]]
- [[3.8.1.4 cuBLAS Day 4 前置知识 - HGEMM 与 GemmEx]]
- [[3.8.1.5 cuBLAS Day 5 前置知识 - Tensor Core 解释]]
- [[3.6 CUDA 生态工具清单]]
- [[CUDA 学习清单]]
