---
title: CUDA Week 2 Parallel Reduction 项目解析
date: 2026-05-12
tags:
  - CUDA
  - infra
aliases:
  - CUDA Week2 项目解析
  - CUDA Parallel Reduction 工程解析
status: active
---

# CUDA Week 2 Parallel Reduction 项目解析

> 这个项目是 `CUDA_learning/week02` 的 Week 2 CUDA 归约工程：用三种 parallel reduction 实现，从 naive baseline 逐步走到 shared memory 和 warp shuffle，并配套 correctness test、enhanced benchmark、Nsight Compute profiling 模板和 Agent workflow 约束。

项目地址：[CUDA_learning/week02](https://github.com/hosendovebelva-boop/CUDA_learning/tree/main/week02)

---

## 1. 项目定位

Week 1 的 `vector add` 解决的是一类最容易并行化的问题：

```text
c[i] = a[i] + b[i]
```

每个线程只负责一个输出元素，线程之间没有数据依赖。Week 2 的 `parallel reduction` 则进入 CUDA 优化里更核心的一类问题：

```text
sum = input[0] + input[1] + ... + input[n - 1]
```

多个线程必须共同产生一个结果，所以项目重点从“能启动 kernel”变成“能组织线程协作”：

```text
CMake 构建
→ 复用 CUDA 错误检查和 RAII 显存管理
→ CPU reference 和测试工具
→ naive reduce baseline
→ shared memory block-level reduce
→ warp shuffle reduce
→ correctness test
→ enhanced benchmark
→ Nsight Compute profiling 模板
→ Agent workflow 约束
```

它对应 [[Week 2 - Reduction + Profiling]] 的核心目标：

1. 理解 reduction 为什么不能像 `vector add` 一样完全独立并行。
2. 理解 partial sum、block 内归约和最终汇总。
3. 对比 global memory、shared memory、register/warp shuffle 的通信成本。
4. 用 CUDA event benchmark 输出 avg/min/max 和 effective bandwidth。
5. 用 Nsight Compute 观察 memory throughput、occupancy 和 warp stall reason。

一句话概括：

```text
这个项目是从“会写 CUDA kernel”进入“会解释 CUDA kernel 为什么快或慢”的第一关。
```

> [!note] 当前实现边界
> 这个 week02 工程的三种 reduce 实现都是“GPU 每个 block 输出一个 partial sum，CPU 侧再累加 partial sums”。它已经足够用于学习 block-level reduction 和 profiling，但还不是完整的多级 GPU-only reduction。

![[图片/SVG/CUDA Week 2 Parallel Reduction 项目解析-01.svg|1002]]

---

## 2. 目录结构

项目结构如下：

```text
week02/
├── CMakeLists.txt
├── CLAUDE.md
├── Makefile
├── README.md
├── include/
│   ├── cuda_check.cuh
│   ├── device_buffer.cuh
│   └── test_utils.cuh
├── src/
│   ├── reduce_naive.cu
│   ├── reduce_naive.cuh
│   ├── reduce_shared.cu
│   ├── reduce_shared.cuh
│   ├── reduce_shuffle.cu
│   └── reduce_shuffle.cuh
├── tests/
│   ├── test_reduce_naive.cu
│   ├── test_reduce_shared.cu
│   └── test_reduce_shuffle.cu
├── benchmarks/
│   └── bench_reduce.cu
└── docs/
    ├── exercises.md
    ├── profiling.md
    └── questions.md
```

各部分职责：

| 路径 | 职责 |
|---|---|
| `CMakeLists.txt` | 定义 `ParallelReduce` 项目、`reduce_lib` 库、三个测试目标和 benchmark 目标 |
| `Makefile` | 包装 configure、build、test、bench、profile 和 clean 命令 |
| `CLAUDE.md` | 约束 Agent 的实现风格、安全边界和 benchmark 正确性要求 |
| `include/cuda_check.cuh` | 复用 Week 1 的 CUDA Runtime API 错误检查 |
| `include/device_buffer.cuh` | 复用 Week 1 的 RAII device memory 封装 |
| `include/test_utils.cuh` | 生成输入、CPU reference 求和、结果校验 |
| `src/reduce_naive.*` | Day 1 baseline：每个 block 只有线程 0 串行累加 |
| `src/reduce_shared.*` | Day 2 shared memory：block 内二分树形归约 |
| `src/reduce_shuffle.*` | Day 3 warp shuffle：warp 内寄存器通信加跨 warp shared memory |
| `tests/` | 每个实现一份 correctness test |
| `benchmarks/bench_reduce.cu` | 三种 kernel 的统一 benchmark，输出 avg/min/max/bandwidth/status |
| `docs/questions.md` | Week 2 必答问题和参考答案 |
| `docs/exercises.md` | 渐进式练习，从 baseline 到 CUB 对比 |
| `docs/profiling.md` | Nsight Compute profiling 记录模板 |

这种结构延续了 [[CUDA Week 1 Hello World 项目解析]] 的工程风格：核心实现、测试、benchmark、文档和 Agent 约束分开，但测试与 benchmark 都链接同一份 `reduce_lib`，避免“测试一个实现，benchmark 另一个实现”。

---

## 3. 构建系统：`CMakeLists.txt`

### 3.1 项目语言与标准

```cmake
cmake_minimum_required(VERSION 3.18)

project(ParallelReduce LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)
```

**关键点**：

- `LANGUAGES CXX CUDA` 让 CMake 同时处理 host 侧 C++ 和 device 侧 CUDA。
- host 侧使用 C++20，CUDA 侧使用 CUDA C++17。
- 这和 Week 1 的项目模板一致，只是项目名换成 `ParallelReduce`。

CUDA 项目的 CMake 基础可以回看 [[14.3 CMake基础]]，CUDA Runtime 使用方式可以回看 [[3.2 CUDA Runtime 进阶|CUDA Runtime 进阶]]。

### 3.2 核心库：`reduce_lib`

```cmake
add_library(reduce_lib
    src/reduce_naive.cu
    src/reduce_shared.cu
    src/reduce_shuffle.cu
)
target_include_directories(reduce_lib PUBLIC include src)
```

这里把三种 reduce 实现统一做成一个库目标 `reduce_lib`。

这样设计的好处是：

1. 三种实现共享同一套 include 路径。
2. correctness test 和 benchmark 都链接同一个库。
3. 新增 reduce 版本时，只需要把 `.cu` 加入 `reduce_lib`，再补测试和 benchmark 项。

### 3.3 测试目标

```cmake
foreach(day naive shared shuffle)
    add_executable(test_reduce_${day} tests/test_reduce_${day}.cu)
    target_link_libraries(test_reduce_${day} PRIVATE reduce_lib)
endforeach()
```

这个 `foreach` 会生成三个测试程序：

| 目标 | 测试对象 |
|---|---|
| `test_reduce_naive` | `reduce_naive<T>` |
| `test_reduce_shared` | `reduce_shared<T>` |
| `test_reduce_shuffle` | `reduce_shuffle<T>` |

这比手写三个重复的 `add_executable` 更紧凑，但没有引入复杂抽象。它适合当前项目，因为三个测试文件命名严格一致。

### 3.4 Benchmark 目标

```cmake
add_executable(bench_reduce benchmarks/bench_reduce.cu)
target_link_libraries(bench_reduce PRIVATE reduce_lib)
```

`bench_reduce` 同时 benchmark 三种实现。它不是单独复制 kernel 代码，而是调用 `reduce_lib` 暴露的模板函数，因此 benchmark 结果和测试覆盖的是同一批实现。

---

## 4. Makefile：命令入口

`Makefile` 把常用命令包装成更短的入口：

```makefile
BUILD_DIR := build
CUDA_ARCH ?= 75

configure:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_CUDA_ARCHITECTURES=$(CUDA_ARCH)

build: configure
	cmake --build $(BUILD_DIR)
```

**关键点**：

- 默认构建目录是 `build`。
- 默认 CUDA 架构是 `75`，对应 `sm_75`。
- 可以用 `make build CUDA_ARCH=86` 改成其他 GPU 架构。

常用命令：

```bash
make build
make test
make test-naive
make test-shared
make test-shuffle
make bench
make profile-naive
make profile-shared
make profile-shuffle
make clean
```

其中：

| 命令 | 作用 |
|---|---|
| `make test` | 构建后依次运行三种 correctness test |
| `make bench` | 构建后运行统一 benchmark |
| `make profile-*` | 用 `ncu --set full` 对指定测试程序做 Nsight Compute profiling |
| `make clean` | 删除 `build` 目录 |

`profile-*` 使用测试程序作为 profiling 入口，而不是 benchmark 程序。这样做更容易聚焦单个 kernel，但要注意 profiling 本身会引入额外开销，不能直接把 Nsight 时间和 benchmark 时间混为一谈。

---

## 5. Agent 约束：`CLAUDE.md`

Week 2 的 `CLAUDE.md` 继续把 Agent 辅助开发纳入工程边界。它规定项目目标是三种 progressively optimized reduction：

```text
Naive reduce
→ Shared memory reduce
→ Warp shuffle reduce
```

代码风格约束包括：

1. host 侧使用 Modern C++。
2. CUDA 资源使用 RAII，例如 `DeviceBuffer`。
3. kernel 保持小而可测。
4. correctness test 和 benchmark 分离。
5. benchmark 输出必须包含 correctness status。
6. 注释解释 CUDA 概念，而不是解释显而易见的 C++ 语法。
7. 所有 reduce 函数共享同一种模板 API 形状。

安全约束包括：

1. 不擅自安装 CUDA Toolkit、改 GPU driver、改系统 PATH。
2. 不擅自删除 benchmark 数据。
3. 不运行破坏性 git 命令。
4. 不能为了让 benchmark 数字更好看而跳过 correctness check。

这里最重要的是第 4 点。Reduction 写错时可能“更快”，因为它少算了数据，所以 benchmark 必须始终保留正确性检查。这一点和 [[3.5.1 CUDA Week 2 辅助笔记 - Benchmark + Profiling]] 的要求一致。

---

## 6. 测试工具：`include/test_utils.cuh`

### 6.1 输入生成：`generate_input<T>`

`generate_input<T>(n)` 用固定随机种子生成测试数据：

```cpp
std::mt19937 rng(42);
```

对于浮点类型：

```cpp
std::uniform_real_distribution<T> dist(0.0, 1.0);
```

对于整数类型：

```cpp
std::uniform_int_distribution<T> dist(0, 99);
```

固定种子的价值是可复现：同样的 `n` 和类型，每次生成的输入一致。这样 benchmark 和 correctness test 出问题时，定位成本更低。

### 6.2 CPU reference：`sum_reference<T>`

`sum_reference<T>` 是 correctness test 的基准。对于浮点类型，它用 `double` 累加：

```cpp
double acc = 0.0;
```

这样做是为了减少 CPU reference 自身的精度损失。GPU 并行归约会改变加法顺序，FP32 加法不满足严格结合律，所以 GPU 结果和 CPU 串行结果可能有微小差异。

对于整数类型，它直接用 `T acc = 0` 精确累加。

### 6.3 结果检查：`check_result<T>`

浮点类型使用相对误差：

```text
rel_err = abs(gpu_result - cpu_result) / max(abs(cpu_result), 1e-6)
pass    = rel_err < 1e-3
```

整数类型使用精确匹配：

```text
pass = gpu_result == cpu_result
```

这体现了 Week 2 的正确性判断原则：

| 类型 | 判断方式 | 原因 |
|---|---|---|
| `float` | 允许 `1e-3` 相对误差 | 并行归约改变加法顺序，会产生舍入差异 |
| `int` | 必须完全相等 | 整数加法在不溢出的前提下应精确一致 |

这部分可以和 [[3.5 CUDA Week 2 前置知识 - Reduction + Profiling]] 中的 FP32 误差讨论一起看。

---

## 7. 对外 API：三个 reduce 函数

三个实现的头文件都暴露同一种模板 API：

```cpp
template <typename T>
T reduce_naive(const T* d_input, int n, int threads_per_block = 256);

template <typename T>
T reduce_shared(const T* d_input, int n, int threads_per_block = 256);

template <typename T>
T reduce_shuffle(const T* d_input, int n, int threads_per_block = 256);
```

参数含义：

| 参数 | 含义 |
|---|---|
| `d_input` | GPU device memory 上的输入数组 |
| `n` | 输入元素数量 |
| `threads_per_block` | 每个 block 的线程数，默认 256 |
| 返回值 | 最终 sum，当前实现会在 CPU 侧汇总 block partial sums |

这种 API 有两个特点：

1. 测试和 benchmark 可以用同一套调用方式比较三种实现。
2. `threads_per_block` 暴露出来，方便后续做 block size 和 occupancy 实验。

当前 `.cu` 文件显式实例化了：

```cpp
template float reduce_xxx<float>(const float*, int, int);
template int reduce_xxx<int>(const int*, int, int);
```

所以当前工程实际覆盖 `float` 和 `int`。如果要扩展到 `double`，需要补显式实例化、测试和 benchmark 矩阵。

---

## 8. Naive Reduce：`src/reduce_naive.cu`

![[图片/SVG/CUDA Week 2 Parallel Reduction 项目解析-03.svg|921]]

### 8.1 Kernel 思路

Naive 版本每个 block 负责一段连续输入：

```text
block_start = blockIdx.x * blockDim.x
block_end   = min(block_start + blockDim.x, n)
```

但真正做累加的只有线程 0：

```cpp
if (threadIdx.x == 0) {
    T sum = 0;
    for (int i = block_start; i < block_end; ++i) {
        sum += input[i];
    }
    partial_sums[blockIdx.x] = sum;
}
```

这意味着如果 `threads_per_block = 256`，每个 block 里 255 个线程都没有参与计算。

### 8.2 为什么还要写这个低效版本

Naive 版本不是为了快，而是为了建立 baseline。

它暴露了 reduction 的第一层问题：

```text
只把数据拆成 block 还不够；
如果 block 内没有并行协作，GPU 的线程资源会大量闲置。
```

性能上，它可能出现这些特征：

1. block 内并行度极低。
2. 每个 block 的线程 0 串行读 global memory。
3. 其他线程只承担 launch 和调度成本，没有贡献有效工作。
4. Nsight Compute 中可能看到 global memory 等待相关 stall。

### 8.3 Host 侧流程

`reduce_naive<T>` 的 host 封装流程是：

```text
检查 n <= 0
→ 计算 num_blocks
→ 分配 d_partial
→ 启动 reduce_naive_kernel
→ cudaGetLastError
→ cudaDeviceSynchronize
→ 把 partial sums 拷回 CPU
→ CPU 侧最终累加
```

核心 grid 计算：

```cpp
int num_blocks = (n + threads_per_block - 1) / threads_per_block;
```

这和 Week 1 的 vector add 一样，用向上取整处理非 block 对齐长度。

---

## 9. Shared Memory Reduce：`src/reduce_shared.cu`

### 9.1 Kernel 思路

Shared memory 版本让 block 内所有线程都参与第一阶段加载：

```cpp
sdata[tid] = (gid < n) ? input[gid] : T(0);
__syncthreads();
```

然后在 shared memory 中做二分树形归约：

```cpp
for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
    if (tid < stride) {
        sdata[tid] += sdata[tid + stride];
    }
    __syncthreads();
}
```

最后由线程 0 写出该 block 的 partial sum：

```cpp
if (tid == 0) {
    partial_sums[blockIdx.x] = sdata[0];
}
```

### 9.2 动态 shared memory

kernel 使用动态 shared memory：

```cpp
extern __shared__ char shared_raw[];
T* sdata = reinterpret_cast<T*>(shared_raw);
```

host 侧 launch 时传入 shared memory 字节数：

```cpp
size_t shared_bytes = threads_per_block * sizeof(T);
reduce_shared_kernel<<<num_blocks, threads_per_block, shared_bytes>>>(...);
```

这样 `float` 和 `int` 可以复用同一个模板 kernel，不需要写固定大小的 shared array。

### 9.3 为什么需要 `__syncthreads()`

树形归约是分轮进行的。第 k 轮中，线程 `tid` 会读取 `sdata[tid + stride]`，而这个位置可能刚刚被上一轮的另一个线程写过。

如果不同步，就可能出现：

```text
线程 A 还没写完上一轮结果
线程 B 已经读这个位置进入下一轮
→ B 读到旧值或未定义的中间状态
```

`__syncthreads()` 是 block 级 barrier，只同步同一个 block 内的线程，不能跨 block 同步。这个限制正是 reduction 通常要分阶段处理的原因之一。

### 9.4 性能意义

Shared memory 版本相对 naive 版本的核心改进是：

| 维度 | Naive | Shared Memory |
|---|---|---|
| block 内并行度 | 只有线程 0 工作 | 多个线程共同归约 |
| 中间数据位置 | 主要依赖 global memory 读取 | shared memory 内反复读写 |
| 同步 | 无 block 内协作 | 每轮需要 `__syncthreads()` |
| 主要学习点 | baseline 低效 | block-level cooperation |

它不保证在所有输入规模下都更快。对于很小的 `n`，kernel launch overhead 可能盖过 shared memory 优势；对于更复杂的访问模式，还要考虑 bank conflict、occupancy 和 shared memory 资源占用。

---

## 10. Warp Shuffle Reduce：`src/reduce_shuffle.cu`

### 10.1 Warp 内归约

Warp shuffle 版本先定义一个 device 函数：

```cpp
template <typename T>
__device__ T warp_reduce_sum(T val) {
    for (int offset = warpSize / 2; offset > 0; offset >>= 1) {
        val += __shfl_down_sync(0xffffffff, val, offset);
    }
    return val;
}
```

`warpSize` 通常是 32，所以 offset 依次是：

```text
16 → 8 → 4 → 2 → 1
```

每一轮都让 lane `i` 读取 lane `i + offset` 的寄存器值并相加。最终每个 warp 的 lane 0 得到该 warp 的 sum。

### 10.2 为什么 shuffle 快

Shared memory reduce 的线程通信路径大致是：

```text
thread register
→ shared memory
→ __syncthreads()
→ another thread reads shared memory
```

Warp shuffle 的路径更短：

```text
thread register
→ warp shuffle
→ another lane register
```

在同一个 warp 内，线程以 lockstep 方式执行，所以 warp 内 shuffle 不需要 block-level `__syncthreads()`。这就是它比纯 shared memory 归约更轻的地方。

### 10.3 两层归约结构

`reduce_shuffle_kernel` 不是只用 shuffle 完成整个 block。它分两层：

```text
第一层：每个 warp 内用 __shfl_down_sync 归约
第二层：每个 warp 的 lane 0 写入 shared memory
第三层：第一个 warp 读取这些 warp sums，再用 shuffle 归约
```

代码中的关键变量：

```cpp
int lane = tid % warpSize;
int warp_id = tid / warpSize;
```

每个 warp 的 lane 0 写入 shared memory：

```cpp
if (lane == 0) {
    sdata[warp_id] = val;
}
__syncthreads();
```

然后第一个 warp 做跨 warp 汇总：

```cpp
int num_warps = blockDim.x / warpSize;
val = (tid < num_warps) ? sdata[tid] : T(0);

if (warp_id == 0) {
    val = warp_reduce_sum(val);
}
```

### 10.4 资源使用

Shared memory 版本需要：

```text
threads_per_block * sizeof(T)
```

Shuffle 版本只需要：

```text
(threads_per_block / 32) * sizeof(T)
```

默认 `threads_per_block = 256` 时：

| 类型 | Shared Memory Reduce | Warp Shuffle Reduce |
|---|---:|---:|
| `float` | `256 * 4 = 1024` bytes | `8 * 4 = 32` bytes |
| `int` | `256 * 4 = 1024` bytes | `8 * 4 = 32` bytes |

这能减少 shared memory 资源占用，也减少 block 内 barrier 的数量。

### 10.5 实现前提

当前实现默认 `threads_per_block` 是 32 的整数倍，因为：

```cpp
int num_warps = threads_per_block / 32;
```

如果未来允许任意 block size，需要额外处理最后一个不完整 warp。当前项目没有做这个泛化，是合理的入门工程选择，因为默认值 256 已经满足学习目标。

---

## 11. Correctness Test：`tests/`

三个测试文件结构基本一致：

```text
test_reduce_naive.cu
test_reduce_shared.cu
test_reduce_shuffle.cu
```

每个测试都会对 `float` 和 `int` 各跑一组 size：

| `n` | 目的 |
|---:|---|
| `1` | 单元素边界 |
| `1024` | 刚好是 4 个 block，默认 256 threads/block |
| `1 << 20` | 大规模输入 |
| `1000` | 非 block size 整数倍 |

测试流程：

```text
generate_input<T>(n)
→ CPU sum_reference<T>
→ DeviceBuffer<T> 分配输入显存
→ cudaMemcpy HostToDevice
→ 调用 reduce_xxx<T>
→ check_result<T>
→ 汇总 all_pass
```

这组测试覆盖了 Week 2 reduction 最容易出错的三类问题：

1. 最后一个 block 越界读取。
2. 浮点数并行归约误差。
3. 大规模数据下 partial sums 汇总错误。

> [!warning] Benchmark 之前必须先过 correctness
> Reduction 的错误经常表现为“速度更快”，因为少做了加法或跳过了边界数据。这个项目把 correctness test 和 benchmark 分开，但 benchmark 里仍保留 correctness status，这是正确做法。

---

## 12. Benchmark：`benchmarks/bench_reduce.cu`

![[图片/SVG/CUDA Week 2 Parallel Reduction 项目解析-02.svg|868]]

### 12.1 Benchmark 目标

`bench_reduce.cu` 对三种实现做统一性能对比：

```text
Naive
Shared
Shuffle
```

并分别测试：

```text
float
int
```

输出指标包括：

| 指标 | 含义 |
|---|---|
| `Avg(ms)` | 多次重复运行平均耗时 |
| `Min(ms)` | 最快一次耗时 |
| `Max(ms)` | 最慢一次耗时 |
| `BW(GB/s)` | effective bandwidth |
| `Status` | benchmark 内部 correctness check 是否通过 |

### 12.2 Warmup 和 repeat

`bench_one` 默认：

```cpp
int warmup = 5;
int repeat = 20;
```

Warmup 不计时，用来减少冷启动、频率变化、cache 状态等因素对结果的影响。之后每次 timed run 都单独创建 CUDA event pair：

```text
cudaEventCreate(start/stop)
→ cudaEventRecord(start)
→ 调用 reduce 函数
→ cudaEventRecord(stop)
→ cudaEventSynchronize(stop)
→ cudaEventElapsedTime
→ cudaEventDestroy
```

这里计到的是整个 `reduce_xxx` host wrapper 的 GPU 时间线范围。由于当前 `reduce_xxx` 内部包含 kernel、同步、partial sums 拷回和 CPU 汇总，benchmark 数字不等同于纯 kernel-only 时间。这一点后续做 profiling 时要分清。

### 12.3 测试规模

benchmark 使用三个 size：

| Size | `n` | 观察重点 |
|---|---:|---|
| `1K` | `1024` | 小规模 launch overhead |
| `1M` | `1 << 20` | 常规吞吐 |
| `64M` | `1 << 26` | 大规模 memory bandwidth |

小规模下，kernel launch overhead 可能占主导；大规模下，global memory bandwidth 和归约组织方式更能体现差异。

### 12.4 Effective bandwidth

项目中的带宽计算是：

```text
bandwidth_GB_s = n * sizeof(T) / avg_ms / 1e6
```

换成更直观的单位：

```text
bytes   = n * sizeof(T)
seconds = avg_ms * 1e-3
GB/s    = bytes / seconds / 1e9
```

对于 sum reduction，每个元素至少从 global memory 读一次，所以 effective bandwidth 是一个核心指标。但要注意，当前 wrapper 还包含 partial sums 拷回和 CPU 汇总，因此它更像“这个 reduce API 的有效吞吐”，不是 Nsight 里单个 kernel 的 DRAM throughput。

### 12.5 输出格式

benchmark 表头固定为：

```text
N       | Kernel   |   Avg(ms) |   Min(ms) |   Max(ms) |  BW(GB/s) | Status
```

这比只输出一个耗时更适合做性能分析：

1. `Avg` 看总体水平。
2. `Min` 接近最佳情况。
3. `Max` 暴露抖动。
4. `BW` 方便和 GPU 峰值内存带宽比较。
5. `Status` 防止错误实现混入性能对比。

这正好对应 [[3.5.1 CUDA Week 2 辅助笔记 - Benchmark + Profiling]] 中的 benchmark matrix 要求。

---

## 13. Nsight Compute Profiling：`docs/profiling.md`

Week 2 不只要求“哪个版本更快”，还要求解释“为什么更快”。`docs/profiling.md` 就是为这个目的准备的证据模板。

### 13.1 Profiling 命令

Makefile 提供三个入口：

```bash
make profile-naive
make profile-shared
make profile-shuffle
```

底层命令形式是：

```bash
ncu --set full ./build/test_reduce_naive
ncu --set full ./build/test_reduce_shared
ncu --set full ./build/test_reduce_shuffle
```

如果要保存报告，可以参考 docs 里的形式：

```bash
ncu --set full --target-processes all -o report_naive ./build/test_reduce_naive
```

### 13.2 模板记录哪些指标

每个 kernel 变体都要记录：

| 指标 | 作用 |
|---|---|
| Kernel time | 单个 kernel 的实际执行时间 |
| Effective bandwidth | 与 benchmark 口径对照 |
| Peak memory bandwidth | 计算带宽利用率 |
| Memory Throughput | Nsight 观测到的内存吞吐 |
| Achieved Occupancy | 实际活跃 warp 比例 |
| Warp Stall Reasons | 判断瓶颈来自内存、同步还是调度 |
| Registers per Thread | 判断寄存器是否限制 occupancy |
| Shared Memory per Block | 判断 shared memory 是否限制 occupancy |
| L1/L2 Hit Rate | 判断 cache 行为 |

可以和 [[3.4 CUDA Nsight Compute 指标速查|CUDA Nsight Compute 指标速查]] 一起使用。

### 13.3 三种 kernel 的预期观察点

| Kernel | 重点看什么 | 可能解释 |
|---|---|---|
| `reduce_naive_kernel` | Long Scoreboard、memory throughput、warp 活跃情况 | 线程 0 串行读 global memory，block 内并行度低 |
| `reduce_shared_kernel` | `__syncthreads()` 相关 stall、shared memory usage、occupancy | block 内合作增加，但每轮 barrier 有成本 |
| `reduce_shuffle_kernel` | register/shuffle 指令、较少 shared memory、跨 warp同步 | warp 内通信更轻，只在跨 warp 阶段用少量 shared memory |

这部分的学习价值是把“代码结构”映射到“性能证据”：

```text
代码里用了什么通信方式
→ Nsight 里应该看到什么资源消耗
→ benchmark 数字为什么会这样
```

---

## 14. README 推荐阅读顺序

建议按这个顺序读 week02：

1. `README.md`：先理解本周目标、三种归约策略和构建命令。
2. `include/test_utils.cuh`：确认 correctness reference 和误差口径。
3. `src/reduce_naive.cu`：建立低效 baseline。
4. `src/reduce_shared.cu`：理解 shared memory 和 `__syncthreads()`。
5. `src/reduce_shuffle.cu`：理解 warp-level primitive。
6. `tests/test_reduce_*.cu`：确认测试矩阵覆盖哪些边界。
7. `benchmarks/bench_reduce.cu`：看 benchmark 是否包含 warmup、repeat、bandwidth 和 correctness status。
8. `docs/questions.md`：用问答检查自己是否理解原理。
9. `docs/profiling.md`：跑 Nsight Compute 后填证据。
10. `docs/exercises.md`：做进一步优化练习。

---

## 15. Week 2 验收目标

这个项目跑通后，应该能回答以下问题：

1. Reduction 和 vector add 的并行模式有什么不同？
2. 为什么每个 block 先输出 partial sum？
3. 为什么一个 kernel 内通常不能直接跨 block 做全局同步？
4. Naive reduce 为什么低效？
5. Shared memory reduce 为什么需要 `__syncthreads()`？
6. Shared memory 为什么不等于一定更快？
7. Warp shuffle 为什么能减少 shared memory 和同步成本？
8. FP32 reduction 为什么允许相对误差？
9. Benchmark 为什么要有 warmup、repeat、avg/min/max？
10. Effective bandwidth 为什么适合评价 reduction？
11. Nsight Compute 中 memory throughput、occupancy、warp stall reason 分别说明什么？

对应交付物：

| 交付物 | 验收方式 |
|---|---|
| 三种 reduce 实现 | `make test` 全部 PASS |
| Benchmark | `make bench` 输出三种 kernel、多种 size、两种类型和 correctness status |
| Profiling 模板 | `docs/profiling.md` 能记录三种 kernel 的 Nsight 指标 |
| 学习复盘 | 能用指标解释 naive、shared、shuffle 的差异 |

---

## 16. 下一步学习建议

### 16.1 先跑 correctness，再跑 benchmark

建议顺序：

```bash
make test
make bench
```

如果 correctness 不通过，不要看 benchmark 数字。Reduction 的性能分析必须建立在正确结果上。

### 16.2 再做 Nsight Compute

正确性和 benchmark 都稳定后，再运行：

```bash
make profile-naive
make profile-shared
make profile-shuffle
```

把结果填入 `docs/profiling.md`，重点记录：

1. GPU 型号和 CUDA 版本。
2. `n`、blockDim、gridDim。
3. kernel time。
4. memory throughput。
5. achieved occupancy。
6. top stall reasons。
7. register count 和 shared memory per block。

不要只写“shuffle 更快”，要写“shuffle 更快的证据是什么”。

### 16.3 进入 Week 3

Week 2 学的是 reduction 中的线程协作和 profiling。下一阶段 [[Week 3 - Transpose + Memory Coalescing]] 会把重点转到 global memory 访问模式：

```text
Week 2: 线程如何协作汇总数据
Week 3: 线程如何合并访问 global memory
```

这两个主题组合起来，就是后续 tiled matmul、RMSNorm、Softmax、Attention kernel 的基础。

---

## 17. 关键要点总结

1. Week 2 的项目核心不是“求一个 sum”，而是通过 sum reduction 学会 GPU 线程协作。
2. Naive 版本故意低效，用来证明 block 内没有并行归约时 GPU 资源会被浪费。
3. Shared memory 版本把 block 内数据放到 on-chip memory 中做树形归约，但需要 `__syncthreads()` 保证每轮结果可见。
4. Warp shuffle 版本用寄存器级 lane 通信完成 warp 内归约，再用少量 shared memory 做跨 warp 汇总。
5. 当前工程的最终汇总在 CPU 侧完成，因此 benchmark 数字要按 API 整体开销理解，不要误当成纯 kernel-only 时间。
6. Correctness test 覆盖 `float`、`int`、单元素、大规模和非 block 对齐长度。
7. Benchmark 同时输出 avg/min/max/bandwidth/status，适合观察 launch overhead、吞吐和抖动。
8. Nsight Compute 的价值是把“我觉得它快”变成“指标支持它为什么快”。

---

## 关联知识

- [[CUDA Week 1 Hello World 项目解析]]
- [[Week 2 - Reduction + Profiling]]
- [[3.5 CUDA Week 2 前置知识 - Reduction + Profiling]]
- [[3.5.1 CUDA Week 2 辅助笔记 - Benchmark + Profiling]]
- [[3.4 CUDA Nsight Compute 指标速查|CUDA Nsight Compute 指标速查]]
- [[3.2 CUDA Runtime 进阶|CUDA Runtime 进阶]]
- [[Week 3 - Transpose + Memory Coalescing]]
- [[14.3 CMake基础]]
- [[独享智能指针]]

---

## 参考

- [CUDA_learning/week02](https://github.com/hosendovebelva-boop/CUDA_learning/tree/main/week02)
- [NVIDIA Parallel Reduction Whitepaper](https://developer.download.nvidia.com/assets/cuda/files/reduction.pdf)
- [CUB Library - DeviceReduce](https://nvlabs.github.io/cub/structcub_1_1_device_reduce.html)
