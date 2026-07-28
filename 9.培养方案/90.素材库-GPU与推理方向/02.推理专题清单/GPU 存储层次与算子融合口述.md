---
title: GPU 存储层次与算子融合口述
date: 2026-05-24
tags:
  - AI-infra
  - 素材库-GPU与推理方向
  - 推理专题清单
roadmap_week: Week 3-4, Week 9-16
sort_order: "03.50"
status: active
---

# GPU 存储层次与算子融合口述

> [!info] 所属路线
> - 总纲 Week：Week 3-4，Week 9-16
> - 排序：03.50
> - 用途：支撑 transpose、matmul、FlashAttention、fusion 和高性能 kernel 口述。

> [!goal] 目标
> 把 HBM、shared memory / SRAM、register、cache、DMA / async copy 和 kernel fusion 组织成面试能口述的工程模型，用来支撑 [[LLM Kernel 专题清单]]、[[FlashAttention 版本演进与面试口述]] 和 serving 性能分析。

---

## 1. 先建立层次模型

| 层次 | 直觉 | 面试关注点 |
|---|---|---|
| HBM / global memory | 容量大、延迟高、带宽宝贵 | 减少往返、coalescing、memory-bound |
| L2 cache | 跨 SM 缓存 | 数据复用、cache hit、带宽压力 |
| shared memory / SRAM | block 内显式管理的片上存储 | tiling、transpose、bank conflict |
| register | 每个 thread 私有，最快 | register pressure、occupancy |
| Tensor Core / compute units | 矩阵计算单元 | GEMM、MMA、WGMMA、算力利用 |

口述模板：

```text
GPU 优化经常是在 HBM 和片上存储之间做数据复用。HBM 容量大但贵，shared memory 和 register 快但小。好的 kernel 会尽量把会重复使用的数据搬到片上，在多个计算步骤中复用，减少 global memory 往返。
```

---

## 2. HBM 为什么是高频瓶颈

如果一个 kernel 每个元素只做很少计算，却要读写大量 global memory，它通常是 memory-bound。

典型例子：

- vector add：读 A、读 B、写 C，计算只有一次加法。
- RMSNorm：需要读一整行、做 reduction、再写回。
- softmax：max、sum、normalize 多次访问一行。
- dequant 单独 kernel：读 int8、读 scale、写 fp16，中间结果还会被 matmul 再读。

面试口述：

```text
判断 memory-bound 的直觉是 arithmetic intensity 低，也就是每搬运一个 byte 做的计算少。vector add 就是典型例子；GEMM 则因为每块数据能被重复用于很多乘加，更可能 compute-bound。
```

---

## 3. shared memory / SRAM 的作用

shared memory 常见用途：

- matmul tiling：复用 A/B tile。
- transpose：改善 global memory coalescing。
- reduction：保存 block 内中间结果。
- attention：缓存 Q/K/V block，减少 HBM 访问。

注意点：

- shared memory 不是越多越好，会影响 occupancy。
- 访问模式不当会产生 bank conflict。
- padding 有时能减少 transpose 的 bank conflict。

---

## 4. register pressure 和 occupancy

register 很快，但每个 SM 的 register 总量有限。单个 thread 用太多 register，会减少同时驻留的 warp / block 数。

口述模板：

```text
fusion 或复杂 kernel 可能减少 HBM 读写，但也可能让每个 thread 保存更多中间变量，增加 register pressure，导致 occupancy 下降。是否值得要看 Nsight 指标和端到端 benchmark，而不是只凭直觉。
```

---

## 5. DMA / async copy / pipeline 的直觉

在高性能 kernel 中，希望数据搬运和计算重叠：

```text
load tile k+1
while compute tile k
store / prefetch next
```

在新硬件上，异步数据搬运和矩阵指令可以配合 pipeline：

- producer 负责搬运。
- consumer 负责计算。
- 多 stage buffer 隐藏 memory latency。

面试不用一开始讲指令细节，先讲清楚核心思想：

```text
异步流水的目标是避免计算单元等数据，让数据搬运和矩阵计算重叠。
```

---

## 6. 算子融合为什么可能更快

两个 kernel 如果中间结果写回 HBM，再被下一个 kernel 读出：

```text
kernel A: read x -> compute tmp -> write tmp to HBM
kernel B: read tmp -> compute y -> write y
```

fusion 后：

```text
fused kernel: read x -> compute tmp in register/shared memory -> compute y -> write y
```

收益：

- 少一次 kernel launch。
- 少一次中间结果 HBM write。
- 少一次中间结果 HBM read。
- 可能提高 cache / register 复用。

典型例子：

- bias + GELU。
- residual + RMSNorm。
- dequant + matmul。
- attention 中 mask + softmax + dropout / scale。

---

## 7. fusion 为什么也可能变慢

| 风险 | 原因 |
|---|---|
| register pressure 增加 | 中间变量更多 |
| occupancy 下降 | 每个 block / thread 资源占用变大 |
| shared memory 占用增加 | tile 或 staging buffer 更多 |
| 分支复杂 | mask、边界、dtype 路径混在一起 |
| correctness 风险 | 数值顺序变化、dtype 转换变化 |
| 调试困难 | fused kernel 更难定位错误 |

口述模板：

```text
fusion 的收益是减少 global memory 往返和 launch overhead，但代价是资源占用和复杂度。小算子链通常适合 fusion；如果 fusion 后 register pressure 太高或 occupancy 掉太多，未必更快。
```

---

## 8. 和 FlashAttention 的联系

FlashAttention 可以理解成更系统的 IO-aware fusion / tiling：

```text
QK -> mask -> softmax -> PV
```

这些步骤不把完整 score 和 probability 矩阵写回 HBM，而是在 block 内用 Online Softmax 累加输出。

所以它的核心表达是：

- 减少 HBM traffic。
- 提高片上复用。
- 用 Online Softmax 保持数值正确。
- 后续版本继续优化 parallelism 和 pipeline。

---

## 9. 3 分钟口述验收

1. HBM、shared memory、register 的区别是什么？
2. 为什么 vector add 通常 memory-bound？
3. 为什么 matmul tiling 能提升性能？
4. bank conflict 是什么，transpose 为什么容易遇到？
5. fusion 为什么减少 HBM 读写？
6. fusion 为什么可能导致 register pressure 和 occupancy 问题？
7. FlashAttention 如何体现 IO-aware 思想？
