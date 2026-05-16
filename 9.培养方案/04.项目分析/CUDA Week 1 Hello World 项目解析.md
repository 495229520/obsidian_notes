---
title: CUDA Week 1 Hello World 项目解析
date: 2026-05-08
tags:
  - infra
  - CUDA
  - 项目分析
aliases:
  - CUDA Week1 项目解析
  - CUDA Hello World 工程解析
status: active
---

# CUDA Week 1 Hello World 项目解析

> 这个项目是 `CUDA_learning/week01` 的 Week 1 CUDA 入门工程：用 `vector add` 打通 CUDA 项目模板、RAII 显存管理、kernel launch、correctness test、CUDA event benchmark 和 Agent workflow 约束。

项目地址：[CUDA_learning/week01](https://github.com/hosendovebelva-boop/CUDA_learning/tree/main/week01)

---

## 1. 项目定位

这个项目不是简单地写一个单文件 `.cu` 程序，而是把 CUDA Hello World 做成一个最小工程闭环：

```text
CMake 构建
→ CUDA 错误检查
→ RAII 管理 GPU 显存
→ vector add kernel
→ correctness test
→ CUDA event benchmark
→ Agent workflow 约束
```

它对应 [[Week 1 - CUDA + Agent workflow]] 的核心目标：

1. 用 CMake 组织 CUDA 项目。
2. 用 `vector add` 验证 CUDA 执行模型。
3. 用 correctness test 证明结果正确。
4. 用 CUDA event benchmark 证明性能。
5. 用 `CLAUDE.md` 约束 Agent 辅助开发边界。

一句话概括：

```text
这个项目是 CUDA 入门的“工程化 Hello World”。
```

---

## 2. 目录结构

项目结构如下：

```text
week01/
├── CMakeLists.txt
├── CLAUDE.md
├── Makefile
├── README.md
├── include/
│   ├── cuda_check.cuh
│   └── device_buffer.cuh
├── src/
│   ├── vector_add.cu
│   └── vector_add.cuh
├── tests/
│   └── test_vector_add.cu
├── benchmarks/
│   └── bench_vector_add.cu
└── docs/
```

各部分职责：

| 路径 | 职责 |
|---|---|
| `CMakeLists.txt` | 定义 CUDA/C++ 构建规则 |
| `Makefile` | 包装常用构建、测试、benchmark 命令 |
| `CLAUDE.md` | 约束 Agent 可做、需确认、禁止的操作 |
| `include/cuda_check.cuh` | 统一 CUDA Runtime API 错误检查 |
| `include/device_buffer.cuh` | 用 RAII 管理 GPU device memory |
| `src/vector_add.cuh` | 对外声明 host API 和 kernel launch 封装 |
| `src/vector_add.cu` | 实现 kernel、launch 函数和 host 侧完整流程 |
| `tests/test_vector_add.cu` | correctness test，只验证结果是否正确 |
| `benchmarks/bench_vector_add.cu` | CUDA event benchmark，测 kernel 时间和有效带宽 |

这种结构的价值在于：测试、benchmark 和核心实现分离，但复用同一份 `vector_add_lib`，避免“测试一份代码、benchmark 另一份代码”。

---

## 3. 构建系统：`CMakeLists.txt`

### 3.1 启用 CUDA 语言

```cmake
cmake_minimum_required(VERSION 3.18)

project(cuda01 LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)
```

**关键点**：

- `LANGUAGES CXX CUDA` 表示项目同时包含普通 C++ host code 和 CUDA device code。
- `.cu` 文件会交给 CUDA 编译链处理。
- host 侧使用 C++20，CUDA 侧使用 CUDA C++17，适合作为入门项目。

CUDA 项目的 CMake 基础可以回看 [[14.3 CMake基础]]。

### 3.2 核心库与可执行文件

```cmake
add_library(vector_add_lib
    src/vector_add.cu
)

target_include_directories(vector_add_lib
    PUBLIC
        include
        src
)
```

这里把 `src/vector_add.cu` 做成库 `vector_add_lib`。这样测试程序和 benchmark 程序都链接同一份实现。

```cmake
add_executable(test_vector_add
    tests/test_vector_add.cu
)

target_link_libraries(test_vector_add
    PRIVATE
        vector_add_lib
)

add_executable(bench_vector_add
    benchmarks/bench_vector_add.cu
)

target_link_libraries(bench_vector_add
    PRIVATE
        vector_add_lib
)
```

**设计思路**：

- `test_vector_add` 只负责正确性测试。
- `bench_vector_add` 只负责性能测量。
- 两者都复用 `vector_add_lib`，保证测试和 benchmark 指向同一份 kernel 实现。

---

## 4. CUDA 错误检查：`include/cuda_check.cuh`

CUDA Runtime API 是 C 风格接口，很多函数返回 `cudaError_t`：

```cpp
cudaMalloc(...)
cudaMemcpy(...)
cudaEventCreate(...)
cudaFree(...)
```

如果不检查返回值，错误可能延迟到后续同步或拷贝阶段才暴露，定位成本很高。

项目中封装为：

```cpp
inline void check_cuda(cudaError_t status, const char* expression, const char* file, int line) {
    if (status == cudaSuccess) {
        return;
    }

    throw std::runtime_error(
        std::string("CUDA error: ") + cudaGetErrorString(status) +
        "\n  expression: " + expression +
        "\n  location: " + file + ":" + std::to_string(line));
}

#define CUDA_CHECK(expr) check_cuda((expr), #expr, __FILE__, __LINE__)
```

**关键点**：

- `cudaGetErrorString(status)` 把错误码转换成人类可读信息。
- `#expr` 记录失败的表达式。
- `__FILE__` 和 `__LINE__` 记录错误位置。
- `CUDA_CHECK(expr)` 把 C 风格错误码转换成 C++ 异常。

使用方式：

```cpp
CUDA_CHECK(cudaMemcpy(d_a.get(), a.data(), d_a.bytes(), cudaMemcpyHostToDevice));
CUDA_CHECK(cudaDeviceSynchronize());
```

这符合 CUDA 入门阶段的基本要求：每次 Runtime API 调用都应该能暴露错误，而不是静默失败。

---

## 5. RAII 显存封装：`include/device_buffer.cuh`

### 5.1 为什么需要 `DeviceBuffer`

裸 CUDA 写法通常是：

```cpp
float* d_a = nullptr;
cudaMalloc(&d_a, n * sizeof(float));

// 使用 d_a

cudaFree(d_a);
```

这有三个问题：

1. 中途 `return` 或抛异常时容易忘记 `cudaFree`。
2. 多个指针误指向同一块显存时可能 double free。
3. 业务逻辑里到处散落 `cudaMalloc/cudaFree`，维护成本高。

所以项目用 `DeviceBuffer<T>` 做 RAII 封装。RAII 的核心思想可以回看 [[独享智能指针]]：

```text
构造函数获取资源
析构函数释放资源
对象生命周期就是资源生命周期
```

### 5.2 构造函数：申请 GPU 显存

```cpp
template <typename T>
class DeviceBuffer {
public:
    explicit DeviceBuffer(std::size_t count) : count_(count) {
        if (count_ > 0) {
            CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&ptr_), count_ * sizeof(T)));
        }
    }
```

**关键点**：

- `T` 让这个类能管理 `float`、`int`、`double` 等不同类型的 device memory。
- `count_ * sizeof(T)` 自动计算字节数。
- `cudaMalloc` 需要 `void**`，所以这里使用 `reinterpret_cast<void**>(&ptr_)`。
- `count_ > 0` 避免申请 0 字节显存。

可以把它理解成：

```text
DeviceBuffer<float> d_a(n)
≈ 在 GPU 上申请 n 个 float 的显存
```

### 5.3 析构函数：释放 GPU 显存

```cpp
    ~DeviceBuffer() {
        if (ptr_ != nullptr) {
            cudaFree(ptr_);
        }
    }
```

**关键点**：

- 对象离开作用域时自动调用析构函数。
- 析构函数中释放 device memory。
- 析构函数不抛异常，因为析构阶段如果抛异常可能导致 `std::terminate`。

这让 GPU 显存也拥有类似 `std::unique_ptr` 的自动生命周期管理。

### 5.4 禁止拷贝：防止 double free

```cpp
    DeviceBuffer(const DeviceBuffer&) = delete;
    DeviceBuffer& operator=(const DeviceBuffer&) = delete;
```

如果允许拷贝，两个对象会拥有同一块 GPU 显存：

```text
a.ptr_ ──┐
         ├── GPU memory
b.ptr_ ──┘
```

作用域结束时两个对象都会 `cudaFree(ptr_)`，导致重复释放。因此 `DeviceBuffer` 必须是独占所有权，和 `std::unique_ptr` 一样不可复制。

### 5.5 支持移动：转移所有权

```cpp
    DeviceBuffer(DeviceBuffer&& other) noexcept : ptr_(other.ptr_), count_(other.count_) {
        other.ptr_ = nullptr;
        other.count_ = 0;
    }
```

移动构造的含义是：

```text
把 other 拥有的 GPU 显存转移给当前对象。
然后把 other 置空，避免 other 析构时释放同一块显存。
```

这也是 `std::unique_ptr` 的语义：不能复制所有权，但可以移动所有权。

### 5.6 `get()`、`size()`、`bytes()`

```cpp
    T* get() { return ptr_; }
    const T* get() const { return ptr_; }
    std::size_t size() const { return count_; }
    std::size_t bytes() const { return count_ * sizeof(T); }
```

用途：

| 方法 | 作用 |
|---|---|
| `get()` | 借出底层 device pointer，传给 `cudaMemcpy` 或 kernel launch |
| `size()` | 返回元素数量 |
| `bytes()` | 返回字节数，避免手写 `size() * sizeof(T)` |

注意：`get()` 只是借出指针，不转移所有权，不能对 `get()` 返回的指针手动 `cudaFree`。

---

## 6. 对外 API：`src/vector_add.cuh`

这个头文件声明了两层 API。

### 6.1 高层 host API

```cpp
std::vector<float> vector_add(const std::vector<float>& a, const std::vector<float>& b);
```

调用者只需要传入 CPU 侧的 `std::vector<float>`，函数内部负责：

1. 申请 GPU 显存。
2. Host to Device 拷贝。
3. 启动 `vector_add_kernel`。
4. Device to Host 拷回结果。

这让 `tests/` 和 `benchmarks/` 可以复用同一份实现。

### 6.2 低层 kernel launch 封装

```cpp
void launch_vector_add_kernel(const float* d_a,
                              const float* d_b,
                              float* d_c,
                              int n,
                              int threads_per_block = 256);
```

这个函数要求传入的是 device pointer，不能传 `std::vector::data()` 这样的 host pointer。

它主要服务 benchmark：benchmark 会提前准备好 device memory，然后在 CUDA event 计时区间里重复调用 launch 函数，从而测 kernel-only 时间。

---

## 7. 核心实现：`src/vector_add.cu`

### 7.1 Kernel：每个线程处理一个元素

```cpp
__global__ void vector_add_kernel(const float* a, const float* b, float* c, int n) {
    const int idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (idx < n) {
        c[idx] = a[idx] + b[idx];
    }
}
```

**关键点**：

- `__global__` 表示这是由 CPU 侧启动、在 GPU 上执行的 kernel。
- 每个线程都会执行同一份代码。
- `idx` 是当前线程负责的全局数组下标。
- `idx < n` 防止最后一个 block 中多出来的线程越界访问。

CPU 写法是：

```cpp
for (int i = 0; i < n; ++i) {
    c[i] = a[i] + b[i];
}
```

CUDA 写法的思路是：

```text
把 CPU 循环的每一次迭代，分配给不同 GPU thread。
```

也就是：

```text
CPU：一个线程做 n 次加法
CUDA：n 个线程各做一次加法
```

### 7.2 Kernel launch：计算 block 数并启动 GPU

```cpp
void launch_vector_add_kernel(const float* d_a,
                              const float* d_b,
                              float* d_c,
                              int n,
                              int threads_per_block) {
    const int blocks = (n + threads_per_block - 1) / threads_per_block;

    vector_add_kernel<<<blocks, threads_per_block>>>(d_a, d_b, d_c, n);
    CUDA_CHECK(cudaGetLastError());
}
```

`blocks` 的计算是整数向上取整：

```text
blocks = ceil(n / threads_per_block)
```

例如 `n = 1000`、`threads_per_block = 256`：

```text
blocks = 4
总线程数 = 4 * 256 = 1024
```

多出来的 24 个线程会被 kernel 中的 `if (idx < n)` 拦住。

`cudaGetLastError()` 用来检查 kernel launch 是否成功。运行期错误通常还需要后面的 `cudaDeviceSynchronize()` 暴露。

### 7.3 Host 侧完整流程

```cpp
std::vector<float> vector_add(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size()) {
        throw std::invalid_argument("vector_add requires input vectors with the same size");
    }

    if (a.empty()) {
        return {};
    }

    const int n = static_cast<int>(a.size());
    std::vector<float> c(a.size(), 0.0f);

    DeviceBuffer<float> d_a(a.size());
    DeviceBuffer<float> d_b(b.size());
    DeviceBuffer<float> d_c(c.size());

    CUDA_CHECK(cudaMemcpy(d_a.get(), a.data(), d_a.bytes(), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_b.get(), b.data(), d_b.bytes(), cudaMemcpyHostToDevice));

    launch_vector_add_kernel(d_a.get(), d_b.get(), d_c.get(), n);
    CUDA_CHECK(cudaDeviceSynchronize());

    CUDA_CHECK(cudaMemcpy(c.data(), d_c.get(), d_c.bytes(), cudaMemcpyDeviceToHost));
    return c;
}
```

这段代码实现了完整 CUDA 数据流：

```text
检查输入
→ 准备 host 输出 c
→ 申请 device memory
→ H2D 拷贝输入
→ 启动 kernel
→ 同步等待 GPU 完成
→ D2H 拷回结果
→ 返回 host 输出
```

这正是[[3.1 CUDA Week 1 零基础系统入门|CUDA 零基础系统入门]]] 中“分配 → 拷贝 → 计算 → 拷回”的工程实现。

