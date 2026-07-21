---
title: Week 14 - cuBLASLt + Advanced GEMM Baselines
date: 2026-05-17
tags:
  - AI-infra/素材库-GPU与推理方向/GPU周计划/计划
aliases:
  - 阶段二 Week 14
  - cuBLASLt
  - Advanced GEMM Baselines
status: active
---

# Week 14 - cuBLASLt + Advanced GEMM Baselines

> [!goal] 本周目标
> 在普通 [[3.8.1 cuBLAS GEMM Baseline]]、CUTLASS 和 Triton 已经稳定后，引入 cuBLASLt 和更完整的 vendor baseline 思路。重点是理解 cuBLASLt 为什么适合更灵活的 layout、algorithm selection、workspace、epilogue 和 heuristic，而不是把它当成另一个简单函数名。

## 学习目标

完成这一周后，应该能回答七个问题：

1. **cuBLAS 和 cuBLASLt 的定位差异是什么？**
2. **为什么 cuBLASLt 更适合 advanced GEMM baseline？**
3. **layout、algorithm、workspace、heuristic 分别影响什么？**
4. **epilogue fusion 和普通 GEMM 有什么关系？**
5. **cuBLASLt 结果如何放进现有 comparison report？**
6. **为什么不能把 cuBLASLt 结果和手写 kernel 当成同类贡献？**
7. **advanced baseline 对面试表达有什么价值？**

## 1. 为什么本周做这个

前几周已经有：

- 手写 CUDA v1 / v2。
- Triton matmul 和初版 autotune。
- CUTLASS example / profiler。
- cuBLAS SGEMM / HGEMM / GemmEx。

普通 cuBLAS 已经足够做 baseline，但阶段二还需要知道工业库如何处理更复杂 GEMM 场景：

```text
layout descriptor
algorithm heuristic
workspace
mixed precision
epilogue
```

这就是 cuBLASLt 的价值。它不是 Week 4 的入门内容，而是普通 baseline 稳定之后，用来理解更灵活 GEMM 调度和工程配置的高级参考。

## 2. 必做实现 / 实验场景

| 模块 | 目标 | 验收方式 |
|---|---|---|
| cuBLASLt 最小 GEMM | 跑通一个 FP32 或 FP16 case | correctness pass |
| descriptor notes | matrix layout、operation descriptor | `cublaslt_notes.md` |
| heuristic / algorithm | 记录选择过程和 workspace | 日志可追溯 |
| benchmark 接入 | cuBLAS / cuBLASLt 同表 | 同 shape 比较 |
| epilogue 调研 | bias / activation 等融合位置 | 只做说明或最小实验 |
| report 更新 | advanced baseline 章节 | 不夸大结论 |

建议先用少量 shape：

| M | N | K | 用途 |
|---:|---:|---:|---|
| 1024 | 1024 | 1024 | sanity |
| 1024 | 4096 | 4096 | LLM-like projection |
| 4096 | 4096 | 4096 | large GEMM |

## 3. 实现顺序

### Day 1：cuBLASLt 定位和接口地图

- 阅读 cuBLASLt 的最小 GEMM 调用链。
- 记录 handle、operation descriptor、matrix layout、preference、algorithm。
- 写 `cublaslt_notes.md`。

验收：

- 能说明 cuBLASLt 和 `cublasSgemm` 的区别。
- 能画出 descriptor -> heuristic -> matmul 的调用链。
- 不急着加入复杂 epilogue。

### Day 2：最小 cuBLASLt GEMM

- 跑通一个 FP32 或 FP16 GEMM。
- 和 reference 对拍。
- 记录 input/output/compute type。

验收：

- correctness pass。
- 命令和代码路径可复现。
- benchmark 不混入 H2D / D2H 时间。

### Day 3：heuristic 和 workspace

- 尝试获取多个 algorithm candidate。
- 记录 workspace size。
- 选择一个稳定算法运行。

验收：

- `cublaslt_notes.md` 写清 heuristic 不是魔法。
- 记录至少一个 algorithm / workspace 配置。
- 不宣称找到全局最优。

### Day 4：接入统一 benchmark 表

- 将 cuBLASLt 结果加入 existing matrix。
- 同 shape 下对比 cuBLAS、cuBLASLt、CUTLASS、Triton、CUDA。
- ratio 明确 baseline 是 cuBLAS 还是 cuBLASLt。

验收：

- 表格字段完整。
- README 写清 cuBLASLt 是 advanced vendor baseline。
- 不跨 dtype 乱比较。

### Day 5：epilogue fusion 调研

