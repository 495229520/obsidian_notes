---
tags:
  - AI-infra
  - 素材库-GPU与推理方向
  - 项目分析
  - Week04
---
# Week04 渐进式练习

> 配合 [[CUDA Week 4 MatMul v0 项目解析]] 使用。答案基于仓库源码与本卡（GTX 1660 SUPER, sm_75）实测推理。涉及具体 TFLOPS/Nsight 数字处给出**本卡实测值或预期方向**，换卡请在你的 GPU 上 `make bench` / `make profile-*` 后回填到 [[9.培养方案/90.素材库-GPU与推理方向/04.项目分析/Week04/profiling|profiling]]。

> [!note] 运行说明
> ```bash
> make            CUDA_ARCH=75
> make test       CUDA_ARCH=75      # 四版本对拍
> make bench      CUDA_ARCH=75      # 多 shape TFLOPS 表
> make profile-tiled CUDA_ARCH=75  # 单 kernel Nsight
> ```

---

## Day 1：问题建模 + 基础设施 + roofline

### 练习 1.1：FLOPs 为什么是 2·M·N·K

benchmark 用 `FLOPs = 2·M·N·K` 算 TFLOPS。这个 2 从哪来？

**答案：**

`C` 有 `M×N` 个输出元素，每个元素是长度 K 的内积：K 次乘 + K 次加 ≈ `2K` 次浮点运算。所以总量 `= M·N·2K = 2·M·N·K`。TFLOPS `= FLOPs / 时间 / 1e12`，衡量算力利用率。

### 练习 1.2：算术强度与 ridge point

本卡峰值 ≈ 5.03 TFLOPS、带宽 ≈ 336 GB/s。算 ridge point，并解释它怎么判瓶颈。

**答案：**

`ridge point = 峰值算力 / 峰值带宽 ≈ 5.03e12 / 336e9 ≈ 15 FLOP/Byte`。一个 kernel 的有效算术强度（FLOP/访存字节）若**低于** 15 → 受带宽限制（memory-bound），**高于** 15 → 受算力限制（compute-bound）。本周所有优化都是抬高有效算术强度，把工作点从 ridge point 左边推到右边。

### 练习 1.3：matmul 为什么能 compute-bound，vector add 不能

同样是 GPU kernel，为什么 matmul 可以做到 compute-bound，而 vector add / transpose 永远 memory-bound？

**答案：**

看算术强度的量级。matmul 搬 O(N²) 数据却做 O(N³) 计算，强度是 **O(N)**——N 越大强度越高，只要把访存压下来计算就能成主导。vector add / transpose 每个元素只对应 O(1) 次运算，强度是常数且极低，无论怎么优化都跨不过 ridge point。这也是 GEMM 成为深度学习核心算子的根本原因。

### 练习 1.4：matmul 的正确性为什么不能用严格相等

transpose 用 `1e-5` 严格逐元素比，matmul 却用相对误差 `2e-2`。为什么不能照搬？

**答案：**

matmul 在 K 维做 K 次浮点累加，FP32 舍入误差随 K 累积；且 GPU 与 CPU 累加**顺序不同**，结果本就不会逐位相等。K=1024 时绝对误差很容易超过 `1e-5`，严格阈值会 **false-fail**。所以改用相对误差 `|gpu-ref|/(|ref|+eps) < rel_tol`，CPU 参考还用 `double` 累加抬高基准精度。含浮点归约的算子（GEMM/reduction）都该这么验。

---

## Day 2：v0 naive

### 练习 2.1：grid/block 与 (row,col) 的对应

naive 里 `row = blockIdx.y*blockDim.y + threadIdx.y`、`col = blockIdx.x*...x`。为什么 `grid.x` 对应 N、`grid.y` 对应 M？写反会怎样？

**答案：**

`C` 是 `M`（行）×`N`（列），习惯上 `x` 维映射列、`y` 维映射行，所以 `grid.x` 覆盖 N 列、`grid.y` 覆盖 M 行。写反（grid 维度和 M/N 对不上）轻则越界、重则结果全错，且方阵下不易发现——这是 CUDA 最容易写反的地方，必须用非方阵测试暴露。

### 练习 2.2：warp 视角拆 naive 的访存

固定一个 warp（`threadIdx.x` 连续 → `col` 连续），分析 `A[row*K+k]` 和 `B[k*N+col]` 的访存模式。

**答案：**

- 读 `B[k*N + col]`：连续 `col` → 连续地址 → **coalesced** ✓
- 读 `A[row*K + k]`：同 warp 内 `row`、`k` 都相同 → 32 线程读**同一地址** → broadcast ✓

访存模式本身不坏。naive 的问题不在 coalescing，而在**没有复用**：每个 k 都重新从 global 取。

