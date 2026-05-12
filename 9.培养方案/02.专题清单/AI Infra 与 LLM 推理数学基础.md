---
title: AI Infra 与 LLM 推理数学基础
date: 2026-05-12
tags:
  - CUDA
  - infra
aliases:
  - LLM 推理数学基础
  - AI Infra 数学基础
status: active
---

# AI Infra 与 LLM 推理数学基础

> [!goal] 目标
> 这篇笔记把数值分析里真正会落到 [[AI Agent Native AI Infra GPU Performance Engineer 培养方案]] 的公式整理成一条工程主线：从浮点误差、reduction 误差、GEMM 累加误差，到 stable softmax、RMSNorm、量化和近似函数实现。

这不是一篇完整的数值分析课程笔记，而是给 AI Infra / LLM 推理准备的“公式到实现”桥接笔记：每个公式都要能回答它在推理系统、GPU kernel、correctness test 或 benchmark 里对应什么。

```text
数值分析公式
→ 推理算子
→ dtype / accumulation / tolerance
→ correctness test
→ performance / cost 判断
```

---

## 1. 这篇笔记在培养方案中的位置

在 [[AI Agent Native AI Infra GPU Performance Engineer 培养方案]] 中，数值分析的作用不是为了刷数学题，而是为了判断：

- FP16 / BF16 / FP8 / INT8 结果是否可靠；
- reference test 的 `atol / rtol` 怎么设；
- Softmax / RMSNorm / reduction 是否会因为极端输入失稳；
- GEMM / Tensor Core / quantization 的精度收益和吞吐收益是否值得；
- Agent 生成的 kernel 是否只是“看起来能跑”。

这篇笔记和已有专题的分工是：

| 笔记 | 重点 |
|---|---|
| [[数值分析与 GPU Kernel 正确性]] | 如何做 correctness test、benchmark、边界 case 和工程验收 |
| [[LLM Kernel 专题清单]] | LLM 推理常见 kernel 的学习顺序和实现任务 |
| [[第01次课-绪论与误差理论笔记]] | 数值分析课程中的误差理论基础 |
| 本笔记 | 把常用公式直接映射到 LLM 推理算子和低精度工程判断 |

核心映射：

| 数学主题 | 推理中的对应对象 | 工程落点 |
|---|---|---|
| 浮点误差模型 | FP16 / BF16 / FP8 / INT8 运算 | dtype 选择、accumulator 选择 |
| 绝对误差 / 相对误差 | kernel 输出与 reference 的差距 | `atol / rtol`、`allclose` |
| Reduction 误差 | sum、max、sum of squares | Softmax、LayerNorm、RMSNorm |
| 条件数 | 输入扰动被放大的程度 | 极端 logits、极小方差、长序列 |
| Stable Softmax / LogSumExp | Attention 和 logits 归一化 | overflow / underflow 控制 |
| GEMM 点积 | Linear、QKV、MLP、lm_head | Tensor Core、FP32 accumulation |
| Quantization | INT8 / FP8 / KV cache 量化 | scale、zero point、dequant 误差 |
| Taylor / Newton | 激活函数、`exp`、`rsqrt` 近似 | 近似误差和吞吐权衡 |

> [!important] 核心判断
> AI Infra 的数学基础不是“会推公式”，而是能把公式变成工程判断：这个 kernel 为什么要用 FP32 累加？为什么 softmax 要减最大值？为什么测试不能只用 `==`？为什么 INT8 更快但可能改变 logits 分布？

---

## 2. 浮点误差模型：低精度推理的底层前提

浮点运算的基本误差模型是：

$$
\operatorname{fl}(a \circ b) = (a \circ b)(1 + \delta), \quad |\delta| \le u
$$

其中：

- $\operatorname{fl}(\cdot)$ 表示计算机实际得到的浮点结果；
- $\circ$ 表示加、减、乘、除等基本运算；
- $u$ 是机器精度（unit roundoff）；
- $\delta$ 是一次浮点运算引入的相对误差。

它告诉你：GPU 上的浮点运算不是实数运算，而是每一步都带有有限精度误差的近似运算。

在 LLM 推理中，这个模型几乎无处不在：

```text
GEMM / MatMul
Attention score
Softmax
LayerNorm / RMSNorm
量化 / 反量化
reduction 求和
采样概率归一化
```

尤其是低精度推理：

