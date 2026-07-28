---
tags:
  - AI-infra
  - 素材库-GPU与推理方向
  - 项目分析
  - Week04
---
# Week04 Benchmark & Profiling

> 配合 [[CUDA Week 4 MatMul v0 项目解析]]。与前几周不同，本周仓库 `docs/benchmark.md` / `docs/profiling.md` 已有**本卡（GTX 1660 SUPER, Turing, sm_75）实测数据**，下文如实记录并给出分析方法论。换卡请在你的 GPU 上重测后回填到「其它 GPU」一节，不要直接套用本卡数字。

---

## 1. 运行方式

```bash
make bench CUDA_ARCH=75                       # 多 shape TFLOPS 表（含正确性 Check）
make profile-naive    CUDA_ARCH=75
make profile-tiled    CUDA_ARCH=75
make profile-register CUDA_ARCH=75
make profile-cublas   CUDA_ARCH=75
```

> [!note] profile 钩子隔离单个 kernel
> `--profile --kernel X` 模式下，warmup 后用 `cudaProfilerStart/Stop` 只圈住目标 kernel 的一次 launch，避免 Nsight 把其它 shape/kernel 混进报告。

> [!note] 常见权限/PATH 坑
> - 普通用户跑 Nsight 可能报 `ERR_NVGPUCTRPERM`（计数器权限）→ 用 `sudo`。
> - `sudo` 后 root PATH 找不到 `ncu` → 传绝对路径 `/usr/local/cuda/bin/ncu`。
> - CUDA 13 移除了 `cudaDeviceProp::clockRate`，峰值算力改用 `cudaDeviceGetAttribute(cudaDevAttrClockRate)` 运行时查询。

---

## 2. 本卡参数与 roofline

```text
GPU:        GTX 1660 SUPER (Turing, sm_75)
峰值 FP32:  22 SM × 64 core × 2 × 1.785 GHz ≈ 5.03 TFLOPS
显存带宽:   ≈ 336 GB/s
ridge point = 5.03 TFLOPS / 336 GB/s ≈ 15 FLOP/Byte
```

算术强度 < 15 → memory-bound；> 15 → compute-bound。**本周所有优化本质都是抬高有效算术强度，把工作点从 ridge point 左边推到右边。**

---

## 3. 该盯哪些指标

| 指标 | 看什么 | 预期走向（naive→cuBLAS） |
|---|---|---|
| TFLOPS | 结果吞吐 | 单调上升 |
| SM (Compute) Throughput % | 算力 pipe 利用率 | 见下注，非纯 FP32 口径 |
| Achieved Occupancy % | warp 驻留率 | naive/tiled 高，register 因 reg pressure 降 |
| DRAM Throughput % | 显存带宽利用率 | naive 低（浪费）→ tiling 后变化 |
| Reg/thread | 寄存器压力 | register 版升到 64 |
| Shmem/block | shared 用量 | 0 → 2KB → 4KB → 16.4KB |
| 主要 stall reason | 瓶颈定位 | LG memory queue → MIO/shared |

> [!note] SM Throughput% ≠ FP32 利用率
> Nsight 的 Compute (SM) Throughput 把所有 pipe 算进去，不是纯 FP32 占峰值比。对应 FP32 roofline 约为 naive 8% / tiled 12% / register 26% / cuBLAS 55%——这才是和 TFLOPS 表对得上的口径。

---

## 4. 实测结果（GTX 1660 SUPER）

### 4.1 Benchmark TFLOPS（1024³）

| Kernel | Avg(ms) | TFLOPS | 相对 cuBLAS | 相对峰值 |
|---|---:|---:|---:|---:|
| naive    | 6.6449 | 0.32 | 11% | 6% |
| tiled    | 3.6260 | 0.59 | 20% | 12% |
| register | 1.5572 | 1.38 | 47% | 27% |
| cuBLAS   | 0.7345 | 2.92 | 100% | 58% |

> 排序在所有有意义 shape 上稳定：**cuBLAS > register > tiled > naive**。提升来源：naive→tiled（shared 把 global 访存降到 1/TILE，≈+84%）、tiled→register（计算/访存比翻倍，≈×2.3）、register→cuBLAS（剩 ~2× 来自库级工程优化）。

