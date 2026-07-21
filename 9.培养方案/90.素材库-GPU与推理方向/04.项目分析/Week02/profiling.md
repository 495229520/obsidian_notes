---
tags:
  - AI-infra/素材库-GPU与推理方向/项目分析/Week02
---
# Week02 Nsight Compute Profiling

使用 Nsight Compute (`ncu`) 对三种 reduce kernel 进行性能分析。

## 运行方式

普通用户直接运行时会遇到 performance counters 权限限制：

```text
ERR_NVGPUCTRPERM
```

使用 `sudo make profile-*` 时，root 的 PATH 找不到 `ncu`，因此通过 `NCU` 传入绝对路径：

```bash
sudo make profile-naive CUDA_ARCH=75 NCU=/usr/local/cuda/bin/ncu
sudo make profile-shared CUDA_ARCH=75 NCU=/usr/local/cuda/bin/ncu
sudo make profile-shuffle CUDA_ARCH=75 NCU=/usr/local/cuda/bin/ncu
```

完整输出保存位置：

```text
docs/profile_results/profile_naive.txt
docs/profile_results/profile_shared.txt
docs/profile_results/profile_shuffle.txt
```

下面记录 `float n=1M` 和 `int n=1M` 的代表性结果。对应 launch shape 均为：

```text
gridDim = 4096
blockDim = 256
threads = 1,048,576
GPU = GTX 1660 Super, CC 7.5
```

---

## Float Results

| Kernel | Duration(us) | Mem Throughput(GB/s) | Achieved Occupancy | Registers / Thread | Dynamic Shared / Block | L1/TEX Hit | L2 Hit | Main Stall / Observation |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| naive | 95.58 | 50.26 | 39.74% | 40 | 0 B | 87.35% | 52.54% | L1TEX scoreboard 10.8 cycles, 64.1%; only ~2.5 useful threads/warp |
| shared | 65.02 | 74.09 | 89.11% | 16 | 1.02 KB | 1.09% | 4.50% | No Eligible 66.60%; predication leaves ~19.91 useful threads/warp |
| shuffle | 54.91 | 87.52 | 64.41% | 16 | 32 B | 1.64% | 4.18% | L1TEX scoreboard 4.7 cycles, 33.4%; best runtime |

## Int Results

| Kernel | Duration(us) | Mem Throughput(GB/s) | Achieved Occupancy | Registers / Thread | Dynamic Shared / Block | L1/TEX Hit | L2 Hit | Main Stall / Observation |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| naive | 93.02 | 51.58 | 40.34% | 25 | 0 B | 87.36% | 52.36% | L1TEX scoreboard 11.8 cycles, 66.5%; only ~4.3 useful threads/warp |
| shared | 64.93 | 74.51 | 88.99% | 16 | 1.02 KB | not recorded here | not recorded here | No Eligible 66.64%; predication leaves ~19.9 useful threads/warp |
| shuffle | 54.85 | 87.66 | 64.63% | 16 | 32 B | not recorded here | not recorded here | L1TEX scoreboard 4.8 cycles; best runtime |

---

## Cross-Kernel Comparison

| Kernel | Type | Kernel Time (us) | Mem Throughput (GB/s) | Achieved Occupancy | Relative Time |
| --- | --- | ---: | ---: | ---: | ---: |
| Naive | float | 95.58 | 50.26 | 39.74% | 1.00x |
| Shared | float | 65.02 | 74.09 | 89.11% | 0.68x |
| Shuffle | float | 54.91 | 87.52 | 64.41% | 0.57x |
| Naive | int | 93.02 | 51.58 | 40.34% | 1.00x |
| Shared | int | 64.93 | 74.51 | 88.99% | 0.70x |
| Shuffle | int | 54.85 | 87.66 | 64.63% | 0.59x |

## Key Takeaways

1. `reduce_naive_kernel` 的 theoretical occupancy 是 100%，但 achieved occupancy 只有约 40%。根因不是资源上限，而是每个 block 基本只有线程 0 在做有效累加，warp 内大部分线程空转。
2. `reduce_shared_kernel` 的 achieved occupancy 最高，float 达到 89.11%，int 达到 88.99%。它让 block 内线程都参与归约，但归约后半段有效线程数递减，仍有 predication 和同步阶段开销。
3. `reduce_shuffle_kernel` 运行时间最短，memory throughput 最高。它的 achieved occupancy 低于 shared，但 warp 内寄存器通信减少了 shared memory 访问和同步成本。
4. 对 float n=1M，kernel duration 排序是 `shuffle < shared < naive`：`54.91 us < 65.02 us < 95.58 us`。
5. 下一步优化方向：每线程多元素加载、二级 GPU 归约，减少 partial sums 回传 CPU；也可以加入 CUB baseline 做对照。

## Nsight Compute Cheat Sheet

```bash
# Full profiling
sudo make profile-naive CUDA_ARCH=75 NCU=/usr/local/cuda/bin/ncu

# Direct command
sudo /usr/local/cuda/bin/ncu --set full ./build/test_reduce_naive

# Save report
sudo /usr/local/cuda/bin/ncu --set full -o report_naive ./build/test_reduce_naive

# Open saved report in GUI
ncu-ui report_naive.ncu-rep
```
