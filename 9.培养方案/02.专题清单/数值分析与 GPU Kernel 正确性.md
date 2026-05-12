---
title: 数值分析与 GPU Kernel 正确性
date: 2026-05-08
tags:
  - 培养方案
  - AIInfra
  - CUDA
  - Triton
  - 数值分析
  - GPU
  - Kernel
  - LLM推理
aliases:
  - GPU Kernel 数值正确性
  - 数值分析到 GPU Performance
status: active
---

# 数值分析与 GPU Kernel 正确性

> [!goal] 目标
> 把 [[第01次课-绪论与误差理论笔记]] 中的误差理论，落到 [[AI Agent Native AI Infra GPU Performance Engineer 培养方案]] 的代码实践里：写 CUDA / Triton kernel 时，不只追求“跑得快”，还要能证明**结果正确、误差可控、低精度收益真实、benchmark 可信**。

这篇笔记不是重新学数学课，而是建立一条工程桥梁：

```text
数值分析概念
→ GPU kernel 写法
→ correctness test
→ benchmark / profiling
→ 面试与上线判断
```

![[图片/SVG/9_2_1_1.svg|760]]

---

## 1. 为什么数学公式不等于可靠代码

在数学上，两个公式可能完全等价；但在计算机上，由于浮点数位宽有限，同一个数学问题的不同计算顺序会产生完全不同的误差行为。

[[第01次课-绪论与误差理论笔记]] 中的递推例子说明：

- 正向递推会把初始误差按阶乘级放大。
- 反向递推会让误差逐步衰减。
- 数学等价不代表计算稳定。

这件事在 GPU kernel 中同样成立。

例如 softmax：

$$
\operatorname{softmax}(x_i)=\frac{e^{x_i}}{\sum_j e^{x_j}}
$$

数学上可以改写成：

$$
\operatorname{softmax}(x_i)=\frac{e^{x_i-m}}{\sum_j e^{x_j-m}},\quad m=\max_j x_j
$$

两个公式数学等价，但工程上第二个更稳定，因为最大指数项变成 $e^0=1$，显著降低 overflow 风险。

![[图片/SVG/9_2_1_2.svg|760]]

---

## 2. 误差理论到 GPU 代码的映射

| 数值分析概念 | 数学含义 | GPU / LLM Kernel 中的代码问题 |
|---|---|---|
| 舍入误差 | 浮点数表示有限导致的误差 | FP16 / BF16 / FP8 / INT8 计算是否可靠 |
| 绝对误差 | $|x-x^*|$ | `atol` 怎么设置 |
| 相对误差 | $\frac{|x-x^*|}{|x^*|}$ | `rtol` 怎么设置 |
| 有效数字 | 近似值中可信的数字位数 | FP16 / BF16 的精度上限 |
| 误差传播 | 输入误差如何影响输出 | reduction、softmax、RMSNorm 中误差如何累积 |
| 条件数 / 病态问题 | 问题本身对扰动敏感 | 极端输入、长序列、特殊 shape 是否放大误差 |
| 数值稳定性 | 算法是否放大误差 | naive softmax vs max-shift softmax |
| 数值实验 | 用实验验证理论判断 | correctness + benchmark + Nsight |

> [!important] 核心判断
> GPU Performance Engineer 不是只看 kernel 的耗时，而是要同时回答：这个结果在数值上可信吗？误差是否可解释？优化后的吞吐提升有没有牺牲正确性或质量？

---

## 3. Softmax：从公式到稳定代码

### 3.1 不稳定写法

```python
import torch

x = torch.tensor([1000.0, 1001.0, 1002.0])
y = torch.exp(x) / torch.sum(torch.exp(x))
print(y)  # 可能出现 nan，因为 exp(1002) overflow
```

这对应误差理论里的问题：中间结果超出浮点表示范围，算法在执行过程中失控。

### 3.2 稳定写法

```python
import torch

x = torch.tensor([1000.0, 1001.0, 1002.0])
m = torch.max(x)
y = torch.exp(x - m) / torch.sum(torch.exp(x - m))
print(y)
```

这里的核心不是“换了一个近似公式”，而是做了一个**数学等价但数值稳定的等价变换**。

### 3.3 CUDA / Triton kernel 中的含义

Softmax kernel 通常至少包含三步：

