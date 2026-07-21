# Week02 渐进式练习

## 本次运行记录

本项目按 GTX 1660 Super 对应的 `sm_75` 架构构建并运行：

```bash
make test CUDA_ARCH=75
make bench CUDA_ARCH=75
```

结果：

- `test_reduce_naive`、`test_reduce_shared`、`test_reduce_shuffle` 全部通过。
- `float` 与 `int` 都覆盖了 `n=1`、`n=1024`、`n=1048576`、`n=1000`。
- benchmark 使用 `Warmup: 5 runs | Repeat: 20 runs per (size, kernel)`。

正确性测试摘要：

```text
naive<float/int>   all PASS
shared<float/int>  all PASS
shuffle<float/int> all PASS
```

Benchmark 实测结果：

| Type | N | Kernel | Avg(ms) | Min(ms) | Max(ms) | BW(GB/s) | Status |
| --- | ---: | --- | ---: | ---: | ---: | ---: | --- |
| float | 1K | Naive | 0.021 | 0.020 | 0.024 | 0.19 | OK |
| float | 1K | Shared | 0.018 | 0.017 | 0.019 | 0.23 | OK |
| float | 1K | Shuffle | 0.018 | 0.018 | 0.019 | 0.23 | OK |
| float | 1M | Naive | 0.223 | 0.220 | 0.232 | 18.80 | OK |
| float | 1M | Shared | 0.195 | 0.193 | 0.199 | 21.50 | OK |
| float | 1M | Shuffle | 0.182 | 0.179 | 0.190 | 23.11 | OK |
| float | 64M | Naive | 6.458 | 6.431 | 6.509 | 41.57 | OK |
| float | 64M | Shared | 5.547 | 5.024 | 6.017 | 48.39 | OK |
| float | 64M | Shuffle | 4.448 | 4.422 | 4.552 | 60.35 | OK |
| int | 1K | Naive | 0.019 | 0.018 | 0.023 | 0.21 | OK |
| int | 1K | Shared | 0.017 | 0.016 | 0.018 | 0.24 | OK |
| int | 1K | Shuffle | 0.021 | 0.017 | 0.047 | 0.19 | OK |
| int | 1M | Naive | 0.204 | 0.198 | 0.217 | 20.59 | OK |
| int | 1M | Shared | 0.176 | 0.173 | 0.181 | 23.84 | OK |
| int | 1M | Shuffle | 0.168 | 0.165 | 0.182 | 24.98 | OK |
| int | 64M | Naive | 6.263 | 6.247 | 6.302 | 42.86 | OK |
| int | 64M | Shared | 5.412 | 5.019 | 6.078 | 49.60 | OK |
| int | 64M | Shuffle | 4.440 | 4.429 | 4.451 | 60.46 | OK |

---

## Day 1: Naive Reduce

### 练习 1.1: 理解 baseline

运行 `make test-naive`，观察所有测试是否通过。然后思考：

- 每个 block 中有 256 个线程，但只有线程 0 在工作。其他 255 个线程在做什么？
- 这种实现的 GPU 利用率大概是多少？

**答案：**

本机运行 `make test CUDA_ARCH=75` 后，三种 reduce 的正确性测试全部通过。代码逻辑上，`reduce_naive_kernel` 中只有：

```cpp
if (threadIdx.x == 0) {
    ...
}
```

会进入累加循环。其他 255 个线程会被启动、占用调度资源，但不会做有效求和工作，只是在条件判断后退出。

按线程数量粗略估算，block 内有效线程比例是：

```text
1 / 256 = 0.390625%
```

实际 GPU 利用率还会更低，因为线程 0 串行读取 global memory，无法用同一个 block 内的其他线程隐藏内存延迟。

### 练习 1.2: atomicAdd 变体

修改 naive kernel，让每个线程用 `atomicAdd` 把自己的元素加到 `partial_sums[blockIdx.x]`。

- 这样做会比线程 0 循环更快吗？为什么？
- 提示：atomicAdd 在 global memory 上的争用会导致串行化。

**答案：**