---

## 8. Correctness Test：`tests/test_vector_add.cu`

测试文件的核心函数是：

```cpp
void run_case(const std::vector<float>& a, const std::vector<float>& b) {
    const std::vector<float> c = vector_add(a, b);

    if (c.size() != a.size()) {
        throw std::runtime_error("output size does not match input size");
    }

    for (std::size_t i = 0; i < c.size(); ++i) {
        expect_close(c[i], a[i] + b[i], static_cast<int>(i));
    }
}
```

测试逻辑是：

```text
CUDA 输出 c[i]
对比 CPU reference a[i] + b[i]
```

### 8.1 小规模测试

```cpp
run_case({1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f});
```

验证最小例子：

```text
[1, 2, 3] + [4, 5, 6] = [5, 7, 9]
```

这是 Week 1 的基本验收。

### 8.2 非 block 对齐长度测试

```cpp
std::vector<float> a(1000);
std::vector<float> b(1000);
```

`1000` 不能被默认 block size `256` 整除，用来验证：

```cpp
if (idx < n)
```

是否正确保护最后一个 block 中多出来的线程。

### 8.3 大规模测试

```cpp
a.resize(1 << 20);
b.resize(1 << 20);
```

`1 << 20` 是约 100 万个元素，用来验证 kernel 在更接近 GPU 并行规模的数据上也能正确运行。