```text
1. row-wise max reduce
2. exp(x - max) + row-wise sum reduce
3. normalize and store
```

伪代码：

```cpp
// 每一行做一次 softmax
float row_max = reduce_max(x[row, :]);
float sum = 0.0f;
for (int j = 0; j < hidden; ++j) {
    sum += expf(x[row, j] - row_max);
}
for (int j = 0; j < hidden; ++j) {
    y[row, j] = expf(x[row, j] - row_max) / sum;
}
```

必须做的 correctness case：

```python
cases = [
    torch.randn(4, 128),
    torch.zeros(4, 128),
    torch.full((4, 128), 1000.0),
    torch.tensor([[1000.0, 1001.0, 1002.0]]),
]
```

测试不能只用随机小数，因为随机输入可能掩盖 overflow、underflow 和边界问题。

---

## 4. RMSNorm：为什么 FP16 输入常用 FP32 累加

RMSNorm 公式：

$$
y = \frac{x}{\sqrt{\frac{1}{d}\sum_{i=1}^{d}x_i^2+\varepsilon}}\cdot w
$$

它的关键计算是 row-wise reduction：

$$
\sum_i x_i^2
$$

如果输入是 FP16，但累加也用 FP16，误差会在求和过程中不断积累。更常见的工程写法是：

```text
输入：FP16 / BF16
中间累加：FP32
输出：FP16 / BF16
```

### 4.1 PyTorch reference

```python
import torch


def rmsnorm_ref(x: torch.Tensor, weight: torch.Tensor, eps: float = 1e-6):
    x_float = x.float()
    variance = torch.mean(x_float * x_float, dim=-1, keepdim=True)
    y = x_float * torch.rsqrt(variance + eps)
    return (y * weight.float()).to(x.dtype)
```

这里 `.float()` 就是数值分析里的“控制舍入误差传播”：输入可以是低精度，但 reduction 的关键中间量使用更高精度。

### 4.2 CUDA kernel 里的意识

伪代码：

```cpp
float sum = 0.0f;
for (int j = threadIdx.x; j < hidden; j += blockDim.x) {
    float v = static_cast<float>(x[row * hidden + j]);
    sum += v * v;
}

sum = block_reduce_sum(sum);
float inv_rms = rsqrtf(sum / hidden + eps);
```

关键点：

- `x` 可以是 `half`。
- `sum` 应该是 `float`。
- `eps` 不能随意删掉。
- hidden size 越大，reduction 的误差累积越值得关注。

---

## 5. correctness test：把误差理论写进测试

### 5.1 不要用 `==`

错误写法：

```python
assert torch.equal(y_cuda, y_ref)
```

浮点数计算中，不同计算顺序、不同 dtype、不同硬件路径都可能产生微小差异。更合理的写法是：

```python
torch.testing.assert_close(
    y_cuda,
    y_ref,
    rtol=1e-3,
    atol=1e-3,
)
```

### 5.2 `atol` 和 `rtol` 的意义

来自误差理论：

- 绝对误差：$|x-x^*|$
- 相对误差：$\frac{|x-x^*|}{|x^*|}$

对应到 PyTorch：

```text
|actual - expected| <= atol + rtol * |expected|
```

所以：

- 接近 0 的数更依赖 `atol`。
- 大数更依赖 `rtol`。
- FP32、FP16、BF16、INT8 的阈值不能一刀切。

### 5.3 推荐测试模板

```python
import torch


def check_kernel(actual, expected, dtype):
    if dtype == torch.float32:
        rtol, atol = 1e-5, 1e-6
    elif dtype in (torch.float16, torch.bfloat16):
        rtol, atol = 1e-2, 1e-2
    else:
        raise ValueError(f"unsupported dtype: {dtype}")

    torch.testing.assert_close(actual, expected, rtol=rtol, atol=atol)
```

> [!warning] 注意
> 具体阈值要根据算子、shape、dtype 和 reference 路径调整。这里是模板，不是永久标准。笔记和 README 中必须写清楚阈值选择理由。

---

## 6. Quantization：近似替代不是免费午餐

量化推理常见形式：

$$
x_{fp16}\approx scale\cdot (q_{int8}-zero\_point)
$$

这本质上是数值分析里的“近似替代”：用更低 bit 的表示换取更低显存、更高吞吐或更低成本。