### 练习 2.3：naive 慢的根因是 occupancy 吗

Nsight 显示 naive 的 achieved occupancy ≈ 99%。那它慢是因为线程不够多吗？该怎么救？

**答案：**

不是。occupancy 已接近满，再堆线程无用。根因是**数据复用太差**：A 的每行被读 N 次、B 的每列被读 M 次，warp 平均 ~29.9 cycles 卡在 global memory queue。唯一出路是**提高数据复用**——把高频数据搬到 shared memory，这正是 tiled 版本要做的。

---

## Day 3：v1 tiled（shared memory）

### 练习 3.1：复用倍数怎么来

tiled 把 global 访问量降为 naive 的 `1/TILE`（TILE=16）。这个倍数怎么推？

**答案：**

每个从 global 读进 shared 的元素，会被 block 内 `TILE` 个线程复用：`As[ty][kk]` 被同行 16 个线程用、`Bs[kk][tx]` 被同列 16 个线程用。一次 global 读支撑 16 次计算访问，所以 global 访问量降为 `1/TILE = 1/16`。

### 练习 3.2：两个 `__syncthreads()` 各防什么

tiled 内层循环有两处 `__syncthreads()`（载入后、计算后）。分别去掉会出什么错？

**答案：**

- 去掉**第一个**（载入后）：有线程在 tile 还没填满时就开始算 → 读到旧/未初始化数据。
- 去掉**第二个**（计算后）：有线程还在用当前 tile 计算，别的线程已进入下一轮覆盖 shared → 写后读冲突。

两者都导致**间歇性错误**，且常常小 shape 测不出、大 shape 才暴露。一个都不能少。

### 练习 3.3：非对齐 shape 的边界补 0

M/N/K 不是 TILE 整数倍时，越界的 shared 槽位补 0。为什么补 0 不影响结果？

**答案：**

补 0 的项进入点积是 `0 × something = 0`，对累加和无贡献，同时避免读越界 global。测试用 `130×70×90`、`17×33×65` 这类非对齐 shape 专门验证这条路径。

### 练习 3.4：tiled 的新瓶颈在哪

实测 tiled 比 naive 快（5.64→3.52 ms），但主要 stall 变了。变成什么？说明什么？

**答案：**

主要 stall 从 naive 的"LG memory queue（等 DRAM）"转移到 **MIO/shared memory queue（~15.4 cyc, 45.9%）**。说明 shared tiling 把瓶颈往后推了一层：从"等 DRAM"变成"等 shared memory / 同步"。shared 带宽和 `__syncthreads` 开销成了新天花板——这正是 register blocking 要解决的。

---

## Day 4：v2 register blocking

### 练习 4.1：一个线程算多少输出，多少累加器

register 版的 `BM/BN/BK/TM/TN` 各是什么？一个线程算几个输出、用几个累加器？

**答案：**

`BM=BN=64, BK=8`（block 算 64×64、沿 K 每次 8），`TM=TN=4`（一个线程算 4×4=16 个输出）。每线程 16 个累加器 `acc[4][4]` 全放寄存器。block 仍 256 线程（`(BM/TM)*(BN/TN)=16×16`），但负责的 C 子块从 16×16 放大到 64×64。

### 练习 4.2：计算/访存比怎么翻倍

内层每个 `kk` 做了几次 shared load、几次 FMA？算计算/访存比，对比 tiled。

**答案：**

每个 `kk`：读 4 个 A + 4 个 B = **8 次 shared load**，做 4×4 = **16 次 FMA**。计算/访存比 = 16/8 = **2:1**，相比 tiled 的 ~1:2 提升约 4 倍。关键在 `regA[i]`、`regB[j]` 进寄存器后，在 4 次 FMA 里被复用，shared 带宽不再是瓶颈。这就是"用寄存器把内积变外积"。

### 练习 4.3：`As` 为什么转置存

register 版 `__shared__ float As[BK][BM]`（行=k、列=m，转置）。为什么不存成 `As[BM][BK]`？

**答案：**

内层按固定 `kk` 取 `As[kk][...]` 这一整列 A。转置成 `[BK][BM]` 后，同一 `kk` 的 A 元素在 shared 里**连续**，避免 bank conflict 并便于向量化读取。若按 `[BM][BK]` 存，取一列就变成跨 stride 访问，触发 bank conflict。

### 练习 4.4：register pressure 为什么可能反而变慢

register 版 reg/thread 升到 64、occupancy 从 ~99% 掉到 92.4%。为什么寄存器多反而可能拖慢？4×4 是最优吗？

**答案：**

