---
tags:
  - AI-infra
  - 素材库-GPU与推理方向
  - 项目分析
  - Week01
---
# Week01 渐进式练习

> 配合 [[CUDA Week 1 Hello World 项目解析]] 使用。答案基于仓库源码推理；凡涉及实测时间/带宽数字之处，给出**预期方向**，真实数值请在你的 GPU 上 `make bench` 后回填到 [[9.培养方案/90.素材库-GPU与推理方向/04.项目分析/Week01/profiling|profiling]]。

> [!note] 运行说明
> 建议按你的架构构建，例如 GTX 1660 Super：
> ```bash
> make test  CUDA_ARCH=75
> make bench CUDA_ARCH=75
> ```

---

## Day 1：工程骨架 + 错误检查

### 练习 1.1：为什么把 kernel 编成库而不是直接写进测试

`src/vector_add.cu` 被编成 `vector_add_lib`，test 和 benchmark 都链接它。如果不这么做、各自复制一份 kernel，会有什么问题？

**答案：**

会出现"测的代码 ≠ 跑的代码"。一旦两份实现漂移（改了一处忘了另一处），correctness test 通过但 benchmark 测的是另一份逻辑，性能数字毫无意义。编成单一库强制"测试和 benchmark 指向同一份 kernel 实现"，这是受控实验的前提。

### 练习 1.2：CUDA_CHECK 记录了哪三样信息

看宏 `#define CUDA_CHECK(expr) check_cuda((expr), #expr, __FILE__, __LINE__)`，它在出错时能告诉你什么？

**答案：**

三样：`cudaGetErrorString(status)` 给出人类可读错误信息、`#expr` 给出失败的表达式原文、`__FILE__:__LINE__` 给出出错位置。把 C 风格返回码转成带完整上下文的 C++ 异常，定位成本从"翻整个调用链"降到"看一行"。

### 练习 1.3：不检查返回值会怎样

如果 `cudaMemcpy` 失败却不检查返回值，错误会在什么时候、以什么形式暴露？

**答案：**

CUDA 错误会**延迟**到后续某次同步或拷贝调用才返回，届时报的错误码和真正的出错点已经对不上，极难定位。所以每个 Runtime 调用都套 `CUDA_CHECK`，让错误"就地爆炸"。

---

## Day 2：DeviceBuffer 与 RAII

### 练习 2.1：裸 cudaMalloc/cudaFree 的三个隐患

不用 `DeviceBuffer`、直接 `cudaMalloc` + `cudaFree`，列出至少三个问题。

**答案：**

1. 中途 `return` 或抛异常时忘记 `cudaFree` → 显存泄漏。
2. 多个指针误指向同一块显存 → double free。
3. `cudaMalloc/cudaFree` 散落在业务逻辑里 → 难维护、易漏配对。

`DeviceBuffer` 用 RAII 把"构造申请、析构释放"绑死，对象生命周期 = 显存生命周期，三个问题一起消失。

### 练习 2.2：为什么禁拷贝

`DeviceBuffer(const DeviceBuffer&) = delete;`。如果允许拷贝会发生什么？

**答案：**

拷贝会让两个对象持有同一个 `ptr_`，作用域结束时各自析构都调用 `cudaFree(ptr_)` → **double free**，行为未定义。显存是独占资源，所以必须禁拷贝，和 `std::unique_ptr` 同理。

### 练习 2.3：移动构造做了哪两步

```cpp
DeviceBuffer(DeviceBuffer&& o) noexcept : ptr_(o.ptr_), count_(o.count_) { o.ptr_ = nullptr; o.count_ = 0; }
```

为什么必须把 `o.ptr_` 置空？

**答案：**

移动 = "接管 + 置空"。第一步接管 `o` 的指针和计数；第二步把 `o.ptr_` 置空，否则 `o` 析构时还会 `cudaFree` 那块已被新对象接管的显存 → double free。置空保证所有权唯一。

### 练习 2.4：get() 返回的指针能不能 cudaFree

调用方拿到 `d_a.get()` 后，能不能对它手动 `cudaFree`？

**答案：**

不能。`get()` 只是**借出**底层指针给 `cudaMemcpy`/kernel 用，不转移所有权。手动 `cudaFree` 后 `DeviceBuffer` 析构时会再 free 一次 → double free。所有权始终归 `DeviceBuffer`。

---

## Day 3：kernel 与索引

### 练习 3.1：手算全局下标

`threads_per_block = 256`，`blockIdx.x = 3`，`threadIdx.x = 10`，这个线程负责哪个元素？

**答案：**

`idx = blockIdx.x * blockDim.x + threadIdx.x = 3 * 256 + 10 = 778`。它负责 `c[778] = a[778] + b[778]`。

### 练习 3.2：删掉 `if (idx < n)` 会怎样

在 `n = 1000` 上去掉边界守卫，会发生什么？

**答案：**

