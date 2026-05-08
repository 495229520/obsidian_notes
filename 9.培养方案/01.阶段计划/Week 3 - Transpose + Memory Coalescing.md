---
title: Week 3 - Transpose + Memory Coalescing
date: 2026-05-06
tags:
  - 培养方案
  - CUDA
  - GPU
  - MemoryCoalescing
  - Transpose
  - infra
aliases:
  - CUDA Week 3
  - CUDA Transpose
status: active
---

# Week 3 - Transpose + Memory Coalescing

> 第三周目标：用矩阵转置理解 memory coalescing、shared memory tile、bank conflict 和 padding。这个主题专门训练“为什么访存模式决定 CUDA 性能”。

---

## 学习目标

1. 能实现 naive transpose。
2. 能解释 coalesced read / uncoalesced write。
3. 能实现 shared memory tiled transpose。
4. 能复现 shared memory bank conflict。
5. 能用 padding 减少 bank conflict。
6. 能写 `profiling.md` 解释 naive 和 tiled 的性能差距。

---

## 1. 为什么学 transpose

矩阵转置看起来只是：

```cpp
out[col * height + row] = in[row * width + col];
```

但它会暴露 CUDA 访存优化的关键矛盾：

- 读连续，写可能跨步。
- 写连续，读可能跨步。
- shared memory 可以把全局内存的跨步访问重排成更规整的访问。
- shared memory 本身也有 bank conflict。

所以 transpose 是学习 memory coalescing 的经典实验。

---

## 2. 必做实现

| 版本 | 目标 | 重点观察 |
|---|---|---|
| CPU reference | 正确性基准 | 行列映射是否正确 |
| copy kernel | 只复制不转置 | 理想访存 baseline |
| naive transpose | 直接读写 global memory | 写入是否 coalesced |
| tiled transpose | shared memory tile 重排 | global memory 读写都尽量连续 |
| padded tiled transpose | tile 加 padding | bank conflict 变化 |

---

## 3. 实现顺序

### Day 1：CPU reference + copy baseline

- 写 CPU transpose。
- 写 CUDA copy kernel。
- 用 copy kernel 测一个接近理想的 global memory bandwidth。
- 所有后续版本都和 copy baseline 对比。

验收：

- square matrix 正确。
- non-square matrix 正确。
- width / height 不是 tile size 整数倍时正确。

### Day 2：naive transpose

- 每个线程处理一个元素。
- global thread index 映射到 `(row, col)`。
- 直接写 `out[col * height + row]`。

必须记录：

- 读是否连续？
- 写是否连续？
- 和 copy baseline 的带宽差距有多大？

### Day 3：shared memory tiled transpose

- 每个 block 处理一个 tile。
- 先从 global memory 连续读入 shared memory。
- block 内同步。
- 再从 shared memory 读出，连续写回 global memory。

必须回答：

- shared memory tile 存的是什么？
- 为什么它能改善 global memory 写入模式？
- 为什么需要 `__syncthreads()`？

### Day 4：bank conflict 与 padding

- 复现不加 padding 的 shared memory tile。
- 将 tile 从 `[TILE][TILE]` 改为 `[TILE][TILE + 1]`。
- 比较 Nsight 中 shared memory 相关指标。

必须回答：

- bank conflict 是什么？
- 为什么二维 tile 转置容易产生 bank conflict？
- 为什么 `+1` padding 有用？

### Day 5：benchmark + profiling

建议 shape：

| Matrix | 目的 |
|---|---|
| 1024 x 1024 | 基础对比 |
| 4096 x 4096 | 大规模带宽 |
| 2048 x 3072 | 非方阵 |
| 1000 x 1000 | 非 tile 对齐 |

Nsight 关注：

- global memory load/store throughput。
- shared memory bank conflict。
- achieved occupancy。
- warp stall reason。

---

## 4. profiling.md 模板

```text
Experiment:
GPU:
CUDA:
Matrix shape:
Tile size:

copy baseline bandwidth:
naive transpose bandwidth:
tiled transpose bandwidth:
padded tiled transpose bandwidth:

Observation:
Bottleneck:
Evidence:
Conclusion:
```

---

## 5. 常见坑

> [!warning] 只测方阵
> 方阵太容易掩盖行列映射错误，必须测试非方阵。

> [!warning] 忘记边界检查
> tile 覆盖矩阵边缘时，线程可能越界读写。

> [!warning] 误以为 shared memory 总是更快
> shared memory 是工具，不是保证。错误的 tile 设计可能引入 bank conflict 和额外同步。

---

## 6. 本周交付物

| 交付物 | 内容 | 验收 |
|---|---|---|
| transpose kernels | copy、naive、tiled、padded tiled | correctness pass |
| benchmark | 多 shape、多版本 | 输出带宽对比 |
| profiling.md | memory coalescing 与 bank conflict 分析 | 结论能被指标支持 |
| README | 解释 shared memory transpose 为什么快 | 能回答面试问题 |

---

## 面试问题

- memory coalescing 为什么重要？
- naive transpose 的读和写分别是什么访存模式？
- shared memory transpose 为什么更快？
- shared memory tile 里存什么？
- bank conflict 是什么？
- padding 为什么能减少 bank conflict？
- transpose 是 memory-bound 还是 compute-bound？

---

## 关联知识

- [[CUDA 学习清单]]
- [[Week 2 - Reduction + Profiling]]
- [[CUDA Nsight Compute 指标速查]]
- [[Week 4 - MatMul v0]]
