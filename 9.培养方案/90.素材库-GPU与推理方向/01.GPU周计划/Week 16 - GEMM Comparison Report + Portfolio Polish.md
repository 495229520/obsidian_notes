---
title: Week 16 - GEMM Comparison Report + Portfolio Polish
date: 2026-05-17
tags:
  - infra
  - CUDA
  - 阶段计划
aliases:
  - 阶段二 Week 16
  - GEMM Comparison Report
  - GEMM Portfolio Polish
status: active
---

# Week 16 - GEMM Comparison Report + Portfolio Polish

> [!goal] 本周目标
> 收束阶段二前八周的 GEMM 深入，把 Week 9-15 的代码、benchmark、profiling、CUTLASS / cuBLAS / cuBLASLt baseline、Triton autotune 和 mixed precision 结果整理成一份可展示的 `matmul-lab-cuda-triton-cutlass` 作品集报告。重点是可复现、可解释、不过度包装。

## 学习目标

完成这一周后，应该能回答八个问题：

1. **这个 GEMM 项目证明了哪些能力？**
2. **哪些实现是你手写的，哪些是工业 baseline？**
3. **benchmark 如何保证公平？**
4. **correctness 如何保证不被性能优化破坏？**
5. **profiling 结论有哪些证据？**
6. **哪些地方仍然不如 cuBLAS / CUTLASS，为什么？**
7. **README 怎样写才像工程项目而不是笔记堆砌？**
8. **这个项目如何衔接下一阶段 LLM kernels / attention / serving？**

## 1. 为什么本周做这个

过去七周已经完成了很多材料：

- CUDA naive / tiled / v2。
- Triton matmul / autotune。
- CUTLASS example / profiler。
- cuBLAS / cuBLASLt。
- mixed precision / Tensor Core 解释。
- profiling / roofline report。

如果不收束，这些材料会变成散落的实验。Week 16 的任务是把它们整理成作品集：

```text
repo structure
-> reproducible commands
-> correctness tests
-> benchmark tables
-> profiler evidence
-> comparison report
-> interview story
```

这周不是继续堆新功能，而是完成“可信交付”。

## 2. 必做实现 / 实验场景

| 模块 | 目标 | 验收方式 |
|---|---|---|
| README polish | 项目目标、运行方式、结果摘要 | 第三个人能复跑 |
| report finalize | GEMM comparison report 定稿 | 结论可追溯 |
| artifact index | CSV、profiler、logs、notes 索引 | 文件不散 |
| interview story | 3 分钟 / 10 分钟版本 | 能讲清能力 |
| limitations | 写清没做什么 | 不过度包装 |
| next steps | LLM kernels / attention / serving / multi-GPU inference 衔接 | 路线自然 |

最终实现列表建议清楚分组：

| 分组 | 实现 |
|---|---|
| Hand-written | CUDA naive、CUDA tiled、CUDA v2 |
| DSL | Triton matmul、Triton autotune |
| Template baseline | CUTLASS |
| Vendor baseline | cuBLAS、cuBLASLt |

## 3. 实现顺序

### Day 1：整理 repo 和 artifact index

- 检查目录结构。
- 整理 benchmark raw data。
- 整理 profiler 输出。
- 整理 notes / report。

验收：

- `artifacts_index.md` 列出所有重要文件。
- raw data、summary、report 分开。
- 不删除失败实验，只标注状态。

### Day 2：README 结构

README 至少包含：

```text
Project goal
Implemented kernels
Baselines
Correctness
Benchmark protocol
Results summary
Profiling findings
How to reproduce
Limitations
Next steps
```

验收：

- 新读者先看 README 就知道项目价值。
- 命令可以复制运行。
- baseline 边界写清楚。

### Day 3：最终 comparison report

定稿 `gemm_comparison_report.md`：

- FP32 结果。
- mixed precision 结果。
- CUDA / Triton / CUTLASS / cuBLAS / cuBLASLt 对比。
- profiling findings。
- roofline thinking。
- limitations。

验收：

- 每个主结论都能追溯到证据。
- 图表不脱离原始 CSV。
- 不跨 dtype 乱比较。

### Day 4：correctness 和 benchmark 复查

- 复查 reference test。
- 复查 tolerance。
- 复查 warmup / repeat / timing scope。
- 复查 benchmark 是否混入 H2D / D2H。

验收：

- `correctness.md` 或 README 小节清楚。
- benchmark 协议固定。
- 所有异常点有解释或标记。

### Day 5：面试表达整理

准备两版表达：

```text
3 分钟版本：项目目标 -> 实现 -> benchmark -> 结论
10 分钟版本：correctness -> optimization -> baseline -> profiling -> limitations
```

验收：

- 能讲清“我手写了什么”。
- 能讲清“我复现 / 使用了什么 baseline”。
- 能讲清“我没有证明什么”。

### Day 6：作品集 polish

- 整理 README 表格。
- 整理 report 摘要。
- 检查链接。
- 检查命令。
- 检查图表和表格标题。