- 了解 bias、activation、scale 等 epilogue 的位置。
- 如果成本可控，跑一个最小 bias epilogue case。
- 如果不跑实验，写清楚为何暂不展开。

验收：

- 能说明 epilogue fusion 可能减少 memory traffic / launch overhead。
- 能说明 fusion 也会带来 correctness 和配置复杂度。
- 不把 epilogue 调研扩成新主线。

### Day 6：advanced baseline 对比

- 选择 2 到 3 个 shape 做 cuBLAS vs cuBLASLt 对比。
- 记录 layout、dtype、compute type、workspace、algorithm。
- 写差异解释。

验收：

- comparison report 有 cuBLASLt 小节。
- 不把 cuBLASLt 结果当作手写优化成果。
- 解释 advanced baseline 对项目可信度的价值。

### Day 7：整理报告和下一步边界

更新 GEMM comparison report：

```text
cuBLASLt setup
descriptor and heuristic
workspace
benchmark table
epilogue notes
limitations
```

验收：

- 第三个人能复现 cuBLASLt benchmark。
- report 能解释为什么引入 cuBLASLt。
- 下周转入 profiling / roofline 收束。

## 4. Benchmark / Profiling 指标

cuBLASLt 需要额外记录：

| 字段 | 含义 |
|---|---|
| layout | A/B/C 的 layout descriptor |
| op | trans / non-trans |
| compute type | FP32 / FP16 / BF16 等 |
| algorithm | heuristic 选择的 algo |
| workspace | workspace size |
| epilogue | none / bias / activation |

最小对比：

| Impl | Shape | Input | Compute | Workspace | Time(ms) | TFLOPS | Note |
|---|---|---|---|---:|---:|---:|---|
| cuBLAS SGEMM | 1024x1024x1024 | FP32 | FP32 | 0 | TBD | TBD | simple baseline |
| cuBLASLt | 1024x1024x1024 | FP32 | FP32 | TBD | TBD | TBD | heuristic |

## 5. 常见坑

> [!warning] 不要把 cuBLASLt 当成只是另一个名字
> cuBLASLt 的重点是 layout、heuristic、workspace 和 epilogue 等更灵活配置。

> [!warning] 不要忽略 workspace
> 不同 workspace 可能影响可选算法和性能，报告里要记录。

> [!warning] 不要把 heuristic 结果当成绝对最优
> heuristic 是候选选择，不等于全局最优证明。

> [!warning] 不要让 epilogue 抢主线
> epilogue fusion 只作为 advanced baseline 认知，不在本周展开成完整 fusion 项目。

## 6. 本周交付物

| 交付物 | 内容 | 验收 |
|---|---|---|
| cuBLASLt 最小 GEMM | descriptor、layout、heuristic、matmul | correctness pass |
| `cublaslt_notes.md` | 调用链、workspace、heuristic、限制 | 能解释定位 |
| benchmark 更新 | cuBLASLt 行进入统一表 | 同 shape 对比 |
| report advanced baseline 章节 | cuBLASLt 和 cuBLAS / CUTLASS 关系 | 不夸大 |

## 验收标准

- [ ] cuBLASLt 最小 GEMM 跑通。
- [ ] 至少 2 个 shape 有 cuBLASLt benchmark。
- [ ] 记录 layout、compute type、algorithm、workspace。
- [ ] cuBLASLt 结果进入 comparison report。
- [ ] 能说明 cuBLASLt 和 cuBLAS / CUTLASS 的区别。
- [ ] epilogue 只做最小调研或清晰说明不展开。

## 面试问题

- cuBLASLt 和 cuBLAS 有什么区别？
- cuBLASLt heuristic 是什么？
- workspace 为什么影响 GEMM 性能？
- epilogue fusion 解决什么问题？
- 为什么 cuBLASLt 是 advanced baseline，不是你的手写 kernel？
- cuBLASLt 和 CUTLASS 的学习价值分别是什么？

## 关联知识

- [[AI Agent Native AI Infra GPU Performance Engineer 培养方案]]
- [[Week 10 - GEMM Triton + Benchmark Protocol]]
- [[Week 11 - CUTLASS GEMM + Industrial Baseline]]
- [[Week 12 - Tensor Core + Mixed Precision GEMM]]
- [[Week 13 - CUDA GEMM Warp Tiling + Pipeline]]
- [[Week 15 - GEMM Profiling + Roofline Report]]
- [[3.8.1 cuBLAS GEMM Baseline]]
- [[3.8.1.4 cuBLAS Day 4 前置知识 - HGEMM 与 GemmEx]]
- [[3.8.1.5 cuBLAS Day 5 前置知识 - Tensor Core 解释]]
- [[3.6 CUDA 生态工具清单]]