不一定更快，通常也不是一个好版本。每个 block 内 256 个线程都对同一个 `partial_sums[blockIdx.x]` 做 `atomicAdd`，这些原子操作会在同一个 global memory 地址上争用，硬件必须保证加法的顺序一致性，热点地址会导致大量串行化。

它相比线程 0 串行循环的优点是 global load 可以由多个线程发起，读输入的并行度更高；缺点是所有加法集中到同一个 global atomic 地址。对于小 block 可能偶尔接近 naive，但对大规模输入通常不如 shared memory 或 shuffle reduce。

更合理的做法是每个线程先在寄存器里保存局部和，再用 shared memory 或 warp shuffle 做 block 内归约，最后每个 block 只写一次 partial sum。

### 练习 1.3: 增大每个线程的工作量

修改 kernel，让每个线程负责多个元素（例如 4 个），然后线程 0 累加所有线程的局部和。

- 这会改善性能吗？
- 提示：减少了 block 数量，但每个线程做更多工作。

**答案：**

如果“每个线程先算局部和，但最后仍然只有线程 0 累加所有线程结果”，会有一定改善，但不彻底。改善点是每个线程能并行读取多个元素，输入读取阶段的并行度比原始 naive 高；问题是最后仍然有一个串行瓶颈。

如果局部和放到 shared memory 中，再做 block 内并行归约，它就逐步接近 shared memory reduce 的结构，性能会明显好于原始 naive。

---

## Day 2: Shared Memory Reduce

### 练习 2.1: 观察加速

运行 `make bench`，对比 naive 和 shared 的性能。

- 加速比是多少？
- 加速比随 n 增大如何变化？

**答案：**

计算方式：

```text
speedup = naive Avg(ms) / shared Avg(ms)
```

实测加速比：

| Type | N | Naive Avg(ms) | Shared Avg(ms) | Speedup |
| --- | ---: | ---: | ---: | ---: |
| float | 1K | 0.021 | 0.018 | 1.17x |
| float | 1M | 0.223 | 0.195 | 1.14x |
| float | 64M | 6.458 | 5.547 | 1.16x |
| int | 1K | 0.019 | 0.017 | 1.12x |
| int | 1M | 0.204 | 0.176 | 1.16x |
| int | 64M | 6.263 | 5.412 | 1.16x |

结论：

- shared memory reduce 相比 naive 稳定快约 `1.12x-1.17x`。
- `1K` 下加速比参考意义较弱，因为 kernel launch overhead 占主导。
- `1M` 和 `64M` 下 shared 的优势稳定，但没有达到数量级提升，原因是当前实现仍然每个线程只读一个元素，且最终 partial sums 在 CPU 端累加。

### 练习 2.2: bank conflict 分析

当前的二分归约中，stride 从 `blockDim.x/2` 开始每轮减半。思考：

- 当 stride = 16 时，线程 0 访问 `sdata[0]` 和 `sdata[16]`。这两个地址在同一个 bank 吗？
- 如果改用从 stride = 1 开始每轮翻倍的方式（即相邻线程合作），会有 bank conflict 吗？

**答案：**

假设 `T=float`，shared memory 有 32 个 bank，连续 4 字节落到连续 bank：

```text
bank(index) = index % 32
```

所以：

```text
sdata[0]  -> bank 0
sdata[16] -> bank 16
```

它们不在同一个 bank。

当前代码使用的是 sequential addressing：

```cpp
if (tid < stride) {
    sdata[tid] += sdata[tid + stride];
}
```

在 `stride = 16` 时，线程 0-15 分别访问 `0..15` 和 `16..31`，两组访问都覆盖不同 bank，没有明显 bank conflict。

如果改成从 `stride = 1` 开始翻倍的 interleaved addressing，例如：

```cpp
int index = 2 * stride * tid;
sdata[index] += sdata[index + stride];
```

早期 CUDA 资料常指出这种写法更容易带来 shared memory bank conflict 和 warp divergence。即使在现代 GPU 上 bank conflict 被缓解，它也会让活跃线程分布更稀疏，分支效率更差，所以通常不如当前这种从大 stride 向小 stride 递减的写法。