> [!warning] 小 shape 无参考意义
> 128³ 下 launch/调度开销远大于计算，register 版甚至最慢（64×64 tile 对 128×128 只 4 个 block，几无并行度）。评估吞吐必须用 ≥1024³。

### 4.2 Nsight Compute（1024³）

| Kernel | Duration | SM tput% | Achieved occ% | DRAM tput% | Reg/thread | Shmem/block | 主要 stall |
|---|---:|---:|---:|---:|---:|---:|---|
| naive    | 5.64 ms | 61.5 | 99.1 | 15.3 | 52 | 0 B      | LG memory queue 29.9cyc / 67.8% |
| tiled    | 3.52 ms | 73.9 | 99.3 | 24.4 | 39 | 2.05 KB  | MIO/shared 15.4cyc / 45.9% |
| register | 1.68 ms | 26.6 | 92.4 | 9.8  | 64 | 4.10 KB  | MIO/shared 17.8cyc / 46.7% |
| cuBLAS   | 0.78 ms | 55.3 | 91.0 | 17.6 | 57 | 16.38 KB | MIO/shared 8.7cyc / 47.4% |

> benchmark 与 Nsight 的 ms 略有差异属正常（计时口径、profiler 开销不同），关注**相对趋势**而非绝对值对齐。

---

## 5. 分析

### Observation
- naive：occupancy 拉满（99%），却卡在 global memory queue（29.9 cyc / 67.8%）。
- tiled：主要 stall 从 DRAM 等待转为 shared/同步，SM tput 61.5%→73.9%、DRAM 15.3%→24.4%。
- register：occupancy 因 reg/thread=64 掉到 92.4%，但 FP32 roofline 升到 26%、整体最快（toy 中）。
- cuBLAS：每次 issue 间隔仅 8.7 cyc（toy register 17.8），指令调度成熟度差距明显。

### Bottleneck
- naive → 低数据复用导致的 memory-bound（带宽浪费，而非 occupancy 不足）。
- tiled → shared memory 带宽 + `__syncthreads` 开销。
- register → 寄存器压力压低 occupancy；另有 global coalescing 不理想、partial wave tail。
- cuBLAS → 基本工作在 compute 屋顶下。

### Evidence
roofline：`ridge ≈ 15 FLOP/Byte`。naive 有效算术强度 ≪ ridge（memory-bound 区）；tiling + register blocking 把强度逐步推过 ridge → 走向 compute-bound；cuBLAS 基本在 compute 屋顶下。

### Conclusion
**提升数据复用 → 抬高算术强度 → 把瓶颈从带宽换到算力。** 这条线索能解释每一步 TFLOPS 的提升，也指明下一步方向（增大 register tile / warp-tiling、double buffering、`float4` 向量化、修 coalescing）。

---

## 6. 其它 GPU（实测后回填）

### 环境

```text
GPU:
CUDA Toolkit:
Driver:
峰值 FP32:
显存带宽:
ridge point:
CUDA_ARCH:
```

### Benchmark（make bench，建议 1024³ 起）

| Kernel | Avg(ms) | TFLOPS | 相对 cuBLAS | 相对峰值 | Check |
|---|---:|---:|---:|---:|:--:|
| naive    |  |  |  |  |  |
| tiled    |  |  |  |  |  |
| register |  |  |  |  |  |
| cuBLAS   |  |  |  |  |  |

### Nsight Compute

| Kernel | Duration | SM tput% | Occ% | DRAM% | Reg/thr | Shmem/blk | 主要 stall |
|---|---:|---:|---:|---:|---:|---:|---|
| naive    |  |  |  |  |  | — |  |
| tiled    |  |  |  |  |  |  |  |
| register |  |  |  |  |  |  |  |
| cuBLAS   |  |  |  |  |  |  |  |

---

## 7. Nsight 速记

```bash
sudo /usr/local/cuda/bin/ncu --set full ./build/bench_matmul --profile --kernel tiled
sudo /usr/local/cuda/bin/ncu --set full -o report_register ./build/bench_matmul --profile --kernel register
ncu-ui report_register.ncu-rep
```

更多指标解释见 [[3.4 CUDA Nsight Compute 指标速查|CUDA Nsight Compute 指标速查]]。
