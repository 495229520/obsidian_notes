---
tags:
  - 培养方案
  - CUDA
  - Agent
  - infra
---

# Week 1 - CUDA + Agent workflow

> 第一周的目标是建立一个最小但完整的 GPU 开发闭环：用 CUDA 项目模板组织代码，用 `vector add` 验证环境，用 benchmark 框架衡量性能，再用 `CLAUDE.md` 和权限边界把 Agent 辅助开发变成可控流程。

---

## 学习目标

完成这一周后，应该能够回答四个问题：

1. **CUDA 程序如何组织？** 能看懂一个包含 `src/`、`include/`、`tests/`、`benchmarks/` 的项目模板。
2. **GPU kernel 如何运行？** 能解释 host/device 内存、kernel launch、grid/block 配置和同步。
3. **性能如何被证明？** 能用 warm-up、重复运行、CUDA event 和吞吐量指标写出基本 benchmark。
4. **Agent 如何安全参与开发？** 能用 `CLAUDE.md` 写清项目约束，并区分自动执行、需要确认和禁止执行的操作。

---

## 1. CUDA 开发环境与项目模板

CUDA 项目不要从单个 `.cu` 文件开始长期堆代码。第一周就应该建立一个小型模板，因为 GPU 项目通常同时包含：kernel 代码、host 侧封装、正确性测试、性能测试和构建脚本。

### 1.1 推荐目录结构

```text
cuda-week1/
├── CMakeLists.txt
├── include/
│   └── cuda_check.cuh
├── src/
│   ├── vector_add.cu
│   └── vector_add.cuh
├── tests/
│   └── test_vector_add.cu
└── benchmarks/
    └── bench_vector_add.cu
```

**设计思路**：

- `include/` 放通用工具，例如 CUDA 错误检查宏或 RAII 封装。
- `src/` 放真正的 kernel 与 host 调用函数。
- `tests/` 只验证结果是否正确，不负责测性能。
- `benchmarks/` 只测性能，但也要保留最小正确性校验，避免把错误结果测得很快。
- `CMakeLists.txt` 记录编译规则，避免每次手写 `nvcc` 命令。

这个结构和普通 C++ 项目类似，只是源文件多了 `.cu`，构建系统需要启用 CUDA 语言支持。CMake 的基础语法可以回看 [[14.3 CMake基础]]。

### 1.2 最小 CMake 模板

下面的模板把 CUDA 作为一等语言启用，并指定 CUDA 标准。它适合第一周练习，不追求复杂的跨平台封装。

```cmake
cmake_minimum_required(VERSION 3.24)
project(cuda_week1 LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

add_library(vector_add_lib
    src/vector_add.cu
)

target_include_directories(vector_add_lib PUBLIC include src)

add_executable(test_vector_add tests/test_vector_add.cu)
target_link_libraries(test_vector_add PRIVATE vector_add_lib)

add_executable(bench_vector_add benchmarks/bench_vector_add.cu)
target_link_libraries(bench_vector_add PRIVATE vector_add_lib)
```

**关键点**：

- `project(... LANGUAGES CXX CUDA)` 告诉 CMake 同时处理 C++ 和 CUDA 源文件。
- `vector_add_lib` 把核心实现做成库，测试和 benchmark 复用同一份代码，避免两边复制 kernel。
- `target_include_directories` 只暴露必要头文件路径，保持依赖关系清晰。
- 第一周不急着加入复杂选项，先确保能稳定构建、测试、运行。

---

## 2. 第一个 CUDA 程序：vector add

`vector add` 是 CUDA 的 Hello World：给定两个数组 `a`、`b`，在 GPU 上计算 `c[i] = a[i] + b[i]`。它简单到不会被算法细节干扰，但足够覆盖 CUDA 的核心执行模型。

### 2.1 Kernel：每个线程处理一个元素

CUDA kernel 是在 GPU 上执行的函数，用 `__global__` 标记。下面的 kernel 把全局线程编号 `idx` 映射到数组下标。

```cpp
__global__ void vector_add_kernel(const float* a, const float* b, float* c, int n) {
    const int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        c[idx] = a[idx] + b[idx];
    }
}
```

**关键点**：

- `blockIdx.x` 表示当前 block 的编号。
- `blockDim.x` 表示每个 block 里有多少线程。
- `threadIdx.x` 表示线程在当前 block 内的编号。
- `idx < n` 是边界保护，因为线程总数通常会向上取整，不一定刚好等于元素个数。