### 练习 2.3: 循环展开

在归约循环的最后几轮（stride <= 32 时），block 内只有一个 warp 在工作。

- 这时还需要 `__syncthreads()` 吗？
- 尝试用 `#pragma unroll` 或手动展开最后 5 轮，观察性能变化。

**答案：**

当只剩一个 warp 参与归约时，warp 内线程天然 lockstep 执行。传统优化里可以用 `volatile` shared memory 或 warp shuffle 去掉最后几轮的 `__syncthreads()`。

不过从代码安全性看，直接删除 `__syncthreads()` 要谨慎：

- block 级别 shared memory 归约在 `stride > 32` 时仍然需要同步。
- `stride <= 32` 时可以改成 warp-level reduction，最好直接使用 `__shfl_down_sync`，这也是本项目 `reduce_shuffle` 的方向。

预期性能变化：循环展开能减少 loop overhead 和部分 barrier 开销，但相比从 shared memory 改成 shuffle，收益通常较小。大 N 下更可能受 global memory 带宽限制。

### 练习 2.4: 每线程多元素

修改 shared memory kernel，让每个线程在加载阶段读取 2 个或 4 个元素（先局部累加，再写入 shared memory）。

- 这减少了 kernel launch 的 block 数量。
- 观察对性能的影响。

**答案：**

这是常见优化。每个线程读取多个元素并在寄存器中先求局部和，可以减少 block 数量和 partial sums 数量：

```text
原始：num_blocks = ceil(n / blockDim.x)
2 元素/线程：num_blocks = ceil(n / (2 * blockDim.x))
4 元素/线程：num_blocks = ceil(n / (4 * blockDim.x))
```

好处：

- 减少 block 数量。
- 减少写回 `partial_sums` 的数量。
- 提高每个线程的算术工作量，有助于摊薄地址计算和调度开销。

风险：

- 每线程寄存器使用增加，可能降低 occupancy。
- 如果读取模式处理不好，可能影响 memory coalescing。

对 64M 这种大输入通常有帮助；对 1K 这种小输入，kernel launch overhead 仍然占主导。

---

## Day 3: Warp Shuffle Reduce

### 练习 3.1: 理解 shuffle 指令

在 `warp_reduce_sum` 中加入调试打印（仅对一个 warp），输出每轮 shuffle 后每个 lane 的值。

- 观察值如何逐步汇聚到 lane 0。
- 提示：用 `if (blockIdx.x == 0 && threadIdx.x < 32)` 限制输出。

**答案：**

`warp_reduce_sum` 当前逻辑是：

```cpp
for (int offset = warpSize / 2; offset > 0; offset >>= 1) {
    val += __shfl_down_sync(0xffffffff, val, offset);
}
```

如果一个 warp 的输入是 lane 编号 `0..31`，每轮后 lane 0 会累加更大的连续范围：

```text
offset = 16: lane 0 得到 lane 0 + lane 16
offset = 8 : lane 0 得到 lane 0 + lane 8 + lane 16 + lane 24
offset = 4 : lane 0 覆盖 8 个 lane 的和
offset = 2 : lane 0 覆盖 16 个 lane 的和
offset = 1 : lane 0 覆盖 32 个 lane 的和
```

最终只有 lane 0 的值是整个 warp 的完整和。其他 lane 的值是各自后缀范围的局部和，不应该作为最终结果使用。

### 练习 3.2: 性能对比

运行 `make bench`，对比三种实现。

- shuffle 比 shared memory 快多少？
- 随 n 增大，差距如何变化？
- 为什么对于非常大的 n，三种实现的差距可能缩小？

**答案：**

计算方式：

```text
shuffle_vs_shared = shared Avg(ms) / shuffle Avg(ms)
```

实测结果：