---

## 9. Benchmark：`benchmarks/bench_vector_add.cu`

benchmark 和 correctness test 分开：

```text
tests/test_vector_add.cu       只关心结果是否正确
benchmarks/bench_vector_add.cu 关心 kernel 时间和有效带宽
```

但 benchmark 中仍保留最小正确性检查，因为 GPU 程序中“算错但很快”没有意义。

### 9.1 结果检查

```cpp
bool check_result(const std::vector<float>& a, const std::vector<float>& b, const std::vector<float>& c) {
    constexpr float tolerance = 1e-5f;
    for (std::size_t i = 0; i < c.size(); ++i) {
        if (std::fabs(c[i] - (a[i] + b[i])) > tolerance) {
            return false;
        }
    }
    return true;
}
```

这和测试文件的思想一致：CUDA 输出必须和 CPU reference 对齐。

### 9.2 Kernel-only benchmark

```cpp
float benchmark_kernel_once(DeviceBuffer<float>& d_a,
                            DeviceBuffer<float>& d_b,
                            DeviceBuffer<float>& d_c,
                            int n,
                            int repeat)
```

这个函数接收已经准备好的 device memory，所以它测的是 kernel-only 时间，不包含 H2D 和 D2H 拷贝。

### 9.3 Warm-up

```cpp
launch_vector_add_kernel(d_a.get(), d_b.get(), d_c.get(), n);
CUDA_CHECK(cudaDeviceSynchronize());
```

