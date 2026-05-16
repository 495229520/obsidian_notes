---
title: Week 2 - Reduction + Profiling
date: 2026-05-06
tags:
  - infra
  - CUDA
  - 阶段计划
aliases:
  - CUDA Week 2
  - Reduction Profiling
status: active
---

# Week 2 - Reduction + Profiling

> 第二周目标：用 reduction 打穿 GPU 并行归约、shared memory、warp-level primitive 和第一次 Nsight Compute 分析。它是从“能写 kernel”进入“能解释性能”的第一关。

---

## 学习目标

1. 能写出 naive reduce、block reduce、shared memory reduce、warp shuffle reduce。
2. 能解释 reduction 为什么需要分阶段处理。
3. 能用 CUDA event 比较不同实现。
4. 能用 Nsight Compute 观察 memory throughput、achieved occupancy、warp stall reason。
5. 能写一篇 `profiling.md`，说明哪个实现更快、为什么更快、证据是什么。

---

## 1. Reduction 要解决什么

Reduction 是把一组数据压缩成一个结果：

```cpp
sum = a[0] + a[1] + ... + a[n - 1];
max = max(a[0], a[1], ..., a[n - 1]);
```

它和 `vector add` 的差别在于：

- `vector add` 中每个输出元素互不依赖。
- `reduction` 中多个线程要共同产生一个结果。
- 线程之间需要同步或分阶段汇总。

这正好引出 CUDA 优化的核心问题：**如何让大量线程协作，同时避免同步和访存开销吞掉收益。**

---

## 2. 必做实现

| 版本 | 目标 | 重点观察 |
|---|---|---|
| CPU reference | 正确性基准 | 与 CUDA 输出比较 |
| naive reduce | 每个 block 输出一个 partial sum | 全局访存和边界处理 |
| shared memory reduce | block 内用 shared memory 汇总 | `__syncthreads()` 与 shared memory |
| warp shuffle reduce | warp 内用 shuffle 汇总 | 减少 shared memory 和同步 |
| multi-pass reduce | 多次 kernel 把 partial sum 汇总成最终结果 | kernel launch 和中间 buffer |
| reduce max | 把 sum 模式推广到 max | 初值和边界处理 |
| row-wise reduce | 为 RMSNorm / Softmax 做准备 | 一行一个 block 或多个 warp |

---

## 3. 实现顺序

### Day 1：CPU reference + naive reduce

- 建立输入生成器。
- 写 CPU `sum_reference`。
- 写 naive CUDA partial sum。
- 每个 block 输出一个 partial sum。
- CPU 侧再汇总 partial sum。

验收：

- `n = 1`、`n = 1024`、`n = 1 << 20` 都正确。
- `n` 不是 block size 整数倍时正确。

### Day 2：shared memory reduce

- 每个线程从 global memory 读一个或多个元素。
- 写入 shared memory。
- block 内逐步二分归约。
- 每轮归约后使用 `__syncthreads()`。

必须回答：

- 为什么 block 内可以用 shared memory？
- 为什么每轮归约需要同步？
- 为什么 shared memory 不等于一定更快？

### Day 3：warp shuffle reduce

- 学习 `__shfl_down_sync`。
- warp 内不用 shared memory 做归约。
- block 内可以先 warp reduce，再把每个 warp 的结果写入 shared memory。

必须回答：

- warp 内线程为什么可以用 shuffle 交换数据？
- shuffle reduce 和 shared memory reduce 的差别是什么？
- 哪些场景 shuffle 更合适？

### Day 4：benchmark

- 统一不同版本的输入、输出和计时方式。
- 每个 size 跑 warm-up。
- 每个版本重复运行多次。
- 输出平均值、最小值、最大值。
- 计算 effective bandwidth。

建议 size：

| Size | 目的 |
|---:|---|
| 1K | 小规模 launch overhead 观察 |
| 1M | 常规吞吐观察 |
| 64M | 大规模 memory bandwidth 观察 |

### Day 5：Nsight Compute

重点看：

- memory throughput。
- achieved occupancy。
- warp stall reason。
- register count。
- shared memory usage。

记录到 `profiling.md`：

```text
Kernel:
Shape:
GPU:
CUDA:
Time:
Effective bandwidth:
Nsight key metrics:
Bottleneck:
Next optimization:
```

### Day 6-7：复盘与文档

- README 写清楚每个版本的设计。
- benchmark 表格固定下来。
- `profiling.md` 写出结论。
- 把最有价值的错误记录到 debug note。

---

## 4. 常见坑

> [!warning] 忘记处理非整除长度
> 最后一个 block 往往不是满的，每次从 global memory 读取前都要检查边界。

> [!warning] 把 floating point 误差当 bug
> 并行归约改变加法顺序，FP32 结果可能和 CPU 串行求和有微小差异。测试应使用容忍误差。

> [!warning] 只看耗时不看正确性
> reduction 写错后可能更快，因为它少算了数据。benchmark 必须保留 correctness check。

> [!warning] occupancy 低就盲目调 block size
> occupancy 只是线索。要结合 memory throughput、stall reason 和实际耗时判断。

---

## 5. 本周交付物

| 交付物 | 内容 | 验收 |
|---|---|---|
| reduction kernels | naive、shared memory、warp shuffle | correctness pass |
| benchmark | 多 size、多版本、CUDA event | 输出稳定表格 |
| profiling.md | Nsight Compute 指标和结论 | 结论能被指标支持 |
| README | 解释每个版本设计 | 能回答面试问题 |

---

## 面试问题

- Reduction 为什么不能像 vector add 一样每个线程独立写一个输出？
- 为什么需要 shared memory？
- `__syncthreads()` 同步的是谁？
- warp shuffle reduce 为什么可能比 shared memory reduce 快？
- 多 block reduction 为什么通常需要多阶段？
- FP32 reduction 为什么会有误差？
- 如何判断 reduction 是 memory-bound 还是 compute-bound？

---

## 关联知识

- [[CUDA 学习清单]]
- [[3.4 CUDA Nsight Compute 指标速查|CUDA Nsight Compute 指标速查]]
- [[3.2 CUDA Runtime 进阶|CUDA Runtime 进阶]]
- [[Week 3 - Transpose + Memory Coalescing]]