执行结构可以这样理解：

```text
Grid
└── Block 0: thread 0, thread 1, ..., thread 255
└── Block 1: thread 0, thread 1, ..., thread 255
└── Block 2: thread 0, thread 1, ..., thread 255
```

每个线程只做一次加法。真正的并行性来自大量线程同时执行，而不是单个线程更聪明。

### 2.2 Host 侧调用：分配、拷贝、启动、同步

CPU 侧代码负责准备数据、把数据拷贝到 GPU、启动 kernel，再把结果拷贝回来。下面用一个小的 RAII 封装管理 device memory，避免手动资源泄漏。

```cpp
#include <cuda_runtime.h>

#include <stdexcept>
#include <vector>

inline void check_cuda(cudaError_t status) {
    if (status != cudaSuccess) {
        throw std::runtime_error(cudaGetErrorString(status));
    }
}

template <typename T>
class DeviceBuffer {
public:
    explicit DeviceBuffer(std::size_t count) : count_(count) {
        check_cuda(cudaMalloc(&ptr_, count_ * sizeof(T)));
    }

    ~DeviceBuffer() {
        cudaFree(ptr_);
    }

    DeviceBuffer(const DeviceBuffer&) = delete;
    DeviceBuffer& operator=(const DeviceBuffer&) = delete;

    T* get() { return ptr_; }
    const T* get() const { return ptr_; }
    std::size_t size() const { return count_; }

private:
    T* ptr_ = nullptr;
    std::size_t count_ = 0;
};
```

**设计思路**：

- CUDA Runtime API 本身是 C 风格接口，会返回 `cudaError_t`，所以要在边界处转换成 C++ 异常或错误处理。
- `DeviceBuffer` 把 `cudaMalloc/cudaFree` 包在构造/析构里，符合 RAII 思想；RAII 的资源管理方式可以回看 [[独享智能指针]]。
- 禁止拷贝是必要的，否则两个对象会持有同一块 device memory，析构时发生重复释放。

### 2.3 完整的 vector add 函数

这个函数是外部调用入口：输入 host 侧 `std::vector<float>`，输出 host 侧结果。它隐藏了 device memory 的细节，让测试代码只关心输入输出。

```cpp
std::vector<float> vector_add(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size()) {
        throw std::invalid_argument("input sizes must match");
    }

    const auto n = static_cast<int>(a.size());
    std::vector<float> c(a.size());

    DeviceBuffer<float> d_a(a.size());
    DeviceBuffer<float> d_b(b.size());
    DeviceBuffer<float> d_c(c.size());

    check_cuda(cudaMemcpy(d_a.get(), a.data(), a.size() * sizeof(float), cudaMemcpyHostToDevice));
    check_cuda(cudaMemcpy(d_b.get(), b.data(), b.size() * sizeof(float), cudaMemcpyHostToDevice));

    constexpr int threads_per_block = 256;
    const int blocks = (n + threads_per_block - 1) / threads_per_block;
    vector_add_kernel<<<blocks, threads_per_block>>>(d_a.get(), d_b.get(), d_c.get(), n);
    check_cuda(cudaGetLastError());
    check_cuda(cudaDeviceSynchronize());

    check_cuda(cudaMemcpy(c.data(), d_c.get(), c.size() * sizeof(float), cudaMemcpyDeviceToHost));
    return c;
}
```

**关键点**：

- `std::vector` 负责 host 侧内存，适合第一周练习；如果对 `vector` 本身不熟，可以回看 [[14.1 vector]]。
- `blocks = (n + threads_per_block - 1) / threads_per_block` 是整数向上取整，确保所有元素都有线程处理。
- `cudaGetLastError()` 检查 kernel launch 是否成功。
- `cudaDeviceSynchronize()` 等待 GPU 执行完成，否则 host 代码可能在 kernel 还没结束时继续往下走。
- `cudaMemcpyDeviceToHost` 把结果从 GPU 拷回 CPU，测试代码才能检查。

> [!warning] 常见误区
> kernel launch 默认是异步的。只写 `vector_add_kernel<<<...>>>(...)` 并不代表计算已经完成；需要同步或后续拷贝来建立完成点。

---

## 3. Benchmark 框架：如何证明优化有效

性能测试不是“运行一次然后打印毫秒数”。GPU 程序尤其容易被首次初始化、数据拷贝、异步执行影响。第一周的 benchmark 目标是建立正确习惯，而不是追求极限性能。

