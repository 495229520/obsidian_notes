---
tags:
  - AI-infra
  - 素材库-GPU与推理方向
  - 项目分析
  - Week04
---
# Week04 必答问题

> 配合 [[CUDA Week 4 MatMul v0 项目解析]]。这些是阶段计划与 README 里的核心面试问题，逐题展开。数字为本卡（GTX 1660 SUPER）实测。

---

## 1. matmul 为什么比 vector add / transpose 更"值得"优化？

**答：** 看算术强度（arithmetic intensity = FLOPs / 访存字节）。matmul 搬 O(N²) 数据却做 O(N³) 计算，强度是 **O(N)**——N 越大强度越高。这意味着只要把访存压下来，计算就能成主导，kernel 可以从 memory-bound 一路走到 compute-bound、逼近 FP32 峰值。

vector add / transpose 每个元素只对应 O(1) 次运算，强度是极低的常数，无论怎么优化都跨不过 roofline 的 ridge point，永远 memory-bound。这也是 GEMM 成为深度学习与 LLM 推理核心算子的根本原因。

---

## 2. naive 为什么慢？根因是 occupancy 不够吗？

**答：** 不是。Nsight 显示 naive 的 achieved occupancy ≈ 99%，线程已驻留满。它慢的根因是**数据复用太差**：每线程算一个 `C[row][col]`，沿 K 维直接反复读 global memory——A 的每行被读 N 次、B 的每列被读 M 次。算术强度极低，warp 平均 ~29.9 cycles 卡在 global memory queue（67.8%），典型 memory-bound。

再堆线程没用（occupancy 已满）。唯一出路是**提高数据复用**，把高频数据搬进 shared memory。注意 naive 的访存模式本身不坏：读 B coalesced、读 A broadcast，问题纯在"重复读"。

---

## 3. shared memory tile 里存什么？复用倍数怎么来？

**答：** 一个 block 负责输出 C 的一个 `TILE×TILE`（16×16）子块。沿 K 维把 K 切成 `ceil(K/TILE)` 个 k-tile，逐块累加。每个 k-tile，block 内 256 个线程**先协作**把 A、B 的当前小方块搬进 `As/Bs` shared memory，再从 shared 反复读做点积。

每个从 global 读进来的元素被 `TILE` 个线程复用（`As[ty][kk]` 被同行 16 线程用、`Bs[kk][tx]` 被同列 16 线程用），所以 **global 访问量降为 naive 的 1/TILE = 1/16**。实测耗时 5.64→3.52 ms，但主要 stall 从"等 DRAM"转移到"等 shared/同步"——瓶颈被往后推了一层。

---

## 4. 为什么 tiled 需要两个 `__syncthreads()`，一个都不能少？

**答：** CUDA 线程不直接传消息，靠 **shared memory + 屏障**间接通讯：一个线程写进 shared 的数据要被别的线程读到，中间必须隔一道屏障。

- **第一个屏障**（载入后、计算前）：防止"还没载入完就开始算"——读到旧/未初始化的 shared 数据。
- **第二个屏障**（计算后、覆盖前）：防止"有人还在用当前 tile，就被别人覆盖进下一块"——写后读冲突。

漏掉任意一个都会得到**间歇性错误结果**，而且往往小 shape 测不出来、大 shape 才暴露（线程调度时序差异）。

---

## 5. register blocking 在做什么？计算/访存比怎么翻倍？

**答：** 让一个线程算 **4×4 = 16 个输出**（一个 micro-tile），16 个累加器 `acc[4][4]` 全放寄存器；block 仍 256 线程，但负责的 C 子块从 16×16 放大到 64×64。

内层每个 `kk`：从 shared 读 **4 个 A + 4 个 B（8 次 shared load）**做 **4×4 = 16 次 FMA**。`regA[i]`、`regB[j]` 进寄存器后在 4 次乘加里被复用，计算/访存比从 tiled 的 ~1:2 升到 **2:1**。本质是"用寄存器把内积变外积"：`regA`（4×1）⊗ `regB`（1×4）填满 4×4 的 acc——这是所有高性能 GEMM 的核心 pattern。实测耗时降到 1.68 ms、FP32 roofline 达 26%。

补充：`As` 在 shared 里**转置存放** `As[BK][BM]`，让内层按固定 `kk` 取 A 的一列时元素连续，避免 bank conflict 并便于向量化。

