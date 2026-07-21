# Week03 Nsight Compute Profiling

> 配合 [[CUDA Week 3 Transpose 项目解析]]。仓库 `docs/profiling.md` 目前是**空模板**，本文给出方法论、该看哪些指标、预期定性结论，以及与仓库一致的**待填表格**——真实数字请在你的 GPU 上实测后回填，不要照抄预测值。

---

## 1. 运行方式

```bash
make bench CUDA_ARCH=<你的架构>          # 先拿到带宽与正确性
make profile-copy   CUDA_ARCH=<arch>
make profile-naive  CUDA_ARCH=<arch>
make profile-tiled  CUDA_ARCH=<arch>
make profile-padded CUDA_ARCH=<arch>
```

> [!note] 常见权限/PATH 坑（沿用 Week02 经验）
> - 普通用户跑 Nsight 可能报 `ERR_NVGPUCTRPERM`（性能计数器权限）→ 用 `sudo`。
> - `sudo` 后 root 的 PATH 找不到 `ncu` → 传绝对路径，如 `NCU=/usr/local/cuda/bin/ncu`。

---

## 2. 该盯哪些指标

transpose 是纯 memory-bound，且四版本 block 配置一致，所以 occupancy 基本是常量、不是变量；真正的区别集中在 global coalescing 与 shared bank conflict 两处。

| 指标 | 看什么 | 预期 |
|---|---|---|
| Global Load Efficiency | 读是否 coalesced | 四版本都应接近 100% |
| Global Store Efficiency | 写是否 coalesced | **naive 显著偏低**；其余接近满 |
| Memory Throughput (GB/s) | 有效带宽 | copy ≳ padded > tiled ≫ naive |
| Shared Memory Bank Conflicts | shared 是否撞 bank | **tiled 大量；padded ≈ 0**；copy/naive 不用 shared |
| Achieved Occupancy | 排除混淆变量 | 四版本接近，确认差异不来自占用率 |
| Warp Stall Reason | 主要停顿原因 | naive 偏 Long Scoreboard（等 global）；tiled 偏 shared 相关 stall |

---

## 3. 预期定性结论（待实测验证）

1. **naive 垫底**：store efficiency 低、global 写 transaction 数远超理想，是和其它三者拉开差距的唯一原因。
2. **tiled 修好 global、却被 shared 拖累**：global load/store efficiency 都高，但 shared bank conflict 计数很大，写阶段 shared load 被 32-way 串行化。
3. **padded 最优**：与 tiled 唯一差别是 `+1` padding，bank conflict 应降到≈0，带宽最接近 copy 上界。
4. **copy 是上界**：无转置、无 shared，纯 coalesced 搬运。

排序：**copy ≳ padded > tiled ≫ naive**。

---

## 4. 待填表格（实测后回填）

### 环境

```text
GPU:
CUDA Toolkit:
Driver:
OS:
CUDA_ARCH:
```

### Benchmark（make bench）

#### 1024 × 1024

| Kernel | Avg(ms) | Min(ms) | Max(ms) | BW(GB/s) | Check |
|---|---:|---:|---:|---:|:--:|
| copy   |  |  |  |  |  |
| naive  |  |  |  |  |  |
| tiled  |  |  |  |  |  |
| padded |  |  |  |  |  |

#### 4096 × 4096

| Kernel | Avg(ms) | Min(ms) | Max(ms) | BW(GB/s) | Check |
|---|---:|---:|---:|---:|:--:|
| copy   |  |  |  |  |  |
| naive  |  |  |  |  |  |
| tiled  |  |  |  |  |  |
| padded |  |  |  |  |  |

#### 2048 × 3072（非方阵）

| Kernel | Avg(ms) | Min(ms) | Max(ms) | BW(GB/s) | Check |
|---|---:|---:|---:|---:|:--:|
| copy   |  |  |  |  |  |
| naive  |  |  |  |  |  |
| tiled  |  |  |  |  |  |
| padded |  |  |  |  |  |

#### 1000 × 1000（非 tile 对齐）

| Kernel | Avg(ms) | Min(ms) | Max(ms) | BW(GB/s) | Check |
|---|---:|---:|---:|---:|:--:|
| copy   |  |  |  |  |  |
| naive  |  |  |  |  |  |
| tiled  |  |  |  |  |  |
| padded |  |  |  |  |  |

### Nsight Compute（代表性形状，如 4096²）

| Kernel | Duration | Mem Throughput(GB/s) | Global Load Eff | Global Store Eff | Shared Bank Conflicts | Achieved Occupancy | 主要 stall |
|---|---:|---:|---:|---:|---:|---:|---|
| copy   |  |  |  |  | — |  |  |
| naive  |  |  |  |  | — |  |  |
| tiled  |  |  |  |  |  |  |  |
| padded |  |  |  |  |  |  |  |

---

## 5. 分析（实测后填写）

### Observation


### Bottleneck


### Evidence


### Conclusion


---

## 6. Nsight 速记

```bash
sudo /usr/local/cuda/bin/ncu --set full ./build/bench_transpose
sudo /usr/local/cuda/bin/ncu --set full -o report_tiled ./build/test_transpose_tiled
ncu-ui report_tiled.ncu-rep
```

更多指标解释见 [[3.4 CUDA Nsight Compute 指标速查|CUDA Nsight Compute 指标速查]]。
