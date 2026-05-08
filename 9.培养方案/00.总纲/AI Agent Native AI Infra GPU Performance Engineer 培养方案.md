---
title: AI Agent Native AI Infra GPU Performance Engineer 培养方案
date: 2026-05-06
tags:
  - 培养方案
  - AIInfra
  - CUDA
  - Triton
  - LLM推理
  - Agent工程流
  - infra
status: active
---

# AI Agent Native 的 AI Infra / GPU Performance Engineer 培养方案

> [!goal] 总目标
> 从“会写 CUDA / Triton kernel，能做 AI Infra 实习”升级为“会用 AI Agent 加速工程开发，但自己能负责 GPU 性能分析、LLM 推理成本优化、kernel 正确性与上线判断”。

这条路线的核心不是放弃 CUDA / Triton，而是把目标从“手写 kernel 的候选人”升级成：

```text
AI Infra Performance Engineer
= AI Agent 工程效率
+ CUDA / Triton / C++ kernel
+ GPU profiling
+ LLM inference engine
+ 推理成本优化
```

普通工程代码会越来越便宜，但以下能力会越来越值钱：

- 性能判断
- 成本判断
- 系统取舍
- 硬件理解
- benchmark 可信度
- 线上推理效率
- Agent 生成代码的审查能力

关联笔记：

- [[0.ai 编程/1. claude/1.1 claude code语法|Claude Code 语法]]
- [[0.ai 编程/2. GPT/2.1 codex语法|Codex 语法]]

## 目标升级

原目标：

```text
CUDA / Triton kernel 实习生
```

升级后目标：

```text
AI Agent-native 的 AI Infra / GPU Performance Engineer
```

这意味着你不只是能写 kernel，还要能回答：

- 这个 kernel 为什么慢？
- 它是 memory-bound 还是 compute-bound？
- Nsight 指标能不能支持你的判断？
- 这个优化是否真的降低了 TTFT / TPOT / cost？
- Agent 生成的代码怎么验证 correctness？
- benchmark 数据是否公平、可复现、可解释？
- 这个改动是否值得上线？

> [!important] 核心人设
> 我会用 AI Agent 快速搭工程、生成测试和 benchmark，但 kernel 核心逻辑、correctness、profiling 结论、性能报告和最终上线判断都由我人工验证。

## 为什么补 compiler-aware 视角

这条路线仍然是 **LLM Inference Performance / GPU Infra**，不是转成纯编译器工程师路线。补 compiler-aware 视角，是为了能把高层算子、kernel 实现、profiling 结论、serving 指标和 lowering / codegen correctness 串起来。

不赌 2028 一定是推理爆发年，但按“推理工程链路继续变深、推理成本优化持续重要”来准备；即使赛道节奏变化，kernel + profiling + serving benchmark + compiler-aware 的能力组合仍然可迁移。

AI 可以生成工程代码和初版 kernel，但不能替代以下判断：

- benchmark 是否公平、可信、可复现；
- 性能瓶颈是否被硬件指标支持；
- lowering / codegen 生成的代码是否经过 reference、shape、dtype 和边界条件验证；
- 优化是否真的改善 TTFT / TPOT / TPS / cost，而不是只改善单个 toy benchmark。

## 能力主线

| 主线 | 要练什么 | 最终简历表达 |
|---|---|---|
| AI Agent 工程流 | Claude Code / Codex / Cursor、MCP、hooks、自动化测试、PR review | 能用 Agent 提高开发效率，但有人工审查和性能验证流程 |
| Kernel 能力 | CUDA、Triton、RMSNorm、Softmax、MatMul、RoPE、Attention | 能写并优化 LLM 常见算子 |
| GPU 性能分析 | Nsight Compute、Nsight Systems、CUDA event、roofline、memory-bound / compute-bound | 能解释 kernel 为什么慢、怎么优化 |
| 推理成本优化 | vLLM、SGLang、FlashInfer、TensorRT-LLM、KV cache、batching、quantization | 能用 TTFT / TPOT / TPS / cost per 1M tokens 评估系统 |
| 编译器 / Lowering 认知 | Triton lowering、MLIR basics、IR / pass / backend lowering、codegen correctness、operator fusion | 能解释高层算子如何走到 kernel / codegen，并知道 generated code 的 correctness 与性能风险 |

最低可验证能力：