| Type | N | Shared Avg(ms) | Shuffle Avg(ms) | Shuffle vs Shared |
| --- | ---: | ---: | ---: | ---: |
| float | 1K | 0.018 | 0.018 | 1.00x |
| float | 1M | 0.195 | 0.182 | 1.07x |
| float | 64M | 5.547 | 4.448 | 1.25x |
| int | 1K | 0.017 | 0.021 | 0.81x |
| int | 1M | 0.176 | 0.168 | 1.05x |
| int | 64M | 5.412 | 4.440 | 1.22x |

结论：

- `1K` 下差距很小，int 的 shuffle 还因为一次 `Max(ms)=0.047` 的抖动导致平均值慢于 shared。
- `1M` 下 shuffle 比 shared 快约 `1.05x-1.07x`。
- `64M` 下 shuffle 优势更明显，float 约 `1.25x`，int 约 `1.22x`。
- 这说明当前实现中，warp shuffle 减少 shared memory 访问和同步后，在大输入下能转化为更高带宽。

当前 `reduce_shuffle` 仍然用少量 shared memory 汇总每个 warp 的结果，所以它不是“完全无 shared memory”，但 shared memory 使用量只有 `num_warps * sizeof(T)`。

### 练习 3.3: 只用 shuffle（不用 shared memory）

尝试实现一个完全不用 shared memory 的版本：

- 每个 warp 的 lane 0 用 `atomicAdd` 加到 `partial_sums[blockIdx.x]`。
- 与当前的两层归约相比，性能如何？

**答案：**

预期会慢于当前两层归约。原因是每个 block 有多个 warp，如果每个 warp 的 lane 0 都对同一个 `partial_sums[blockIdx.x]` 做 `atomicAdd`，一个 block 内会产生 `num_warps` 次原子争用。

当前版本的流程是：

```text
warp 内 shuffle -> 每个 warp 写一个 shared memory 槽位 -> 第一个 warp 再 shuffle -> block 只写一次 partial sum
```

这个设计把 global 写回压缩到每个 block 一次，避免了 block 内 global atomic 热点。完全不用 shared memory 的版本代码更短，但通常性能更差。

### 练习 3.4: `__shfl_xor_sync`

将 `__shfl_down_sync` 替换为 `__shfl_xor_sync`（butterfly pattern）。

- 两者的区别是什么？
- 在哪些场景下 xor 模式更有优势？

**答案：**

`__shfl_down_sync(mask, val, offset)` 是向下取值：

```text
lane i 读取 lane i + offset
```

它天然适合 reduction，因为最终结果汇聚到 lane 0。

`__shfl_xor_sync(mask, val, laneMask)` 是 butterfly 交换：

```text
lane i 读取 lane i ^ laneMask
```

它的优势是通信模式对称，每轮后更多 lane 可以得到部分结果。对于 all-reduce、butterfly reduction、FFT、warp 内需要所有 lane 都得到最终结果的场景，xor 模式更合适。

对于“只需要 lane 0 输出 block partial sum”的 reduce，`shfl_down` 更直接。

---

## 综合练习

### 练习 C1: 支持任意类型

将三种实现扩展到支持 `double` 类型。

- 需要注意什么？（提示：`atomicAdd` 对 double 的支持取决于 GPU 架构）

**答案：**

当前代码已经是模板实现，但只显式实例化了：

```cpp
template float reduce_*(const float*, int, int);
template int reduce_*(const int*, int, int);
```

要支持 `double`，需要在三个 `.cu` 文件中增加：

```cpp
template double reduce_naive<double>(const double*, int, int);
template double reduce_shared<double>(const double*, int, int);
template double reduce_shuffle<double>(const double*, int, int);
```

同时测试文件也要增加 `double` 用例。

本项目当前三种实现没有在 kernel 中使用 `atomicAdd`，所以不受 double atomic 支持限制。如果实现 atomic 版本，需要注意：

- `atomicAdd(double*)` 原生支持通常要求 compute capability >= 6.0。
- GTX 1660 Super 是 sm_75，支持 double atomicAdd。
- double 吞吐远低于 float，性能会明显下降。

### 练习 C2: 多级归约

当前实现只做一级归约（GPU partial sums + CPU 最终累加）。

