---
tags:
  - AI-infra
  - 素材库-GPU与推理方向
  - 项目分析
  - Week01
---
# Week01 Benchmark & Profiling

> 配合 [[CUDA Week 1 Hello World 项目解析]]。本文给出方法论、该看哪些指标、预期定性结论，以及与仓库一致的**待填表格**——真实数字请在你的 GPU 上实测后回填，不要照抄预测值。

---

## 1. 运行方式

```bash
make bench CUDA_ARCH=<你的架构>          # kernel 时间 + 有效带宽 + 正确性
# 可选：用 Nsight 看为什么这么快/慢
make profile CUDA_ARCH=<arch>           # 若仓库提供；否则手动 ncu，见 §6
```

> [!note] 常见权限/PATH 坑
> - 普通用户跑 Nsight 可能报 `ERR_NVGPUCTRPERM`（性能计数器权限）→ 用 `sudo`。
> - `sudo` 后 root 的 PATH 找不到 `ncu` → 传绝对路径，如 `/usr/local/cuda/bin/ncu`。

---

## 2. 该盯哪些指标

`vector add` 是纯 memory-bound、单 kernel、无 shared memory，所以关注点很集中：有效带宽是否逼近 GPU 显存带宽上限，以及访存是否 coalesced。

| 指标 | 看什么 | 预期 |
|---|---|---|
| Kernel Duration (ms) | 单次 kernel 时间 | 随 N 线性增长 |
| Effective Bandwidth (GB/s) | `3·N·4 / 时间` | 大 N 时逼近显存带宽上限 |
| Memory Throughput (%) | 显存带宽利用率 | 应很高（memory-bound 特征） |
| Global Load/Store Efficiency | 访存是否 coalesced | 接近 100%（相邻线程访问相邻地址） |
| Achieved Occupancy | SM 占用率 | 较高，但不是瓶颈 |
| Compute (SM) Throughput (%) | 算力利用率 | 应很低（只有一次加法） |

判定 memory-bound 的核心证据：**Memory Throughput 远高于 Compute Throughput**。

---

## 3. 预期定性结论（待实测验证）

1. **memory-bound**：3 次访存对 1 次加法，算术强度极低；实测应见 Memory Throughput 高、Compute Throughput 低。
2. **带宽随 N 增大趋于稳定**：小 N 被 launch overhead 稀释，带宽偏低；N 增大后进入稳态，逼近显存带宽上限。
3. **访存天然 coalesced**：相邻 `threadIdx.x` → 相邻 `idx` → 相邻地址，load/store efficiency 应接近 100%，无需额外优化。
4. **occupancy 不是瓶颈**：即便占用率不满，带宽也可能已接近上限——memory-bound kernel 的瓶颈在带宽而非线程并行度。

---

## 4. 待填表格（实测后回填）

### 环境

```text
GPU:
CUDA Toolkit:
Driver:
CMake:
Compiler:
CUDA_ARCH:
OS:
```

### Benchmark（make bench）

| N | Kernel(ms) | Bandwidth(GB/s) | Check |
|---|---:|---:|:--:|
| 1 << 16  (~65K)  |  |  |  |
| 1 << 20  (~1M)   |  |  |  |
| 1 << 24  (~16M)  |  |  |  |
| 1 << 26  (~67M)  |  |  |  |

> 关注带宽随 N 的变化：小 N 偏低（overhead 主导），大 N 趋于稳态、逼近显存带宽峰值。

### Nsight Compute（代表性规模，如 1<<24）

| 指标 | 实测值 | 备注 |
|---|---:|---|
| Duration |  |  |
| Memory Throughput (%) |  | 预期高 |
| Compute (SM) Throughput (%) |  | 预期低 |
| Global Load Efficiency |  | 预期 ≈100% |
| Global Store Efficiency |  | 预期 ≈100% |
| Achieved Occupancy |  |  |
| 主要 stall |  | 预期 Long Scoreboard（等 global） |

---

## 5. 分析（实测后填写）

### Observation


### Bottleneck


### Evidence


### Conclusion


---

## 6. Nsight 速记

```bash
sudo /usr/local/cuda/bin/ncu --set full ./build/bench_vector_add
sudo /usr/local/cuda/bin/ncu --set full -o report_vadd ./build/test_vector_add
ncu-ui report_vadd.ncu-rep
```

更多指标解释见 [[3.4 CUDA Nsight Compute 指标速查|CUDA Nsight Compute 指标速查]]。