第一次 kernel launch 可能包含 CUDA context 初始化、cache 状态变化、GPU 频率变化等额外开销，所以不计入统计。

### 9.4 CUDA event 计时

```cpp
CUDA_CHECK(cudaEventRecord(start));
for (int i = 0; i < repeat; ++i) {
    launch_vector_add_kernel(d_a.get(), d_b.get(), d_c.get(), n);
}
CUDA_CHECK(cudaEventRecord(stop));
CUDA_CHECK(cudaEventSynchronize(stop));
```

CUDA event 记录的是 GPU stream 时间线。这里测的是：

```text
repeat 次 vector_add_kernel 的 GPU 执行总时间
```

最后返回平均时间：

```cpp
return elapsed_ms / static_cast<float>(repeat);
```

### 9.5 有效带宽计算

```cpp
const double bytes = 3.0 * static_cast<double>(n) * sizeof(float);
const double seconds = static_cast<double>(kernel_ms) / 1000.0;
const double bandwidth_gbs = bytes / seconds / 1.0e9;
```

`vector add` 每个元素大约访问三次内存：

```text
读取 a[i]：4 bytes
读取 b[i]：4 bytes
写入 c[i]：4 bytes
```

所以有效数据量约为：

```text
3 * n * sizeof(float)
```

这个指标用于观察 `vector add` 的有效显存带宽。因为 `vector add` 计算量很小，通常更接近 memory-bound kernel。