- 实现两级 GPU 归约：第一次 kernel 得到 partial sums，第二次 kernel 归约 partial sums。
- 对于非常大的 n，这会比 CPU 最终累加更快吗？

**答案：**

对于大 N，通常会更快。当前流程是：

```text
GPU: input -> partial_sums
CPU: cudaMemcpy partial_sums -> host
CPU: 串行累加 partial_sums
```

当 `n = 64M`、`threads_per_block = 256` 时：

```text
num_blocks = 64M / 256 = 262144
```

也就是说需要把 262144 个 partial sums 拷回 CPU，再由 CPU 累加。这个开销不算巨大，但已经不是零。

两级 GPU 归约可以变成：

```text
kernel 1: input -> partial_sums_1
kernel 2: partial_sums_1 -> partial_sums_2
CPU: 只拷回很少的 partial_sums_2 或最终 1 个值
```

好处是减少 host-device 同步和 PCIe 拷贝；代价是多一次 kernel launch。对小 N 不划算，对非常大的 N 更可能划算。

### 练习 C3: 与 CUB 对比

使用 NVIDIA CUB 库的 `cub::DeviceReduce::Sum` 作为性能参考。

- 你的最优实现与 CUB 的差距是多少？
- CUB 做了哪些你没做的优化？

**答案：**

当前项目还没有集成 CUB benchmark，所以没有实测差距。预期 CUB 会更快，尤其是在大 N 和不同数据类型/不同 GPU 架构下更稳定。

CUB 可能使用的优化包括：

- 每线程多元素加载，提升内存吞吐。
- vectorized load，减少指令数量。
- 根据架构选择不同 block size 和算法。
- 更完整的多级归约，减少 CPU 端最终累加。
- 对边界条件、occupancy、寄存器压力做了大量调优。

本项目的最佳版本 `reduce_shuffle` 是教学实现，结构清晰，但没有做架构级自动调参。

---

## Day 4: Enhanced Benchmark

### 练习 4.1: 解读 benchmark 输出

运行 `make bench`，观察输出表格：

- 对比三种 kernel 在 1K/1M/64M 下的有效带宽（BW(GB/s)）。
- 1K 下三种 kernel 的带宽差距大吗？为什么？
- 64M 下哪个 kernel 的带宽最接近 GPU 峰值？

**答案：**

实测带宽：

| Type | N | Naive GB/s | Shared GB/s | Shuffle GB/s |
| --- | ---: | ---: | ---: | ---: |
| float | 1K | 0.19 | 0.23 | 0.23 |
| float | 1M | 18.80 | 21.50 | 23.11 |
| float | 64M | 41.57 | 48.39 | 60.35 |
| int | 1K | 0.21 | 0.24 | 0.19 |
| int | 1M | 20.59 | 23.84 | 24.98 |
| int | 64M | 42.86 | 49.60 | 60.46 |

结论：

- `1K` 下带宽都很低，只有 `0.19-0.24 GB/s`，三者差距不大，因为读取数据只有 4KB，kernel launch overhead 和调度开销远大于实际计算。
- `1M` 下 shared 和 shuffle 开始明显高于 naive。
- `64M` 下 shuffle 最快，float 为 `60.35 GB/s`，int 为 `60.46 GB/s`，是三种实现里最接近 GPU 峰值的版本。

GTX 1660 Super 的理论显存带宽约为：

```text
14 Gbps * 192 bit / 8 = 336 GB/s
```

真实 effective bandwidth 会低于这个数值。这个 reduction benchmark 只按读取 `n * sizeof(T)` 计算有效带宽，没有把 partial sum 写回、同步、kernel launch、CPU 最终累加计入有效字节。

### 练习 4.2: Min/Max 分析

观察每个 (size, kernel) 组合的 Min(ms) 和 Max(ms)：

- Max/Min 的比值大约是多少？这反映了什么？
- 第一次 timed run 通常比后续慢吗？如果是，为什么？（提示：考虑 GPU 频率调节和 cache 冷启动）
- 修改 warmup 次数（从 5 改为 0 和 20），观察 Min/Max 的变化。

**答案：**

分析方法：