### 3.1 Benchmark 要测什么

| 指标 | 含义 | 为什么重要 |
|---|---|---|
| 平均耗时 | 多次运行后的平均 kernel 时间 | 避免单次波动误导判断 |
| 最小耗时 | 多次运行中的最快时间 | 接近理想执行路径 |
| 吞吐量 | 每秒处理的数据量 | 比单纯毫秒数更适合比较规模 |
| 正确性 | 输出是否符合预期 | 错误结果再快也没有意义 |

对于 `vector add`，一次计算读取 `a` 和 `b`，写入 `c`，每个元素大约访问 `3 * sizeof(float)` 字节。吞吐量可以粗略写成：

```text
bandwidth = 3 * n * sizeof(float) / time
```

这里得到的是有效内存带宽，用来观察 kernel 是否主要受内存访问限制。

### 3.2 使用 CUDA event 计时

CUDA event 在 GPU 时间线上记录事件，比 CPU 侧 `std::chrono` 更适合测 kernel 本身耗时。下面只测 kernel，不包含 host/device 数据拷贝。

```cpp
float benchmark_vector_add_kernel(const float* d_a, const float* d_b, float* d_c, int n, int repeat) {
    constexpr int threads_per_block = 256;
    const int blocks = (n + threads_per_block - 1) / threads_per_block;

    cudaEvent_t start = nullptr;
    cudaEvent_t stop = nullptr;
    check_cuda(cudaEventCreate(&start));
    check_cuda(cudaEventCreate(&stop));

    vector_add_kernel<<<blocks, threads_per_block>>>(d_a, d_b, d_c, n);
    check_cuda(cudaDeviceSynchronize());

    check_cuda(cudaEventRecord(start));
    for (int i = 0; i < repeat; ++i) {
        vector_add_kernel<<<blocks, threads_per_block>>>(d_a, d_b, d_c, n);
    }
    check_cuda(cudaEventRecord(stop));
    check_cuda(cudaEventSynchronize(stop));

    float elapsed_ms = 0.0f;
    check_cuda(cudaEventElapsedTime(&elapsed_ms, start, stop));

    check_cuda(cudaEventDestroy(stop));
    check_cuda(cudaEventDestroy(start));
    return elapsed_ms / static_cast<float>(repeat);
}
```

**关键点**：

- 第一次 kernel launch 作为 warm-up，不计入最终结果。
- `repeat` 次循环可以降低计时噪声。
- `cudaEventRecord(start)` 和 `cudaEventRecord(stop)` 记录在 GPU stream 上，不是 CPU 墙钟时间。
- 这里只测 kernel 时间；如果要测端到端耗时，应把 `cudaMemcpy` 也纳入计时，并单独标注。

### 3.3 Benchmark 输出建议

第一周建议输出成表格，方便后续优化时对比。

```text
N           Kernel(ms)    Bandwidth(GB/s)    Check
1048576     0.082         153.4              PASS
4194304     0.310         162.3              PASS
16777216    1.210         166.3              PASS
```

**为什么要保留 `Check` 列**：

优化 GPU 程序时，很容易因为边界条件、同步或内存拷贝错误得到错误结果。把正确性检查放进 benchmark 输出，可以强迫自己每次性能比较都同时验证结果。

---

## 4. CLAUDE.md：把项目约束写给 Agent

`CLAUDE.md` 是 Agent 参与项目时读取的工程说明文件。它不只是“给 AI 看的 README”，而是把项目约束、命令、风格和安全边界写成可执行上下文。Claude Code 的基础命令可以回看 [[1.1 claude code语法]]。

### 4.1 CUDA 项目中的 CLAUDE.md 应写什么

```markdown
# CLAUDE.md

## Project

This is a CUDA learning project for Week 1.

## Build

- Configure: `cmake -S . -B build`
- Build: `cmake --build build`
- Test: `./build/test_vector_add`
- Benchmark: `./build/bench_vector_add`

## Code Style

- Use Modern C++ on host side.
- Prefer RAII wrappers for CUDA resources.
- Keep kernel code small and measurable.
- Do not mix correctness tests and benchmark results.

## Safety

- Do not delete benchmark data without confirmation.
- Do not run destructive shell commands.
- Ask before installing dependencies or changing GPU driver/toolkit settings.
```

**关键点**：