### 9.6 输出格式

benchmark 输出列包括：

```text
N    Kernel(ms)    Bandwidth(GB/s)    Check
```

这正好对应 Week 1 的 benchmark 验收要求。

---

## 10. README 推荐阅读顺序

README 中建议按以下顺序读项目：

1. `CLAUDE.md`：先看 Agent 可以做什么、不能做什么。
2. `include/cuda_check.cuh`：理解 CUDA 错误检查。
3. `include/device_buffer.cuh`：理解 RAII 管理 GPU 显存。
4. `src/vector_add.cuh`：理解对外 API。
5. `src/vector_add.cu`：重点理解 kernel、grid/block、Host/Device 拷贝。
6. `tests/test_vector_add.cu`：理解 correctness test。
7. `benchmarks/bench_vector_add.cu`：理解 warm-up、repeat、CUDA event、有效带宽。

这个顺序是合理的，因为它从工程约束开始，再进入工具封装、核心实现、测试和性能测量。

---

## 11. Week 1 验收目标

这个项目完成后，至少应该能做到：

- [ ] 能用 CMake 构建 CUDA 项目。
- [ ] 能解释 `include/`、`src/`、`tests/`、`benchmarks/` 的职责。
- [ ] 能解释 `CUDA_CHECK` 如何暴露 CUDA Runtime API 错误。
- [ ] 能解释 `DeviceBuffer<T>` 如何用 RAII 管理 GPU 显存。
- [ ] 能解释 `vector_add_kernel` 中每个线程为什么只处理一个元素。
- [ ] 能解释 `blocks = (n + threads_per_block - 1) / threads_per_block` 的向上取整意义。
- [ ] 能解释为什么 kernel 中需要 `if (idx < n)`。
- [ ] 能跑通小规模、非 block 对齐长度和大规模 correctness test。
- [ ] 能用 CUDA event benchmark 得到 kernel 平均时间。
- [ ] 能计算 `vector add` 的有效带宽。