```text
jitter_ratio = Max(ms) / Min(ms)
```

实测 `Max/Min`：

| Type | N | Naive | Shared | Shuffle |
| --- | ---: | ---: | ---: | ---: |
| float | 1K | 1.20 | 1.12 | 1.06 |
| float | 1M | 1.05 | 1.03 | 1.06 |
| float | 64M | 1.01 | 1.20 | 1.03 |
| int | 1K | 1.28 | 1.12 | 2.76 |
| int | 1M | 1.10 | 1.05 | 1.10 |
| int | 64M | 1.01 | 1.21 | 1.00 |

结论：

- 大多数组合的 `Max/Min` 在 `1.00-1.20`，稳定性可以接受。
- `int 1K Shuffle` 的 `Max/Min = 2.76`，这是小 N 下单次抖动被放大的典型现象；它的绝对时间仍然只有 `0.017-0.047 ms`。
- `64M Shared` 的抖动约 `1.20x-1.21x`，比 naive/shuffle 明显，可能与同步、shared memory 归约阶段或 GPU 运行状态波动有关。
- warmup 从 5 改为 0 时，第一次 timed run 更可能偏慢，Max/Min 会变大。
- warmup 改为 20 时，GPU 状态更稳定，但也可能因为温度和功耗限制导致频率下降，需要观察平均值是否变化。

### 练习 4.3: 带宽利用率计算

查找你的 GPU 的峰值内存带宽（可用 `nvidia-smi -q -d MEMORY` 或查阅规格表）。

- 计算 64M float 下 shuffle reduce 的带宽利用率：`effective_bw / peak_bw * 100%`
- 如果利用率低于 50%，分析可能的原因（partial sums 写回、CPU 端最终累加等）。

**答案：**

GTX 1660 Super 理论显存带宽约 `336 GB/s`。

公式：

```text
utilization = shuffle_64M_BW / 336 * 100%
```

实测：

```text
float: 60.35 / 336 * 100% = 18.0%
int:   60.46 / 336 * 100% = 18.0%
```

利用率低于 50%，可能原因：

- 当前实现每个线程只读一个元素，内存吞吐不够饱满。
- 每个 block 只产生一个 partial sum，但最终 partial sums 要拷回 CPU 侧累加。
- benchmark 的时间包含 device synchronize、函数包装和多次 kernel 调用的开销。
- reduction 本身有加法依赖链，不是纯 streaming copy。
- block size 和每线程元素数没有针对 GTX 1660 Super 调优。

### 练习 4.4: 自定义 benchmark 规模

在 `bench_reduce.cu` 中添加额外的测试规模（例如 256、4K、256K、4M、16M）。

- 画出 N vs. effective bandwidth 的曲线。
- 观察带宽在什么 N 值开始饱和。
- 这个“饱和点”与 GPU 的 SM 数量有什么关系？

**答案：**

可以把 `sizes[]` 改成：

```cpp
const SizeEntry sizes[] = {
    {"256",  256},
    {"1K",   1024},
    {"4K",   4 << 10},
    {"256K", 256 << 10},
    {"1M",   1 << 20},
    {"4M",   4 << 20},
    {"16M",  16 << 20},
    {"64M",  1 << 26},
};
```

预期曲线：

- 小 N：有效带宽很低，launch overhead 主导。
- 中等 N：带宽快速上升。
- 大 N：带宽进入平台期，接近该实现能达到的稳定吞吐。

GTX 1660 Super 有 22 个 SM。想让 GPU 饱和，至少需要足够多 block 让每个 SM 同时驻留多个 block。以 `threads_per_block = 256` 为例：

```text
N = 1K    -> 4 blocks，远少于 22 SM，无法填满 GPU
N = 4K    -> 16 blocks，仍不足
N = 256K  -> 1024 blocks，足够填满 GPU
```

所以饱和点通常出现在 block 数远大于 SM 数之后，而不是刚好等于 SM 数时。

---

## Day 5: Nsight Compute Profiling

### Day 5 运行记录

先直接运行：