---

## 6. register pressure 为什么可能反而让 kernel 变慢？

**答：** 累加器 + 临时寄存器占用很多寄存器，register 版 reg/thread 升到 64。每线程寄存器越多 → 一个 SM 能同时驻留的 warp 越少 → occupancy 下降（本卡从 ~99% 掉到 92.4%）→ 用并发掩盖访存/指令延迟的能力变弱。

所以**复用收益和 occupancy 损失之间存在平衡点**。4×4 在本卡上收益仍大于损失（整体更快）；再加大到 8×8 micro-tile 可能因 occupancy 跌太多而得不偿失，需实测确定。这是 GEMM 调优的核心权衡之一。

---

## 7. cuBLAS 是 column-major，项目是 row-major，怎么零转置对接？

**答：** 用一个恒等式：**row-major 的 M×N 矩阵在内存里和 column-major 的 N×M 逐字节一致**，即「row-major C」==「column-major Cᵀ」。又因为 `Cᵀ = (A·B)ᵀ = Bᵀ·Aᵀ`：

```text
把 row-major B(K×N) 直接当 column-major 读 → 就是 Bᵀ(N×K)，lda=N
把 row-major A(M×K) 直接当 column-major 读 → 就是 Aᵀ(K×M)，ldb=K
column-major 下算 (N×K)·(K×M) = N×M = Cᵀ
写回 dC 按 row-major 读出，正好是 M×N 的 C ✓
```

落到 `cublasSgemm`：两个 `CUBLAS_OP_N`、传 `N,M,K`、A 参数填 `dB,N`、B 参数填 `dA,K`、C 填 `dC,N`。零拷贝零转置。不理清这点会让 cuBLAS 结果和自研 kernel 对不上，误以为自己写错了。

---

## 8. 为什么打不过 cuBLAS 不算失败？它当 baseline 的价值是什么？

**答：** cuBLAS 在本卡跑 2.92 TFLOPS（峰值 58%，kernel 是 `volta_sgemm_32x128_nn`），用了多层 tiling（block/warp/thread）、double buffering、向量化访存、精细指令调度和逐架构调优。本项目只做单层 shared tiling + 一层 register tiling，**差距是工程深度，不是方法错误**。

把 cuBLAS 当天花板的价值是**量化"还差多少、差在哪"**：例如每次 issue 间隔 register 版 17.8 cyc vs cuBLAS 8.7 cyc，指令调度成熟度的差距一目了然。这指明了下一步方向（warp-tiling、double buffering、`float4`、修 coalescing），而非盲目堆代码。

---

## 9. matmul 的正确性验证为什么用相对误差而非严格相等？

**答：** matmul 在 K 维做 K 次浮点累加，FP32 舍入误差随 K 累积；且 GPU 与 CPU 累加**顺序不同**，结果本就不会逐位相等。K=1024 时绝对误差很容易超过 transpose 用的 `1e-5`，严格阈值会 **false-fail**。

所以改用**相对误差** `|gpu-ref|/(|ref|+eps) < rel_tol`（项目取 `2e-2`），CPU 参考用 `double` 累加抬高基准精度，随机矩阵取值压在 `[-1,1]`（固定种子 42）降低大 K 数值膨胀。这是 GEMM/reduction 这类含浮点归约算子做正确性验证的通用方法。

---

## 10. 怎么用 roofline 判断一个 kernel 的瓶颈、以及优化方向？

**答：** 先算 ridge point = 峰值算力 / 峰值带宽，本卡 ≈ `5.03 TFLOPS / 336 GB/s ≈ 15 FLOP/Byte`。再估 kernel 的有效算术强度（FLOP / 实际访存字节）：

- 低于 ridge point → **memory-bound**，优化方向是减少访存 / 提高复用（如 shared tiling）。
- 高于 ridge point → **compute-bound**，优化方向是提高指令级并行、向量化、用专用硬件（Tensor Core）。

本周四版本正是沿这条线走：naive 远在 ridge 左侧（memory-bound）→ tiling + register blocking 把有效算术强度逐步推过 ridge → cuBLAS 基本工作在 compute 屋顶下。**一句话：提升数据复用 → 抬高算术强度 → 把瓶颈从带宽换到算力。**