- `Build` 告诉 Agent 如何验证修改，减少“改完但不知道怎么测”的问题。
- `Code Style` 把 host 侧 Modern C++ 与 CUDA C API 的边界讲清楚。
- `Safety` 把高风险动作前置声明，避免 Agent 为了完成任务而擅自执行破坏性操作。
- `CLAUDE.md` 的本质接近 system prompt 的项目层扩展；相关原理可以回看 [[06 - System Prompt 不是文案，而是配置层]]。

### 4.2 好的 CLAUDE.md 是约束，不是口号

| 模糊写法 | 更好的写法 |
|---|---|
| “保持代码高质量” | “host 侧使用 RAII 管理 CUDA 资源，不手写裸 `cudaFree` 分散在业务函数中” |
| “运行测试” | “修改 kernel 后运行 `./build/test_vector_add` 和 `./build/bench_vector_add`” |
| “注意安全” | “安装依赖、删除文件、修改驱动配置前必须询问用户” |

Agent 不能自动猜出项目真正关心什么。写得越具体，Agent 越可能做出符合项目预期的动作。

---

## 5. Agent 权限边界：什么能自动做，什么必须确认

Agent workflow 的核心不是“让模型自由发挥”，而是建立一个受控循环：观察当前状态、提出计划、执行可逆动作、验证结果、汇报变化。Agent 的运行机制可以回看 [[02 - Agent 运行闭环]]。

### 5.1 权限边界分层

| 层级 | 示例 | 是否可自动执行 | 原因 |
|---|---|---|---|
| 只读探索 | 读取文件、搜索符号、查看构建脚本 | 通常可以 | 不改变系统状态 |
| 本地可逆修改 | 编辑学习代码、补充注释、调整 CMake | 可以，但要可回滚 | 影响局部文件，可通过 diff 检查 |
| 资源消耗操作 | 运行 benchmark、长时间编译 | 视情况确认 | 可能占用 GPU/CPU 较久 |
| 共享状态操作 | push、发 PR、发布结果 | 必须确认 | 会影响他人或远端系统 |
| 高风险操作 | 删除数据、重置仓库、改驱动、跳过权限检查 | 默认禁止或必须明确授权 | 难以恢复，影响范围大 |

### 5.2 CUDA 场景下的具体边界

第一周 CUDA 学习中，尤其要注意这些边界：

- **可以自动做**：读取 `.cu`、`.cuh`、`CMakeLists.txt`，搜索 kernel，解释报错，运行短测试。
- **建议先说明再做**：运行大型 benchmark、生成大量输出文件、长时间占用 GPU。
- **必须确认**：安装 CUDA Toolkit、修改系统环境变量、改显卡驱动、删除 benchmark 结果、提交或推送代码。
- **不要做**：绕过权限检查、强制删除未知文件、为了让测试通过而跳过正确性校验。

工具调用本身也需要可靠性保护：参数要受 schema 限制，失败要反馈给模型，危险动作要有人类确认。这个思路可以参考 [[05 - 工具调用的四层保险]]。

---

## 6. AI 提效后的 1-2 天工作流

> [!important] 节奏调整
> 如果已经借助 AI Agent 在 1 天内理解并完成 CUDA 入门理论、项目解析和 Week 1 Hello World 工程阅读，则 Week 1 不再需要按 7 天展开。新的目标是用 1-2 天完成闭环验收，然后尽快进入 [[Week 2 - Reduction + Profiling]]。

### Day 1：快速打通 CUDA Hello World 闭环

- 阅读 [[3.1 CUDA 零基础系统入门|CUDA 零基础系统入门]]，确认能解释 host/device、grid/block/thread、kernel launch、显存拷贝和同步。
- 阅读 [[CUDA Week 1 Hello World 项目解析]]，按项目结构理解 `include/`、`src/`、`tests/`、`benchmarks/` 的职责。
- 在本机或 Linux + GTX 1660S 上构建 `week01` 项目。
- 跑通 `test_vector_add`，确认小规模、非 block 对齐长度和大规模输入都正确。
- 跑通 `bench_vector_add`，记录 `N`、`Kernel(ms)`、`Bandwidth(GB/s)`、`Check`。

**验收标准**：能用自己的话讲清楚 `vector add` 从 host vector 到 device memory、kernel 执行、D2H 拷回、correctness test 和 CUDA event benchmark 的完整流程。

### Day 2：补齐工程边界与复盘

