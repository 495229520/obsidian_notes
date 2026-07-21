---
title: Week 15 - GEMM Profiling + Roofline Report
date: 2026-05-17
tags:
  - AI-infra/素材库-GPU与推理方向/GPU周计划/计划
aliases:
  - 阶段二 Week 15
  - GEMM Profiling
  - Roofline Report
status: active
---

# Week 15 - GEMM Profiling + Roofline Report

> [!goal] 本周目标
> 把 Week 9-14 的 CUDA、Triton、CUTLASS、cuBLAS、cuBLASLt 和 mixed precision 结果从“很多 benchmark 数字”整理成一份有解释力的 profiling / roofline report。重点是用 Nsight Compute、TFLOPS、arithmetic intensity、occupancy、memory throughput、stall reason 和 correctness 证据解释性能差距。

## 学习目标

完成这一周后，应该能回答八个问题：

1. **GEMM 为什么通常要同时看 TFLOPS 和 roofline thinking？**
2. **arithmetic intensity 如何帮助判断 compute-bound / memory-bound？**
3. **Nsight Compute 里哪些指标最适合解释 GEMM？**
4. **occupancy 高为什么不等于性能一定高？**
5. **memory throughput 高为什么也不等于实现一定好？**
6. **如何比较 CUDA / Triton / CUTLASS / cuBLAS 的 profiling 结果？**
7. **如何把 correctness、benchmark、profiling 连成同一条证据链？**
8. **GEMM report 怎样避免只堆截图？**

## 1. 为什么本周做这个

前面几周已经逐步加入：

- CUDA v1 / v2。
- Triton matmul / autotune。
- CUTLASS baseline。
- cuBLAS / cuBLASLt。
- FP32 / FP16 / BF16 / GemmEx。

现在问题不再是“有没有数字”，而是：

```text
这些数字说明了什么？
为什么某个实现快？
为什么某个实现慢？
这个结论能不能被 profiler 支撑？
```

本周要把零散结果变成工程报告。

## 2. 必做实现 / 实验场景

| 模块 | 目标 | 验收方式 |
|---|---|---|
| benchmark matrix 冻结 | 选定最终核心 shape | 不再随意加实验 |
| Nsight profile | CUDA v1/v2、代表 Triton、可选 CUTLASS | 指标可追溯 |
| roofline thinking | 计算 FLOPs、数据量、arithmetic intensity | report 有判断 |
| 结果清洗 | 保留 raw data，派生 summary | 不改原始数据 |
| report 图表 | 表格、趋势、结论 | 结论能回到 CSV |
| limitations | 写清不能证明的部分 | 不夸大 |

核心 shape 建议冻结为：

| Shape | 用途 |
|---|---|
| 512x512x512 | 小规模 sanity |
| 1024x1024x1024 | 常规对比 |
| 1024x4096x4096 | LLM-like projection |
| 4096x4096x4096 | 大 GEMM |
| 127x129x131 | correctness / boundary |

## 3. 实现顺序

### Day 1：冻结 benchmark matrix

- 选定最终 shape。
- 选定最终实现列表。
- 确认每个实现都有 correctness 结果。
- 备份 raw CSV / JSON。

验收：

- `benchmark_matrix.md` 写清最终实验矩阵。
- raw data 不手工修改。
- 每个结论都能追溯到文件。

### Day 2：Nsight Compute 采样

- 对 CUDA tiled v1、CUDA v2 运行 Nsight Compute。
- 如果可行，对 Triton 生成的 kernel 做采样。
- 记录命令和结果路径。

验收：

- `profiling.md` 有命令、shape、实现名。
- 至少两个实现有 profiler 指标。
- 记录工具版本和 GPU 信息。

### Day 3：整理关键指标

关注：

- achieved occupancy。
- SM utilization。
- memory throughput。
- shared memory bank conflict。
- register count。
- stall reason。
- tensor core 或相关低精度指标，如果可得。

验收：

- 每个指标都解释“看它是为了回答什么问题”。
- 不把截图当结论。
- 至少一个慢点能被指标解释。

### Day 4：roofline thinking

- 计算 GEMM FLOPs。
- 粗略估算数据移动量。
- 计算 arithmetic intensity。
- 判断不同实现更像 memory-bound、compute-bound 还是调度不足。

验收：

- report 里有 roofline thinking 段落。
- 不需要画严格 roofline 模型，但要有判断逻辑。
- 说明估算的限制。

