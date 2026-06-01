---
title: CUDA Week 2 Parallel Reduction 项目解析
date: 2026-05-12
tags:
  - infra
  - CUDA
  - 项目分析
aliases:
  - CUDA Week2 项目解析
  - CUDA Parallel Reduction 工程解析
status: active
---

# CUDA Week 2 Parallel Reduction 项目解析

> 这个项目是 `CUDA_learning/week02` 的 Week 2 CUDA 归约工程：用三种 parallel reduction 实现，从 naive baseline 逐步走到 shared memory 和 warp shuffle，并配套 correctness test、enhanced benchmark、Nsight Compute profiling 模板和 Agent workflow 约束。

项目地址：[CUDA_learning/week02](https://github.com/hosendovebelva-boop/CUDA_learning/tree/main/week02)

配套辅助笔记：[[3.5.2 CUDA Week 2 辅助笔记 - Mark Harris Reduction 优化路线]]

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

## 6. 基础设施：`include/cuda_check.cuh` 逐行精讲

`cuda_check.cuh` 是整个项目的错误处理基石，Week 1 就已经建立，Week 2 继续复用。文件虽短（33 行），但包含了 CUDA 工程中最核心的安全模式。

### 6.1 完整源码

```cpp
#pragma once

#include <cuda_runtime.h>

#include <stdexcept>
#include <string>

inline void check_cuda(cudaError_t status, const char* expression,
                       const char* file, int line) {
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

### 6.2 逐行分析

**`#pragma once`**：头文件保护，防止同一翻译单元重复包含。比传统的 `#ifndef` / `#define` / `#endif` 更简洁，主流编译器都支持。

**`#include <cuda_runtime.h>`**：引入 CUDA Runtime API 的类型定义，主要是 `cudaError_t` 枚举和 `cudaGetErrorString()` 函数。

**`inline void check_cuda(...)`**：这是一个普通的 inline 函数而不是宏。把核心逻辑放在函数里而不是宏里有几个好处：

1. 函数有类型安全，宏没有。
2. 函数可以设断点调试。
3. `inline` 避免了多翻译单元链接时的重复定义问题。

参数含义：

| 参数 | 含义 | 来源 |
|---|---|---|
| `status` | CUDA API 返回的错误码 | 被检查的 CUDA 调用 |
| `expression` | 被检查的表达式文本 | 宏中的 `#expr` 字符串化 |
| `file` | 源文件名 | 宏中的 `__FILE__` |
| `line` | 行号 | 宏中的 `__LINE__` |

**`cudaGetErrorString(status)`**：CUDA Runtime 提供的函数，将 `cudaError_t` 枚举值转换为人类可读的错误描述字符串，例如 `"out of memory"` 或 `"invalid device pointer"`。

**`throw std::runtime_error(...)`**：选择抛异常而不是 `exit(1)` 或返回错误码，原因是：

1. 异常会触发 RAII 析构链，`DeviceBuffer` 等对象会自动释放显存。
2. 调用者可以选择 catch 或不 catch。
3. 异常消息包含完整上下文：错误描述 + 出错表达式 + 源码位置。

### 6.3 宏：`CUDA_CHECK`

```cpp
#define CUDA_CHECK(expr) check_cuda((expr), #expr, __FILE__, __LINE__)
```

这里的关键技巧是 **`#expr`（字符串化操作符）**：

```cpp
CUDA_CHECK(cudaMalloc(&ptr, 1024));
// 展开后：
check_cuda((cudaMalloc(&ptr, 1024)), "cudaMalloc(&ptr, 1024)", __FILE__, __LINE__);
```

预处理器将 `expr` 参数原样转换为字符串字面量。这样出错时打印的消息直接告诉你哪个 CUDA 调用失败了，无需手动写描述。

**为什么用宏而不是纯函数**：因为 `__FILE__` 和 `__LINE__` 必须在调用点展开才能获得正确的位置信息。如果写成纯函数，`__FILE__` 和 `__LINE__` 会指向 `check_cuda` 的定义位置，而不是调用点。

### 6.4 使用示例

项目中所有 CUDA Runtime API 调用都经过 `CUDA_CHECK`：

```cpp
// 显存分配
CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&ptr_), count_ * sizeof(T)));

// 数据传输
CUDA_CHECK(cudaMemcpy(h_partial.data(), d_partial.get(),
                      num_blocks * sizeof(T), cudaMemcpyDeviceToHost));

// kernel launch 后的错误检查
CUDA_CHECK(cudaGetLastError());
CUDA_CHECK(cudaDeviceSynchronize());

// CUDA Event 操作
CUDA_CHECK(cudaEventCreate(&start));
CUDA_CHECK(cudaEventRecord(start));
CUDA_CHECK(cudaEventSynchronize(stop));
CUDA_CHECK(cudaEventElapsedTime(&ms, start, stop));
CUDA_CHECK(cudaEventDestroy(start));
```

注意 kernel launch 本身（`<<<...>>>`）不返回 `cudaError_t`，所以紧跟 `cudaGetLastError()` 来捕获异步错误。

### 6.5 设计取舍

| 决策 | 选择 | 替代方案 | 理由 |
|---|---|---|---|
| 错误处理方式 | 抛异常 | `exit(1)` / 返回码 | 触发 RAII 清理链 |
| 宏 vs 函数 | 宏包装函数 | 纯宏 | 宏只做位置捕获，逻辑在函数里 |
| 异常类型 | `std::runtime_error` | 自定义异常类 | 入门工程不需要复杂异常层次 |
| 头文件保护 | `#pragma once` | include guard | 更简洁，编译器支持良好 |

---

## 7. 基础设施：`include/device_buffer.cuh` 逐行精讲

`DeviceBuffer` 是项目中 GPU 显存管理的核心抽象。它用 RAII（Resource Acquisition Is Initialization）模式封装 `cudaMalloc` / `cudaFree`，是整个项目中 C++ 工程能力最集中的一个文件。

### 7.1 完整源码

```cpp
#pragma once

#include "cuda_check.cuh"

#include <cstddef>

template <typename T>
class DeviceBuffer {
public:
    explicit DeviceBuffer(std::size_t count) : count_(count) {
        if (count_ > 0) {
            CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&ptr_), count_ * sizeof(T)));
        }
    }

    ~DeviceBuffer() {
        if (ptr_ != nullptr) {
            cudaFree(ptr_);
        }
    }

    DeviceBuffer(const DeviceBuffer&) = delete;
    DeviceBuffer& operator=(const DeviceBuffer&) = delete;

    DeviceBuffer(DeviceBuffer&& other) noexcept : ptr_(other.ptr_), count_(other.count_) {
        other.ptr_ = nullptr;
        other.count_ = 0;
    }

    DeviceBuffer& operator=(DeviceBuffer&& other) noexcept {
        if (this == &other) {
            return *this;
        }

        if (ptr_ != nullptr) {
            cudaFree(ptr_);
        }

        ptr_ = other.ptr_;
        count_ = other.count_;
        other.ptr_ = nullptr;
        other.count_ = 0;
        return *this;
    }

    T* get() { return ptr_; }
    const T* get() const { return ptr_; }
    std::size_t size() const { return count_; }
    std::size_t bytes() const { return count_ * sizeof(T); }

private:
    T* ptr_ = nullptr;
    std::size_t count_ = 0;
};
```

### 7.2 RAII 模式：构造即分配，析构即释放

```cpp
explicit DeviceBuffer(std::size_t count) : count_(count) {
    if (count_ > 0) {
        CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&ptr_), count_ * sizeof(T)));
    }
}
```

**`explicit`**：防止隐式转换。没有 `explicit` 的话，`DeviceBuffer<float> buf = 1024;` 能编译通过，这不是期望行为。加了 `explicit` 后，只能写 `DeviceBuffer<float> buf(1024);`。

**成员初始化列表**：`count_(count)` 在构造函数体执行前初始化成员变量。对于 POD 类型这和在函数体内赋值等价，但对于复杂类型有性能差异。这里是好习惯。

**`count_ > 0` 守卫**：如果传入 0，不调用 `cudaMalloc`，`ptr_` 保持 `nullptr`。这避免了 `cudaMalloc(0)` 的未定义行为（不同 CUDA 版本行为不一致）。

**`reinterpret_cast<void**>(&ptr_)`**：`cudaMalloc` 的签名是 `cudaMalloc(void**, size_t)`，需要传入 `void**`。`ptr_` 是 `T*`，所以 `&ptr_` 是 `T**`，需要强转。这是 CUDA C API 的固有模式。

**`count_ * sizeof(T)`**：分配字节数 = 元素数 × 单个元素大小。`float` 是 4 字节，`int` 也是 4 字节，`double` 是 8 字节。

### 7.3 析构函数

```cpp
~DeviceBuffer() {
    if (ptr_ != nullptr) {
        cudaFree(ptr_);
    }
}
```

**空指针守卫**：虽然 `cudaFree(nullptr)` 在 CUDA Runtime 中是安全的（类似 C 的 `free(NULL)`），但显式检查更清晰，也避免了对这个行为的依赖。

**不抛异常**：析构函数里没有用 `CUDA_CHECK`，而是直接调用 `cudaFree` 并忽略返回值。这是因为：

1. C++ 析构函数默认是 `noexcept` 的（C++11 起）。
2. 析构函数抛异常会触发 `std::terminate`。
3. `cudaFree` 失败时（极端罕见），程序已经无法安全恢复。

**RAII 的核心价值**：不管函数正常返回还是抛异常，`DeviceBuffer` 都会自动释放显存。对比手动管理：

```cpp
// 手动管理 — 容易泄漏
T* ptr;
cudaMalloc(&ptr, n * sizeof(T));
// ... 如果中间 throw，ptr 永远不会被 free ...
cudaFree(ptr);

// RAII — 自动释放
DeviceBuffer<T> buf(n);
// ... 即使中间 throw，buf 的析构函数也会释放显存 ...
```

### 7.4 Rule of Five：拷贝与移动

C++ 的 Rule of Five 规定：如果一个类定义了析构函数、拷贝构造、拷贝赋值、移动构造、移动赋值中的任意一个，通常应该定义全部五个。`DeviceBuffer` 完整实现了 Rule of Five。

**拷贝 = 禁止**：

```cpp
DeviceBuffer(const DeviceBuffer&) = delete;
DeviceBuffer& operator=(const DeviceBuffer&) = delete;
```

禁止拷贝的原因：如果两个 `DeviceBuffer` 持有同一个 `ptr_`，析构时会 double free。要支持拷贝需要深拷贝（`cudaMemcpy`），但这个工程不需要，所以直接禁止更安全。

**移动构造函数**：

```cpp
DeviceBuffer(DeviceBuffer&& other) noexcept
    : ptr_(other.ptr_), count_(other.count_) {
    other.ptr_ = nullptr;
    other.count_ = 0;
}
```

移动语义允许转移所有权而不复制数据。关键步骤：

1. 把 `other` 的指针和大小"偷"过来。
2. 把 `other` 的指针置空，防止 `other` 析构时释放已转移的内存。

`noexcept` 标记很重要：STL 容器（如 `std::vector`）在 resize 时，只有 `noexcept` 的移动构造函数才会被优先使用，否则退化为拷贝（而拷贝被 `delete` 了，会编译失败）。

**移动赋值运算符**：

```cpp
DeviceBuffer& operator=(DeviceBuffer&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    if (ptr_ != nullptr) {
        cudaFree(ptr_);         // 释放自身原有的显存
    }

    ptr_ = other.ptr_;          // 接管 other 的资源
    count_ = other.count_;
    other.ptr_ = nullptr;       // 解除 other 的所有权
    other.count_ = 0;
    return *this;
}
```

与移动构造的区别：移动赋值时 `this` 可能已经持有旧资源，必须先释放。

**自赋值检查**：`if (this == &other)` 防止 `buf = std::move(buf)` 时先释放再使用已释放的内存。虽然实际中很少发生，但这是标准的防御性编程。

### 7.5 访问接口

```cpp
T* get() { return ptr_; }
const T* get() const { return ptr_; }
std::size_t size() const { return count_; }
std::size_t bytes() const { return count_ * sizeof(T); }
```

提供两个 `get()` 重载（const 和 non-const），让 `const DeviceBuffer<T>&` 也能获取只读指针。`size()` 返回元素数，`bytes()` 返回字节数。

### 7.6 在项目中的使用模式

```cpp
// 典型使用：分配 → 传数据 → 传给 kernel → 自动释放
DeviceBuffer<T> d_input(n);
CUDA_CHECK(cudaMemcpy(d_input.get(), h_input.data(),
                      n * sizeof(T), cudaMemcpyHostToDevice));

// kernel 内部分配 partial sums
DeviceBuffer<T> d_partial(num_blocks);
reduce_xxx_kernel<<<num_blocks, threads_per_block>>>(d_input, d_partial.get(), n);

// 函数结束时 d_partial 自动释放
```

### 7.7 与 `std::unique_ptr` 的对比

`DeviceBuffer` 的功能类似于带自定义 deleter 的 `std::unique_ptr`：

```cpp
// 如果用 unique_ptr 实现（仅做对比，项目没有这样写）
auto deleter = [](T* p) { cudaFree(p); };
std::unique_ptr<T, decltype(deleter)> ptr(nullptr, deleter);
```

但 `DeviceBuffer` 更好：

| 维度 | `DeviceBuffer` | `unique_ptr` + custom deleter |
|---|---|---|
| 元素数追踪 | 内置 `size()` / `bytes()` | 需要额外变量 |
| 零元素处理 | 内置 `count_ > 0` 守卫 | 需要外部检查 |
| 语义清晰度 | 名字就说明是 GPU 内存 | 通用类型，意图不明 |
| API 简洁度 | `DeviceBuffer<float> buf(n)` | 需要 deleter 模板参数 |

---

## 8. 测试工具：`include/test_utils.cuh` 逐行精讲

### 8.1 文件结构概览

`test_utils.cuh` 提供三个模板函数，构成测试的完整链路：

```text
generate_input<T>(n)    → 生成可复现的随机输入
sum_reference<T>(data, n) → CPU 串行求和作为正确答案
check_result<T>(gpu, cpu, name) → 比较 GPU 结果和 CPU 结果
```

这三个函数被 `tests/` 和 `benchmarks/` 共同使用。

### 8.2 输入生成：`generate_input<T>` 完整分析

```cpp
template <typename T>
std::vector<T> generate_input(int n) {
    std::vector<T> data(n);
    std::mt19937 rng(42);  // 固定种子，结果可复现

    if constexpr (std::is_floating_point_v<T>) {
        std::uniform_real_distribution<T> dist(0.0, 1.0);
        for (int i = 0; i < n; ++i) {
            data[i] = dist(rng);
        }
    } else {
        std::uniform_int_distribution<T> dist(0, 99);
        for (int i = 0; i < n; ++i) {
            data[i] = dist(rng);
        }
    }

    return data;
}
```

**`std::mt19937 rng(42)`**：Mersenne Twister 伪随机数生成器，种子固定为 42。

- **固定种子的价值**：同样的 `n` 和类型，每次生成完全相同的输入。这意味着 correctness test 失败时可以精确复现，benchmark 结果可以跨次比较。
- **为什么是 42**：这是一个任意常数。重要的是固定，不是具体值。

**`if constexpr (std::is_floating_point_v<T>)`**：C++17 编译期分支。

- `if constexpr` 在编译期求值，不满足条件的分支不会被实例化。这比运行时 `if` 更安全，因为 `std::uniform_real_distribution<int>` 会编译失败，但放在 `if constexpr` 的 false 分支中就不会被编译。
- `std::is_floating_point_v<T>` 是类型萃取，对 `float`、`double`、`long double` 返回 `true`。

**浮点分布 `[0.0, 1.0)`**：

- 每个元素在 `[0, 1)` 范围内，这样 n 个元素的 sum 大约等于 `n/2`。
- 对于 `n = 1 << 20 ≈ 1M`，sum 大约在 52 万左右。这个量级不会导致 FP32 溢出（`float` 最大值约 `3.4e38`），但足以暴露并行归约的舍入误差。

**整数分布 `[0, 99]`**：

- 每个元素在 `[0, 99]` 范围内，n 个元素的 sum 大约等于 `n * 49.5`。
- 对于 `n = 1 << 20 ≈ 1M`，sum 大约是 5192 万。`int` 最大值约 21.5 亿，不会溢出。
- 对于 `n = 1 << 26 ≈ 64M`（benchmark 最大规模），sum 大约是 33 亿，接近 `int` 上限。如果换成更大的 n 或更大的值域，需要注意溢出。

**返回 `std::vector<T>`**：利用 NRVO（Named Return Value Optimization），编译器会避免拷贝，直接在调用者的存储空间构造 vector。

### 8.3 CPU 参考求和：`sum_reference<T>` 完整分析

```cpp
template <typename T>
T sum_reference(const std::vector<T>& data, int n) {
    if constexpr (std::is_floating_point_v<T>) {
        double acc = 0.0;
        for (int i = 0; i < n; ++i) {
            acc += static_cast<double>(data[i]);
        }
        return static_cast<T>(acc);
    } else {
        T acc = 0;
        for (int i = 0; i < n; ++i) {
            acc += data[i];
        }
        return acc;
    }
}
```

**浮点路径用 `double` 累加**：

这是关键设计。FP32（`float`）的尾数只有 23 位（约 7 位十进制精度）。当累加器变大后，小数的有效位会被截断：

```text
// 假设 acc = 500000.0f，加上 0.5f
// float 精度下，500000.0 + 0.5 = 500000.5（OK）
// 但 acc = 5000000.0f 时，0.5 就可能被截断
```

用 `double`（53 位尾数，约 15 位十进制精度）累加，可以在百万级元素时保持精度，为 correctness 判断提供更可靠的 reference。

**整数路径直接累加**：整数加法是精确的（不溢出时），不需要提升精度。

**最后 `static_cast<T>(acc)` 回转**：double → float 的转换会有舍入，但只发生一次（在最终结果上），而不是每次累加都舍入。

### 8.4 结果验证：`check_result<T>` 完整分析

```cpp
template <typename T>
bool check_result(T gpu_result, T cpu_result, const std::string& name) {
    bool pass = false;

    if constexpr (std::is_floating_point_v<T>) {
        T denom = std::abs(cpu_result) > static_cast<T>(1e-6)
                    ? std::abs(cpu_result) : static_cast<T>(1e-6);
        T rel_err = std::abs(gpu_result - cpu_result) / denom;
        pass = rel_err < static_cast<T>(1e-3);

        if (pass) {
            std::printf("[PASS] %s  gpu=%.6f  cpu=%.6f  rel_err=%.2e\n",
                        name.c_str(), static_cast<double>(gpu_result),
                        static_cast<double>(cpu_result),
                        static_cast<double>(rel_err));
        } else {
            std::printf("[FAIL] %s  gpu=%.6f  cpu=%.6f  rel_err=%.2e\n",
                        name.c_str(), static_cast<double>(gpu_result),
                        static_cast<double>(cpu_result),
                        static_cast<double>(rel_err));
        }
    } else {
        pass = (gpu_result == cpu_result);

        if (pass) {
            std::printf("[PASS] %s  gpu=%d  cpu=%d\n",
                        name.c_str(), static_cast<int>(gpu_result),
                        static_cast<int>(cpu_result));
        } else {
            std::printf("[FAIL] %s  gpu=%d  cpu=%d\n",
                        name.c_str(), static_cast<int>(gpu_result),
                        static_cast<int>(cpu_result));
        }
    }

    return pass;
}
```

**分母守卫 `1e-6`**：当 `cpu_result` 接近 0 时，直接除以它会得到极大的相对误差或除以零。用 `max(abs(cpu_result), 1e-6)` 作为分母，保证了数值稳定性。

**相对误差阈值 `1e-3`**：

- 对于 `n = 1`（单元素），GPU 和 CPU 应完全一致，`rel_err = 0`。
- 对于 `n = 1 << 20`（百万级），并行归约改变了加法顺序，FP32 舍入误差可能累积到 `1e-5` ~ `1e-4` 量级。`1e-3` 的阈值留有余量。
- 实测结果验证了这一点：`n = 1024` 时 `rel_err = 5.97e-08`，远低于阈值。

**整数路径精确匹配**：整数加法满足结合律（不溢出时），加法顺序不影响结果，所以 GPU 和 CPU 必须完全一致。

**`std::printf` 而不是 `std::cout`**：项目中统一使用 C 风格格式化输出。对于性能测试的输出格式控制（如 `%.6f`、`%.2e`），`printf` 比 `cout` 更直观。

这体现了 Week 2 的正确性判断原则：

| 类型 | 判断方式 | 原因 |
|---|---|---|
| `float` | 允许 `1e-3` 相对误差 | 并行归约改变加法顺序，会产生舍入差异 |
| `int` | 必须完全相等 | 整数加法在不溢出的前提下应精确一致 |

这部分可以和 [[3.5 CUDA Week 2 前置知识 - Reduction + Profiling]] 中的 FP32 误差讨论一起看。

---

## 9. 对外 API：三个 reduce 函数

### 9.1 头文件声明

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

### 9.2 头文件设计模式

以 `reduce_naive.cuh` 为例：

```cpp
#pragma once

/*
 * reduce_naive.cuh
 *
 * Day1: Naive parallel reduction — 每个 block 内只有线程 0 循环累加。
 * 目的：建立 baseline，体会没有 block 内并行归约的低效。
 */

template <typename T>
T reduce_naive(const T* d_input, int n, int threads_per_block = 256);
```

三个头文件结构完全一致：`#pragma once` + 注释 + 函数声明。注释解释的是 CUDA 概念（"只有线程 0 循环累加"），而不是 C++ 语法（没有解释 `template` 关键字）。这符合 `CLAUDE.md` 中"注释解释 CUDA 概念"的要求。

### 9.3 声明与实现分离

模板函数通常需要在头文件中提供完整定义。但本项目使用了**显式实例化**技巧：

```text
.cuh 头文件 → 只有声明
.cu  源文件 → 完整定义 + 显式实例化
```

`.cu` 文件底部的显式实例化：

```cpp
// reduce_naive.cu 底部
template float reduce_naive<float>(const float*, int, int);
template int reduce_naive<int>(const int*, int, int);
```

这种模式的好处：

1. **编译隔离**：`test_reduce_naive.cu` 只需要 `#include "reduce_naive.cuh"`，不需要看到 kernel 实现。
2. **编译速度**：kernel 代码只在 `reduce_naive.cu` 中编译一次，而不是在每个包含头文件的翻译单元都编译一次。
3. **类型控制**：只有 `float` 和 `int` 被实例化，其他类型（如 `double`）会链接失败，而不是编译出未测试的代码。

**扩展到 `double` 时**：需要在每个 `.cu` 文件底部添加：

```cpp
template double reduce_xxx<double>(const double*, int, int);
```

同时需要补充测试和 benchmark。

---

## 10. Naive Reduce：`src/reduce_naive.cu` 逐行精讲

![[图片/SVG/CUDA Week 2 Parallel Reduction 项目解析-03.svg|921]]

### 10.1 Kernel 完整源码与逐行分析

```cpp
template <typename T>
__global__ void reduce_naive_kernel(const T* input, T* partial_sums, int n) {
    // 每个 block 负责 blockDim.x 个连续元素
    int block_start = blockIdx.x * blockDim.x;
    int block_end = min(block_start + blockDim.x, n);

    // 只有线程 0 做累加
    if (threadIdx.x == 0) {
        T sum = 0;
        for (int i = block_start; i < block_end; ++i) {
            sum += input[i];
        }
        partial_sums[blockIdx.x] = sum;
    }
}
```

**`__global__`**：CUDA 函数修饰符，表示这个函数在 GPU 上执行，由 CPU 调用（通过 `<<<...>>>` 语法）。`__global__` 函数返回类型必须是 `void`。

**`const T* input`**：指向 GPU global memory 的只读输入数组。`const` 修饰告诉编译器 kernel 不会修改输入数据，编译器可能据此做优化（如使用只读缓存路径 `__ldg`）。

**`T* partial_sums`**：指向 GPU global memory 的输出数组，每个 block 写一个 partial sum。大小 = `num_blocks`。

**`int block_start = blockIdx.x * blockDim.x`**：

- `blockIdx.x`：当前 block 在 grid 中的索引（0, 1, 2, ...）。
- `blockDim.x`：每个 block 的线程数（默认 256）。
- `block_start`：当前 block 负责的第一个输入元素的全局索引。

**`int block_end = min(block_start + blockDim.x, n)`**：

- 最后一个 block 可能不满 256 个元素。例如 `n = 1000` 时，block 3（最后一个）负责索引 `[768, 1000)`，只有 232 个元素。
- `min` 防止读越界。这里用的是 CUDA 内置的 `min` 函数（device 代码中可用）。

**`if (threadIdx.x == 0)`**：

这是 naive 版本的核心特征——**只有线程 0 工作**。

```text
block 0: thread 0 工作，thread 1-255 空闲
block 1: thread 0 工作，thread 1-255 空闲
...
```

256 个线程中只有 1 个在干活，GPU 利用率约 `1/256 ≈ 0.39%`。但这 255 个空闲线程并非"免费"——它们仍然占用寄存器、warp 调度槽和 SM 资源。

**`sum += input[i]`**：线程 0 串行遍历 global memory。每次访问 `input[i]` 可能触发 400-800 cycles 的 global memory 延迟。由于是单线程串行访问，GPU 的内存系统无法并行处理多个请求来隐藏延迟。

**`partial_sums[blockIdx.x] = sum`**：每个 block 的线程 0 将累加结果写入 `partial_sums` 数组的对应位置。写入 global memory 的延迟也是数百 cycles，但只写一次。

### 10.2 线程活动可视化

以 `n = 16`、`threads_per_block = 4` 为例（实际默认 256，这里缩小方便展示）：

```text
输入数组：[3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5, 8, 9, 7, 9, 3]

Block 0 (input[0..3]):          Block 1 (input[4..7]):
  T0: sum = 3+1+4+1 = 9          T0: sum = 5+9+2+6 = 22
  T1: (空闲)                      T1: (空闲)
  T2: (空闲)                      T2: (空闲)
  T3: (空闲)                      T3: (空闲)

Block 2 (input[8..11]):         Block 3 (input[12..15]):
  T0: sum = 5+3+5+8 = 21         T0: sum = 9+7+9+3 = 28
  T1: (空闲)                      T1: (空闲)
  T2: (空闲)                      T2: (空闲)
  T3: (空闲)                      T3: (空闲)

partial_sums = [9, 22, 21, 28]
CPU 最终累加：9 + 22 + 21 + 28 = 80
```

### 10.3 为什么还要写这个低效版本

Naive 版本不是为了快，而是为了建立 baseline。它暴露了 reduction 的第一层问题：

```text
只把数据拆成 block 还不够；
如果 block 内没有并行协作，GPU 的线程资源会大量闲置。
```

性能上，它可能出现这些特征：

1. **block 内并行度极低**：SM 上只有 1/256 的线程在有效计算。
2. **无法隐藏内存延迟**：单线程串行读 global memory，没有其他线程的计算可以填充等待时间。
3. **warp 分支分歧（divergence）**：`if (threadIdx.x == 0)` 导致同一 warp 中 32 个线程走不同路径。线程 0 进入循环，其他 31 个线程被 mask 掉。warp 的执行时间由最慢路径决定。
4. **Nsight Compute 预期**：Long Scoreboard stall（等待 global memory），很低的 compute throughput。

### 10.4 Host 侧封装：`reduce_naive<T>` 完整分析

```cpp
template <typename T>
T reduce_naive(const T* d_input, int n, int threads_per_block) {
    if (n <= 0) return T(0);

    int num_blocks = (n + threads_per_block - 1) / threads_per_block;

    // 分配 partial sums 的设备内存
    DeviceBuffer<T> d_partial(num_blocks);

    // 启动 kernel
    reduce_naive_kernel<<<num_blocks, threads_per_block>>>(
        d_input, d_partial.get(), n);
    CUDA_CHECK(cudaGetLastError());
    CUDA_CHECK(cudaDeviceSynchronize());

    // 拷回 CPU 侧，累加得到最终结果
    std::vector<T> h_partial(num_blocks);
    CUDA_CHECK(cudaMemcpy(h_partial.data(), d_partial.get(),
                          num_blocks * sizeof(T), cudaMemcpyDeviceToHost));

    // CPU 侧用 double 累加（float 时避免精度丢失）
    if constexpr (std::is_floating_point_v<T>) {
        double total = 0.0;
        for (int i = 0; i < num_blocks; ++i) {
            total += static_cast<double>(h_partial[i]);
        }
        return static_cast<T>(total);
    } else {
        T total = 0;
        for (int i = 0; i < num_blocks; ++i) {
            total += h_partial[i];
        }
        return total;
    }
}
```

逐步分解：

**边界检查**：`if (n <= 0) return T(0);` 直接处理空输入，避免后续除零或无意义的 kernel launch。

**Grid 计算**：`(n + threads_per_block - 1) / threads_per_block` 是向上取整除法（ceiling division）的标准写法。例如 `n = 1000, threads_per_block = 256` → `num_blocks = (1000 + 255) / 256 = 4`（实际需要 4 个 block 覆盖 `[0, 1000)`）。

**RAII 分配**：`DeviceBuffer<T> d_partial(num_blocks)` 在 GPU 上分配 `num_blocks * sizeof(T)` 字节。函数结束时（正常返回或异常）自动释放。

**Kernel launch**：`<<<num_blocks, threads_per_block>>>` 指定 grid 维度和 block 维度。这是一维 grid 和一维 block，满足当前一维 reduction 的需求。

**错误检查双保险**：

```cpp
CUDA_CHECK(cudaGetLastError());      // 检查 launch 配置错误（如 block size 超限）
CUDA_CHECK(cudaDeviceSynchronize()); // 等待 kernel 完成并检查执行错误
```

`cudaGetLastError()` 捕获的是异步错误（kernel launch 是异步的），`cudaDeviceSynchronize()` 同时等待完成和检查运行时错误。

**D2H 拷贝**：`cudaMemcpy(..., cudaMemcpyDeviceToHost)` 将 partial sums 从 GPU 拷回 CPU。这是一个同步操作（会等待传输完成）。

**CPU 最终累加**：`if constexpr` 区分浮点和整数路径。浮点路径用 `double` 累加，和 `sum_reference` 一致，保证 host wrapper 本身不引入额外误差。

### 10.5 三个 host wrapper 的共同模式

三种 reduce 实现的 host wrapper 结构完全一致：

```text
1. 边界检查 (n <= 0)
2. 计算 num_blocks
3. RAII 分配 d_partial
4. Launch kernel（不同的 kernel 函数名和 shared memory 参数）
5. cudaGetLastError + cudaDeviceSynchronize
6. cudaMemcpy D2H
7. CPU 侧 if constexpr 累加
8. 返回结果
```

这种统一结构使得三种实现的差异只体现在 kernel 本身，host 代码几乎可以用 diff 比较。具体差异只有两处：

| 差异点 | Naive | Shared | Shuffle |
|---|---|---|---|
| kernel 函数名 | `reduce_naive_kernel` | `reduce_shared_kernel` | `reduce_shuffle_kernel` |
| launch 参数 | `<<<N, B>>>` | `<<<N, B, B*sizeof(T)>>>` | `<<<N, B, (B/32)*sizeof(T)>>>` |

第三个 `<<<...>>>` 参数是动态 shared memory 大小，naive 不使用 shared memory 所以省略（默认 0）。

---

## 11. Shared Memory Reduce：`src/reduce_shared.cu` 逐行精讲

![[图片/SVG/CUDA Week 2 Parallel Reduction 项目解析-04.svg|900]]

### 11.1 Kernel 完整源码与逐行分析

```cpp
template <typename T>
__global__ void reduce_shared_kernel(const T* input, T* partial_sums, int n) {
    extern __shared__ char shared_raw[];
    T* sdata = reinterpret_cast<T*>(shared_raw);

    int tid = threadIdx.x;
    int gid = blockIdx.x * blockDim.x + threadIdx.x;

    // 从 global memory 读入 shared memory，越界写 0
    sdata[tid] = (gid < n) ? input[gid] : T(0);
    __syncthreads();

    // 二分归约
    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (tid < stride) {
            sdata[tid] += sdata[tid + stride];
        }
        __syncthreads();
    }

    // 线程 0 写出该 block 的 partial sum
    if (tid == 0) {
        partial_sums[blockIdx.x] = sdata[0];
    }
}
```

### 11.2 动态 Shared Memory 机制深入

**`extern __shared__ char shared_raw[]`**：

`extern __shared__` 声明了动态大小的 shared memory 数组。与静态 shared memory 的对比：

```cpp
// 静态 shared memory — 大小在编译期确定
__shared__ float sdata[256];

// 动态 shared memory — 大小在 launch 时传入
extern __shared__ char shared_raw[];
```

使用 `char` 作为底层类型是因为动态 shared memory 的大小以**字节**为单位在 launch 时指定。然后通过 `reinterpret_cast<T*>` 转换为正确类型的指针。

**为什么需要动态**：这个 kernel 是模板，`T` 可能是 `float`（4 字节）或 `int`（4 字节）或将来的 `double`（8 字节）。如果用静态数组 `__shared__ T sdata[N]`，`N` 必须是编译期常量。动态方式让 host 侧根据 `sizeof(T)` 灵活指定大小。

**Host 侧传入**：

```cpp
size_t shared_bytes = threads_per_block * sizeof(T);
reduce_shared_kernel<<<num_blocks, threads_per_block, shared_bytes>>>(...);
```

`<<<...>>>` 的第三个参数就是动态 shared memory 大小（字节）。对于 `float` + 256 线程：`256 * 4 = 1024` 字节。

**注意事项**：一个 kernel 中只能有一个 `extern __shared__` 声明。如果需要多个不同类型的 shared 数组，需要手动在一个 `char[]` 中划分偏移。

### 11.3 数据加载阶段

```cpp
int tid = threadIdx.x;                           // block 内局部线程 ID (0-255)
int gid = blockIdx.x * blockDim.x + threadIdx.x; // 全局线程 ID

sdata[tid] = (gid < n) ? input[gid] : T(0);
__syncthreads();
```

**线程 ID 计算**：

- `tid`（thread ID）：block 内局部索引，范围 `[0, blockDim.x - 1]`。用于 shared memory 数组下标。
- `gid`（global ID）：全局索引，用于 global memory 数组下标。计算方式和 vector add 一致。

**Coalesced 读取**：

同一 warp 中的 32 个相邻线程（如 tid = 0-31）读取 `input[gid]` 的 gid 值是连续的。GPU 内存控制器可以把这 32 个 4 字节读取合并为 1 个 128 字节的内存事务（memory transaction）。这就是**合并访问（coalesced access）**，是 GPU 内存带宽的关键。

**越界处理**：`(gid < n) ? input[gid] : T(0)` — 最后一个 block 中，超出 `n` 的线程写 0。0 是加法的单位元，不影响归约结果。

**第一个 `__syncthreads()`**：确保所有线程都完成了 global → shared 的加载后，才开始归约。如果不同步，某个线程可能在邻居还没写入 shared memory 时就去读取，得到未初始化的值。

### 11.4 二分树形归约可视化

以 `blockDim.x = 8` 为例（实际默认 256，缩小展示）：

```text
初始 shared memory: sdata = [3, 1, 4, 1, 5, 9, 2, 6]
                             T0 T1 T2 T3 T4 T5 T6 T7

第 1 轮 (stride = 4):
  T0: sdata[0] += sdata[4] → 3+5 = 8
  T1: sdata[1] += sdata[5] → 1+9 = 10
  T2: sdata[2] += sdata[6] → 4+2 = 6
  T3: sdata[3] += sdata[7] → 1+6 = 7
  T4-T7: 不参与 (tid >= stride)
  __syncthreads()
  sdata = [8, 10, 6, 7, -, -, -, -]

第 2 轮 (stride = 2):
  T0: sdata[0] += sdata[2] → 8+6 = 14
  T1: sdata[1] += sdata[3] → 10+7 = 17
  T2-T7: 不参与
  __syncthreads()
  sdata = [14, 17, -, -, -, -, -, -]

第 3 轮 (stride = 1):
  T0: sdata[0] += sdata[1] → 14+17 = 31
  T1-T7: 不参与
  __syncthreads()
  sdata = [31, -, -, -, -, -, -, -]

结果：sdata[0] = 31 = 3+1+4+1+5+9+2+6 ✓
```

**归约轮数**：`log2(blockDim.x)` 轮。256 线程需要 8 轮，1024 线程需要 10 轮。

**每轮活跃线程数**：第 k 轮有 `blockDim.x / 2^k` 个线程工作。总工作量 = `blockDim.x/2 + blockDim.x/4 + ... + 1 = blockDim.x - 1` 次加法，正好等于 `n-1` 次加法的理论最小值。

### 11.5 `__syncthreads()` 放置分析

```cpp
for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
    if (tid < stride) {
        sdata[tid] += sdata[tid + stride];
    }
    __syncthreads();  // 为什么在 if 外面？
}
```

**为什么 `__syncthreads()` 在 `if` 外面**：

`__syncthreads()` 是 **block 级 barrier**，必须被 block 内所有活跃线程执行到。如果放在 `if` 里面：

```cpp
// 错误写法！
if (tid < stride) {
    sdata[tid] += sdata[tid + stride];
    __syncthreads();  // 只有部分线程执行 — 死锁！
}
```

`tid >= stride` 的线程不会执行 barrier，导致 barrier 永远等不齐所有线程 → 死锁或未定义行为。

**Conditional `__syncthreads()` 的规则**：所有线程必须执行到同一个 `__syncthreads()` 调用点。允许不同线程走不同的代码路径到达同一个 barrier，但不允许部分线程跳过 barrier。

### 11.6 Bank Conflict 分析

NVIDIA shared memory 被划分为 **32 个 bank**，每个 bank 宽 4 字节。bank 映射规则：

```text
地址 (byte)     → bank
sdata[0]  (0)   → bank 0
sdata[1]  (4)   → bank 1
sdata[2]  (8)   → bank 2
...
sdata[31] (124) → bank 31
sdata[32] (128) → bank 0  (循环)
sdata[33] (132) → bank 1
...
```

**当前实现是否有 bank conflict？**

在 `stride = blockDim.x / 2` 开始递减的方案中：

- stride = 128：T0 读 sdata[0] 和 sdata[128]。bank(0) = 0, bank(128) = 0。**同一线程读两个同 bank 地址 — 但这不构成 bank conflict**，因为 bank conflict 是指不同线程在同一 cycle 访问同一 bank 的不同地址。
- 关键检查：同一 warp 中的 32 个线程（如 T0-T31）在同一轮中：
  - T0: 读 sdata[0]（bank 0）和 sdata[128]（bank 0）
  - T1: 读 sdata[1]（bank 1）和 sdata[129]（bank 1）
  - ...
  - T31: 读 sdata[31]（bank 31）和 sdata[159]（bank 31）

每个线程访问的两个地址碰巧在同一 bank，但 **不同线程访问不同 bank**。所以一个 warp 内不会发生 bank conflict。

**如果改用 stride=1 递增方案（Mark Harris 论文中的第一种方案）**：

```cpp
// 有 bank conflict 的写法
for (int stride = 1; stride < blockDim.x; stride *= 2) {
    if (tid % (2 * stride) == 0) {
        sdata[tid] += sdata[tid + stride];
    }
}
```

当 stride = 1：T0 读 sdata[0] 和 sdata[1]，T2 读 sdata[2] 和 sdata[3]... 看起来没问题。但当 stride = 32：只有 T0 和 T64 等少数线程工作，且 T0 读 sdata[0]（bank 0）和 sdata[32]（bank 0） — **两次访问同一 bank**。但更严重的是线程活跃模式导致 warp divergence。

**结论**：当前项目使用的 "stride 从大到小" 方案在 bank conflict 方面是友好的。

### 11.7 线程活跃度与 Warp Divergence

```text
threads_per_block = 256 (8 个 warp, 每 warp 32 线程)

Round 1 (stride=128): T0-T127 活跃 → 所有 8 个 warp 中前 4 个全活跃
Round 2 (stride=64):  T0-T63 活跃 → 前 2 个 warp 全活跃
Round 3 (stride=32):  T0-T31 活跃 → 只有 warp 0 全活跃
Round 4 (stride=16):  T0-T15 活跃 → warp 0 内 16 线程活跃 (divergence!)
Round 5 (stride=8):   T0-T7 活跃  → warp 0 内 8 线程活跃
Round 6 (stride=4):   T0-T3 活跃  → warp 0 内 4 线程活跃
Round 7 (stride=2):   T0-T1 活跃  → warp 0 内 2 线程活跃
Round 8 (stride=1):   T0 活跃     → warp 0 内 1 线程活跃
```

后 5 轮中只有 warp 0 在工作，且内部存在 divergence（部分 lane 被 mask）。这是后续 warp shuffle 优化的动机之一。

### 11.8 性能对比意义

| 维度 | Naive | Shared Memory |
|---|---|---|
| block 内并行度 | 只有线程 0 工作 | 多个线程共同归约 |
| 中间数据位置 | 主要依赖 global memory 读取 | shared memory 内反复读写 |
| 同步 | 无 block 内协作 | 每轮需要 `__syncthreads()` |
| 归约复杂度 | O(n) per block (串行) | O(log n) per block (并行) |
| 内存带宽利用 | 单线程无法饱和总线 | 所有线程并行加载，coalesced |
| 主要学习点 | baseline 低效 | block-level cooperation |

它不保证在所有输入规模下都更快。对于很小的 `n`，kernel launch overhead 可能盖过 shared memory 优势；对于更复杂的访问模式，还要考虑 bank conflict、occupancy 和 shared memory 资源占用。

---

## 12. Warp Shuffle Reduce：`src/reduce_shuffle.cu` 逐行精讲

![[图片/SVG/CUDA Week 2 Parallel Reduction 项目解析-05.svg|950]]

### 12.1 `warp_reduce_sum` 设备函数完整分析

```cpp
template <typename T>
__device__ T warp_reduce_sum(T val) {
    // 5 轮 shuffle：offset = 16, 8, 4, 2, 1
    // 0xffffffff 表示所有 32 个 lane 都参与
    for (int offset = warpSize / 2; offset > 0; offset >>= 1) {
        val += __shfl_down_sync(0xffffffff, val, offset);
    }
    return val;
}
```

**`__device__`**：表示此函数只能在 GPU 上调用（由 kernel 或其他 `__device__` 函数调用），不能从 CPU 调用。

**`warpSize`**：CUDA 内置常量，当前所有 NVIDIA GPU 上值为 32。编译器可以将其视为编译期常量进行循环展开。

**`__shfl_down_sync(mask, val, offset)` 详解**：

```text
作用：让 lane i 获取 lane (i + offset) 的 val 值
参数：
  mask = 0xffffffff：32 位掩码，每一位代表一个 lane。全 1 表示所有 32 个 lane 都参与。
  val：当前线程要共享的值
  offset：位移量

返回值：
  如果 i + offset < 32：返回 lane (i + offset) 的 val
  如果 i + offset >= 32：返回当前线程自己的 val（无效数据，但不会崩溃）
```

**`0xffffffff` 掩码的含义**：

从 CUDA 9.0 / Volta 架构（sm_70）起，NVIDIA 引入了 Independent Thread Scheduling，warp 内线程不再保证完全 lockstep。`_sync` 后缀的 shuffle 要求开发者显式指定哪些 lane 参与，runtime 会在 shuffle 前插入同步点，确保指定的 lane 都到达这一点。

`0xffffffff` = 所有 32 个 lane 都参与，是最常见的用法。如果只有部分 lane 有有效数据，可以用更精确的 mask。

### 12.2 `__shfl_down_sync` 数据流完整可视化

以 8 个 lane 为例（实际是 32，缩小展示）：

```text
初始值:  lane: [0]  [1]  [2]  [3]  [4]  [5]  [6]  [7]
         val:   3    1    4    1    5    9    2    6

=== Round 1: offset = 4 ===
  lane 0: val += shfl_down(val, 4) → val[0] += val[4] → 3+5 = 8
  lane 1: val += shfl_down(val, 4) → val[1] += val[5] → 1+9 = 10
  lane 2: val += shfl_down(val, 4) → val[2] += val[6] → 4+2 = 6
  lane 3: val += shfl_down(val, 4) → val[3] += val[7] → 1+6 = 7
  lane 4-7: val += val(自身, 无效)  → 不影响 lane 0 的最终结果

  val:   8   10    6    7    5    9    2    6

=== Round 2: offset = 2 ===
  lane 0: val += shfl_down(val, 2) → 8+6 = 14
  lane 1: val += shfl_down(val, 2) → 10+7 = 17
  lane 2-7: 结果对 lane 0 无影响

  val:  14   17    6    7    ...

=== Round 3: offset = 1 ===
  lane 0: val += shfl_down(val, 1) → 14+17 = 31
  lane 1-7: 结果对 lane 0 无影响

  val:  31   ...

最终：lane 0 的 val = 31 = 3+1+4+1+5+9+2+6 ✓
```

**关键观察**：每轮 shuffle 后，只有低位 lane 的值有意义。最终只有 **lane 0** 持有完整的 sum。其他 lane 的值是部分和，不能直接使用。

对于完整的 32 lane，5 轮 shuffle（offset = 16, 8, 4, 2, 1）即可完成归约。

### 12.3 Kernel 完整源码与逐行分析

```cpp
template <typename T>
__global__ void reduce_shuffle_kernel(const T* input, T* partial_sums, int n) {
    extern __shared__ char shared_raw[];
    T* sdata = reinterpret_cast<T*>(shared_raw);

    int tid = threadIdx.x;
    int gid = blockIdx.x * blockDim.x + threadIdx.x;

    // 读取输入，越界为 0
    T val = (gid < n) ? input[gid] : T(0);

    // 第一层：warp 内归约
    val = warp_reduce_sum(val);

    // 每个 warp 的 lane 0 写入 shared memory
    int lane = tid % warpSize;
    int warp_id = tid / warpSize;

    if (lane == 0) {
        sdata[warp_id] = val;
    }
    __syncthreads();

    // 第二层：前 num_warps 个线程读 shared memory，再做一次 warp 归约
    int num_warps = blockDim.x / warpSize;
    val = (tid < num_warps) ? sdata[tid] : T(0);

    if (warp_id == 0) {
        val = warp_reduce_sum(val);
    }

    // 线程 0 写出 block 的 partial sum
    if (tid == 0) {
        partial_sums[blockIdx.x] = val;
    }
}
```

### 12.4 两层归约结构详解

整个 kernel 分为三个阶段：

**阶段 1：Global → Register**

```cpp
T val = (gid < n) ? input[gid] : T(0);
```

每个线程读取一个元素到自己的寄存器变量 `val` 中。Coalesced access，和 shared 版本的加载阶段一致。

**阶段 2：Warp 内归约（第一层 shuffle）**

```cpp
val = warp_reduce_sum(val);
```

每个 warp（32 线程）独立执行 `warp_reduce_sum`。256 线程 = 8 个 warp，执行后：

```text
Warp 0: lane 0 持有 warp 0 的 32 个元素之和
Warp 1: lane 0 持有 warp 1 的 32 个元素之和
...
Warp 7: lane 0 持有 warp 7 的 32 个元素之和
```

**这一步不需要 `__syncthreads()`**，因为 warp 内天然同步。

**阶段 3：跨 Warp 汇总（shared memory + 第二层 shuffle）**

```cpp
int lane = tid % warpSize;    // 当前线程在 warp 中的位置 (0-31)
int warp_id = tid / warpSize; // 当前线程所在 warp 的编号 (0-7)

if (lane == 0) {
    sdata[warp_id] = val;     // 每个 warp 的 lane 0 写一个值
}
__syncthreads();              // 确保所有 warp 都写完
```

此时 `sdata[0..7]` 中存放了 8 个 warp sum。接下来需要把这 8 个值再归约为 1 个：

```cpp
int num_warps = blockDim.x / warpSize;  // = 8
val = (tid < num_warps) ? sdata[tid] : T(0);

if (warp_id == 0) {
    val = warp_reduce_sum(val);
}
```

只有 **warp 0** 的 **lane 0-7** 有有效数据（lane 8-31 读到 `T(0)`）。warp 0 再做一次 `warp_reduce_sum`，将 8 个 warp sum 归约为最终结果。

**为什么用 `if (warp_id == 0)` 而不是 `if (tid < num_warps)`**：

如果写成 `if (tid < num_warps)` 就只有 8 个线程调用 `warp_reduce_sum`，但 `warp_reduce_sum` 内部使用了 `mask = 0xffffffff`，期望 32 个 lane 都参与。必须让整个 warp 0（32 个线程）都执行 `warp_reduce_sum`，只是 lane 8-31 的输入值为 0（不影响结果）。

### 12.5 通信路径对比

```text
=== Shared Memory Reduce 的通信路径 ===

Thread register          （每个线程有自己的值）
    ↓ store
Shared memory            （所有线程的值集中在 shared memory）
    ↓ __syncthreads()    （等待所有线程写完）
    ↓ load + store       （归约轮：读两个值、加法、写回）
    ↓ __syncthreads()    （等待当前轮完成）
    ↓ ... 重复 8 轮 ...
最终 sdata[0] 是结果

总同步点：8 次 __syncthreads() + 1 次加载后同步 = 9 次 barrier
Shared memory 访问次数：256 + 128 + 64 + ... + 1 ≈ 511 次读 + 255 次写

=== Warp Shuffle Reduce 的通信路径 ===

Thread register          （每个线程有自己的值）
    ↓ __shfl_down_sync   （warp 内寄存器直接交换，5 轮）
    ↓ 无需 syncthreads  （warp 内天然同步）
Warp 0-7 的 lane 0 持有各 warp sum
    ↓ store to shared    （只写 8 个值）
    ↓ __syncthreads()    （唯一一次 barrier）
    ↓ load from shared   （只读 8 个值）
    ↓ __shfl_down_sync   （warp 0 再做 5 轮）
最终 thread 0 的 val 是结果

总同步点：1 次 __syncthreads()
Shared memory 访问次数：8 次写 + 8 次读 = 16 次
```

### 12.6 资源使用对比

Shared memory 版本需要：

```text
threads_per_block * sizeof(T) = 256 * 4 = 1024 bytes
```

Shuffle 版本只需要：

```text
(threads_per_block / 32) * sizeof(T) = 8 * 4 = 32 bytes
```

| 类型 | Shared Memory Reduce | Warp Shuffle Reduce | 节省比例 |
|---|---:|---:|---:|
| `float` | 1024 bytes | 32 bytes | 96.9% |
| `int` | 1024 bytes | 32 bytes | 96.9% |

更少的 shared memory 消耗意味着：

1. 每个 SM 可以同时驻留更多 block（更高的 occupancy）。
2. 更少的 bank conflict 可能性。
3. 更多的 shared memory 预算留给其他用途。

### 12.7 为什么 Shuffle 更快

| 因素 | Shared Memory Reduce | Warp Shuffle Reduce |
|---|---|---|
| 通信延迟 | ~20-30 cycles (shared memory) | ~1 cycle (register shuffle) |
| 同步开销 | 9 次 `__syncthreads()` | 1 次 `__syncthreads()` |
| Shared memory 压力 | 高（1024 字节，频繁读写） | 低（32 字节，读写各 8 次） |
| Warp divergence | 后 5 轮只有 warp 0 活跃，且内部 diverge | 所有 warp 并行做第一层，只有最后第二层 diverge |
| 指令数 | 更多（load/store shared memory） | 更少（直接 shuffle 指令） |

### 12.8 实现前提与限制

当前实现默认 `threads_per_block` 是 32 的整数倍：

```cpp
int num_warps = threads_per_block / 32;  // 整除
```

如果 `threads_per_block = 200`（不是 32 的倍数），`num_warps = 6`（截断），最后 8 个线程（200 - 192）形成一个不完整 warp。当前代码不处理这种情况。

**第二层 shuffle 的 lane 数限制**：`warp_reduce_sum` 用 `0xffffffff` mask 假设 32 个 lane 都参与。如果 `num_warps < 32`（当前 = 8），lane 8-31 的输入为 0 是安全的。但如果 `num_warps > 32`（需要 `threads_per_block > 1024`，超出 CUDA 限制），当前方案就无法处理。实际上 CUDA 限制 `threads_per_block ≤ 1024`，所以 `num_warps ≤ 32`，恰好一个 warp 就能完成第二层。

---

## 13. Correctness Test：`tests/` 逐行精讲

### 13.1 文件结构

三个测试文件结构基本一致：

```text
test_reduce_naive.cu
test_reduce_shared.cu
test_reduce_shuffle.cu
```

以 `test_reduce_naive.cu` 为例做完整分析，其他两个文件只是把 `reduce_naive` 替换为 `reduce_shared` / `reduce_shuffle`。

### 13.2 `run_test<T>` 模板函数完整分析

```cpp
template <typename T>
bool run_test(int n, const char* type_name) {
    auto h_input = generate_input<T>(n);
    T cpu_sum = sum_reference<T>(h_input, n);

    DeviceBuffer<T> d_input(n);
    CUDA_CHECK(cudaMemcpy(d_input.get(), h_input.data(),
                          n * sizeof(T), cudaMemcpyHostToDevice));

    T gpu_sum = reduce_naive<T>(d_input.get(), n);

    char name[256];
    std::snprintf(name, sizeof(name), “naive<%s> n=%d”, type_name, n);
    return check_result<T>(gpu_sum, cpu_sum, name);
}
```

逐步分解：

**`auto h_input = generate_input<T>(n)`**：生成 host 侧测试输入。`auto` 推导为 `std::vector<T>`。NRVO 避免拷贝。

**`T cpu_sum = sum_reference<T>(h_input, n)`**：CPU 串行求和得到正确答案。**这一步在 GPU 运算之前**，确保 reference 不依赖 GPU 状态。

**`DeviceBuffer<T> d_input(n)`**：在 GPU 上分配 `n * sizeof(T)` 字节。如果分配失败，`CUDA_CHECK` 会抛异常，`DeviceBuffer` 析构时不需要释放（ptr_ 仍为 nullptr）。

**`cudaMemcpy(..., cudaMemcpyHostToDevice)`**：将 host 数据拷贝到 GPU。这是同步操作，返回时数据已经在 GPU 上。

**`T gpu_sum = reduce_naive<T>(d_input.get(), n)`**：调用被测函数。`d_input.get()` 返回 `T*` 类型的 GPU 指针。

**`std::snprintf(name, sizeof(name), ...)`**：构造测试名字符串，如 `”naive<float> n=1024”`。用 `snprintf` 而不是 `std::string` 拼接，保持 C 风格输出的一致性。`sizeof(name) = 256` 防止缓冲区溢出。

**`return check_result<T>(gpu_sum, cpu_sum, name)`**：比较 GPU 和 CPU 结果，打印 PASS/FAIL，返回布尔值。

### 13.3 `main()` 函数完整分析

```cpp
int main() {
    std::printf(“=== Test reduce_naive ===\n\n”);

    bool all_pass = true;

    // float 测试
    std::printf(“--- float ---\n”);
    all_pass &= run_test<float>(1, “float”);
    all_pass &= run_test<float>(1024, “float”);
    all_pass &= run_test<float>(1 << 20, “float”);
    all_pass &= run_test<float>(1000, “float”);

    // int 测试
    std::printf(“\n--- int ---\n”);
    all_pass &= run_test<int>(1, “int”);
    all_pass &= run_test<int>(1024, “int”);
    all_pass &= run_test<int>(1 << 20, “int”);
    all_pass &= run_test<int>(1000, “int”);

    std::printf(“\n%s\n”, all_pass ? “All tests PASSED” : “Some tests FAILED”);
    return all_pass ? 0 : 1;
}
```

**`all_pass &= run_test<>()`**：位与赋值。只要任何一个 `run_test` 返回 `false`，`all_pass` 就变成 `false` 且不会再变回 `true`。等价于 `all_pass = all_pass && run_test<>()`，但不会短路（所有测试都会执行，即使前面已经 FAIL）。

**`return all_pass ? 0 : 1`**：Unix 惯例，返回 0 表示成功，非 0 表示失败。`make test` 通过进程返回码判断是否通过。

### 13.4 测试矩阵设计意图

| `n` | 目的 | 可能暴露的 bug |
|---:|---|---|
| `1` | 单元素边界 | `num_blocks = 1`，只有一个 block 且只有 1 个有效元素，其余 255 个线程越界 |
| `1024` | 刚好 4 个 block | 所有 block 都满载（256 线程 × 4 block = 1024），无越界问题 |
| `1 << 20` | 大规模（1,048,576） | 大量 block（4096 个），partial sums 数组很大，CPU 汇总链路压力 |
| `1000` | 非 block 对齐 | 最后一个 block 只有 `1000 - 256*3 = 232` 个有效元素，测试越界处理 |

两种类型 × 四种大小 = 8 个测试用例。这个矩阵覆盖了：

1. **边界条件**：单元素、非对齐。
2. **规模梯度**：小（1）、中（1K）、大（1M）。
3. **类型差异**：浮点误差 vs 整数精确。

### 13.5 三个测试文件的代码复用模式

三个文件几乎一模一样，唯一差异是调用的 reduce 函数和打印的名字：

```cpp
// test_reduce_naive.cu
T gpu_sum = reduce_naive<T>(d_input.get(), n);
std::snprintf(name, sizeof(name), “naive<%s> n=%d”, type_name, n);

// test_reduce_shared.cu
T gpu_sum = reduce_shared<T>(d_input.get(), n);
std::snprintf(name, sizeof(name), “shared<%s> n=%d”, type_name, n);

// test_reduce_shuffle.cu
T gpu_sum = reduce_shuffle<T>(d_input.get(), n);
std::snprintf(name, sizeof(name), “shuffle<%s> n=%d”, type_name, n);
```

这种”几乎重复但不完全一样”的模式，在小项目中是合理的。如果要消除重复，可以把 `run_test` 参数化为函数指针，但会增加代码复杂度。当前的做法符合 CLAUDE.md 的”保持简单”原则。

> [!warning] Benchmark 之前必须先过 correctness
> Reduction 的错误经常表现为”速度更快”，因为少做了加法或跳过了边界数据。这个项目把 correctness test 和 benchmark 分开，但 benchmark 里仍保留 correctness status，这是正确做法。

---

## 14. Benchmark：`benchmarks/bench_reduce.cu` 逐行精讲

![[图片/SVG/CUDA Week 2 Parallel Reduction 项目解析-02.svg|868]]

### 14.1 文件结构概览

`bench_reduce.cu` 由以下部分组成：

```text
BenchResult<T>       → 数据结构：存储单次 benchmark 结果
bench_one<T>()       → 核心函数：对单个 reduce 函数做 warmup + timed runs
bandwidth_gb_s()     → 工具函数：计算有效带宽
run_benchmark<T>()   → 驱动函数：遍历 sizes × kernels，打印表格
main()               → 入口：分别对 float 和 int 运行 benchmark
```

### 14.2 `BenchResult<T>` 数据结构

```cpp
template <typename T>
struct BenchResult {
    T value;        // 最后一次 timed run 的返回值（用于 correctness check）
    float avg_ms;   // 平均耗时
    float min_ms;   // 最小耗时
    float max_ms;   // 最大耗时
};
```

模板参数 `T` 是 reduce 结果的类型（`float` 或 `int`），用来存储 GPU 返回值以便后续做 correctness check。

### 14.3 `bench_one<T>()` 完整分析

```cpp
template <typename T, typename ReduceFunc>
BenchResult<T> bench_one(ReduceFunc func, const T* d_input, int n,
                         int warmup = 5, int repeat = 20) {
    // Warmup runs (not timed)
    for (int i = 0; i < warmup; ++i) {
        func(d_input, n);
    }
    CUDA_CHECK(cudaDeviceSynchronize());

    // Timed runs: each invocation gets its own event pair
    std::vector<float> timings(repeat);
    T result = T(0);

    for (int i = 0; i < repeat; ++i) {
        cudaEvent_t start, stop;
        CUDA_CHECK(cudaEventCreate(&start));
        CUDA_CHECK(cudaEventCreate(&stop));

        CUDA_CHECK(cudaEventRecord(start));
        result = func(d_input, n);
        CUDA_CHECK(cudaEventRecord(stop));
        CUDA_CHECK(cudaEventSynchronize(stop));

        float ms = 0.0f;
        CUDA_CHECK(cudaEventElapsedTime(&ms, start, stop));
        timings[i] = ms;

        CUDA_CHECK(cudaEventDestroy(start));
        CUDA_CHECK(cudaEventDestroy(stop));
    }

    float sum = 0.0f;
    float min_ms = timings[0];
    float max_ms = timings[0];
    for (float t : timings) {
        sum += t;
        min_ms = std::min(min_ms, t);
        max_ms = std::max(max_ms, t);
    }

    return {result, sum / repeat, min_ms, max_ms};
}
```

**模板参数 `ReduceFunc`**：

这是一个模板类型参数，不指定具体类型。它可以是函数指针、lambda、或任何可调用对象。只要 `func(d_input, n)` 能编译通过即可。这种技术叫做**鸭子类型**（duck typing）：不管 `ReduceFunc` 是什么类型，只要它表现得像一个接受 `(const T*, int)` 参数的函数就行。

**Warmup 阶段**：

```cpp
for (int i = 0; i < warmup; ++i) {
    func(d_input, n);
}
CUDA_CHECK(cudaDeviceSynchronize());
```

Warmup 的作用：

1. **GPU 频率提升（Boost Clock）**：GPU 在空闲后可能降频。warmup 让 GPU 先”热起来”，达到稳定的 boost 频率。
2. **JIT 编译缓存**：某些 CUDA runtime 操作（如首次 kernel launch）有额外开销。
3. **Cache 预热**：L1/L2 cache 和 TLB 在首次访问时是冷的。
4. **内存页表建立**：GPU 页表可能在首次访问时才建立映射。

最后的 `cudaDeviceSynchronize()` 确保所有 warmup kernel 都执行完毕。

**每次 timed run 独立创建 Event**：

```cpp
cudaEvent_t start, stop;
CUDA_CHECK(cudaEventCreate(&start));
CUDA_CHECK(cudaEventCreate(&stop));

CUDA_CHECK(cudaEventRecord(start));    // 在 GPU 时间线上记录起始点
result = func(d_input, n);             // 执行被测函数
CUDA_CHECK(cudaEventRecord(stop));     // 记录结束点
CUDA_CHECK(cudaEventSynchronize(stop)); // 等待 GPU 执行到 stop event

float ms = 0.0f;
CUDA_CHECK(cudaEventElapsedTime(&ms, start, stop)); // 计算 start→stop 的时间差
```

**为什么每次都 create/destroy Event 而不是复用**：这是一种防御性做法。Event 对象在 Record 后会绑定到特定的 GPU 时间点，多次 Record 同一个 Event 在某些 CUDA 版本中可能有边界行为。每次创建新的 Event 更安全。

**CUDA Event 计时 vs CPU 计时**：

```text
CPU 计时（如 std::chrono）：
  T_cpu = kernel_launch_overhead + kernel_execution + sync_wait
  包含 CPU 侧调度延迟，受 OS 调度抖动影响

CUDA Event 计时：
  T_event = GPU 上 start event 到 stop event 的时间
  直接在 GPU 时间线上测量，精度约 0.5 μs
  不受 CPU 调度抖动影响
```

**统计计算**：

```cpp
float sum = 0.0f;
float min_ms = timings[0];
float max_ms = timings[0];
for (float t : timings) {
    sum += t;
    min_ms = std::min(min_ms, t);
    max_ms = std::max(max_ms, t);
}
return {result, sum / repeat, min_ms, max_ms};
```

手动循环计算 avg/min/max，没有用 `<algorithm>` 的 `std::accumulate`、`std::min_element` 等。简单直接，一次遍历完成所有统计。

### 14.4 `bandwidth_gb_s()` 带宽计算

```cpp
static float bandwidth_gb_s(int n, std::size_t elem_size, float avg_ms) {
    if (avg_ms <= 0.0f) return 0.0f;
    double bytes = static_cast<double>(n) * static_cast<double>(elem_size);
    double seconds = static_cast<double>(avg_ms) * 1e-3;
    return static_cast<float>(bytes / seconds / 1e9);
}
```

**`static`**：文件作用域的静态函数，只在当前 `.cu` 文件中可见。

**计算推导**：

```text
bytes   = n × sizeof(T)          // 总读取数据量
seconds = avg_ms × 10^-3         // 毫秒转秒
GB/s    = bytes / seconds / 10^9  // 字节/秒 转 GB/s
```

**为什么用 `double` 中间计算**：`n = 1 << 26 = 67,108,864`，乘以 `sizeof(float) = 4` 得到 `268,435,456` 字节。`float` 可以精确表示到约 `16,777,216`（2^24），所以超过这个值的整数在 `float` 中会丢失精度。用 `double` 避免这个问题。

**`avg_ms <= 0.0f` 守卫**：防止除以零。如果计时异常返回 0 或负值，直接返回 0 带宽。

### 14.5 `run_benchmark<T>()` 驱动函数完整分析

```cpp
template <typename T>
void run_benchmark(const char* type_name) {
    struct SizeEntry {
        const char* label;
        int n;
    };

    const SizeEntry sizes[] = {
        {“1K”,  1024},
        {“1M”,  1 << 20},
        {“64M”, 1 << 26},
    };
```

**局部 struct**：`SizeEntry` 只在这个函数内使用，作为局部类型定义。将标签和数值关联，比用两个平行数组更不容易出错。

```cpp
    auto wrap_naive   = [](const T* d, int nn) -> T { return reduce_naive<T>(d, nn); };
    auto wrap_shared  = [](const T* d, int nn) -> T { return reduce_shared<T>(d, nn); };
    auto wrap_shuffle = [](const T* d, int nn) -> T { return reduce_shuffle<T>(d, nn); };

    KernelEntry kernels[] = {
        {“Naive”,   +wrap_naive},
        {“Shared”,  +wrap_shared},
        {“Shuffle”, +wrap_shuffle},
    };
```

**Lambda 包装的原因**：

`reduce_naive<T>` 的签名是 `T(const T*, int, int)`（三个参数，第三个有默认值），但 `bench_one` 期望的是 `T(const T*, int)`（两个参数）。Lambda 做了参数适配：

```cpp
auto wrap_naive = [](const T* d, int nn) -> T {
    return reduce_naive<T>(d, nn);  // 省略第三个参数，使用默认值 256
};
```

**`+wrap_naive` 中的一元 `+` 运算符**：

这是一个 C++ 技巧。无捕获的 lambda 可以隐式转换为函数指针，但通常需要显式转换或在特定上下文中才会触发。一元 `+` 运算符会触发这个转换：

```cpp
auto lambda = [](int x) -> int { return x; };
// decltype(lambda)  是一个编译器生成的匿名类类型
// decltype(+lambda) 是 int(*)(int)，即函数指针类型
```

`KernelEntry` 的 `func` 成员类型是 `T (*)(const T*, int)`（函数指针），所以 `+wrap_naive` 将 lambda 转换为匹配的函数指针。

```cpp
    for (const auto& sz : sizes) {
        int n = sz.n;
        auto h_input = generate_input<T>(n);
        T cpu_ref = sum_reference<T>(h_input, n);

        DeviceBuffer<T> d_input(n);
        CUDA_CHECK(cudaMemcpy(d_input.get(), h_input.data(),
                              n * sizeof(T), cudaMemcpyHostToDevice));

        for (const auto& kern : kernels) {
            auto r = bench_one<T>([&](const T* d, int nn) {
                return kern.func(d, nn);
            }, d_input.get(), n);
```

**双重嵌套循环**：外层遍历 size（1K, 1M, 64M），内层遍历 kernel（Naive, Shared, Shuffle）。对于每个 (size, kernel) 组合，运行一次完整的 warmup + timed benchmark。

**`bench_one` 的 lambda 参数**：`[&](const T* d, int nn) { return kern.func(d, nn); }` 用引用捕获 `kern`，把函数指针调用包装在 lambda 中。

```cpp
            // Correctness check
            bool ok = true;
            if constexpr (std::is_floating_point_v<T>) {
                T denom = std::abs(cpu_ref) > T(1e-6) ? std::abs(cpu_ref) : T(1e-6);
                ok = std::abs(r.value - cpu_ref) / denom < T(1e-3);
            } else {
                ok = (r.value == cpu_ref);
            }
```

**Benchmark 内的 correctness check**：与 `check_result` 逻辑完全一致（相对误差 `1e-3` / 整数精确匹配），但没有复用 `check_result` 函数（因为不需要打印详细的 gpu/cpu 值）。结果只以 `OK` 或 `!!` 形式显示在表格中。

这体现了 CLAUDE.md 的要求：**”benchmark 输出必须包含 correctness status”**。

```cpp
            float bw = bandwidth_gb_s(n, sizeof(T), r.avg_ms);

            std::printf(“%-7s | %-8s | %9.3f | %9.3f | %9.3f | %9.2f | %s\n”,
                        sz.label, kern.name,
                        r.avg_ms, r.min_ms, r.max_ms, bw,
                        ok ? “OK” : “!!”);
```

**格式化输出**：`%-7s` 左对齐 7 字符宽，`%9.3f` 右对齐 9 字符宽 3 位小数。这些宽度恰好让表格对齐，配合表头的分隔线构成整齐的输出。

### 14.6 `main()` 入口

```cpp
int main() {
    std::printf(“Parallel Reduce Benchmark (Enhanced)\n”);
    std::printf(“=====================================\n”);
    std::printf(“Warmup: 5 runs | Repeat: 20 runs per (size, kernel)\n”);

    run_benchmark<float>(“float”);
    run_benchmark<int>(“int”);

    std::printf(“\nDone.\n”);
    return 0;
}
```

先跑 `float` 再跑 `int`。两种类型的 benchmark 完全独立，各自重新生成输入、分配 GPU 内存、做 warmup。

### 14.7 测试规模选择的意义

| Size | `n` | 数据量 | 观察重点 |
|---|---:|---:|---|
| `1K` | `1024` | 4 KB | kernel launch overhead 占主导，三种实现差距极小 |
| `1M` | `1,048,576` | 4 MB | 归约算法差异开始显现，内存带宽部分利用 |
| `64M` | `67,108,864` | 256 MB | 内存带宽成为主要瓶颈，归约效率差异最大化 |

### 14.8 Effective Bandwidth 的含义与局限

**含义**：

```text
effective_bw = data_read / time
```

对于 reduction，每个元素只被读一次，没有写输出（partial sums 很少可忽略），所以：

```text
effective_bw ≈ n * sizeof(T) / kernel_time
```

这衡量的是”kernel 把数据从内存搬到计算单元的速率”。理想情况下应接近 GPU 峰值内存带宽。

**局限**：

当前 benchmark 计时范围是整个 `reduce_xxx` host wrapper，包括：

```text
kernel launch      ← GPU 计算
cudaDeviceSynchronize  ← 等待 GPU
cudaMemcpy D2H     ← partial sums 拷回（但数据量很小）
CPU 累加           ← CPU 侧最终汇总
```

所以 benchmark 的 effective bandwidth 比纯 kernel-only 的带宽偏低。对于大 n（64M），kernel 时间占绝对主导，这个偏差很小；对于小 n（1K），overhead 占主导，effective bandwidth 完全无法反映 kernel 本身的效率。

### 14.9 输出格式

benchmark 表头固定为：

```text
N       | Kernel   |   Avg(ms) |   Min(ms) |   Max(ms) |  BW(GB/s) | Status
```

这比只输出一个耗时更适合做性能分析：

1. `Avg` 看总体水平。
2. `Min` 接近最佳情况（最少干扰）。
3. `Max` 暴露抖动（可能来自 GPU 降频、OS 调度、cache 未命中等）。
4. `BW` 方便和 GPU 峰值内存带宽比较。
5. `Status` 防止错误实现混入性能对比。

这正好对应 [[3.5.1 CUDA Week 2 辅助笔记 - Benchmark + Profiling]] 中的 benchmark matrix 要求。

### 14.10 实测 benchmark 结果

本次测试配置：

```text
Warmup: 5 runs
Repeat: 20 runs per (size, kernel)
Kernels: Naive / Shared / Shuffle
Data types: float / int
Status: 全部 OK
```

`float` 测试结果：

| N | Kernel | Avg(ms) | Min(ms) | Max(ms) | BW(GB/s) | Status |
|---|---|---:|---:|---:|---:|---|
| 1K | Naive | 0.024 | 0.020 | 0.057 | 0.17 | OK |
| 1K | Shared | 0.018 | 0.018 | 0.020 | 0.22 | OK |
| 1K | Shuffle | 0.018 | 0.018 | 0.019 | 0.22 | OK |
| 1M | Naive | 0.223 | 0.219 | 0.232 | 18.78 | OK |
| 1M | Shared | 0.194 | 0.193 | 0.199 | 21.61 | OK |
| 1M | Shuffle | 0.182 | 0.180 | 0.184 | 23.08 | OK |
| 64M | Naive | 6.364 | 6.292 | 6.453 | 42.18 | OK |
| 64M | Shared | 5.481 | 5.040 | 5.727 | 48.98 | OK |
| 64M | Shuffle | 4.450 | 4.420 | 4.506 | 60.32 | OK |

`int` 测试结果：

| N | Kernel | Avg(ms) | Min(ms) | Max(ms) | BW(GB/s) | Status |
|---|---|---:|---:|---:|---:|---|
| 1K | Naive | 0.020 | 0.019 | 0.022 | 0.21 | OK |
| 1K | Shared | 0.017 | 0.017 | 0.019 | 0.24 | OK |
| 1K | Shuffle | 0.017 | 0.017 | 0.018 | 0.24 | OK |
| 1M | Naive | 0.202 | 0.199 | 0.207 | 20.75 | OK |
| 1M | Shared | 0.175 | 0.173 | 0.179 | 23.96 | OK |
| 1M | Shuffle | 0.168 | 0.164 | 0.217 | 24.94 | OK |
| 64M | Naive | 6.256 | 6.240 | 6.298 | 42.91 | OK |
| 64M | Shared | 5.414 | 4.995 | 5.942 | 49.58 | OK |
| 64M | Shuffle | 4.413 | 4.395 | 4.482 | 60.82 | OK |

观察结果：

1. `1K` 规模下三种实现差距很小，主要受 launch overhead、计时粒度和固定开销影响。
2. `1M` 规模下 Shared 和 Shuffle 开始稳定快于 Naive，说明 block 内协作归约已经带来收益。
3. `64M` 规模下差距最明显，Shuffle 的有效带宽约为 `60 GB/s`，高于 Shared 的约 `49 GB/s` 和 Naive 的约 `42 GB/s`。
4. `float` 和 `int` 的趋势基本一致，说明这组 benchmark 主要体现的是归约组织方式和内存访问效率，而不是数据类型本身的算术成本。

初步结论：在当前实现中，warp shuffle reduce 通过 warp 内寄存器交换减少 shared memory 访问和同步开销，因此在大规模输入上表现最好。下一步应结合 Nsight Compute 检查 memory throughput、occupancy 和 stall reason，确认这个结论是否和硬件指标一致。

---

## 15. Nsight Compute Profiling：`docs/profiling.md`

Week 2 不只要求“哪个版本更快”，还要求解释“为什么更快”。`docs/profiling.md` 就是为这个目的准备的证据模板。

### 15.1 Profiling 命令

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

### 15.2 模板记录哪些指标

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

### 15.3 三种 kernel 的预期观察点

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

## 16. CMakeLists.txt 与 Makefile 联动精讲

### 16.1 完整 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.18)

project(ParallelReduce LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

# 库目标（三个 reduce 实现）
add_library(reduce_lib
    src/reduce_naive.cu
    src/reduce_shared.cu
    src/reduce_shuffle.cu
)
target_include_directories(reduce_lib PUBLIC include src)

# 测试目标（每天一个）
foreach(day naive shared shuffle)
    add_executable(test_reduce_${day} tests/test_reduce_${day}.cu)
    target_link_libraries(test_reduce_${day} PRIVATE reduce_lib)
endforeach()

# Benchmark 目标
add_executable(bench_reduce benchmarks/bench_reduce.cu)
target_link_libraries(bench_reduce PRIVATE reduce_lib)
```

**`target_include_directories(reduce_lib PUBLIC include src)`**：`PUBLIC` 的意义是——所有链接到 `reduce_lib` 的目标（测试和 benchmark）也自动继承 `include` 和 `src` 两个目录。所以 `test_reduce_naive.cu` 可以直接 `#include "reduce_naive.cuh"`，而不需要单独设置 include 路径。

**`target_link_libraries(... PRIVATE reduce_lib)`**：`PRIVATE` 表示链接关系不传递。如果有第三方目标链接到 `test_reduce_naive`（实际没有），它不会自动获得 `reduce_lib` 的 include 路径。对于终端可执行文件，`PRIVATE` 是最常用的选择。

### 16.2 完整 Makefile

```makefile
BUILD_DIR := build
CUDA_ARCH ?= 75

.PHONY: all configure build test test-naive test-shared test-shuffle bench \
       profile-naive profile-shared profile-shuffle clean

all: build

configure:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_CUDA_ARCHITECTURES=$(CUDA_ARCH)

build: configure
	cmake --build $(BUILD_DIR)

test: build
	./$(BUILD_DIR)/test_reduce_naive
	./$(BUILD_DIR)/test_reduce_shared
	./$(BUILD_DIR)/test_reduce_shuffle

test-naive: build
	./$(BUILD_DIR)/test_reduce_naive

test-shared: build
	./$(BUILD_DIR)/test_reduce_shared

test-shuffle: build
	./$(BUILD_DIR)/test_reduce_shuffle

bench: build
	./$(BUILD_DIR)/bench_reduce

profile-naive: build
	ncu --set full ./$(BUILD_DIR)/test_reduce_naive

profile-shared: build
	ncu --set full ./$(BUILD_DIR)/test_reduce_shared

profile-shuffle: build
	ncu --set full ./$(BUILD_DIR)/test_reduce_shuffle

clean:
	rm -rf $(BUILD_DIR)
```

**`:=` vs `?=`**：

- `BUILD_DIR := build`：立即赋值，`BUILD_DIR` 始终是 `build`。
- `CUDA_ARCH ?= 75`：条件赋值，只在 `CUDA_ARCH` 未被环境变量或命令行参数设置时才赋值为 `75`。用户可以 `make build CUDA_ARCH=86` 覆盖。

**`.PHONY`**：声明这些 target 不对应实际文件。没有 `.PHONY` 时，如果目录里恰好有叫 `test` 的文件，`make test` 会认为 target 已最新而跳过执行。

**依赖链**：`test: build` → `build: configure`。所以执行 `make test` 会自动触发 `configure` → `build` → `test`。

**`-DCMAKE_CUDA_ARCHITECTURES=$(CUDA_ARCH)`**：告诉 CMake 生成 `sm_75` 的 GPU 代码（对应 GTX 1660 SUPER / Turing 架构）。如果 GPU 是 Ampere，应改为 `86`。

**`ncu --set full`**：Nsight Compute CLI，`--set full` 收集所有可用性能指标。profile 目标使用 test 程序（而不是 benchmark）作为 profiling 入口，因为 test 程序更简洁，kernel 调用更少，更容易聚焦分析。

---

## 17. README 推荐阅读顺序

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

## 18. Week 2 验收目标

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

## 19. 下一步学习建议

### 19.1 先跑 correctness，再跑 benchmark

建议顺序：

```bash
make test
make bench
```

如果 correctness 不通过，不要看 benchmark 数字。Reduction 的性能分析必须建立在正确结果上。

### 19.2 再做 Nsight Compute

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

### 19.3 进入 Week 3

Week 2 学的是 reduction 中的线程协作和 profiling。下一阶段 [[Week 3 - Transpose + Memory Coalescing]] 会把重点转到 global memory 访问模式：

```text
Week 2: 线程如何协作汇总数据
Week 3: 线程如何合并访问 global memory
```

这两个主题组合起来，就是后续 tiled matmul、RMSNorm、Softmax、Attention kernel 的基础。

---

## 20. GPU 内存层次与本项目的映射关系

理解三种 reduce 实现的性能差异，需要先理解 NVIDIA GPU 的内存层次：

![[图片/SVG/CUDA Week 2 Parallel Reduction 项目解析-06.svg|900]]

### 20.1 内存层次金字塔

```text
                    ┌─────────────────┐
                    │   Registers     │  ← Warp Shuffle Reduce
                    │   ~1 cycle      │     直接在寄存器间交换
                    │   64KB/SM       │
                    ├─────────────────┤
                    │ Shared Memory   │  ← Shared Memory Reduce
                    │   ~20-30 cycles │     block 内线程通过 smem 通信
                    │   48-96KB/SM    │
                    ├─────────────────┤
                    │   L1 Cache      │  ← 自动缓存，不可编程
                    │   ~30-40 cycles │
                    ├─────────────────┤
                    │   L2 Cache      │
                    │   ~200 cycles   │
                    │   全 GPU 共享    │
                    ├─────────────────┤
                    │ Global Memory   │  ← Naive Reduce
                    │   ~400-800 cyc  │     线程 0 串行读 DRAM
                    │   GDDR6 / HBM   │
                    └─────────────────┘
速度：从上到下递减      容量：从上到下递增
```

### 20.2 三种实现在内存层次中的位置

| 实现 | 数据存放层 | 通信延迟 | 同步开销 |
|---|---|---|---|
| Naive | Global Memory → 寄存器（单线程） | ~400-800 cycles/element | 无 |
| Shared | Global → Shared → 寄存器 | ~20-30 cycles/access | 每轮 `__syncthreads()` |
| Shuffle | Global → 寄存器 → 寄存器（shuffle） | ~1 cycle/shuffle | warp 内天然同步 |

### 20.3 为什么 Shuffle 不能完全消除 Shared Memory

一个 block 可以有多个 warp（默认 8 个）。Shuffle 只能在 **同一 warp 的 32 个 lane** 之间通信。要跨 warp 通信，必须用 shared memory 或 global memory。

所以 shuffle reduce 的架构是：

```text
Warp 0-7 各自 shuffle 归约 (寄存器内)
    → 每个 warp 的 lane 0 写入 shared memory (跨 warp 的唯一桥梁)
    → Warp 0 读 shared memory，再 shuffle 归约
```

如果 block 只有 1 个 warp（32 线程），则完全不需要 shared memory。但 256 线程需要跨 8 个 warp 汇总，必须经过 shared memory 中转。

### 20.4 SM 资源竞争与 Occupancy

每个 SM（Streaming Multiprocessor）的资源是有限的：

```text
GTX 1660 SUPER (sm_75, Turing):
  - 64 CUDA Cores per SM
  - 22 SMs
  - 65536 registers per SM
  - 64KB shared memory per SM (configurable L1/shared split)
  - 最多 1024 threads per block
  - 最多 16 blocks per SM (受资源限制)
  - 最多 32 warps per SM
```

三种实现的资源消耗比较：

| 资源 | Naive | Shared | Shuffle |
|---|---|---|---|
| Shared Memory / block | 0 bytes | 1024 bytes | 32 bytes |
| 理论最大 blocks/SM | 受寄存器限制 | 受 smem 限制 (64KB/1KB=64, 但 block 数上限 16) | 接近上限 |
| 有效工作线程比 | 1/256 = 0.4% | 递减（第1轮128, ... 第8轮1） | 几乎全部 |

---

## 21. 三种实现的完整执行流程对比

以 `n = 2048, threads_per_block = 256` 为例：

```text
num_blocks = 2048 / 256 = 8 个 block

=== Naive Reduce ===
[GPU] Block 0: T0 串行读 input[0..255]，累加，写 partial_sums[0]
[GPU] Block 1: T0 串行读 input[256..511]，累加，写 partial_sums[1]
...
[GPU] Block 7: T0 串行读 input[1792..2047]，累加，写 partial_sums[7]
[D2H] cudaMemcpy: partial_sums[0..7] → CPU
[CPU] total = partial_sums[0] + ... + partial_sums[7]

=== Shared Memory Reduce ===
[GPU] Block 0:
  加载: 256 线程并行读 input[0..255] → sdata[0..255]
  归约: 8 轮二分树 (128→64→32→16→8→4→2→1 线程活跃)
  写出: T0 写 partial_sums[0] = sdata[0]
... (8 个 block 并行)
[D2H] cudaMemcpy
[CPU] total = sum(partial_sums)

=== Warp Shuffle Reduce ===
[GPU] Block 0:
  加载: 256 线程并行读 input[0..255] → val (寄存器)
  第一层: 8 个 warp 各自 shuffle 归约 (32→1 per warp, 5 轮)
  桥接: 8 个 lane 0 写 sdata[0..7]
  第二层: Warp 0 读 sdata[0..7]，shuffle 归约 (8→1, 3 有效轮)
  写出: T0 写 partial_sums[0]
... (8 个 block 并行)
[D2H] cudaMemcpy
[CPU] total = sum(partial_sums)
```

### 21.1 时间复杂度分析

| 实现 | Block 内串行步骤 | Block 内并行深度 |
|---|---|---|
| Naive | 256 次 global load + 255 次加法 | 1（无并行） |
| Shared | 1 次 global load + 8 轮归约 | log2(256) = 8 |
| Shuffle | 1 次 global load + 5 轮 warp shuffle + 1 次 smem + ~3 轮 shuffle | ~9 |

虽然 Shared 和 Shuffle 的步骤数接近，但 Shuffle 的每一步延迟（1 cycle）远低于 Shared 的每一步（20-30 cycles + barrier 开销）。

---

## 22. 关键要点总结

1. Week 2 的项目核心不是”求一个 sum”，而是通过 sum reduction 学会 GPU 线程协作。
2. Naive 版本故意低效，用来证明 block 内没有并行归约时 GPU 资源会被浪费。
3. Shared memory 版本把 block 内数据放到 on-chip memory 中做树形归约，但需要 `__syncthreads()` 保证每轮结果可见。
4. Warp shuffle 版本用寄存器级 lane 通信完成 warp 内归约，再用少量 shared memory 做跨 warp 汇总。
5. 当前工程的最终汇总在 CPU 侧完成，因此 benchmark 数字要按 API 整体开销理解，不要误当成纯 kernel-only 时间。
6. Correctness test 覆盖 `float`、`int`、单元素、大规模和非 block 对齐长度。
7. Benchmark 同时输出 avg/min/max/bandwidth/status，适合观察 launch overhead、吞吐和抖动。
8. Nsight Compute 的价值是把”我觉得它快”变成”指标支持它为什么快”。
9. `cuda_check.cuh` 通过宏 + inline 函数实现零成本错误检查，出错时触发 RAII 清理链。
10. `DeviceBuffer` 实现完整的 Rule of Five，用 RAII 确保 GPU 显存不泄漏。
11. `test_utils.cuh` 用 `if constexpr` 实现编译期分支，用 `double` 累加保证 float 参考值精度。
12. 三种 kernel 共享统一的模板 API 和 host wrapper 结构，差异只体现在 kernel 本身和 shared memory 分配。
13. Benchmark 的 lambda + `+` 运算符技巧解决了三参数到二参数的适配问题。
14. GPU 内存层次（Register → Shared → L1 → L2 → Global）直接决定了三种实现的性能差异。

---

## 关联知识

- [[CUDA Week 1 Hello World 项目解析]]
- [[Week 2 - Reduction + Profiling]]
- [[3.5 CUDA Week 2 前置知识 - Reduction + Profiling]]
- [[3.5.1 CUDA Week 2 辅助笔记 - Benchmark + Profiling]]
- [[3.5.2 CUDA Week 2 辅助笔记 - Mark Harris Reduction 优化路线]]
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
