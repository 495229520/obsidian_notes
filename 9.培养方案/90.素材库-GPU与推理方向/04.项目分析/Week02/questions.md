# Week02 必答问题

## Day 2: Shared Memory Reduce

### Q1: 为什么 block 内可以用 shared memory？

**答：** Shared memory 是每个 SM（Streaming Multiprocessor）上的 on-chip SRAM。一个 block 的所有线程被调度到同一个 SM 上执行，因此它们可以共享这块 SRAM。不同 block 可能运行在不同 SM 上，所以 shared memory 只在 block 内可见。

关键特性：
- 延迟约 20-30 cycles（vs global memory 的 400-800 cycles）
- 容量有限（通常 48KB-96KB per SM）
- 生命周期与 block 相同：block 结束后 shared memory 被回收

### Q2: 为什么每轮归约需要 `__syncthreads()`？

**答：** 二分归约是分轮进行的。在第 k 轮中，线程 i 需要读取 `sdata[i + stride]` 的值，而这个值可能是另一个线程在第 k-1 轮写入的。如果不同步：

- 线程 A 可能还在执行第 k-1 轮的写入
- 线程 B 已经进入第 k 轮，读到了过期数据

`__syncthreads()` 是 block 级别的 barrier：确保 block 内所有线程都执行到同一点后才继续。这保证了每轮归约开始时，所有上一轮的写入都已完成。

注意：`__syncthreads()` 只能同步同一 block 内的线程，不能跨 block 同步。

### Q3: 为什么 shared memory 不等于一定更快？

**答：** 几种情况下 shared memory 可能不会带来明显加速：

1. **数据量太小**：如果 block 内只有几个元素，归约循环本身很短，shared memory 的优势被 kernel launch overhead 掩盖。
2. **bank conflict**：shared memory 被分成 32 个 bank。如果多个线程同时访问同一 bank 的不同地址，会产生 bank conflict，导致串行化。
3. **占用率（occupancy）下降**：shared memory 是 SM 级别的共享资源。如果每个 block 用太多 shared memory，SM 上能同时运行的 block 数量减少，降低了硬件利用率。
4. **global memory 已经足够**：如果数据访问模式是 coalesced（合并访问），且只读一次，global memory 的 L1/L2 cache 可能已经足够快。

---

## Day 3: Warp Shuffle Reduce

### Q1: Warp 内线程为什么可以用 shuffle 交换数据？

**答：** Warp 是 GPU 的基本调度单位，由 32 个线程组成。这 32 个线程以 lockstep（锁步）方式执行同一条指令。因为它们天然同步，硬件可以直接在线程的寄存器之间交换数据，无需经过任何中间存储。

`__shfl_down_sync(mask, val, offset)` 的含义：
- `mask`：参与 shuffle 的线程掩码（0xffffffff = 全部 32 个）
- `val`：当前线程的值
- `offset`：从 lane + offset 的线程获取值
- 返回值：lane + offset 线程的 val（如果 lane + offset >= 32，返回自己的 val）

### Q2: Shuffle reduce 和 shared memory reduce 的差别是什么？

**答：**

| 维度 | Shared Memory Reduce | Shuffle Reduce |
|------|---------------------|----------------|
| 存储介质 | on-chip SRAM（shared memory） | 寄存器（register file） |
| 延迟 | ~20-30 cycles | ~1 cycle |
| 同步方式 | 需要 `__syncthreads()` | warp 内天然同步 |
| 作用范围 | 整个 block（最大 1024 线程） | 单个 warp（32 线程） |
| bank conflict | 可能存在 | 不存在 |
| 资源占用 | 消耗 SM 的 shared memory 预算 | 不额外消耗 shared memory |

实际实现中，shuffle reduce 通常用于 warp 内归约，再用少量 shared memory 做跨 warp 的最后一步。两者是互补关系。

### Q3: 哪些场景 shuffle 更合适？

**答：**

1. **小规模归约**（≤ 32 个值）：一个 warp 就能完成，完全不需要 shared memory。
2. **频繁的 warp 内通信**：比如 prefix sum（scan）的 warp 级实现。
3. **需要最低延迟**：shuffle 的寄存器到寄存器通信是最快的线程间数据交换方式。
4. **shared memory 紧张**：当 kernel 已经大量使用 shared memory 做其他用途时，shuffle 可以避免进一步消耗 shared memory 预算。
5. **broadcast 场景**：`__shfl_sync` 可以高效地将一个 lane 的值广播给 warp 内所有线程。

---

## Day 4: Enhanced Benchmark

### Q1: 什么是"有效带宽"（effective bandwidth），为什么它是 reduction 最重要的指标？

**答：** 有效带宽衡量的是 kernel 实际从 global memory 读取数据的速率：

```
bandwidth_GB_s = (n * sizeof(T)) / (kernel_time_s) / 1e9
```

对于 reduction，每个元素只被读取一次，计算量极小（一次加法），因此性能瓶颈几乎完全在内存读取上。有效带宽直接反映了 kernel 利用 GPU 内存带宽的效率。

