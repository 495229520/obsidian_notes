---
title: Week 12 - Tensor Core + Mixed Precision GEMM
date: 2026-05-17
tags:
  - AI-infra
  - 素材库-GPU与推理方向
  - GPU周计划
  - 计划
aliases:
  - 阶段二 Week 12
  - Tensor Core GEMM
  - Mixed Precision GEMM
status: active
---

# Week 12 - Tensor Core + Mixed Precision GEMM

> [!goal] 本周目标
> 在 [[Week 11 - CUTLASS GEMM + Industrial Baseline]] 的工业 baseline 基础上，把 FP16 / BF16、Tensor Core、cuBLAS HGEMM / GemmEx 和 Triton autotune 初版纳入同一套 GEMM benchmark 解释。重点不是手写 Tensor Core MMA kernel，而是能记录 input dtype、output dtype、compute type、tolerance、baseline 边界，并解释为什么 mixed precision GEMM 的性能和 correctness 不能照搬 FP32 口径。

## 学习目标

完成这一周后，应该能回答八个问题：

1. **SGEMM、HGEMM、GemmEx 的实验口径有什么不同？**
2. **为什么 FP16 / BF16 correctness tolerance 不能照搬 FP32？**
3. **FP16 input + FP32 accumulation 和 FP16 accumulate 的区别是什么？**
4. **Tensor Core 为什么和 dtype、shape、alignment、GPU 架构有关？**
5. **什么时候可以说“可能使用 Tensor Core”，什么时候不能断言？**
6. **Triton autotune 初版应该如何纳入 benchmark 协议？**
7. **mixed precision 结果如何和 FP32 CUDA / Triton / CUTLASS / cuBLAS 区分？**
8. **GEMM comparison report 的 limitations 应该如何写？**

## 1. 为什么本周做这个

Week 9 到 Week 11 已经建立了 FP32 GEMM 主线：

```text
CUDA kernels
Triton matmul
CUTLASS example / profiler
cuBLAS SGEMM baseline
统一 benchmark + profiling + report
```

但真实 LLM 推理里的 GEMM 很少只停留在 FP32。继续往下必须面对：

- FP16 / BF16 输入。
- FP32 accumulation。
- Tensor Core 路径。
- cuBLAS HGEMM / GemmEx。
- Triton autotune。
- correctness tolerance 和性能解释的变化。

本周的重点是建立 mixed precision 的实验纪律：

```text
不要只写 dtype = FP16
要写 input dtype / output dtype / compute type / tolerance / baseline
```

## 2. 必做实现 / 实验场景

| 模块 | 目标 | 验收方式 |
|---|---|---|
| FP16 / BF16 reference | 明确 reference 和 tolerance | correctness 不误判 |
| cuBLAS HGEMM / GemmEx | 记录 input/output/compute type | baseline 可解释 |
| Triton FP16 matmul | 接入已有 benchmark runner | correctness pass |
| Triton autotune 初版 | 小范围 sweep / autotune config | 不夸大最优性 |
| CUTLASS mixed precision | 可选跑 example / profiler | 记录限制 |
| Tensor Core notes | 解释可能路径和证据边界 | `tensor_core_notes.md` |
| report upgrade | mixed precision / limitations 章节 | 不混淆 FP32 与 FP16 |

建议 shape：

| M | N | K | 用途 |
|---:|---:|---:|---|
| 512 | 512 | 512 | mixed precision sanity |
| 1024 | 1024 | 1024 | 常规 HGEMM |
| 1024 | 4096 | 4096 | LLM-like projection |
| 4096 | 4096 | 4096 | 大规模吞吐 |
| 128 | 256 | 768 | 小型非方阵 |

## 3. 实现顺序

### Day 1：mixed precision 实验口径

- 复习 [[3.8.1.4 cuBLAS Day 4 前置知识 - HGEMM 与 GemmEx]]。
- 定义 benchmark 字段：
  - input dtype。
  - output dtype。
  - compute type。
  - accumulate type。
  - tolerance。
- 写 FP16 / BF16 correctness 规则。

验收：

- `tensor_core_notes.md` 写清 SGEMM / HGEMM / GemmEx 区别。
- correctness 输出 max abs error / RMS error。
- tolerance 不照搬 FP32。

### Day 2：cuBLAS HGEMM / GemmEx baseline

- 跑 `cublasHgemm` 或 `cublasGemmEx`。
- 记录 input/output/compute type。
- 和 FP32 reference 或合适 reference 对拍。

验收：

- 至少 3 个 shape 有 HGEMM / GemmEx benchmark。
- 表格明确区分 SGEMM、HGEMM、GemmEx。
- README 写清 mixed precision baseline 边界。

### Day 3：Triton FP16 matmul

- 将 Triton matmul 扩展到 FP16 / BF16 输入。
- 明确 accumulation 语义。
- 接入统一 benchmark runner。

验收：

- Triton FP16 / BF16 matmul correctness pass。
- 表格不把 FP32 和 FP16 混成一组比较。
- 记录 shape、dtype、tolerance。

### Day 4：Triton autotune 初版

- 为 Triton matmul 加小范围 autotune config。
- 尝试 BLOCK_M / BLOCK_N / BLOCK_K、num_warps、num_stages。
- 保留每个候选配置结果或日志。

验收：

- 至少 3 组 autotune 配置。
- 结果可以复现，不只保存最佳数字。
- 报告写清 autotune 范围有限，不代表全局最优。

### Day 5：Tensor Core 解释和证据边界

- 复习 [[3.8.1.5 cuBLAS Day 5 前置知识 - Tensor Core 解释]]。
- 记录 GPU compute capability、CUDA、cuBLAS、Triton、CUTLASS 版本。
- 如果 profiler 能看到相关 kernel / 指令，再写更明确结论。