```bash
make profile-naive CUDA_ARCH=75
make profile-shared CUDA_ARCH=75
make profile-shuffle CUDA_ARCH=75
```

三次都因为普通用户没有权限访问 NVIDIA GPU Performance Counters 而失败：

```text
ERR_NVGPUCTRPERM
```

改用 `sudo make profile-*` 后，权限问题消失，但 root 的 PATH 找不到 `ncu`：

```text
make: ncu: 没有那个文件或目录
```

最终使用 `ncu` 绝对路径成功运行：

```bash
sudo make profile-naive CUDA_ARCH=75 NCU=/usr/local/cuda/bin/ncu
sudo make profile-shared CUDA_ARCH=75 NCU=/usr/local/cuda/bin/ncu
sudo make profile-shuffle CUDA_ARCH=75 NCU=/usr/local/cuda/bin/ncu
```

完整输出保存到：

```text
docs/profile_results/profile_naive.txt
docs/profile_results/profile_shared.txt
docs/profile_results/profile_shuffle.txt
```

下面使用 `float n=1M` 这一组作为代表性 profiling 结果；对应 grid 是 `4096 blocks x 256 threads`。

| Kernel | Duration(us) | Memory Throughput(GB/s) | Achieved Occupancy | Theoretical Occupancy | 主要观察 |
| --- | ---: | ---: | ---: | ---: | --- |
| naive | 95.58 | 50.26 | 39.74% | 100% | L1TEX scoreboard stall 10.8 cycles，占 64.1%；平均每 warp 只有 2.5 个有效线程 |
| shared | 65.02 | 74.09 | 89.11% | 100% | occupancy 最高；平均每 warp 31.81 个 active threads，但 predication 后约 19.91 个线程有效 |
| shuffle | 54.91 | 87.52 | 64.41% | 100% | 最快；L1TEX scoreboard stall 4.7 cycles，占 33.4%；平均每 warp 28.35 个非 predicated-off 线程 |

### 练习 5.1: 首次 profiling

运行 `make profile-naive`，观察 Nsight Compute 输出。

- 找到 `reduce_naive_kernel` 的 kernel time。
- 与 benchmark 中报告的时间对比——有差异吗？（提示：profiling 会引入额外开销）
- 记录 achieved occupancy 和 memory throughput。

**答案：**

`reduce_naive_kernel<float>` 在 `n=1M` 下的 Nsight Compute 结果：

```text
Grid: 4096 blocks
Block: 256 threads
Duration: 95.58 us
Memory Throughput: 50.26 GB/s
Theoretical Occupancy: 100%
Achieved Occupancy: 39.74%
```

同一规模的 benchmark 中，float `1M Naive` 的平均时间是 `0.223 ms = 223 us`。Nsight Compute 单个 kernel 的 duration 是 `95.58 us`，比 benchmark 的端到端时间低；这并不矛盾，因为 benchmark 计时的是 `reduce_naive<T>` 整个调用过程，包含 kernel、同步、partial sums 拷回 CPU 和 CPU 最终累加，而 Nsight Compute 这里报告的是 kernel 本身。

主要瓶颈：

```text
L1TEX scoreboard stall: 10.8 cycles, 64.1%
Avg. Active Threads Per Warp: 2.51
Avg. Not Predicated Off Threads Per Warp: 2.50
```

这符合 naive 实现：每个 block 只有线程 0 做串行累加，warp 内大部分线程没有有效工作。

### 练习 5.2: 对比三种 kernel 的 profiling

分别运行 `make profile-naive`、`make profile-shared`、`make profile-shuffle`。

- 在 `docs/profiling.md` 中填写各 kernel 的指标。
- 哪个 kernel 的 achieved occupancy 最高？为什么？
- 哪个 kernel 的 warp stall 最严重？主要 stall 原因是什么？

**答案：**

`float n=1M` 对比：