- 能手写基础 CUDA kernel。
- 能写 Triton softmax / RMSNorm。
- 能接 PyTorch C++ / CUDA extension。
- 能用 CUDA event benchmark。
- 能读 Nsight Compute 的关键指标。
- 能解释 TTFT / TPOT / TPS / RPS / GPU utilization / cost per 1M tokens。
- 能画出 Triton kernel 到 IR / lowering / PTX 的粗链路，并说明哪些环节会影响 correctness 与性能。
- 能让 Agent 生成脚手架、测试、benchmark，但不让 Agent 决定性能结论。

## 路线总图

![AI Agent Native 路线总图|935](../../图片/SVG/ai-agent-native-roadmap.svg)

*图示说明：按“当前起点 → 三个阶段 → 最终目标”重绘，减少原 Mermaid 的交叉感。*

## 项目总图

![AI Agent Native 项目总图|894](../../图片/SVG/ai-agent-native-project-map.svg)

*图示说明：左侧是能力主线，中间是项目阶段，右侧是作品集收口，依赖关系更容易顺着看。*

## 项目真实性核验

核验日期：2026-05-06。

> [!warning] 重要区分
> 下表中的开源项目是真实存在的学习对象；作品集项目名是你未来要创建的个人仓库规划，不能在简历或面试中说成“已有开源项目”。

