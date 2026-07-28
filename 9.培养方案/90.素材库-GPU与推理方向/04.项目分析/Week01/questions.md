---
tags:
  - AI-infra
  - 素材库-GPU与推理方向
  - 项目分析
  - Week01
---
# Week01 必答问题

> 配合 [[CUDA Week 1 Hello World 项目解析]]。这些是 README、阶段计划里的核心面试问题，逐题展开。

---

## 1. CUDA 的执行模型是什么？

**答：** host（CPU）发起一次 kernel launch，GPU 随即产生**大量线程**，每个线程执行**同一份** kernel 代码、处理**不同的数据**——这就是 SIMT（Single Instruction, Multiple Threads）。线程按 **block** 分组，block 组成 **grid**，调度的最小硬件单位是 **warp（32 线程，锁步执行）**。

`vector add` 把 CPU 的 `for (i) c[i]=a[i]+b[i]` 翻译成"n 个线程各算一个元素"：循环被并行展开，迭代变量 `i` 变成每个线程的全局下标 `idx`。

---

## 2. 一个线程怎么知道自己负责哪个元素？

**答：** 靠全局下标公式：

```cpp
const int idx = blockIdx.x * blockDim.x + threadIdx.x;
```

`blockIdx.x` 是 block 编号，`blockDim.x` 是每 block 线程数，`threadIdx.x` 是 block 内编号。三者折算出该线程在整个数组里的位置 `idx`，于是它负责 `c[idx] = a[idx] + b[idx]`。

---

## 3. 为什么 kernel 里必须有 `if (idx < n)`？

**答：** 线程总数是 block 大小的整数倍，`blocks = ceil(n / threads_per_block)`，几乎总会**多启动一些线程**。例如 `n=1000, tpb=256 → blocks=4 → 1024` 个线程，多出 24 个。这些线程的 `idx ∈ [1000,1023]` 超出数组范围，若不拦下会越界读写非法地址 → 结果错误或非法内存访问。`if (idx < n)` 正是这道守卫。

抓这个 bug 的关键是测**非 block 对齐长度**（如 1000）；只测 1024 这种整数倍长度永远发现不了。

---

## 4. `blocks = (n + threads_per_block - 1) / threads_per_block` 为什么这样写？

**答：** 这是整数**向上取整**的惯用法，等价于 `ceil(n / tpb)`。直接 `n / tpb` 会向下取整：`1000/256 = 3`，只启动 768 个线程，覆盖不了后 232 个元素。加上 `tpb - 1` 再整除，保证线程数 ≥ n，把所有元素都覆盖到；多出的线程由 `if (idx < n)` 处理。

---

## 5. `DeviceBuffer<T>` 解决了什么问题？为什么禁拷贝、允移动？

**答：** 它用 **RAII** 把显存生命周期绑死：构造 `cudaMalloc`、析构 `cudaFree`，对象出作用域就自动释放。解决裸 `cudaMalloc/cudaFree` 的三个隐患——忘记释放（泄漏）、多指针指向同一块显存（double free）、malloc/free 散落难维护。

- **禁拷贝**：显存指针是独占资源，拷贝会让两对象持有同一指针，各自析构时 double free。
- **允移动**："接管指针 + 置空源"是合法的所有权转移，源对象析构时不再 free。

这与 `std::unique_ptr` 完全同构：所有权唯一、可移动不可复制。

---

## 6. `CUDA_CHECK` 在做什么？为什么必须包住每个 Runtime 调用？

**答：** CUDA Runtime 是 C 风格、靠返回 `cudaError_t` 报错。不检查的话，错误会**延迟**到后续某次同步/拷贝才暴露，那时报错点和真正出错点已经对不上。`CUDA_CHECK` 用宏把"错误信息 + 表达式原文 `#expr` + `__FILE__:__LINE__`"打包抛成 C++ 异常，让错误**就地爆炸**、带完整上下文，定位成本骤降。

---

## 7. `cudaGetLastError()` 和 `cudaDeviceSynchronize()` 有什么区别？

**答：** kernel launch 是**异步**的，二者抓不同阶段的错误：

- `cudaGetLastError()` 紧跟 launch，抓 **launch 配置错误**（block 维度非法、shared memory 超限等），这些在入队时就能发现。
- `cudaDeviceSynchronize()` 阻塞等 kernel 执行完，抓**执行期错误**（如越界访问），这类错误只有 GPU 真正跑到才会暴露。

所以两者都要查，缺一不可。

---

## 8. 为什么 correctness test 和 benchmark 要分开，benchmark 还得保留正确性检查？

**答：** 职责不同：test 只回答"对不对"（逐元素对比 CPU 参考），benchmark 只回答"多快"（kernel 时间 + 有效带宽）。但二者都链接同一个 `vector_add_lib`，保证测的和跑的是同一份 kernel。

benchmark 仍保留最小正确性检查，是因为"**算错但很快**"的 kernel 毫无价值——性能数字只有结果正确时才有意义。仓库 CLAUDE.md 也硬性要求 benchmark 输出带 `Check` 列。

---

## 9. benchmark 为什么要 warm-up、为什么只测 kernel？

**答：**

- **warm-up**：第一次 launch 含 CUDA context 初始化、cache 预热、GPU 升频等一次性开销（可达毫秒级），必须先空跑一次并同步把它摊掉，否则均值被严重拉高且不可复现。
- **只测 kernel**：`benchmark_kernel_once` 接收已备好的 device memory，计时区间不含 `cudaMemcpy`。H2D/D2H 走 PCIe、与 kernel 执行无关，计入会掩盖 kernel 本身的带宽表现。
- **CUDA event** 记录 GPU stream 时间线，比 host 端 `std::chrono` 更准确反映 kernel 真实执行时间；repeat 取均值抹平抖动。

---

## 10. `vector add` 是 memory-bound 还是 compute-bound？怎么判断？

**答：** **memory-bound**。看算术强度 = 计算量 / 访存量：每个元素读 `a`、读 `b`、写 `c` 共 3 次访存（12 字节），却只对应 1 次加法 FLOP，强度极低。性能完全由显存带宽决定，而非算力。

有效带宽因此用 `bytes = 3 · n · sizeof(float)` 计算。实测中应看到 Memory Throughput 远高于 Compute Throughput，带宽逼近 GPU 显存峰值——这就是 memory-bound 的判定证据。