| Kernel | Duration(us) | Memory Throughput(GB/s) | Achieved Occupancy | Warp Cycles / Issued Inst | 代表性 stall |
| --- | ---: | ---: | ---: | ---: | --- |
| naive | 95.58 | 50.26 | 39.74% | 16.80 | L1TEX scoreboard 10.8 cycles，占 64.1% |
| shared | 65.02 | 74.09 | 89.11% | 21.15 | No Eligible 66.60%；predication 后有效线程约 19.91/warp |
| shuffle | 54.91 | 87.52 | 64.41% | 14.18 | L1TEX scoreboard 4.7 cycles，占 33.4% |

结论：

- achieved occupancy 最高的是 shared：`89.11%`。它让整个 block 的线程都参与归约，线程利用率远高于 naive。
- 运行时间最短的是 shuffle：`54.91 us`。虽然 occupancy 低于 shared，但 warp 内通信更轻，memory throughput 最高。
- naive 的 stall 最严重，核心原因是 L1TEX scoreboard 等待和严重的 warp 内线程浪费。

### 练习 5.3: Warp Stall 分析

在 Nsight Compute 中查看 Warp State Statistics：

- Naive kernel 的主要 stall 原因是什么？（预期：Long Scoreboard —— 等待 global memory）
- Shared kernel 呢？（预期：Wait —— `__syncthreads()` barrier）
- Shuffle kernel 呢？（预期：较少的 stall，可能是 Not Selected）
- 这些结果与你对各 kernel 实现的理解一致吗？

**答案：**

实测结果基本符合代码结构，但 shared 的报告重点不是直接写成 barrier，而是体现为 eligible warp 不足和 predication 损失。

- naive：主要是 L1TEX scoreboard dependency，平均 `10.8 cycles`，占 `64.1%`。这说明线程 0 串行读 global memory 时，warp 经常在等待内存依赖。
- shared：achieved occupancy 很高，但 `No Eligible = 66.60%`，并且 `Avg. Not Predicated Off Threads Per Warp = 19.91`。这对应二分归约里每轮只有部分线程有效工作，再加上多轮同步和 shared memory 访问。
- shuffle：L1TEX scoreboard 降到 `4.7 cycles`，占 `33.4%`，明显低于 naive；warp 内寄存器通信减少了 shared memory 归约开销。

所以结果与预期一致：naive 浪费线程且内存等待重；shared 提升 occupancy 但有同步/分支阶段开销；shuffle 的综合表现最好。

### 练习 5.4: Occupancy 瓶颈定位

在 Nsight Compute 的 Occupancy 面板中：

- 查看每个 kernel 的 theoretical vs. achieved occupancy。
- 确定限制 occupancy 的主要因素（寄存器、shared memory、还是 block 数）。
- 对于 shared memory kernel，尝试将 `threads_per_block` 从 256 改为 128，重新 profile。
  - Occupancy 变化了吗？性能变好还是变差？

**答案：**

三种 kernel 在 `float n=1M` 下的 theoretical occupancy 都是 `100%`，但 achieved occupancy 不同：

```text
naive:   39.74%
shared:  89.11%
shuffle: 64.41%
```

这说明瓶颈不是理论资源上限本身，而是实际执行时 warp 是否持续有有效工作。

对 `float` shared reduce，每个 block 动态 shared memory 用量是：

```text
256 * sizeof(float) = 1024 bytes
```

这很小，不是主要 occupancy 限制。Nsight Compute 显示 shared 的 achieved occupancy 已经达到 `89.11%`，明显高于 naive 和 shuffle。shared 的问题更多是归约后半段活跃线程减少、predication 和同步阶段开销，而不是 shared memory 容量不足。

如果改成 128 threads/block：

```text
shared memory/block = 128 * sizeof(float) = 512 bytes
num_blocks 翻倍
每个 block 的 warp 数从 8 降到 4
```

可能结果：

- 小 N：block 数增加，可能改善 SM 填充。
- 大 N：更多 block 带来更多 partial sums 和调度开销，性能不一定更好。
- shared reduce 每个 block 内归约轮数从 8 轮减少到 7 轮，但 block 数增加，整体收益需要实测。

当前实测下，优先优化方向不是单纯提高 occupancy，而是减少无效线程、减少 partial sums 回传 CPU、实现每线程多元素加载或二级 GPU 归约。
