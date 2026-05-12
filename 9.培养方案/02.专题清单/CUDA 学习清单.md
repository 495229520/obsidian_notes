---
title: CUDA 学习清单
date: 2026-05-06
tags:
  - CUDA
  - infra
aliases:
  - CUDA checklist
  - CUDA 路线清单
status: active
---

# CUDA 学习清单

> 这份清单承接 [[3.1 CUDA Week 1 零基础系统入门|CUDA 零基础系统入门]]、[[Week 1 - CUDA + Agent workflow]] 和 [[AI Agent Native AI Infra GPU Performance Engineer 培养方案]]。目标不是继续堆 CUDA 语法，而是把学习推进到 **性能模型 + 典型算子 + profiling 证据链**。

---

## 0. 当前定位

已有内容：

- [[3.1 CUDA Week 1 零基础系统入门|CUDA 零基础系统入门]]：执行模型、内存模型、`vector add`、CMake、基础 benchmark。
- [[Week 1 - CUDA + Agent workflow]]：CUDA 项目模板、测试、benchmark、Agent 权限边界。
- [[AI Agent Native AI Infra GPU Performance Engineer 培养方案]]：阶段路线、作品集项目、投递目标。

接下来要补齐：

- Week 2：[[Week 2 - Reduction + Profiling]]
- Week 3：[[Week 3 - Transpose + Memory Coalescing]]
- Week 4：[[Week 4 - MatMul v0]]
- profiling：[[3.4 CUDA Nsight Compute 指标速查|CUDA Nsight Compute 指标速查]]
- runtime：[[3.2 CUDA Runtime 进阶|CUDA Runtime 进阶]]
- ecosystem：[[3.6 CUDA 生态工具清单|CUDA 生态工具清单]]
- LLM kernels：[[LLM Kernel 专题清单]]

---

## 1. CUDA 基础闭环

- [ ] 能解释 CUDA 适合什么问题，不适合什么问题。
- [ ] 能区分 host code、device code、kernel。
- [ ] 能写 `__global__` kernel。
- [ ] 能解释 `<<<grid, block>>>`。
- [ ] 能解释 `blockIdx.x * blockDim.x + threadIdx.x`。
- [ ] 能正确使用 `cudaMalloc`、`cudaMemcpy`、`cudaFree`。
- [ ] 能在 kernel launch 后做 `cudaGetLastError` 和同步检查。
- [ ] 能用 RAII 封装 device memory。
- [ ] 能写最小 CMake CUDA 项目。
- [ ] 能实现 `vector add`，并覆盖小规模、大规模、非 block 对齐长度。

验收输出：

- 一个可构建的 `agentic-cuda-kernel-playground`。
- `tests/` 中包含 correctness test。
- `benchmarks/` 中包含 CUDA event benchmark。
- README 写清楚构建、测试、benchmark 命令。

---

## 2. Benchmark 与正确性

- [ ] 能写 CPU reference。
- [ ] 能写 correctness test，比较 CUDA 输出和 CPU/PyTorch 输出。
- [ ] 能区分 correctness test 和 benchmark。
- [ ] 能用 CUDA event timing。
- [ ] 能解释 warm-up 为什么必要。
- [ ] 能重复运行并输出平均值、最小值、最大值。
- [ ] 能计算 effective bandwidth。
- [ ] 能区分 kernel-only 时间和 end-to-end 时间。
- [ ] 能记录 GPU 型号、CUDA 版本、驱动版本、编译参数。
- [ ] 每个 kernel 都保留 `benchmark.md`。

最低 benchmark 表格：

| Kernel | Shape | Dtype | Time(ms) | Bandwidth/TFLOPS | Correct | Note |
|---|---:|---|---:|---:|---|---|
| vector add | 1M | FP32 | TBD | TBD GB/s | pass | kernel-only |

---

## 3. GPU 执行模型

- [ ] thread / block / grid。
- [ ] warp 是什么。
- [ ] SIMT 是什么。
- [ ] warp divergence 为什么慢。
- [ ] block size 如何影响性能。
- [ ] occupancy 是什么。
- [ ] occupancy 高为什么不一定快。
- [ ] register pressure 如何影响 occupancy。
- [ ] shared memory 使用量如何影响 occupancy。
- [ ] 能用 occupancy calculator 或 Nsight 指标解释配置选择。

必须能回答：

- 为什么 `vector add` 通常是 memory-bound？
- 为什么 `matmul` 更偏 compute-bound？
- 为什么 block size 不是越大越好？
- 为什么 occupancy 只是线索，不是最终目标？

---

## 4. CUDA 内存模型

- [ ] global memory。
- [ ] shared memory。
- [ ] register。
- [ ] local memory。
- [ ] constant memory。
- [ ] pinned / page-locked host memory。
- [ ] unified memory / managed memory。
- [ ] memory coalescing。
- [ ] alignment。
- [ ] vectorized load，例如 `float4`、`half2`。
- [ ] shared memory bank conflict。
- [ ] padding 如何减少 bank conflict。
- [ ] 数据搬运为什么可能比计算更贵。

学习顺序：

1. global memory 连续访问。
2. shared memory tiled 访问。
3. bank conflict 复现与 padding。
4. pinned memory 与异步拷贝。
5. Nsight 中观察 memory throughput。

---

## 5. 同步、并发与 Runtime API