`blocks = ceil(1000/256) = 4`，总线程 `4*256 = 1024 > 1000`。多出的 24 个线程 `idx ∈ [1000,1023]` 会越界读写 `a/b/c`，访问非法地址 → 结果错误甚至非法内存访问。所以非对齐长度必须保留 `if (idx < n)`。

### 练习 3.3：blocks 为什么向上取整

`blocks = (n + threads_per_block - 1) / threads_per_block`。为什么不能直接 `n / threads_per_block`？

**答案：**

整数除法向下取整。`1000/256 = 3`，只启动 `3*256 = 768` 个线程，覆盖不了后 232 个元素。`(n + tpb - 1) / tpb` 是向上取整惯用法，保证线程数 ≥ n，多出的由 `if (idx < n)` 拦下。

### 练习 3.4：CPU 循环和 CUDA kernel 的对应关系

把 CPU 的 `for (i=0..n) c[i]=a[i]+b[i]` 映射到 CUDA，"循环变量 i"对应 kernel 里的什么？

**答案：**

CPU 的循环变量 `i` 对应 kernel 里的全局下标 `idx`。CPU 是"一个线程顺序跑 n 次迭代"，CUDA 是"n 个线程各跑一次、`idx` 取遍 `0..n-1`"。循环被**并行展开**成线程。

---

## Day 4：correctness test

### 练习 4.1：三个 test case 各测什么

`{1,2,3}+{4,5,6}`、长度 `1000`、长度 `1<<20`，分别想暴露什么？

**答案：**

- 极小例子：基本功能正确性，结果应为 `{5,7,9}`。
- `1000`：非 256 对齐，检验 `if (idx < n)` 边界守卫。
- `1<<20`（~100 万）：接近真实并行规模，验证大数据下仍正确、无越界。

### 练习 4.2：只测对齐长度的风险

如果只测 `1024`（恰好 `4*256`），能不能发现"忘记边界守卫"的 bug？

**答案：**

不能。`1024` 恰是 block 整数倍，没有多余线程，缺了 `if (idx < n)` 也照样对。必须测**非对齐长度**（如 1000）才能暴露这个 bug。这就是 1000 这个 case 存在的意义。

### 练习 4.3：浮点比较为什么用容差

test 用 `expect_close` / benchmark 用 `fabs(diff) > 1e-5` 而非 `==`。这里 float 加法会有误差吗？

**答案：**

`vector add` 只有一次加法、无累加，GPU 和 CPU 结果其实应当逐位相同。用容差是**通用浮点习惯**（防御性写法），主要价值在更复杂的 kernel（如 reduction 的累加顺序差异）；这里 `1e-5` 容差足够，真正想抓的是索引/边界写错导致的明显偏差。

---

## Day 5：benchmark

### 练习 5.1：有效带宽的系数为什么是 3

`bytes = 3.0 * n * sizeof(float)`，3 从哪来？

**答案：**

每个元素访存三次：读 `a[i]`（4B）、读 `b[i]`（4B）、写 `c[i]`（4B）。所以有效字节 `= 3 × n × sizeof(float)`，带宽 `= bytes / 时间`，反映 kernel 利用显存带宽的效率。

### 练习 5.2：warm-up 为什么必要

benchmark 在计时前先空跑一次并同步。去掉会怎样？

**答案：**

第一次 launch 含 CUDA context 初始化、cache 预热、GPU 升频等一次性开销，可达毫秒级，会把平均时间严重拉高、且不可复现。warm-up 把这些冷启动开销摊掉，让计时只反映稳态 kernel 性能。

### 练习 5.3：为什么只测 kernel、不含拷贝

`benchmark_kernel_once` 接收已备好的 device memory，计时区间不含 `cudaMemcpy`。为什么？

**答案：**

H2D/D2H 拷贝走 PCIe，带宽和性质都与 kernel 执行无关。若计入，测到的是"拷贝+计算"的混合量，掩盖 kernel 本身的带宽表现。只对 launch 计时才能公平评估 kernel。

### 练习 5.4：vector add 是 memory-bound 吗，怎么判断

不看实测，怎么判断 `vector add` 偏 memory-bound 还是 compute-bound？

**答案：**

看算术强度（arithmetic intensity）= 计算量 / 访存量。这里 3 次访存（12B）只对应 1 次加法 FLOP，强度极低 → **memory-bound**。预期实测带宽会接近 GPU 显存带宽上限，而 FLOPS 远未跑满。实测请回填 [[9.培养方案/90.素材库-GPU与推理方向/04.项目分析/Week01/profiling|profiling]]。

### 练习 5.5：cudaGetLastError 与 cudaDeviceSynchronize 分工

launch 后既调 `cudaGetLastError()` 又（在 host 流程里）调 `cudaDeviceSynchronize()`，能不能只留一个？

**答案：**

不能，二者抓不同阶段的错误。`cudaGetLastError()` 紧跟 launch，抓**launch 配置错误**（如 block 维度非法、shared memory 超限）；`cudaDeviceSynchronize()` 等 kernel 跑完，抓**执行期错误**（如越界访问）。launch 是异步的，执行期错误只能等同步才暴露。
