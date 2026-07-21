---
tags:
  - AI-infra/素材库-GPU与推理方向/项目分析/Week03
---
# Week03 必答问题

> 配合 [[CUDA Week 3 Transpose 项目解析]]。这些是 README、阶段计划里的核心面试问题，逐题展开。

---

## 1. memory coalescing 为什么重要？

**答：** GPU 以 **warp（32 线程）** 为单位执行和访存。当一个 warp 的 32 个线程访问的 global 地址落在同一段连续区间（如一个 128B 段）时，硬件把它们合并成**极少数** memory transaction；地址分散时，最坏退化成 32 个 transaction，且每个 transaction 里大量字节被浪费。

global memory 的有效带宽几乎完全由"发起了多少 transaction、利用率多少"决定。对 memory-bound 的 kernel（transpose 就是），coalescing 几乎等价于性能本身。

---

## 2. naive transpose 的读和写分别是什么访存模式？

**答：** 看 `out[x*height + (y+j)] = in[(y+j)*width + x]`，固定一个 warp（相邻 `threadIdx.x`，即相邻 `x`）：

- **读** `in[(y+j)*width + x]`：相邻 `x` → 地址 +1 → 连续 → **coalesced**。
- **写** `out[x*height + (y+j)]`：相邻 `x` → 地址 +height → 间隔 = height → **uncoalesced**。

转置在 global memory 直连时，读写必有一边跨步，naive 把这条跨步落在了"写"上，导致写 transaction 数暴涨，带宽垫底。

---

## 3. shared memory transpose 为什么更快？

**答：** shared memory 充当**访存模式转换器**：

1. 读阶段：`tile[ty+j][tx] = in[...]`，`threadIdx.x` 走连续 global 地址 → coalesced 读入。
2. `__syncthreads()`。
3. 写阶段：`out[...] = tile[tx][ty+j]`，`threadIdx.x` 又走在输出的连续地址上 → coalesced 写回。

转置被搬进片上的 shared memory（写进去 `tile[ty][tx]`、读出来 `tile[tx][ty]`），于是 global 端读写**同时** coalesced，消除了 naive 的 uncoalesced 写。

---

## 4. shared memory tile 里存的是什么？为什么需要 `__syncthreads()`？

**答：** tile 暂存当前 block 负责的那块 `32×32` 子矩阵。读阶段每个线程把自己读到的 global 元素写进 `tile[ty][tx]`；写阶段每个线程按转置索引 `tile[tx][ty]` 取出再写回 global。

写阶段一个线程要读的格子，是**别的线程**在读阶段写进去的——这是一次通过 shared memory 的**线程间通讯**。`__syncthreads()` 是 block 级屏障，确保全 block 读阶段都完成、tile 被填满后，才允许任何线程进入写阶段，否则会读到未初始化数据。注意它只同步 block 内，不能跨 block。

---

## 5. bank conflict 是什么？tiled 版本为什么会有 32-way conflict？

**答：** shared memory 被分成 **32 个 bank**，每 4 字节落到下一个 bank（`bank = (地址/4) % 32`）。同一 warp 内多个线程访问**同一 bank 的不同地址**时，硬件无法并行，只能串行化，这就是 bank conflict。

tiled 写阶段读 `tile[threadIdx.x][c]`：一个 warp 里 `threadIdx.x = 0..31`、列 `c` 固定，访问同列不同行 `tile[0][c]…tile[31][c]`。在 `tile[32][32]` 中地址 `= row*32 + c`，`bank = (row*32+c) % 32 = c`，**与 row 无关** → 32 个 lane 全撞 bank `c` → **32-way conflict**，串行成 32 次。

---

## 6. padding 为什么能消除 bank conflict？

**答：** 把 tile 声明从 `[32][32]` 改成 `[32][33]`，行距从 32 变 33。此时地址 `= row*33 + c`：

```text
bank = (row*33 + c) % 32 = (row + c) % 32      (33 % 32 = 1)
```

`row` 从 0 到 31，`bank = (row+c)%32` 取遍 32 个不同值 → 32 个 lane 命中 32 个不同 bank → **无 conflict**。代价仅是每 block 多 `32 floats = 128B` shared memory，几乎免费。

---

## 7. transpose 是 memory-bound 还是 compute-bound？

**答：** **memory-bound**。每个元素只有一次读、一次写，没有任何算术运算。性能完全取决于访存效率（coalescing + bank conflict），而非计算吞吐。这也是为什么它是学习访存优化的最佳载体——把计算变量清零，性能差异只能归因于访存模式。

---

## 8. 为什么四个版本要共用同一套 block 配置和测试基建？

**答：** 为了做**受控实验**。`TILE_DIM=32 / BLOCK_ROWS=8 / block(32,8)`、grid 计算、`DeviceBuffer`、`generate_matrix`、`cpu_transpose`、`check_matrix` 全部一致，唯一变化的是 kernel 内部访存方式。这样 benchmark 出来的差距就**只能**归因于 coalescing / bank conflict，而不是被 block 大小、occupancy、输入差异等混淆变量污染。这是工程上"隔离单一变量"的标准做法。