| dtype / 路径 | 工程含义 |
|---|---|
| FP32 | 精度高，吞吐和带宽成本较高 |
| TF32 | Ampere 之后常见的 FP32-like Tensor Core 路径，吞吐更高但尾数精度较低 |
| FP16 | 吞吐高、显存省，但动态范围和精度更小 |
| BF16 | 动态范围接近 FP32，尾数精度低于 FP16 |
| FP8 | 极低位宽，依赖 scale、校准和稳定实现 |
| INT8 | 需要量化 scale / zero point，适合吞吐和显存优化 |

所以很多推理 kernel 会采用：

```text
输入：FP16 / BF16 / FP8 / INT8
中间累加：FP32
输出：FP16 / BF16 / FP8 / INT8
```

这不是形式主义，而是在性能和误差之间做折中：输入低精度减少带宽和计算成本，中间 FP32 accumulator 控制长链累加误差。

---

## 3. 绝对误差、相对误差与 allclose 思想

绝对误差（absolute error）：

$$
E_{abs}=|x-\hat{x}|
$$

相对误差（relative error）：

$$
E_{rel}=\frac{|x-\hat{x}|}{|x|}
$$

其中：

- $x$ 是真实值或高精度 reference；
- $\hat{x}$ 是低精度 kernel 或近似算法输出。

在 kernel correctness test 中，它们对应：

```text
FP32 reference vs FP16 kernel
PyTorch reference vs CUDA kernel
naive implementation vs optimized implementation
quantized output vs full precision output
```

只用绝对误差会有问题：当 expected 很大时，固定 `1e-5` 可能过严；当 expected 接近 0 时，只看相对误差又可能爆炸。所以更常见的判断是：

$$
|actual - expected| \le atol + rtol \cdot |expected|
$$

这就是很多框架中 `allclose` 的基本思想。

工程含义：

| 场景 | 应该关注什么 |
|---|---|
| FP32 kernel | 较小 `atol / rtol` |
| FP16 / BF16 kernel | 容忍更大的舍入误差 |
| reduction kernel | 不同累加顺序导致轻微差异 |
| softmax kernel | 极端输入下是否出现 `nan / inf` |
| quantization kernel | 输出误差是否影响 logits / token 选择 |

> [!warning] 常见误区
> CUDA kernel 和 PyTorch reference 不完全 bitwise 相等，不一定说明 kernel 错了；但如果没有解释 dtype、累加顺序和 tolerance 的依据，也不能直接说“误差可以接受”。

---

## 4. Reduction 与误差累积

很多 LLM 推理算子最终都会落到 reduction：

$$
s=\sum_{i=1}^{n}x_i
$$

如果直接顺序相加，误差会随着 $n$ 增大而累积。直觉上，每一次加法都可能引入一个小的舍入误差，长链求和会把这些小误差叠起来。

在 GPU kernel 中，reduction 还有一个额外特点：并行归约的求和顺序通常不同于 CPU 串行求和。

```text
CPU 串行求和：(((x1 + x2) + x3) + ...)
GPU 并行归约：先局部求和，再跨 warp / block 合并
```

因为浮点加法不满足严格结合律：

$$
(a+b)+c \ne a+(b+c)
$$

所以相同输入、相同数学公式，不同 reduction 顺序可能得到略微不同的结果。

常见控制手段：

| 方法 | 作用 |
|---|---|
| FP32 accumulator | 用更高精度累加低精度输入 |
| 分块 reduction | 降低单条长链求和长度 |
| pairwise sum | 比顺序求和更稳定 |
| Kahan summation | 用补偿项减少舍入误差 |
| 合理 `atol / rtol` | correctness test 不要求不合理的 bitwise 相等 |

在 [[Week 2 - Reduction + Profiling]] 里，reduction 不只是 CUDA 入门题，而是后面 Softmax、RMSNorm、LayerNorm、top-k、Attention 的共同基础。

---

## 5. 条件数：问题本身是否容易放大误差

条件数（condition number）衡量一个问题对输入扰动有多敏感。

对矩阵问题，常见形式是：

$$
\kappa(A)=\|A\|\,\|A^{-1}\|
$$

直观理解：

```text
条件数小：输入有一点误差，输出通常也只差一点
条件数大：输入有一点误差，输出可能被明显放大
```

对一般函数 $f(x)$，相对条件数可以写成：