理想情况下，有效带宽应接近 GPU 的峰值内存带宽（例如 T4 约 320 GB/s，A100 约 2039 GB/s）。如果远低于峰值，说明存在优化空间（如 uncoalesced access、launch overhead 等）。

### Q2: 为什么 1K reduction 的有效带宽远低于 64M？

**答：** 1K（1024 个元素）只有 4KB 数据（float），但 kernel launch 本身有固定开销（约 5-20 μs），这个开销在数据量小时占主导地位。

具体分析：
- **Launch overhead**: GPU kernel 启动的固定成本（driver 调度、grid 配置）与数据量无关
- **GPU 利用率低**: 1024 个元素只需 4 个 block（@ 256 threads），远远无法填满 GPU 的所有 SM
- **内存事务粒度**: GPU 内存以 32-byte 或 128-byte 为单位读取，极小数据量无法充分利用内存总线宽度

因此小 N 的"有效带宽"被 launch overhead 稀释，不能反映真实内存性能。

### Q3: Kernel launch overhead 如何影响小 N 的 benchmark？

**答：** CUDA kernel launch 是异步的，但有固定延迟：

1. **CPU 侧**: 驱动程序准备 kernel 参数、配置 grid/block 维度（~3-10 μs）
2. **GPU 侧**: 调度器将 block 分配到 SM（~2-5 μs）
3. **总 overhead**: 通常 5-20 μs，取决于 GPU 和驱动版本

当 kernel 实际计算时间很短（如 1K reduction 可能只需 ~1 μs），launch overhead 可能占总时间的 90% 以上。此时：
- 不同 kernel 实现的时间差异被 overhead 掩盖
- 有效带宽数值无意义
- 应关注绝对时间而非带宽

解决方法：使用大 N（如 64M）来测量"稳态"性能，小 N 主要用于观察 overhead 效应。

---

## Day 5: Nsight Compute Profiling

### Q1: "achieved occupancy" 是什么意思？为什么可能低于理论值？

**答：** Occupancy 是 SM 上活跃 warp 数占最大可承载 warp 数的比例。

- **Theoretical occupancy**: 根据 kernel 的资源使用（寄存器数、shared memory 用量）计算出的上限
- **Achieved occupancy**: 实际运行时的平均活跃 warp 比例

Achieved occupancy 低于理论值的原因：
1. **Grid 太小**: 总 block 数不够填满所有 SM
2. **Tail effect**: 最后一批 block 运行时，部分 SM 已经空闲
3. **不均匀的 block 执行时间**: 某些 block 提前完成，SM 部分空闲
4. **Register spilling**: 实际寄存器使用超过编译时预估，降低并发 block 数

注意：高 occupancy 不一定意味着高性能。如果 kernel 是 compute-bound，中等 occupancy 可能已经足够隐藏延迟。

### Q2: 主要的 warp stall 原因有哪些？各是什么导致的？

**答：**

| Stall Reason | 含义 | 常见原因 |
|-------------|------|---------|
| **Stall Long Scoreboard** | 等待长延迟操作完成 | Global memory 读取（数百 cycles） |
| **Stall Short Scoreboard** | 等待短延迟操作完成 | Shared memory 访问、L1 cache |
| **Stall Wait** | 等待 barrier 同步 | `__syncthreads()` 导致快线程等慢线程 |
| **Stall Not Selected** | warp 就绪但未被选中执行 | SM 上活跃 warp 太多，调度器轮换 |
| **Stall MIO Throttle** | Memory I/O 单元繁忙 | 内存请求过多，内存管线饱和 |
| **Stall Math Pipe Throttle** | 计算管线繁忙 | 计算密集指令连续执行 |

对于 reduction kernel：
- Naive: 主要是 **Long Scoreboard**（thread 0 串行读 global memory）
- Shared: 主要是 **Wait**（频繁 `__syncthreads()`）+ **Short Scoreboard**
- Shuffle: 较少的 stall，因为 warp 内通信极快

### Q3: 寄存器数量和 shared memory 用量如何影响 occupancy？

**答：** SM 的资源是有限的，每个 block 的资源需求越大，SM 能同时运行的 block 越少：

**寄存器（Registers）:**
- 每个 SM 有固定数量的 32-bit 寄存器（如 sm_75: 65536 个）
- 每个线程使用的寄存器越多 → 每个 block 消耗的寄存器总量越大 → SM 能驻留的 block 越少
- 例：每线程 32 个寄存器，256 线程/block = 8192 寄存器/block → SM 最多 8 个 block
- 例：每线程 64 个寄存器，256 线程/block = 16384 寄存器/block → SM 最多 4 个 block

**Shared Memory:**
- 每个 SM 有固定的 shared memory 容量（如 48KB-96KB）
- 每个 block 请求的 shared memory 越多 → SM 能驻留的 block 越少
- 例：每 block 4KB shared memory，48KB 总量 → 最多 12 个 block
- 例：每 block 16KB shared memory，48KB 总量 → 最多 3 个 block

可以使用 `--ptxas-options=-v` 编译选项查看每个 kernel 的寄存器和 shared memory 用量。Nsight Compute 的 Occupancy 分析面板直接显示哪个资源是 occupancy 的限制因素。