- [ ] `__syncthreads()`。
- [ ] warp-level primitive，例如 shuffle。
- [ ] atomic operation。
- [ ] stream 基础。
- [ ] async copy / overlap copy and compute。
- [ ] event 与 stream 的关系。
- [ ] pinned memory + stream 实现 H2D/D2H overlap。
- [ ] CUDA Graph 基础。
- [ ] error handling 统一封装。
- [ ] device query，读取 SM 数、shared memory、warp size、compute capability。

专题笔记：[[3.2 CUDA Runtime 进阶|CUDA Runtime 进阶]]

---

## 6. 必做入门 Kernel

- [ ] `vector add`
- [ ] `saxpy`
- [ ] elementwise multiply
- [ ] ReLU / GELU toy kernel
- [ ] simple reduction sum
- [ ] prefix sum 入门版
- [ ] histogram 或 atomic counter toy example

每个 kernel 的固定交付：

- CPU reference。
- CUDA kernel。
- correctness test。
- CUDA event benchmark。
- 结果表格。
- 一段瓶颈判断。

---

## 7. Reduction 系列

- [ ] naive reduce。
- [ ] block reduce。
- [ ] shared memory reduce。
- [ ] warp shuffle reduce。
- [ ] 多 block reduce。
- [ ] reduce max。
- [ ] row-wise reduce。
- [ ] benchmark 不同数据规模。
- [ ] 用 Nsight 看 memory throughput、occupancy、stall reason。

详细计划：[[Week 2 - Reduction + Profiling]]

---

## 8. Transpose 与访存优化

- [ ] naive transpose。
- [ ] coalesced read / uncoalesced write 分析。
- [ ] shared memory tiled transpose。
- [ ] bank conflict 复现。
- [ ] padding 解决 bank conflict。
- [ ] 写 `profiling.md` 解释 naive 和 tiled 的差距。
- [ ] 能回答“为什么 shared memory transpose 更快”。

详细计划：[[Week 3 - Transpose + Memory Coalescing]]

---

## 9. MatMul / GEMM

- [ ] naive matmul。
- [ ] shared memory tiled matmul。
- [ ] register blocking matmul。
- [ ] FP32 / FP16 对比。
- [ ] cuBLAS baseline。
- [ ] CUTLASS example 复现。
- [ ] Tensor Core 基础。
- [ ] roofline thinking。
- [ ] 多个 M/N/K shape benchmark。
- [ ] README 解释每个版本为什么快/慢。

详细计划：[[Week 4 - MatMul v0]]

---

## 10. LLM 常见算子

- [ ] RMSNorm。
- [ ] LayerNorm 对比。
- [ ] Softmax，包含数值稳定版本。
- [ ] RoPE。
- [ ] SwiGLU / GELU。
- [ ] top-k sampling toy kernel。
- [ ] INT8 dequant toy kernel。
- [ ] kernel fusion。
- [ ] `half2`。
- [ ] 每个算子都有 PyTorch reference。
- [ ] 至少 3 个算子同时写 CUDA 和 Triton 版本。

专题笔记：[[LLM Kernel 专题清单]]

---

## 11. Profiling

- [ ] CUDA event。
- [ ] Nsight Compute。
- [ ] Nsight Systems。
- [ ] memory throughput。
- [ ] achieved occupancy。
- [ ] SM utilization。
- [ ] warp stall reason。
- [ ] register pressure。
- [ ] shared memory bank conflict 指标。
- [ ] roofline 分析。
- [ ] 每次优化必须有 profiler 证据。

专题笔记：[[3.4 CUDA Nsight Compute 指标速查|CUDA Nsight Compute 指标速查]]

---

## 12. CUDA 生态

- [ ] cuBLAS：作为 GEMM baseline。
- [ ] CUTLASS：理解 GEMM hierarchy 和 Tensor Core。
- [ ] CUB：学习 block/warp primitives。
- [ ] Thrust：了解高级并行算法接口。
- [ ] PyTorch C++ / CUDA extension。
- [ ] Triton fused softmax。
- [ ] Triton matmul。
- [ ] Triton RMSNorm。
- [ ] vLLM benchmark。
- [ ] FlashAttention / PagedAttention 思想。
- [ ] TensorRT-LLM 可选，不作为早期主线。

专题笔记：[[3.6 CUDA 生态工具清单|CUDA 生态工具清单]]

---

## 推荐顺序

1. 先完成 `agentic-cuda-kernel-playground`：`vector add`、benchmark、correctness test、`benchmark.md`。
2. 接着学 reduction：这是理解 warp、shared memory、同步和 profiling 的第一道坎。
3. 再学 transpose：专门打穿 memory coalescing 和 bank conflict。
4. 然后学 matmul：进入 compute-bound、tiling、register blocking、Tensor Core。
5. 最后进入 LLM kernels：RMSNorm、Softmax、RoPE、dequant、fusion。
6. 每个阶段都要留下代码、correctness test、benchmark 表、Nsight 截图/摘要、结论说明。

---

## 每周固定复盘

- 本周实现了哪些 kernel？
- 哪些 correctness case 通过？
- 哪个指标证明它更快或更慢？
- Nsight 指标是否支持自己的结论？
- 哪个优化是有效的，哪个只是看起来合理？
- 下周要补哪个最短板？

---

## 关键要点总结

1. CUDA 学习的主线不是语法，而是从正确性到性能证据的闭环。
2. `vector add`、reduction、transpose、matmul 是最小必经路径。
3. Nsight 结论必须和 benchmark 结果一起出现。
4. cuBLAS、CUTLASS、Triton、PyTorch extension 是生态能力，不要早期替代手写基础 kernel。
5. LLM kernel 学习要围绕 RMSNorm、Softmax、RoPE、dequant、fusion 展开。