$$
\kappa_f(x)=\left|\frac{x f'(x)}{f(x)}\right|
$$

它表示输入相对误差大约会被函数放大多少倍。

在 LLM 推理里，这个概念的价值是区分两类问题：

| 类型 | 含义 | 例子 |
|---|---|---|
| 问题本身敏感 | 输入稍微变化，输出就可能变化很大 | logits 很接近时，微小误差改变 top-1 token |
| 算法实现不稳定 | 数学问题可以稳定求解，但写法放大了误差 | naive softmax 对大 logits overflow |

容易放大误差的输入：

- logits 极大或差距极大；
- softmax 分布接近 one-hot；
- LayerNorm 方差非常小；
- RMSNorm 的平方和非常小；
- 长序列 attention 中大量项参与归一化；
- GEMM 中正负项抵消严重；
- quantization scale 过粗导致小值被吞掉。

> [!important] 工程判断
> 不是所有误差都来自 kernel 写错。有些输入分布本身就很敏感，所以 correctness test 需要覆盖极端值、长序列、全零、接近零方差、非对齐 shape 等 case。

---

## 6. Softmax 与 LogSumExp：Attention 中最常见的稳定公式

普通 Softmax 是：

$$
\operatorname{softmax}(x_i)=\frac{e^{x_i}}{\sum_j e^{x_j}}
$$

问题是：当 $x_i$ 很大时，$e^{x_i}$ 可能溢出。

稳定写法是：

$$
\operatorname{softmax}(x_i)=\frac{e^{x_i-m}}{\sum_j e^{x_j-m}},\quad m=\max_j x_j
$$

因为最大项变成：

$$
e^{m-m}=e^0=1
$$

这样可以显著降低 overflow 风险。

在 Attention 中，Softmax 出现在：

$$
\operatorname{Attention}(Q,K,V)=\operatorname{softmax}\left(\frac{QK^T}{\sqrt{d_k}}\right)V
$$

这里的数值风险来自：

- $QK^T$ 可能产生较大的 attention score；
- 长序列会让每一行 softmax 的元素更多；
- mask 可能引入极小值；
- FP16 / BF16 下指数和求和更容易出问题。

所以 [[LLM Kernel 专题清单]] 中的 Softmax 学习点不只是“会写 exp + sum”，而是必须理解：

```text
row-wise max reduce
→ exp(x - max)
→ row-wise sum reduce
→ normalize
```

LogSumExp 是 Softmax 的兄弟公式：

$$
\log\sum_i e^{x_i}
=
m+\log\sum_i e^{x_i-m},\quad m=\max_i x_i
$$

它同样通过减去最大值避免指数溢出。

在推理中，LogSumExp 思想会出现在：

- logits 概率归一化；
- temperature scaling；
- top-k / top-p 采样前后的概率处理；
- loss 或 perplexity 评估；
- 某些 fused sampling / logits processor 实现。

---

## 7. LayerNorm 与 RMSNorm：归一化里的 reduction、eps 与 rsqrt

LayerNorm 公式：

$$
\mu=\frac{1}{n}\sum_{i=1}^{n}x_i
$$

$$
\sigma^2=\frac{1}{n}\sum_{i=1}^{n}(x_i-\mu)^2
$$

$$
y_i=\gamma\frac{x_i-\mu}{\sqrt{\sigma^2+\epsilon}}+\beta
$$

RMSNorm 公式：

$$
rms=\sqrt{\frac{1}{n}\sum_{i=1}^{n}x_i^2+\epsilon}
$$

$$
y_i=\gamma\frac{x_i}{rms}
$$

二者共同点：

- 都有 row-wise reduction；
- 都需要计算平方或平方和；
- 都需要除以一个带 `sqrt` 的归一化因子；
- 都经常使用 FP32 accumulation；
- 都需要 `eps` 防止分母过小或除零。

区别：

| 算子 | 是否减均值 | reduction 内容 | 推理工程重点 |
|---|---|---|---|
| LayerNorm | 是 | mean + variance | 计算更多，稳定性依赖均值和方差 |
| RMSNorm | 否 | mean of squares | 更轻量，LLM 推理中很常见 |

`eps` 的作用不是提高精度，而是提高稳定性：当方差或平方均值接近 0 时，它避免分母过小。

工程实现里常见写法是：

```text
输入：FP16 / BF16
sum 或 sum of squares：FP32 accumulation
归一化因子：rsqrt(var + eps) 或 rsqrt(mean_square + eps)
输出：FP16 / BF16
```

这说明 Norm 类 kernel 的数学核心不是复杂公式，而是：

```text
reduction 误差
+ 低精度累加
+ sqrt / rsqrt 近似
+ eps 稳定性
```

---

## 8. GEMM / MatMul：点积、累加顺序与 Tensor Core 精度

LLM 推理里最核心的计算是矩阵乘法：

$$
C=AB
$$

展开为：

$$
C_{ij}=\sum_{k=1}^{n}A_{ik}B_{kj}
$$

也就是说，每个 $C_{ij}$ 都是一个点积。点积本质上是大量乘法和加法：

```text
multiply
→ accumulate
→ write output
```

这直接对应：

- Linear 层；
- QKV projection；
- MLP up / gate / down projection；
- Attention output projection；
- embedding projection；
- lm_head。

从数值角度看，GEMM 的核心问题是：

| 问题 | 工程含义 |
|---|---|
| 乘法输入低精度 | FP16 / BF16 / FP8 输入带来表示误差 |
| 累加链很长 | $K$ 越大，累加误差越明显 |
| 累加顺序不同 | CUDA / cuBLAS / Triton / CUTLASS 结果可能略有差异 |
| Tensor Core 路径不同 | TF32、FP16、BF16、FP8 的精度和吞吐不同 |
| 输出 cast | FP32 accumulator 最后 cast 回低精度可能再次损失信息 |

所以常见高性能路径是：

```text
FP16 / BF16 input
+ Tensor Core MMA
+ FP32 accumulation
+ FP16 / BF16 output
```

在 [[Week 4 - MatMul v0]] 里，关注的是 tiling、shared memory、Tensor Core、TFLOPS 和 profiling；而在这篇笔记里，关注的是为什么 GEMM correctness 不能只看速度，还要理解累加精度和 tolerance。

---

## 9. Quantization：scale、zero point 与反量化误差

量化（quantization）的核心是用低位宽近似表示高精度数值。

对称量化：

$$
q=\operatorname{round}\left(\frac{x}{s}\right)
$$

$$
x\approx s q
$$

非对称量化：

$$
q=\operatorname{round}\left(\frac{x}{s}\right)+z
$$

$$
x\approx s(q-z)
$$

其中：

- $s$ 是 scale；
- $z$ 是 zero point；
- $q$ 是量化后的整数。

常见 scale 估计：

$$
s=\frac{x_{max}-x_{min}}{q_{max}-q_{min}}
$$

INT8 GEMM 可以粗略理解成：

$$
C \approx s_A s_B \sum_k q_{A,k}q_{B,k}
$$

量化误差主要来自：

| 来源 | 含义 |
|---|---|
| rounding | 连续值映射到离散整数 |
| clipping | 超出量化范围的值被截断 |
| scale 粗糙 | 一个 scale 覆盖过宽范围，小值容易被吞掉 |
| zero point | 非对称量化需要额外偏移修正 |
| dequant | 反量化只是近似恢复，不是无损还原 |

常见粒度：

| 粒度 | 特点 |
|---|---|
| per-tensor | 一个 tensor 共用一个 scale，简单但误差可能大 |
| per-channel | 每个 channel 一个 scale，更精细 |
| group-wise | 每组元素一个 scale，常用于 weight-only quantization |

在 LLM 推理中，量化对应：

- weight-only quantization；
- activation quantization；
- KV cache quantization；
- INT8 / FP8 GEMM；
- dequant fused kernel；
- logits 或 attention 分布变化。

> [!warning] 量化不是免费加速
> INT8 / FP8 可能降低显存和带宽压力，但会引入 rounding、clipping 和 scale 误差。工程判断不能只看吞吐，还要看 logits、sampling、输出 token 和任务质量是否稳定。

---

## 10. Taylor 近似与非线性函数近似

Taylor 展开通式：

$$
f(x)\approx f(a)+f'(a)(x-a)+\frac{f''(a)}{2!}(x-a)^2+\cdots
$$

它的工程意义是：用低成本近似替代高成本精确函数。

在推理 kernel 中，常见近似对象包括：

```text
exp
sigmoid
tanh
GELU
SiLU
rsqrt
log
```

例如 GELU：

$$
\operatorname{GELU}(x)=x\Phi(x)
$$

工程中常见近似形式：

$$
\operatorname{GELU}(x)\approx 0.5x\left(1+\tanh\left(\sqrt{\frac{2}{\pi}}(x+0.044715x^3)\right)\right)
$$

这个例子说明：AI Infra 中的近似函数不是“随便改公式”，而是在可接受误差范围内换取更好的吞吐、延迟或硬件友好性。

判断一个近似是否值得，需要看：

- 适用区间是什么；
- 最大误差和平均误差多大；
- 是否影响 logits 或 token 选择；
- 是否减少 expensive instruction；
- 是否更适合 vectorization / fusion / Tensor Core 路径。

---

## 11. Newton 迭代与 rsqrt：Norm kernel 的底层数学原语

Newton 迭代通式：

$$
x_{k+1}=x_k-\frac{f(x_k)}{f'(x_k)}
$$

在推理 kernel 中，经常关心：

```text
1 / x
sqrt(x)
1 / sqrt(x)
```

快速求 $1/\sqrt{a}$ 的迭代形式可以写成：

$$
y_{k+1}=y_k\left(\frac{3}{2}-\frac{1}{2}ay_k^2\right)
$$

这和 LayerNorm / RMSNorm 中的归一化因子直接相关：

$$
\frac{1}{\sqrt{\sigma^2+\epsilon}}
$$

$$
\frac{1}{\sqrt{\frac{1}{n}\sum_i x_i^2+\epsilon}}
$$

工程实现里，`rsqrt` 往往代表一种实现意识：

```text
先得到硬件近似
→ 必要时迭代修正
→ 用乘法替代除法路径
→ 在误差可接受时换取性能
```

对于 Norm 类 kernel，理解 Newton / rsqrt 的意义，是为了知道 `sqrt`、除法和归一化因子不是普通算术细节，而是影响吞吐和误差的关键路径。

---

## 12. 优先掌握顺序与项目落点

按 AI Infra / LLM 推理的重要性，可以这样排序：

| 优先级 | 数学内容 | 推理中的用途 |
|---|---|---|
| 1 | 浮点误差模型 | FP16 / BF16 / FP8 精度分析 |
| 2 | 绝对误差 / 相对误差 | kernel correctness、tolerance 设计 |
| 3 | Reduction 误差 | Softmax、LayerNorm、RMSNorm、top-k |
| 4 | GEMM 累加误差 | Linear、QKV、MLP、Tensor Core accumulator |
| 5 | Stable Softmax / LogSumExp | Attention 数值稳定、logits 归一化 |
| 6 | LayerNorm / RMSNorm 公式 | norm kernel、FP32 accumulation、`eps` |
| 7 | Quantization 公式 | INT8 / FP8 推理、KV cache 量化 |
| 8 | 条件数 | 判断低精度和极端输入是否危险 |
| 9 | Taylor 近似 | activation、`exp`、`tanh`、`log` 近似 |
| 10 | Newton 迭代 | `rsqrt`、division、norm kernel |

对应到项目路线：

| 项目 / 专题 | 数学基础 |
|---|---|
| [[Week 2 - Reduction + Profiling]] | 求和误差、并行 reduction、FP32 accumulation |
| [[Week 4 - MatMul v0]] | 点积、累加顺序、Tensor Core 精度 |
| [[LLM Kernel 专题清单]] | Softmax、RMSNorm、activation、dequant |
| [[CUDA 学习清单]] | thread / block / memory hierarchy 与数值验证结合 |
| [[3.7 距离空间到 GPU Kernel 正确性 - 数学与代码桥接]] | 从数学定义到 kernel correctness 的桥接意识 |
| [[数值分析与 GPU Kernel 正确性]] | tolerance、edge case、benchmark 可信度 |

一句话总结：

> [!important] 总结
> AI Infra 推理里最常用的数值分析公式，就是浮点误差、reduction 误差、GEMM 累加误差、stable softmax、归一化公式和量化公式。学习路线应围绕“低精度是否可靠、误差是否可控、性能收益是否真实”展开。

面试表达可以这样组织：

```text
我不只是写了 CUDA / Triton kernel，还会用 PyTorch reference 和 dtype-aware tolerance 做 correctness test；
对于 softmax，我会使用 max-shift 避免 overflow；
对于 RMSNorm / GEMM，我会解释为什么低精度输入通常需要 FP32 accumulation；
对于 INT8 / FP8 quantization，我会同时看 scale 误差、dequant 误差和对 logits / token 输出的影响。
```

这样数学基础就不再是抽象公式，而是 AI Infra / GPU Performance Engineer 判断 kernel correctness、低精度可靠性和推理成本优化的底层语言。
