---
tags:
  - AI-infra
  - 素材库-GPU与推理方向
  - 项目分析
  - Week03
---
# Week03 渐进式练习

> 配合 [[CUDA Week 3 Transpose 项目解析]] 使用。答案基于仓库源码推理；凡涉及实测带宽/Nsight 数字之处，给出**预期方向**，真实数值请在你的 GPU 上 `make bench` / Nsight 后回填到 [[9.培养方案/90.素材库-GPU与推理方向/04.项目分析/Week03/profiling|profiling]]。

> [!note] 运行说明
> 本目录暂未附实测结果（仓库 `docs/profiling.md` 为空模板）。建议按你的架构构建，例如 GTX 1660 Super：
> ```bash
> make test CUDA_ARCH=75
> make bench CUDA_ARCH=75
> ```

---

## Day 1：CPU reference + copy baseline

### 练习 1.1：copy 为什么能当带宽上界

运行 `make test-copy`，然后回答：copy kernel 的读和写分别是什么访存模式？为什么它的带宽是后续转置版本的"天花板"？

**答案：**

copy 的读写都是 `[(y+j)*width + x]`，相邻 `threadIdx.x`（相邻 `x`）→ 地址连续 → 读写都 coalesced。它没有任何转置带来的跨步，也不碰 shared memory，所以代表"在当前 (32,8) block 配置下纯搬运能达到的带宽"。任何转置版本都至少要做和 copy 一样多的读写，故 copy 是上界。

### 练习 1.2：每个线程处理几个元素

block 是 `(32, 8) = 256` 线程，但 tile 是 `32×32 = 1024` 个元素。一个线程处理几个？代码哪一行体现？

**答案：**

`1024 / 256 = 4` 个。体现在 `for (int j = 0; j < TILE_DIM; j += BLOCK_ROWS)`：`TILE_DIM/BLOCK_ROWS = 32/8 = 4` 次循环，每次沿 y 方向跳 `BLOCK_ROWS=8` 行。用更少的线程覆盖整个 tile，可以控制每 block 资源、提升 occupancy。

### 练习 1.3：边界检查能不能去掉

把 `if (x < width && (y+j) < height)` 删掉，在 `1000×1000` 上会发生什么？

**答案：**

会越界读写。`1000` 不是 `32` 的整数倍，最后一列/行的 tile 只覆盖部分有效数据，剩余线程的 `x` 或 `y+j` 超出矩阵范围，访问非法地址 → 结果错误甚至非法内存访问。所以非 tile 对齐尺寸必须保留边界守卫。

---

## Day 2：naive transpose

### 练习 2.1：拆解读写模式

在 `out[x*height + (y+j)] = in[(y+j)*width + x]` 中，固定一个 warp（相邻 `threadIdx.x`），分别分析读地址和写地址的步长。

**答案：**

- 读 `in[(y+j)*width + x]`：相邻 `x` → 地址 +1 → 连续 → coalesced。
- 写 `out[x*height + (y+j)]`：相邻 `x` → 地址 +height → 间隔 = height → uncoalesced。

一个 warp 的 32 个 lane 写到 32 个相距 `height` 的地址，几乎每 lane 一个 transaction。

### 练习 2.2：为什么不能"把读做成跨步、写做成连续"

如果改成相邻线程负责输出的连续地址（让写 coalesced），读会变成什么？

**答案：**

读会变跨步。转置的本质是"输入的行 ↔ 输出的列"，在 global memory 直连的情况下，读和写不可能同时连续——必有一边跨步。这正是 naive 的死结，也是引入 shared memory 的动机：把"跨步"挪到片上去做。

### 练习 2.3：预测 naive vs copy 的差距方向

不看实测，先预测 `make bench` 里 naive 相对 copy 的带宽是高还是低，为什么？随矩阵增大差距如何变化？

**答案：**

naive 明显低于 copy，因为它一半访存（写）退化成 uncoalesced。矩阵越大、warp 越多，uncoalesced 写的额外 transaction 越占主导，差距通常更稳定地拉开（小矩阵则被 launch overhead 稀释）。实测请回填 profiling。

---

## Day 3：shared memory tiled transpose

### 练习 3.1：shared memory tile 里存的是什么

`tile[threadIdx.y + j][threadIdx.x]` 写入、`tile[threadIdx.x][threadIdx.y + j]` 读出，分别对应原矩阵的什么？

**答案：**

写入时 `tile[ty][tx]` 存的是输入 tile 的一块（行 ty、列 tx）。读出时按 `tile[tx][ty]` 取，即把行列对调——**转置发生在 shared memory 内部**。global 端读写都保持 `threadIdx.x` 走连续地址，所以都 coalesced。