### Day 5：横向对比

- 按实现维度比较：CUDA、Triton、CUTLASS、cuBLAS、cuBLASLt。
- 按 dtype 维度比较：FP32、FP16 / BF16。
- 按 shape 维度比较：方阵、LLM-like、边界 shape。

验收：

- 至少三张 summary table。
- 每张表都有一句工程解释。
- 不跨 dtype 乱算 ratio。

### Day 6：写 profiling report

报告结构：

```text
Experiment setup
Benchmark matrix
Correctness method
Raw data link
Profiler commands
Key metrics
Roofline thinking
Findings
Limitations
Next steps
```

验收：

- report 不只贴图。
- 每个 finding 都能找到对应证据。
- limitations 明确。

### Day 7：review 和修订

- 检查有没有夸大结论。
- 检查是否混淆 FP32 和 mixed precision。
- 检查 raw data 是否保留。
- 检查 README 是否能引导第三个人复跑。

验收：

- 形成可放进作品集的 `gemm_profiling_report.md`。
- 为 Week 16 的最终 portfolio polish 做准备。
- 明确还缺哪些实验，不假装完整。

## 4. Benchmark / Profiling 指标

report 至少包含：

| 类别 | 指标 |
|---|---|
| correctness | max abs error、RMS error、tolerance |
| performance | time_ms、TFLOPS、ratio vs baseline |
| resource | register count、shared memory usage、occupancy |
| memory | global memory throughput、shared bank conflict |
| execution | SM utilization、stall reason |
| experiment | GPU、CUDA、driver、library versions |

核心原则：

```text
benchmark tells what happened
profiler explains why it may have happened
limitations say what is not proven
```

## 5. 常见坑

> [!warning] 不要把 profiler 截图当结论
> 截图只是证据载体。报告必须写出指标说明了什么、不能说明什么。

> [!warning] 不要跨 dtype 直接比较
> FP32 和 FP16/BF16 分开解释，ratio 必须限定同类口径。

> [!warning] 不要追求覆盖所有 shape
> 本周目标是把核心 shape 解释清楚，不是无限扩大 benchmark matrix。

> [!warning] 不要改 raw data
> 可以派生 summary table，但原始 benchmark 结果必须保留。

## 6. 本周交付物

| 交付物 | 内容 | 验收 |
|---|---|---|
| `benchmark_matrix.md` | 最终 shape / implementation / dtype | 实验边界清楚 |
| `profiling.md` | Nsight 命令和指标摘要 | 可追溯 |
| `gemm_profiling_report.md` | roofline、指标、结论、限制 | 可放作品集 |
| summary tables | CUDA / Triton / CUTLASS / cuBLAS / cuBLASLt | 不混口径 |

## 验收标准

- [ ] 最终 benchmark matrix 冻结。
- [ ] 至少两个手写 CUDA 实现有 Nsight 记录。
- [ ] report 有 roofline thinking 段落。
- [ ] report 至少包含 3 个 finding。
- [ ] 每个 finding 能追溯到 CSV / profiler / 环境记录。
- [ ] limitations 明确写出没有证明的内容。
- [ ] raw data 未被手工修改。

## 面试问题

- GEMM 的 TFLOPS 怎么算？
- arithmetic intensity 怎么理解？
- occupancy 高为什么不一定快？
- stall reason 怎么帮助定位瓶颈？
- Nsight Compute 中你最关注哪些 GEMM 指标？
- 你怎么证明 benchmark 和 profiler 结论可信？
- 你的 CUDA / Triton / CUTLASS / cuBLAS 对比有什么限制？

## 关联知识

- [[AI Agent Native AI Infra GPU Performance Engineer 培养方案]]
- [[Week 9 - GEMM Deep Dive v1]]
- [[Week 10 - GEMM Triton + Benchmark Protocol]]
- [[Week 11 - CUTLASS GEMM + Industrial Baseline]]
- [[Week 12 - Tensor Core + Mixed Precision GEMM]]
- [[Week 13 - CUDA GEMM Warp Tiling + Pipeline]]
- [[Week 14 - cuBLASLt + Advanced GEMM Baselines]]
- [[Week 16 - GEMM Comparison Report + Portfolio Polish]]
- [[3.8 CUDA Week 4 前置知识 - MatMul + Tiling + Roofline]]
- [[3.4 CUDA Nsight Compute 指标速查]]
- [[3.8.1 cuBLAS GEMM Baseline]]