验收：

- 项目可作为简历链接。
- README 不像草稿。
- report 不夸大。

### Day 7：阶段二前八周复盘

写 `gemm_stage2_retro.md`：

- 学到了什么。
- 哪些实现值得保留。
- 哪些优化收益不明显。
- 下一阶段做 LLM kernels / attention 时如何复用 benchmark 协议。
- 后续 serving 路线：PagedAttention、RadixAttention、continuous batching、chunked prefill、PD disaggregation。
- 后续多 GPU 推理路线：TP / DP / PP / EP、NCCL collectives、RDMA、MoE EP / All-to-All。

验收：

- 明确 Week 17 之后进入 LLM 小算子、attention、serving engine 或 multi-GPU inference。
- GEMM 项目形成阶段性闭环。
- 不继续无限优化 GEMM，避免主线拖延。
- 不把 Week 16 包装成已经实现多 GPU runtime，只写后续作品集方向和面试表达边界。

## 4. Benchmark / Profiling 指标

最终 report 至少要有：

| 表格 | 内容 |
|---|---|
| Correctness summary | shape、dtype、error、tolerance |
| FP32 performance | CUDA / Triton / CUTLASS / cuBLAS / cuBLASLt |
| Mixed precision | HGEMM / GemmEx / Triton FP16 / CUTLASS |
| Profiling findings | occupancy、memory throughput、stall、register |
| Engineering tradeoff | implementation、performance、maintainability、debug cost |

一句话原则：

```text
数字证明结果，profiling 解释原因，limitations 保护可信度。
```

## 5. 常见坑

> [!warning] 不要把项目包装成打败 cuBLAS
> 这个项目的价值是理解 GEMM 优化链路、baseline 和 profiler 证据，不是宣称超过工业库。

> [!warning] 不要混淆手写实现和库 baseline
> CUDA / Triton 是自己的实现，CUTLASS / cuBLAS / cuBLASLt 是复现和参考 baseline。

> [!warning] 不要只保留好看的数据
> 异常点、失败 shape、性能不佳的尝试都可以成为有价值的 limitations。

> [!warning] 不要让 GEMM 主线无限延长
> Week 16 要形成阶段性闭环，后续转向 LLM kernels、attention 或 serving。

## 6. 本周交付物

| 交付物 | 内容 | 验收 |
|---|---|---|
| `README.md` 定稿 | 项目目标、运行、结果、限制 | 可作为作品集入口 |
| `gemm_comparison_report.md` | 全量对比和解释 | 结论可追溯 |
| `artifacts_index.md` | raw data、profiler、report 索引 | 文件清楚 |
| `gemm_stage2_retro.md` | 阶段复盘和后续路线 | 能转入下一模块 |
| `multi_gpu_inference_next_steps.md` | TP / DP / PP / EP、NCCL collectives、RDMA、MoE EP / All-to-All 后续索引 | 边界清楚，不冒充已实现 |
| 面试表达 | 3 分钟 / 10 分钟版本 | 能讲清项目 |

## 验收标准

- [ ] README 有完整复现命令。
- [ ] report 包含 FP32 和 mixed precision 两部分。
- [ ] report 明确区分 CUDA / Triton / CUTLASS / cuBLAS / cuBLASLt。
- [ ] correctness、benchmark、profiling 三条证据链完整。
- [ ] limitations 明确。
- [ ] 有 3 分钟和 10 分钟面试表达。
- [ ] Week 17 之后的下一主线明确。
- [ ] 后续多 GPU 推理路线包含 TP / DP / PP / EP、NCCL、RDMA、MoE EP / All-to-All，但明确这不是 Week 16 已完成实现。

## 面试问题

- 这个 GEMM 项目最能证明你的什么能力？
- 你手写了哪些 kernel？
- CUTLASS / cuBLAS / cuBLASLt 在项目里是什么角色？
- 怎么保证 benchmark 公平？
- 怎么保证 correctness？
- 哪个优化最有效？证据是什么？
- 哪个优化没达到预期？你怎么判断？
- 为什么你的实现打不过 cuBLAS 不是失败？
- 这个项目如何连接 LLM inference？

## 关联知识

- [[AI Agent Native AI Infra GPU Performance Engineer 培养方案]]
- [[Week 9 - GEMM Deep Dive v1]]
- [[Week 10 - GEMM Triton + Benchmark Protocol]]
- [[Week 11 - CUTLASS GEMM + Industrial Baseline]]
- [[Week 12 - Tensor Core + Mixed Precision GEMM]]
- [[Week 13 - CUDA GEMM Warp Tiling + Pipeline]]
- [[Week 14 - cuBLASLt + Advanced GEMM Baselines]]
- [[Week 15 - GEMM Profiling + Roofline Report]]
- [[3.8 CUDA Week 4 前置知识 - MatMul + Tiling + Roofline]]
- [[3.8.1 cuBLAS GEMM Baseline]]
- [[3.6 CUDA 生态工具清单]]