### 练习 3.2：`__syncthreads()` 为什么不能省

如果删掉读阶段和写阶段之间的 `__syncthreads()`，可能出什么错？

**答案：**

写阶段线程要读的 `tile[tx][...]` 是**别的线程**在读阶段写入的。没有屏障，某些线程可能在 tile 还没被全部填好时就开始读，读到未初始化/过期数据 → 结果错误。`__syncthreads()` 保证全 block 读阶段完成后才进入写阶段。注意它只能同步 block 内，不能跨 block。

### 练习 3.3：blockIdx 交换的含义

写阶段为什么用 `blockIdx.y` 算 `x_out`、`blockIdx.x` 算 `y_out`？

**答案：**

转置要求输入 tile `(bx,by)` 落到输出 tile `(by,bx)`。block 级别的行列也要互换，所以 `x_out = blockIdx.y*32 + tx`、`y_out = blockIdx.x*32 + ty`。线程级转置靠 shared memory 内的 `tile[tx][ty]`，block 级转置靠 blockIdx 交换，两层缺一不可。

---

## Day 4：bank conflict 与 padding

### 练习 4.1：手算 tiled 的 bank

`tile[32][32]`，写阶段一个 warp 读 `tile[0][c], tile[1][c], …, tile[31][c]`。算每个的 bank，结论是什么？

**答案：**

地址 `= row*32 + c`，`bank = (row*32 + c) % 32 = c`，与 row 无关。32 个 lane 全部命中 bank `c` → **32-way bank conflict**，串行化成 32 次访问。

### 练习 4.2：手算 padded 的 bank

改成 `tile[32][33]` 后重算同一组访问的 bank。

**答案：**

地址 `= row*33 + c`，`bank = (row*33 + c) % 32 = (row + c) % 32`（因为 `33 % 32 = 1`）。`row` 从 0 到 31，`bank` 取遍 32 个不同值 → **无 bank conflict**。

### 练习 4.3：padding 的成本

`+1` padding 多花多少 shared memory？值得吗？

**答案：**

每行多 1 个 float，整块多 `32 floats = 128 字节`。相对每 SM 几十 KB 的 shared memory 预算几乎可忽略，却把写阶段 shared load 从 32 次串行降到 1 次并行，是典型的"极小空间换大吞吐"。

### 练习 4.4：还有别的消除冲突的办法吗

除了 `+1` padding，是否还有其他方式避免这个 32-way conflict？

**答案：**

有。常见替代是 **swizzle**（用异或等位运算重排列索引，如 `tile[tx][ty ^ tx]` 形式的映射），或调整 tile 的存储布局，使同列元素落在不同 bank。padding 最简单直观；swizzle 不额外占 shared memory，但索引更复杂。本项目用 padding 作为教学首选。

---

## Day 5：benchmark + profiling

### 练习 5.1：有效带宽公式

`bench_transpose.cu` 里 `bytes = 2.0 * width * height * sizeof(float)`，为什么是 2 倍？

**答案：**

转置把整个矩阵**读一遍 + 写一遍**，所以有效字节是 `2 × W × H × sizeof(float)`。带宽 `= bytes / 时间`，反映 kernel 利用显存带宽的效率。

### 练习 5.2：为什么每次都校验正确性

benchmark 在计时之外，每个版本还跑一次逐元素比对、输出 `Check` 列。为什么不能省？

**答案：**

性能数字只有在结果正确时才有意义。一个"很快但写错索引"的 kernel 毫无价值。仓库 CLAUDE.md 明确要求"benchmark 输出必须包含正确性状态、不许为了好看跳过校验"。这也能及时抓出边界/转置索引 bug。

### 练习 5.3：用 Nsight 验证 bank conflict

如何用 `make profile-tiled` / `make profile-padded` 证明 padding 真的消除了 bank conflict？

**答案：**

对比两者的 shared memory bank conflict 相关指标（如 shared load/store 的 conflict 次数或 `l1tex` 相关计数器）：tiled 应有大量 conflict，padded 应趋近 0。同时 global load/store efficiency 两者都应接近满（都已 coalesced），从而把"差异只来自 shared bank"这件事坐实。结果回填到 [[9.培养方案/90.素材库-GPU与推理方向/04.项目分析/Week03/profiling|profiling]]。

### 练习 5.4：测试形状的覆盖意图

benchmark 测了 `1024² / 4096² / 2048×3072 / 1000²`，各自想暴露什么？

**答案：**

- `1024²`：基础对比，标准方阵。
- `4096²`：大规模，带宽进入稳态、最能体现版本差异。
- `2048×3072`：非方阵，暴露 `width/height` 用反的 bug。
- `1000²`：非 32 对齐，检验边界守卫是否正确。