每线程寄存器越多 → 一个 SM 能同时驻留的 warp 越少 → occupancy 下降 → 用并发掩盖访存/指令延迟的能力变弱。所以**复用收益和 occupancy 损失存在平衡点**。4×4 在本卡是收益仍 > 损失的点（耗时降到 1.68 ms、roofline 26%）；再加大到 8×8 可能因 occupancy 跌太多而得不偿失。需实测确定。

### 练习 4.5：载入索引与计算索引为何是两套

register 版载入用线性 `tid`、计算用 `(ty,tx)` micro-tile 索引。为什么不能共用一套？

**答案：**

两个阶段目标不同。载入只关心"把 `BK×BM=512` 个元素**均匀**摊给 256 线程"（每人搬 2 个），用线性 `tid` 最简单。计算关心"我负责哪个 4×4 输出块"，要用 `(ty,tx)` 定位 micro-tile。强行共用会让某一阶段的索引变扭曲，反而易错。分成两套独立映射更清晰。

---

## Day 5：v3 cuBLAS + benchmark + profiling

### 练习 5.1：row-major 调 cuBLAS 的恒等式

cuBLAS 是 column-major，项目是 row-major。项目用什么恒等式零转置地解决？

**答案：**

核心恒等式：**row-major 的 M×N 矩阵在内存里和 column-major 的 N×M 逐字节一致**，即「row-major C」==「column-major Cᵀ」。又 `Cᵀ = (A·B)ᵀ = Bᵀ·Aᵀ`。于是把 row-major 的 B、A 直接当 column-major 读就成了 Bᵀ、Aᵀ，调 `cublasSgemm` 时 m/n 互换、A/B 参数互换（填 dB,N / dA,K），算出的 column-major 结果按 row-major 读正好是 C。零拷贝零转置。

### 练习 5.2：为什么 handle 用 static 单例

`get_handle()` 用函数内 `static cublasHandle_t`。为什么不每次调用都 create/destroy？

**答案：**

创建/销毁 cuBLAS handle 有固定开销（分配内部资源）。若每次 SGEMM 都重建，这部分开销会混进计时、污染 benchmark。用函数内 static 单例只创建一次、全程复用，把 handle 开销排除在计时之外。

### 练习 5.3：benchmark 的 ground truth 为什么用 cuBLAS 而非 CPU

正确性参考改用 cuBLAS 在 device 上算，而不是 CPU 三重循环。为什么？

**答案：**

大 shape（如 4096³）CPU 三重循环要 ~1.3e11 次乘加，太慢。先用 cuBLAS 在 device 上算一份 ground truth，再让每个 kernel 与它按相对误差 `2e-2` 比，既快又准。前提是 cuBLAS 本身已被验证可信。

### 练习 5.4：小 shape 的 TFLOPS 为什么没意义

128³ 下 register 版甚至最慢。为什么小 shape 不能用来评估吞吐？

**答案：**

小 shape 下 kernel launch + 调度开销远大于实际计算，掩盖了 kernel 真实吞吐。register 版的 64×64 block tile 对 128×128 只有 4 个 block，几乎没并行度，反而最慢。必须用足够大的 shape（≥1024³）让各版本进入稳态才有可比性。

### 练习 5.5：读 Nsight 表判断瓶颈

给定 1024³ 实测：naive（occ 99%、卡 global queue）、tiled（stall 转 shared）、register（occ 92.4%、roofline 26%）。用"复用→算术强度→瓶颈层级"解释这条优化链。

**答案：**

- naive：occ 拉满却卡 global memory queue → 低复用 memory-bound，增线程无用，必须提复用。
- tiled：shared tiling 把每元素复用 16 次，stall 从"等 DRAM"转到"等 shared/同步"，瓶颈后移一层。
- register：寄存器复用把计算/访存比抬到 2:1，有效算术强度过 ridge point，roofline 升到 26%；代价是 reg pressure 拉低 occupancy，但复用收益盖过损失。
- 主线：**提复用 → 抬算术强度 → 把瓶颈从带宽换到算力**。结果回填 [[9.培养方案/90.素材库-GPU与推理方向/04.项目分析/Week04/profiling|profiling]]。

### 练习 5.6：为什么打不过 cuBLAS 不算失败

register 版 1.38 TFLOPS、cuBLAS 2.92 TFLOPS。差距说明什么？

**答案：**

cuBLAS 用了多层 tiling（block/warp/thread）、double buffering、向量化访存、精细指令调度和逐架构调优；本项目只有单层 shared tiling + 一层 register tiling。差距是**工程深度**而非方法错误。cuBLAS 作为天花板的价值是量化"我们还差多少、差在哪"（如每次 issue 间隔 18.3 vs toy 的 38.2 cycles）。