- 检查 `CLAUDE.md` 是否写清构建、测试、benchmark 命令。
- 记录 GPU 型号、CUDA 版本、Driver、CMake、编译命令和 benchmark 结果。
- 让 Agent review 一次项目结构，但性能结论和 correctness 判断必须人工确认。
- 把遇到的报错、benchmark 波动和解决过程整理成简短记录。
- 如果 Day 1 已全部完成，Day 2 可以直接开始 Week 2 的 reduction 预习。

**验收标准**：下一个 kernel 不需要重新搭项目框架；只需要复用 `cuda_check.cuh`、`DeviceBuffer`、test/benchmark 结构继续开发。

### 加速后的退出条件

满足以下条件即可结束 Week 1，不必为了“学满一周”而停留：

- [ ] 能构建 CUDA 项目。
- [ ] 能解释并实现 `vector_add_kernel`。
- [ ] 能解释 `blocks = (n + threads_per_block - 1) / threads_per_block` 和 `idx < n`。
- [ ] 能用 RAII 管理 device memory。
- [ ] correctness test 覆盖小规模、非 block 对齐长度和大规模输入。
- [ ] benchmark 使用 warm-up、repeat、CUDA event 和正确性检查。
- [ ] benchmark 结果记录了 GPU / CUDA / Driver / 编译参数。
- [ ] Agent 权限边界清楚：Agent 可生成脚手架和测试，但不能替代人工判断 kernel correctness 和性能结论。

---

## 7. 常见问题与排查

### 7.1 `nvcc` 找不到

**现象**：构建时报 `nvcc: command not found` 或 CMake 找不到 CUDA compiler。

**排查顺序**：

1. 确认 CUDA Toolkit 是否安装。
2. 确认 `nvcc --version` 能在终端运行。
3. 确认 CMake 使用的编译器路径正确。
4. 不要让 Agent 自动修改系统 PATH 或安装驱动，除非你明确授权。

### 7.2 kernel 没报错但结果不对

**常见原因**：

- grid/block 数量算错，部分元素没有被处理。
- 忘记 `idx < n`，越界写入。
- 忘记同步，host 侧过早读取结果。
- `cudaMemcpy` 方向写反。

**建议做法**：先用很小的 `N` 打印结果，再扩大规模做 benchmark。

### 7.3 benchmark 波动很大

**常见原因**：

- 把首次 CUDA 初始化时间算进去了。
- 只运行一次，没有重复统计。
- 同时有其他程序占用 GPU。
- 把 host/device 拷贝时间和 kernel 时间混在一起，却没有标注。

**建议做法**：区分 kernel-only benchmark 和 end-to-end benchmark，并在输出中写清楚。

---

## 8. 本阶段交付物

| 交付物 | 内容 | 验收方式 |
|---|---|---|
| CUDA 项目模板 | `CMakeLists.txt`、`src/`、`include/`、`tests/`、`benchmarks/` | 能成功构建 |
| vector add | kernel + host 封装 | 小规模和大规模结果正确 |
| benchmark 框架 | warm-up、repeat、CUDA event、带宽计算 | 输出稳定表格 |
| `CLAUDE.md` | 构建命令、测试命令、风格、安全边界 | Agent 能按说明执行验证 |
| 权限边界清单 | 自动/确认/禁止操作分类 | 开发前可直接引用 |

---

## 关键要点总结

1. CUDA 第一阶段不要追求复杂优化，先建立“能构建、能验证、能测量”的闭环。AI 提效后，这个阶段可以压缩到 1-2 天完成。
2. `vector add` 的价值在于覆盖 CUDA 执行模型：host/device、grid/block、kernel launch、同步和拷贝。
3. benchmark 必须同时包含 warm-up、重复运行、GPU 侧计时和正确性检查。
4. `CLAUDE.md` 是 Agent workflow 的项目约束层，应该写具体命令和明确边界。
5. Agent 权限边界越清楚，AI 辅助开发越稳定；高风险动作必须由用户确认。

---

## 参考

- [[CUDA 学习清单]]
- [[Week 2 - Reduction + Profiling]]
- NVIDIA CUDA C++ Programming Guide
- NVIDIA CUDA C++ Best Practices Guide
- [[14.3 CMake基础]]
- [[02 - Agent 运行闭环]]
- [[05 - 工具调用的四层保险]]
- [[06 - System Prompt 不是文案，而是配置层]]