---

## 12. 下一步学习建议

### 12.1 先在 1660S 上跑通项目

在 Linux 主机上进入 `week01`：

```bash
make
make test
make bench
```

如果使用 CMake 原生命令：

```bash
cmake -S . -B build
cmake --build build
./build/test_vector_add
./build/bench_vector_add
```

GTX 1660 Super 足够完成 Week 1 的全部目标，包括 `vector add`、correctness test、CUDA event benchmark 和基础 profiling。

### 12.2 跑通后记录环境

建议记录：

```text
GPU: GTX 1660 Super
CUDA Toolkit:
Driver:
CMake:
Compiler:
Command:
```

benchmark 数据如果不记录环境，后续无法比较。

### 12.3 下一阶段进入 Week 2

Week 1 跑通后，进入 [[Week 2 - Reduction + Profiling]]。

学习重点从“每个线程独立处理一个元素”升级到：

```text
多个线程如何协作完成 reduction
如何使用 shared memory
如何用 Nsight Compute 解释瓶颈
```

---

## 13. 关键要点总结

1. 这个项目是 CUDA Hello World 的工程化版本，不是单文件玩具程序。
2. `vector_add_kernel` 体现了 CUDA 的核心思想：大量线程执行同一份代码、处理不同数据。
3. `DeviceBuffer<T>` 把 `cudaMalloc/cudaFree` 封装成 RAII，符合 Modern C++ 资源管理习惯。
4. `CUDA_CHECK` 把 C 风格错误码转换成带上下文的 C++ 异常。
5. correctness test 和 benchmark 分离，但 benchmark 仍必须保留正确性检查。
6. CUDA event benchmark 回答“跑多快”，后续 Nsight profiling 才能回答“为什么快或慢”。

---

## 关联知识

- [[Week 1 - CUDA + Agent workflow]] - 这个项目对应的 Week 1 阶段计划
- [[3.1 CUDA Week 1 零基础系统入门|CUDA 零基础系统入门]] - CUDA 执行模型、典型数据流和 benchmark 入门
- [[CUDA 学习清单]] - 后续 CUDA kernel 与 profiling 学习路线
- [[3.2 CUDA Runtime 进阶|CUDA Runtime 进阶]] - event、stream、pinned memory 和 Runtime API
- [[3.4 CUDA Nsight Compute 指标速查|CUDA Nsight Compute 指标速查]] - 后续解释 kernel 性能瓶颈
- [[独享智能指针]] - 理解 `DeviceBuffer<T>` 的 RAII 与独占所有权
- [[14.3 CMake基础]] - 理解 CUDA 项目的 CMake 构建

---

## 参考

- NVIDIA CUDA C++ Programming Guide
- NVIDIA CUDA C++ Best Practices Guide
- NVIDIA Nsight Compute Documentation
- 《Effective C++》Item 13：以对象管理资源
- 《Effective Modern C++》Item 18：使用 `std::unique_ptr` 管理独占所有权资源