验收：

- `tensor_core_notes.md` 能解释 Tensor Core 为什么和 FP16 / BF16 / TF32 相关。
- 如果没有 profiler 证据，只写“可能使用 Tensor Core”。
- 不把 FP32 custom kernel 和 FP16 Tensor Core baseline 当成同类比较。

### Day 6：mixed precision comparison table

- 组织 FP32 和 FP16 / BF16 分开的结果表。
- 对比 CUDA / Triton / CUTLASS / cuBLAS 时保留 dtype 口径。
- 记录 ratio vs cuBLAS 时必须限定同 dtype / 同 compute type。

验收：

- comparison table 有 `input_dtype`、`output_dtype`、`compute_type`。
- FP32 表和 FP16 / BF16 表不混写。
- ratio vs cuBLAS 不跨 dtype 乱算。

### Day 7：report 升级和 limitations

升级 GEMM comparison report：

```text
mixed precision setup
correctness tolerance
cuBLAS HGEMM / GemmEx baseline
Triton autotune scope
Tensor Core evidence boundary
FP32 vs FP16/BF16 result separation
limitations
next step: deeper CUTLASS / cuBLASLt / LLM kernels
```

验收：

- 第三个人能看懂每个数字的 dtype 和 compute type。
- limitations 不空泛，明确写出没有证明的内容。
- 不把 mixed precision 的收益简单写成“Tensor Core 很快”。

## 4. Benchmark / Profiling 指标

mixed precision 表格必须比 FP32 多几个字段：

| Impl | M | N | K | Input | Output | Compute | Time(ms) | TFLOPS | Error | Note |
|---|---:|---:|---:|---|---|---|---:|---:|---:|---|
| cuBLAS HGEMM | 1024 | 1024 | 1024 | FP16 | FP16 | implicit | TBD | TBD | TBD | baseline |
| GemmEx | 1024 | 1024 | 1024 | FP16 | FP16 | FP32 acc | TBD | TBD | TBD | explicit types |
| Triton FP16 | 1024 | 1024 | 1024 | FP16 | FP16 | FP32 acc | TBD | TBD | TBD | autotune v1 |
| CUTLASS | 1024 | 1024 | 1024 | FP16 | FP16 | TBD | TBD | TBD | TBD | profiler |

必须记录：

- tolerance。
- reference dtype。
- input distribution。
- GPU compute capability。
- CUDA / cuBLAS / Triton / CUTLASS 版本。
- 是否有 profiler 证据支持 Tensor Core 结论。

## 5. 常见坑

> [!warning] 不要只写 dtype = FP16
> FP16 input、FP16 output、FP32 accumulation 是不同维度。报告必须写清 input、output、compute type。

> [!warning] 不要用 FP32 阈值误判 FP16
> FP16 / BF16 的舍入误差更大，对拍阈值要合理，并说明 reference 和 tolerance。

> [!warning] 不要跨 dtype 算 ratio vs cuBLAS
> FP32 custom kernel vs FP16 cuBLAS HGEMM 不是公平比较。ratio 必须限定同 shape、同 dtype、同 compute type。

> [!warning] 不要无证据断言 Tensor Core
> 如果没有 profiler 或日志确认，使用“可能使用 Tensor Core 路径”这种谨慎表达。

> [!warning] 不要把 autotune 结果夸大
> Week 12 只做初版 Triton autotune，不声明找到全局最优。

## 6. 本周交付物

| 交付物 | 内容 | 验收 |
|---|---|---|
| mixed precision benchmark | HGEMM / GemmEx / Triton FP16 / 可选 CUTLASS | dtype 字段完整 |
| Triton autotune 记录 | config、shape、结果、限制 | 可复现 |
| `tensor_core_notes.md` | Tensor Core 直觉、证据边界、常见误区 | 表述谨慎 |
| GEMM report mixed precision 章节 | correctness、benchmark、limitations | 不混淆 FP32 / FP16 |
| `benchmark_results.csv` 更新 | 增加 dtype / compute type 字段 | 原始数据可追溯 |

## 验收标准

- [ ] 至少跑通 cuBLAS HGEMM 或 GemmEx。
- [ ] 至少 3 个 shape 有 mixed precision benchmark。
- [ ] Triton FP16 / BF16 matmul 接入统一 benchmark。
- [ ] correctness tolerance 明确记录。
- [ ] 表格包含 input dtype、output dtype、compute type。
- [ ] Tensor Core 结论有证据边界。
- [ ] Triton autotune 至少有 3 组候选配置记录。
- [ ] report limitations 明确写出不能证明的内容。

## 面试问题

- SGEMM、HGEMM、GemmEx 有什么区别？
- 为什么 FP16 input 通常要关注 FP32 accumulation？
- FP16 / BF16 correctness 怎么设 tolerance？
- Tensor Core 为什么快？
- 什么时候可以说这个 case 使用了 Tensor Core？
- 为什么不能把 FP32 custom kernel 和 FP16 cuBLAS 直接算 ratio？
- Triton autotune 调的是什么？
- mixed precision GEMM 报告最容易犯什么错误？

## 关联知识

- [[AI Agent Native AI Infra GPU Performance Engineer 培养方案]]
- [[Week 9 - GEMM Deep Dive v1]]
- [[Week 10 - GEMM Triton + Benchmark Protocol]]
- [[Week 11 - CUTLASS GEMM + Industrial Baseline]]
- [[3.8.1 cuBLAS GEMM Baseline]]
- [[3.8.1.4 cuBLAS Day 4 前置知识 - HGEMM 与 GemmEx]]
- [[3.8.1.5 cuBLAS Day 5 前置知识 - Tensor Core 解释]]
- [[3.6 CUDA 生态工具清单]]
- [[CUDA 学习清单]]