### 6.1 INT8 dequant toy code

```python
import torch


def quantize_int8(x: torch.Tensor):
    max_abs = x.abs().max()
    scale = max_abs / 127.0
    q = torch.clamp(torch.round(x / scale), -128, 127).to(torch.int8)
    return q, scale


def dequantize_int8(q: torch.Tensor, scale: torch.Tensor):
    return q.float() * scale

x = torch.randn(1024)
q, scale = quantize_int8(x)
x_hat = dequantize_int8(q, scale)

abs_err = (x_hat - x).abs().max()
rel_err = ((x_hat - x).abs() / x.abs().clamp_min(1e-6)).max()
print(abs_err, rel_err)
```

这里要记录的不只是速度：

- 最大绝对误差
- 最大相对误差
- 平均误差
- 对下游 matmul / logits / sampling 的影响
- 是否值得为了吞吐牺牲精度

---

## 7. Benchmark：数值实验不是跑一次数字

[[第01次课-绪论与误差理论笔记]] 里强调“数值实验”是判断算法可行性的必要环节。对应 GPU Performance：benchmark 不是只打印一个 `ms`。

最低记录：

```text
GPU:
CUDA:
PyTorch:
Triton:
Kernel:
Shape:
Dtype:
Warmup:
Repeat:
Mean / Min / Max:
Correctness:
Nsight key metrics:
Conclusion:
```

### 7.1 为什么要重复运行

单次 benchmark 可能受到：

- 首次 CUDA 初始化
- GPU 频率波动
- 其他进程占用
- cache 状态
- kernel launch overhead
- 测量同步位置

所以至少要有 warmup 和 repeat。

```python
for _ in range(warmup):
    kernel(x)

torch.cuda.synchronize()

for _ in range(repeat):
    start.record()
    kernel(x)
    end.record()
    torch.cuda.synchronize()
```

---

## 8. Agent 生成代码时的数值审查清单

AI Agent 可以生成 kernel 初版，但不能替你判断数值正确性。

### 8.1 必查项

- [ ] 是否有 PyTorch / CPU reference？
- [ ] 是否覆盖多 dtype？
- [ ] 是否覆盖极端输入？
- [ ] 是否使用合理 `atol / rtol`？
- [ ] reduction 是否使用 FP32 accumulation？
- [ ] softmax 是否做 max-shift？
- [ ] 是否处理 overflow / underflow？
- [ ] benchmark 是否先跑 correctness？
- [ ] benchmark 是否有 warmup / repeat？
- [ ] README 是否解释误差阈值？

### 8.2 面试表达

```text
我使用 Agent 生成测试框架和 benchmark 脚本，但数值正确性由我人工审查：每个 kernel 都有 PyTorch reference、多 dtype / 多 shape 测试、合理的 atol / rtol 设置；对 softmax 使用 max-shift，对 RMSNorm 使用 FP32 accumulation，并通过 Nsight 和重复 benchmark 验证优化收益不是测量噪声。
```

---

## 9. 和培养方案的关系

这篇笔记是培养方案中的“数学到代码”桥接层：

- [[AI Agent Native AI Infra GPU Performance Engineer 培养方案]]：总路线和项目安排。
- [[CUDA 学习清单]]：CUDA 基础、kernel、benchmark、profiling 清单。
- [[LLM Kernel 专题清单]]：RMSNorm、Softmax、RoPE、dequant、fusion 等算子路线。
- [[第01次课-绪论与误差理论笔记]]：误差、稳定性、有效数字、误差传播和数值实验的数学来源。

后续每做一个 kernel 项目，都要在 README 或 `profiling.md` 中回答三个问题：

1. **数学上**：这个算子的稳定写法是什么？
2. **代码上**：dtype、累加精度、边界条件和误差阈值怎么设计？
3. **实验上**：correctness、benchmark 和 profiler 是否支持你的结论？

---

## 10. 关键要点总结

1. 数学公式等价，不代表计算机实现同样稳定。
2. GPU kernel 的 correctness 必须用 reference、误差阈值和边界样例验证。
3. FP16 / BF16 / FP8 / INT8 的收益必须和数值误差一起评估。
4. benchmark 是数值实验，必须可复现、可解释、能和 correctness 绑定。
5. AI Agent 可以提高工程效率，但不能替代数值正确性和性能结论的人工判断。