| 名称 | 类型 | 核验结果 | 用途 | 链接 |
|---|---|---|---|---|
| Triton | 真实开源项目 | 已核验 | 学习 kernel DSL、fused softmax、matmul、autotune | [GitHub](https://github.com/triton-lang/triton) |
| vLLM | 真实开源项目 | 已核验 | 学习 PagedAttention、KV cache、serving benchmark | [GitHub](https://github.com/vllm-project/vllm) |
| SGLang | 真实开源项目 | 已核验 | 学习 serving runtime、continuous batching、prefix cache | [GitHub](https://github.com/sgl-project/sglang) |
| FlashInfer | 真实开源项目 | 已核验 | 学习 LLM serving kernels、attention、paged attention | [GitHub](https://github.com/flashinfer-ai/flashinfer) |
| CUTLASS | 真实开源项目 | 已核验 | 学习 GEMM hierarchy、Tensor Core、CUDA 模板库 | [GitHub](https://github.com/NVIDIA/cutlass) |
| TensorRT-LLM | 真实开源项目 | 已核验 | 学习工业级推理优化、paged KV cache、quantization | [GitHub](https://github.com/NVIDIA/TensorRT-LLM) |
| CUDA Best Practices | 官方文档 | 已核验 | 学习 CUDA memory、parallel execution、instruction efficiency、profiling | [Docs](https://docs.nvidia.com/cuda/cuda-c-best-practices-guide/) |
| Nsight Compute | 官方工具文档 | 已核验 | 学习 kernel profiling、memory throughput、occupancy、stall 分析 | [Docs](https://docs.nvidia.com/nsight-compute/) |
| Claude Code Hooks | 官方文档 | 已核验 | 学习 hooks、permissions、工程自动化边界 | [Docs](https://code.claude.com/docs/en/hooks) |
| NVIDIA GenAI-Perf 指标文章 | 官方技术文章 | 已核验 | 学习 TTFT、ITL、TPS、RPS 等推理 benchmark 指标 | [Blog](https://developer.nvidia.com/blog/llm-performance-benchmarking-measuring-nvidia-nim-performance-with-genai-perf/) |
| agentic-cuda-kernel-playground | 拟建作品集项目 | 不是外部开源项目 | 基础 CUDA kernel + Agent workflow | 本人未来创建 |
| torch-triton-rmsnorm | 拟建作品集项目 | 不是外部开源项目 | CUDA + Triton RMSNorm + PyTorch extension | 本人未来创建 |
| llm-inference-cost-lab-v0 | 拟建作品集项目 | 不是外部开源项目 | 小模型 vLLM / SGLang 成本 benchmark | 本人未来创建 |
| matmul-lab-cuda-triton-cutlass | 拟建作品集项目 | 不是外部开源项目 | 对比 CUDA / Triton / CUTLASS / cuBLAS GEMM | 本人未来创建 |
| tiny-llm-kernels | 拟建作品集项目 | 不是外部开源项目 | LLM 小算子集合 | 本人未来创建 |
| llm-serving-cost-benchmark | 拟建作品集项目 | 不是外部开源项目 | vLLM / SGLang / TensorRT-LLM 推理成本对比 | 本人未来创建 |
| llm-kernel-benchmark-suite | 拟建作品集项目 | 不是外部开源项目 | 秋招统一 kernel benchmark suite | 本人未来创建 |
| mini-vllm-style-kv-cache | 拟建作品集项目 | 不是外部开源项目 | 简化版推理调度器和 paged KV cache toy model | 本人未来创建 |
| agentic-infra-workflow | 拟建作品集项目 | 不是外部开源项目 | AI Agent 工程流模板和审查流程 | 本人未来创建 |

## 阶段一：现在到 2026 年 7 月

目标：拿到暑期 AI Infra 相关实习面试。

阶段策略：

```text
用 AI Agent 快速搭项目
自己补 CUDA / Triton 核心知识
做出可展示的 benchmark
让简历看起来像 AI Infra 候选人
```

### 学习重点

CUDA / GPU 基础：

- thread / block / grid
- warp / SIMT
- global memory
- shared memory
- register
- memory coalescing
- bank conflict
- occupancy
- CUDA event timing

你必须能解释：

- 为什么相邻线程访问连续地址更快？
- 为什么 shared memory transpose 比 naive transpose 快？
- 为什么 occupancy 高不一定性能高？
- 为什么 vector add 是 memory-bound？
- 为什么 matmul 更偏 compute-bound？

Triton 入门：

- Triton program id
- block pointer
- mask load / store
- `num_warps`
- `BLOCK_SIZE`
- fused softmax
- Triton matmul

AI Agent 工程流：

```text
Agent 只负责生成 CMake、测试框架、benchmark 脚本、README 初稿。
kernel 核心逻辑必须人工 review。
每次修改后必须跑 correctness test。
Agent 不允许修改 benchmark 结论。
Agent 不允许删除文件。
```

每个项目建议包含：

- `CLAUDE.md` / `AGENTS.md`
- `tasks.md`
- `benchmark.md`
- `profiling.md`
- `agent_log.md`

LLM 推理成本指标：

| 指标 | 含义 | 为什么重要 |
|---|---|---|
| TTFT | Time To First Token | 用户多久看到第一个 token |
| TPOT / ITL | 每个输出 token 间隔 | 流式输出是否顺滑 |
| TPS | tokens per second | 系统吞吐 |
| RPS | requests per second | 服务能力 |
| GPU utilization | GPU 利用率 | 资源是否浪费 |
| cost / 1M tokens | 每百万 token 成本 | 商业价值 |

### 必做项目 1：agentic-cuda-kernel-playground

定位：基础 CUDA kernel + benchmark + Agent 工程流。

实现 6 个基础 kernel：

- vector add
- reduce sum
- matrix transpose
- naive matmul
- tiled matmul
- softmax

Agent 负责：

- `CMakeLists.txt`
- README 框架
- benchmark 脚本
- correctness test
- GitHub Actions
- 代码注释初稿

你自己负责：

- kernel 核心逻辑
- 性能数据判断
- Nsight 结果解释
- 最终 README 结论

验收标准：

- 每个 kernel 都有 correctness test。
- 每个 kernel 都有 CUDA event benchmark。
- 至少解释 vector add、transpose、matmul 的瓶颈。
- README 明确写出哪些部分由 Agent 生成，哪些部分人工验证。

面试问题：

- transpose 为什么要 shared memory？
- matmul tiling 的本质是什么？
- reduce 怎么做 block-level reduction？
- softmax 怎么保证数值稳定？
- 你怎么证明优化有效？

### 必做项目 2：torch-triton-rmsnorm

定位：最接近 LLM kernel 的短期项目。

实现：

```python
y = my_ops.rmsnorm(x, weight, eps)
```

版本：

- CUDA C++ version
- Triton version

技术点：

- PyTorch C++ / CUDA extension
- Triton kernel
- row-wise reduction
- vectorized load
- FP32 / FP16
- correctness test
- benchmark

验收标准：

- 与 PyTorch reference 输出误差在合理范围内。
- 支持至少 FP32 和 FP16。
- benchmark 覆盖不同 batch size 和 hidden size。
- 能解释 RMSNorm 和 LayerNorm 区别。
- 能判断它更偏 memory-bound 还是 compute-bound。

面试问题：

- RMSNorm 和 LayerNorm 区别是什么？
- 这个 kernel 是 memory-bound 还是 compute-bound？
- hidden size 变化时性能为什么变化？
- Triton 版本和 CUDA 版本哪个更快？为什么？

### 必做项目 3：llm-inference-cost-lab-v0

定位：推理成本和服务指标入门。

模型建议：

- Qwen2.5-0.5B / 1.5B
- TinyLlama
- Llama 3.2 1B

记录字段：

- batch size
- concurrency
- input length
- output length
- TTFT
- TPOT
- TPS
- GPU memory
- cost / 1M tokens

输出表：

| 配置 | TTFT | TPOT | TPS | 显存 | cost / 1M tokens | 结论 |
|---|---:|---:|---:|---:|---:|---|
| batch=1 |  |  |  |  |  | 低延迟 |
| batch=8 |  |  |  |  |  | 平衡 |
| batch=32 |  |  |  |  |  | 高吞吐 |

成本公式：

```text
每百万输出 token 成本
= GPU 每小时价格 / 每小时输出 token 数 * 1,000,000

每小时输出 token 数
= output TPS * 3600
```

面试问题：

- 为什么 batch 变大 TPS 上升但 TPOT 变差？
- TTFT 和 TPOT 分别对应什么阶段？
- prefill 和 decode 的瓶颈有什么不同？
- 成本怎么算？

### 阶段一 8 周冲刺计划

CUDA 细化任务、必做 kernel 和专题补课清单见 [[CUDA 学习清单]]。

| 周次     | 主题                            | 产出                                                                           |
| ------ | ----------------------------- | ---------------------------------------------------------------------------- |
| Week 1 | CUDA + Agent workflow         | CUDA 项目模板、vector add、benchmark 框架、`CLAUDE.md`、Agent 权限边界                     |
| Week 2 | Reduction + Profiling         | reduce sum、warp / block reduction、CUDA event、第一次 Nsight Compute              |
| Week 3 | Transpose + Memory Coalescing | naive transpose、shared memory transpose、bank conflict padding、`profiling.md` |
| Week 4 | MatMul v0                     | naive matmul、tiled matmul、cuBLAS 对比、差距解释                                     |
| Week 5 | Triton                        | Triton fused softmax、Triton matmul、Triton RMSNorm 初版                         |
| Week 6 | PyTorch Extension             | CUDA RMSNorm、PyTorch extension、correctness test、FP32 / FP16 对比               |
| Week 7 | LLM Cost Lab                  | vLLM 小模型、TTFT / TPOT / TPS、cost / 1M tokens                                  |
| Week 8 | 投递包装                          | README、benchmark 表、10 个面试 Q&A、简历、开始投递                                        |

### 阶段一验收标准

到 2026 年 7 月，你要达到：

- 能手写基础 CUDA kernel。
- 能写 Triton softmax / RMSNorm。
- 能接 PyTorch extension。
- 能用 Agent 搭工程框架。
- 能用 CUDA event benchmark。
- 能解释 TTFT / TPOT / TPS。
- 能在简历上写 2 到 3 个 AI Infra 项目。

## 阶段二：2026 年 7 月到 2027 年 3 月

目标：从“能投 AI Infra 实习”升级到“转正实习竞争力”。

这个阶段从单个 kernel 进入：

- GEMM
- Attention
- KV cache
- quantization
- inference engine
- cost optimization

### 学习重点

GEMM 深入：

- naive matmul
- shared memory tiled matmul
- register blocking
- warp-level tiling
- Tensor Core
- CUTLASS
- cuBLAS baseline
- Triton matmul autotune

LLM kernel 深入：

- RMSNorm
- RoPE
- Softmax
- fused bias + activation
- top-k / top-p sampling
- dequant + matmul toy version

推理系统：

- prefill
- decode
- KV cache
- paged KV cache
- continuous batching
- chunked prefill
- prefix cache
- speculative decoding
- quantization

Compiler-aware kernel 路线：

- Triton lowering / MLIR basics：理解 Triton kernel 不是黑盒，知道 IR、dialect、pass、lowering 的基本概念。
- TVM 或 IREE 二选一入门：建立 compiler stack 地图感，不把它们同时作为深入主线。
- codegen correctness：generated code 需要 reference test、shape / dtype 覆盖和边界条件验证。
- operator fusion 直觉：fusion 可能减少访存和 launch overhead，也可能带来 register pressure、occupancy、correctness 风险。
- 目标边界：看懂链路、能解释取舍，不从零实现完整编译器 backend。

AI Agent 进阶：

| Agent | 负责内容 | 人工检查点 |
|---|---|---|
| Kernel Agent | 初版 CUDA / Triton 代码 | 算法正确性、边界条件、访存模式 |
| Benchmark Agent | benchmark matrix、脚本、图表 | 测量公平性、warmup、数据可信度 |
| Profiler Agent | 整理 Nsight 输出 | bottleneck 结论是否被指标支持 |
| Reviewer Agent | correctness / performance review | 是否误判、是否过度修改 |
| Doc Agent | README、图表、报告 | 结论是否夸大，是否能被数据支持 |

原则：

```text
Agent 可以生成代码。
Agent 不可以决定性能结论。
Agent 不可以绕过 correctness test。
Agent 不可以改 benchmark 数据。
```

### 必做项目 4：matmul-lab-cuda-triton-cutlass

实现并对比：

- naive CUDA matmul
- shared memory tiled matmul
- register blocking matmul
- Triton matmul
- CUTLASS GEMM
- cuBLAS baseline

技术点：

- tiling
- shared memory
- register pressure
- occupancy
- Tensor Core
- FP32 / FP16
- roofline thinking

验收标准：

- 每个版本有统一 benchmark。
- benchmark 覆盖多个 M / N / K shape。
- cuBLAS 是 baseline，不把打不过 cuBLAS 解释成失败。
- README 解释每个版本快慢原因。
- Nsight 分析至少包含 memory throughput、occupancy、stall 原因之一。
- 输出一篇 CUDA / Triton / CUTLASS / cuBLAS GEMM comparison report，统一 GPU、shape、dtype、warmup、测量方法，比较性能、可维护性、调优空间、correctness 风险和开发成本。

面试问题：

- 为什么 naive matmul 慢？
- shared memory tile 存什么？
- register blocking 有什么用？
- Tensor Core 为什么快？
- 为什么你的实现打不过 cuBLAS？

### 必做项目 5：tiny-llm-kernels

实现 LLM 推理常见小算子：

- RMSNorm
- RoPE
- Softmax
- SwiGLU / GELU
- top-k sampling
- INT8 dequant toy kernel

技术点：

- row-wise reduction
- warp reduction
- vectorized load
- `half2`
- kernel fusion
- Triton vs CUDA

验收标准：

- 所有算子都有 PyTorch reference。
- 至少 3 个算子有 CUDA 和 Triton 两个版本。
- 每个算子都要判断 memory-bound / compute-bound。
- 至少写一篇“哪些算子适合 fusion”的总结。
- 可选：实现一个 toy fusion demo，例如 bias + GELU、residual + RMSNorm 或 dequant + matmul 的简化版本，并给出 fusion 前后的 correctness 与 benchmark 对比。

### 必做项目 6：llm-serving-cost-benchmark

对比至少两个 serving engine：

- vLLM
- SGLang
- 可选：TensorRT-LLM

测试维度：

- 不同并发
- 不同 prompt 长度
- 不同 output 长度
- 不同 quantization
- 不同 batch 参数
- 不同 KV cache 设置

输出内容：

- 性能表
- 成本表
- 推荐配置
- 哪些场景用 vLLM
- 哪些场景用 SGLang
- 哪些场景需要 TensorRT-LLM

面试问题：

- 什么情况下低 TTFT 比高 TPS 更重要？
- 什么情况下 batch size 应该提高？
- 为什么长上下文会让 KV cache 成为瓶颈？
- quantization 降成本的代价是什么？

### 阶段二开源学习路线

| 项目 | 学习重点 | 初级贡献 | 中级贡献 |
|---|---|---|---|
| Triton | kernel DSL、softmax、matmul | 修 docs / 补 benchmark | 添加 tutorial / 小 kernel |
| vLLM | PagedAttention、KV cache、benchmark | 复现 issue / 补文档 | benchmark PR / 小 bugfix |
| SGLang | serving runtime、continuous batching | 跑 benchmark / 文档修正 | server 参数实验报告 |
| FlashInfer | attention / PageAttention kernels | examples 复现 | benchmark / API 示例 |
| CUTLASS | GEMM hierarchy、Tensor Core | 跑 examples | 学习笔记 / profiler 对比 |
| TensorRT-LLM | paged KV cache、in-flight batching、quantization | 跑 sample | benchmark 对比报告 |

### 阶段二验收标准

到 2027 年 3 月，你应该能做到：

- 能讲清 GEMM 优化路线。
- 能写 LLM 小算子。
- 能用 Nsight Compute 分析瓶颈。
- 能搭 vLLM / SGLang benchmark。
- 能解释 TTFT / TPOT / TPS / cost。
- 能画出 Triton / MLIR / backend lowering 的粗链路。
- 能说明 CUDA / Triton / CUTLASS / cuBLAS 在 GEMM 上各自适合什么场景。
- 至少产出 1 篇 GEMM comparison report，而不是只跑 benchmark 数字。
- 能用 AI Agent 快速生成工程代码。
- 能人工 review Agent 生成的性能代码。
- 有 1 到 2 个开源 issue / PR 记录。

## 阶段三：2027 年 3 月到 2027 年 9 月

目标：秋招拿正式 offer。

这个阶段要从“学习项目”变成“面试作品集”。

### 高级学习点

- FlashAttention 思想
- PagedAttention
- KV cache block manager
- continuous batching
- prefix cache
- speculative decoding
- FP8 / INT8 / INT4 quantization
- Tensor Core
- CUTLASS GEMM
- Nsight Systems timeline
- multi-agent engineering workflow

### 可选高级项目：toy lowering / fusion demo

这个项目是 compiler-aware 加分项，不是阶段三必做主线。fusion pass 和 toy kernel codegen 二选一即可，目标是证明你理解“算子表示 → lowering / codegen → generated kernel → correctness / benchmark”的链路。

可选方向一：toy fusion demo。

- 选择一个小型融合模式，例如 bias + GELU、residual + RMSNorm 或 dequant + matmul toy version。
- 写出 fusion 前后的 PyTorch / NumPy reference。
- 对比 fusion 前后的 correctness、launch overhead、memory traffic 和简单 benchmark。

可选方向二：toy kernel codegen。

- 设计一个极简 AST / IR，只表达 elementwise 或 row-wise reduction。
- 将它 lowering 到 Triton / CUDA kernel skeleton。
- 用 reference test 验证 generated code 的 correctness。
- 用少量 shape benchmark 说明 codegen 结果的性能边界。

面试表达不要说“做了完整编译器”，而是强调：

```text
我通过一个小型 lowering / fusion demo，理解了高层算子表示、kernel 生成、correctness 验证和 benchmark 之间的关系。
```

### 秋招项目 7：llm-kernel-benchmark-suite

把之前做的 kernel 统一成一个 benchmark suite：

- CUDA RMSNorm
- Triton RMSNorm
- CUDA softmax
- Triton softmax
- RoPE
- fused activation
- sampling
- matmul variants
- toy attention

必须有：

- correctness test
- benchmark script
- Nsight Compute 结果
- 多 shape 对比
- 多 dtype 对比
- README 图表
- Agent-assisted development note

面试问题：

- 你怎么保证 benchmark 公平？
- warmup 怎么做？
- 为什么这个 shape 快，那个 shape 慢？
- 这个 kernel 的瓶颈是什么？
- 为什么没有直接用 PyTorch / Triton / cuBLAS？

### 秋招项目 8：mini-vllm-style-kv-cache

实现一个简化版推理调度器：

- request queue
- prefill / decode 区分
- KV cache block table
- paged KV cache toy model
- batch decode
- TTFT / TPOT 统计
- cost per 1M tokens 计算

价值：

```text
证明你不只是 kernel 选手，而是理解 inference engine。
```

面试问题：

- KV cache 为什么会成为显存瓶颈？
- paged KV cache 解决什么问题？
- prefill 和 decode 为什么瓶颈不同？
- continuous batching 怎么提升 GPU 利用率？

### 秋招项目 9：agentic-infra-workflow

整理一套可展示的 AI Agent 工程流。

内容：

- `CLAUDE.md` / `AGENTS.md` 模板
- kernel development prompt
- benchmark prompt
- Nsight report prompt
- PR review prompt
- dangerous command deny list
- GitHub Actions correctness test
- 自动生成 benchmark report

回答面试官时要强调：

> 我用 Agent 生成 boilerplate、测试和 benchmark，但 kernel 核心逻辑、性能结论、Nsight 分析和最终 PR 都由我人工验证。

### 阶段三开源目标

秋招前至少做到以下之一：

- 1 个 merged PR。
- 3 个高质量 issue reproduction。
- 2 篇 benchmark report。
- 1 个 compiler-aware 小作品或高质量报告，能讲清输入表示、lowering / codegen、生成结果、correctness 验证方式和性能验证方式。
- 1 个被项目 maintainer 回复认可的性能分析。

优先级：

```text
Triton > vLLM > SGLang > FlashInfer > PyTorch extension > CUTLASS
```

原因：

- Triton 入门贡献相对友好。
- vLLM / SGLang 更贴近推理系统。
- FlashInfer 更贴近 kernel 但门槛高。
- CUTLASS 适合读和做 benchmark，不适合新手直接改核心。

## 最小可行作品集

### 2026 暑期实习版本

至少完成 3 个项目：

1. `agentic-cuda-kernel-playground`
2. `torch-triton-rmsnorm`
3. `llm-inference-cost-lab-v0`

表达能力：

```text
我有 CUDA 基础。
我能写真实 LLM 小算子。
我知道推理服务的成本指标。
我能用 Agent 加速工程，但有人工验证流程。
```

### 2027 秋招加强版本

升级成 5 个项目组合：

1. `llm-kernel-benchmark-suite`
2. `matmul-lab-cuda-triton-cutlass`
3. `tiny-llm-kernels`
4. `mini-vllm-style-kv-cache`
5. `agentic-infra-workflow`

组合表达：

```text
我会用 AI Agent 快速做工程。
我能写 CUDA / Triton kernel。
我能做 Nsight profiling。
我懂 LLM 推理系统。
我能用数据计算推理成本。
```

## GPU 最低成本策略

阶段一不需要 A100 / H100。

| 任务 | GPU |
|---|---|
| CUDA 编译测试 | T4 / L4 足够 |
| Triton softmax / RMSNorm | T4 / L4 / A10 |
| matmul 初版 | A10 / 4090 更好 |
| vLLM 小模型 | L4 / A10 / 4090 |
| Nsight profiling | 云主机，能装 Nsight Compute 即可 |

推荐节奏：

```text
每周租 1 次 GPU
每次 4 到 6 小时
平时本地写代码和让 Agent 生成框架
租 GPU 时集中编译、跑 benchmark、截图、记录数据
```

阶段二建议固定一种主力 GPU：

```text
日常开发：L4 / A10 / 4090
最终 benchmark：A100 可选
不要日常租 H100
```

所有 benchmark 必须记录：

- GPU 型号
- CUDA 版本
- PyTorch 版本
- Triton 版本
- batch size
- seq length
- dtype
- warmup 次数
- 测量方式

## 每周执行模板

适用于每天 2 到 4 小时。

### 周一：理论 + Agent 任务拆解

- 30 分钟：读 CUDA / Triton / vLLM 文档。
- 30 分钟：让 Agent 总结本周任务和代码结构。
- 90 分钟：人工写核心 kernel / 推理实验设计。
- 30 分钟：写 `tasks.md`。

产出：

- 本周目标
- kernel design note
- Agent prompt

### 周二：Coding

- 30 分钟：让 Agent 生成测试框架。
- 90 分钟：你写核心 kernel。
- 30 分钟：跑 correctness test。
- 30 分钟：让 Agent review 代码。

产出：

- 可运行 kernel
- correctness test
- review notes

### 周三：Benchmark

- 30 分钟：Agent 生成 benchmark matrix。
- 90 分钟：跑不同 shape / dtype。
- 30 分钟：保存结果。
- 30 分钟：人工判断异常数据。

产出：

- `benchmark.csv`
- `benchmark.md`

### 周四：Profiling

- 60 分钟：跑 Nsight Compute / Systems。
- 60 分钟：分析 memory throughput / occupancy / stall。
- 30 分钟：让 Agent 整理报告。
- 30 分钟：你修改报告结论。

产出：

- `profiling.md`
- Nsight 截图
- bottleneck 结论

### 周五：优化

- 30 分钟：列出 3 个优化假设。
- 90 分钟：实现其中 1 个。
- 30 分钟：跑 benchmark。
- 30 分钟：决定保留还是 revert。

产出：

- before / after 对比
- 优化结论

### 周六：GPU 集中实验

- 4 到 6 小时租 GPU。
- 集中跑 benchmark。
- 跑 vLLM / SGLang serving。
- 记录 TTFT / TPOT / TPS / cost。

产出：

- 成本表
- 性能图
- README 更新

### 周日：复盘 + 简历化

- 60 分钟：写周报。
- 60 分钟：更新 README。
- 60 分钟：写面试问答。
- 30 分钟：更新简历 bullet。
- 30 分钟：整理下周计划。

产出：

- 一条简历 bullet
- 一篇技术笔记
- 5 个面试 Q&A

## 简历表达

不要写：

```text
熟悉 CUDA，了解 vLLM，会使用 AI 编程工具。
```

应该写：

```text
实现 CUDA / Triton RMSNorm、Softmax、Tiled MatMul 等 LLM 常见算子，使用 CUDA event 与 Nsight Compute 进行性能分析；构建 vLLM/SGLang 推理 benchmark，统计 TTFT、TPOT、TPS 与 cost / 1M tokens，并通过 batch size、sequence length、dtype 配置分析延迟-吞吐-成本权衡。
```

再加一句：

```text
使用 Claude Code / Codex 辅助生成工程脚手架、测试与 benchmark，但 kernel 核心逻辑、correctness、profiling 结论和性能报告均人工审查。
```

项目 bullet 模板：

```text
基于 CUDA / Triton 实现 RMSNorm、Softmax、Tiled MatMul 等 LLM 常见算子，使用 PyTorch reference 进行 correctness test，并通过 CUDA event / Nsight Compute 分析 memory throughput、occupancy 与 stall 原因。
```

```text
构建 vLLM / SGLang 小模型推理 benchmark，覆盖不同 batch size、并发、prompt length、output length 与 dtype 配置，统计 TTFT、TPOT、TPS、显存占用和 cost / 1M tokens，分析延迟、吞吐和成本权衡。
```

```text
设计 AI Agent-assisted kernel development workflow，使用 Claude Code / Codex 生成工程脚手架、测试和 benchmark，并通过权限限制、review checklist、correctness gate 与人工 profiling 结论保证 Agent 代码可审查、可复现、可上线。
```

## 面试问题清单

### Kernel

- thread / block / grid 的关系是什么？
- warp 和 SIMT 是什么？
- memory coalescing 为什么重要？
- shared memory transpose 为什么更快？
- bank conflict 是什么，padding 怎么解决？
- occupancy 高一定好吗？
- vector add 为什么是 memory-bound？
- matmul 为什么更偏 compute-bound？
- softmax 如何保证数值稳定？
- RMSNorm 和 LayerNorm 区别是什么？
- RoPE 可以和 attention 前处理融合吗？
- INT8 dequant 为什么常和 matmul 融合？

### Profiling

- CUDA event timing 怎么写？
- warmup 为什么必要？
- benchmark 如何保证公平？
- Nsight Compute 主要看哪些指标？
- memory throughput 低说明什么？
- occupancy 低一定是问题吗？
- stall reason 如何指导优化？
- roofline thinking 怎么判断瓶颈？

### Serving

- TTFT 和 TPOT 分别代表什么？
- prefill 和 decode 的瓶颈有什么不同？
- 为什么 batch size 增大会提升 TPS 但可能恶化 TPOT？
- KV cache 为什么是显存瓶颈？
- paged KV cache 解决什么问题？
- continuous batching 如何提高 GPU 利用率？
- prefix cache 适合什么场景？
- speculative decoding 的收益和代价是什么？
- quantization 降成本的代价是什么？
- cost / 1M tokens 怎么算？

### Agent Workflow

- 哪些代码是 Agent 生成的？
- 哪些结论必须你人工判断？
- 你如何防止 Agent 写错 kernel？
- 你如何防止 Agent 改 benchmark 数据？
- Agent 生成的测试是否可信？
- PR review checklist 包含哪些内容？
- 为什么 AI Agent 会改变 AI Infra 工程师的工作方式？

## 执行原则

### Agent 可以做的事

- 生成项目脚手架。
- 生成 CMake / Python packaging。
- 生成 correctness test。
- 生成 benchmark matrix。
- 生成 README 初稿。
- 整理 Nsight 输出。
- 生成图表脚本。
- 做初步代码 review。

### Agent 不可以替你做的事

- 决定 kernel 核心逻辑是否正确。
- 修改 benchmark 数据。
- 删除或覆盖实验记录。
- 绕过 correctness test。
- 夸大性能结论。
- 把一次随机 benchmark 当成稳定结论。
- 替你判断是否能上线。

### 每个项目的强制 gate

- correctness test 通过。
- benchmark 命令可复现。
- 记录 GPU / CUDA / PyTorch / Triton 版本。
- README 区分 Agent 生成部分和人工验证部分。
- profiling 结论必须有指标支撑。
- 简历 bullet 只写自己能解释清楚的内容。

## 近期行动清单

- [ ] 创建 `agentic-cuda-kernel-playground` 仓库。
- [ ] 写第一版 `AGENTS.md` 和 `CLAUDE.md`。
- [ ] 实现 vector add。
- [ ] 加入 CUDA event benchmark。
- [ ] 写 correctness test。
- [ ] 记录第一篇 `benchmark.md`。
- [ ] 学 Triton fused softmax tutorial。
- [ ] 学 vLLM benchmark 脚本。
- [ ] 整理第一版简历 bullet。

## 一句话定位

```text
我要成为能指挥 AI Agent 快速开发，但自己能验证正确性、分析 GPU 性能、降低 LLM 推理成本的人。
```
