---
tags:
  - AI-infra/素材库-GPU与推理方向/项目分析/Week05
---
# Week05 渐进式练习

> 配合 [[Week 5 ServingBench 服务基准测试框架 项目解析]] 使用。答案基于仓库源码与 docs 推理；凡涉及实测 TTFT/TPOT/TPS 数字之处，给出**预期方向**，真实数值请在你的 GPU 上 `make smoke` / `make bench-all` / `make report` 后回填到 [[9.培养方案/90.素材库-GPU与推理方向/04.项目分析/Week05/profiling|profiling]]。

> [!note] 运行说明
> 本机 GTX 1660 SUPER 6 GB / Turing(sm_75)，已用缩小版 smoke 真实跑通五类 workload 形状。GPU benchmark 需脱离 sandbox 执行；`make check` / `--dry-run` 可在无 GPU / 无 vLLM 时离线验证配置与命令：
> ```bash
> make check                                   # 每个 config 解析成真实 argv，不执行
> python -m harness.run_benchmark --config configs/low_latency.yaml --dry-run
> ```

---

## Day 1：指标与负载设计

### 练习 1.1：TTFT / TPOT / E2E 各由哪个阶段决定

`measurement.percentile_metrics: "ttft,tpot,itl,e2el"` 记录四个延迟指标。说出 TTFT、TPOT、E2E 分别主要由推理的哪个阶段决定，并写出 E2E 的近似公式。

**答案：**

- **TTFT**（首 token 延迟）由 **prefill** 决定——首 token 之前要先处理完整个 prompt。
- **TPOT**（每输出 token 延迟）由 **decode** 决定——逐 token 生成的"打字速度"。
- **E2E** ≈ `TTFT + (output_len - 1) × TPOT`，即首 token 等待 + 后续逐 token 累加。

ITL 是相邻 token 实际间隔，TPOT 是 ITL 的一种平均口径；记 ITL 分布能暴露 tail 卡顿。

### 练习 1.2：为什么 `benchmark_config.yaml` 要把 environment 和 seed 写进去

base config 里有 `environment.{gpu,cuda,driver,pytorch,triton}`、`measurement.seed: 0`、`cost.gpu_hourly_usd: 1.20`。这些和"测得快不快"无关，为什么还要写进配置？

**答案：**

对应铁律一"配置是实验的唯一真相来源"。serving benchmark 的可信度依赖**可复现**：换机器、换时间重跑要能对齐结果。版本（vLLM/PyTorch/Triton）直接影响 kernel 选择与性能，driver/GPU 决定能力，`seed` 固定随机 prompt 采样，`gpu_hourly_usd` 是 cost/1M 的唯一假设。把它们写进 YAML，实验才能"仅凭一个文件重建"，而不依赖记忆里的命令行。

### 练习 1.3：base 与场景文件的合并规则

`low_latency.yaml` 只有 5 个 workload 字段，没写 model/engine/measurement。它跑起来用的是什么 model？合并发生在哪个函数？

**答案：**

用 base `benchmark_config.yaml` 的 `Qwen/Qwen2.5-0.5B-Instruct` fp16。`config.py:load_config` 调 `_deep_merge(base_raw, scenario_raw)` 递归合并：dict 递归、标量覆盖。场景文件只写差异，未写的字段全部继承 base。这样改一个场景不会牵动公共配置，且公共配置只有一处。

---

## Day 2：跑通单引擎 baseline

### 练习 2.1：健康检查为什么不能只等 `/health`

`reproduce.sh` 的健康检查循环里，除了 `curl /health`，还有一句 `kill -0 "$SERVER_PID"`。去掉它会怎样？

**答案：**

如果 server 进程**启动即崩**（如 OOM、模型下载失败），`/health` 永远不会通。只等 `/health` 会一直循环到 `HEALTH_TIMEOUT` 超时（默认 300s）才退出，白等。`kill -0 PID` 探测进程是否还活着；进程一死就立刻 `tail` 日志并退出，把失败原因（如显存不足）第一时间暴露出来。

### 练习 2.2：warmup 产物为什么要删

`run_benchmark.py` 跑 `warmup + repeats` 次，但 warmup 的 JSON 用 `unlink(missing_ok=True)` 删掉。为什么不留着？

**答案：**

第一次跑包含 CUDA graph 捕获、权重加载、缓存预热等一次性开销，指标偏差大。若留下来会被 `summarize.py` 当成正式数据聚合进 CSV，污染统计。主动删除保证只有稳态的 `repN` 进入结果。这与 Week 1-4 用 cudaEvent 计时前先 warmup 多次是同一思路。

### 练习 2.3：`--ignore-eos` 解决什么问题

`to_vllm_args` 在 `streaming=false` 时加 `--ignore-eos`。它对指标的可比性有什么作用？

**答案：**

模型可能在没吐满 `random-output-len` 之前就生成 EOS 提前停止，导致不同请求实际输出长度不一，TPOT/TPS 不可比。`--ignore-eos` 强制忽略 EOS、吐满指定 token 数，保证每个请求输出长度固定——这是"固定 output 分布"这一公平性前提的实现手段。

### 练习 2.4：没装 vLLM 能验证什么

在没有 GPU、没装 vLLM 的环境里，`make check` 能验证 harness 的哪些部分？不能验证什么？

**答案：**

能验证：YAML 解析、base+场景深合并、`_validate` 校验、`to_vllm_args` / `build_command` 的字段→flag 映射（每个 config 解析成真实 argv 打印出来）。`run_benchmark` 找不到 `vllm` 会自动降级 dry-run。不能验证：真实指标、server 健康、显存是否够、模型能否加载——这些必须真机跑。

---

## Day 3：request rate / max concurrency 矩阵

### 练习 3.1：开环与闭环

用一句话区分 `request_rate` 和 `max_concurrency`，并说明 `request_rate=4` 但 `max_concurrency=1` 会发生什么。

**答案：**

`request_rate` 是每秒**发出**多少（开环，不管服务端处理得过来与否），`max_concurrency` 是同时**在飞**上限（闭环，到上限就不发新请求）。`rate=4` + `concurrency=1` 时，虽然想每秒发 4 个，但同一时刻只允许 1 个在飞，后续请求被客户端按住排队——并发被**人为**卡死，吞吐上不去，但这不是服务端的真实能力上限。

### 练习 3.2：`high_throughput.yaml` 为什么用 `inf` + `64`

`high_throughput.yaml` 配 `request_rate: inf`、`max_concurrency: 64`。这等价于什么压测模式？

**答案：**

`rate=inf` 表示尽可能快地发，`max_concurrency=64` 把在飞数封顶 64。组合起来 ≈ "稳态 64 路并发压测"：客户端始终维持 64 个在飞请求，完成一个立刻补一个。这能把服务端压到 64 并发下的吞吐上限，同时观察 p95/TPOT 在高并发下如何恶化。

### 练习 3.3：扫描时怎么找拐点

要找 "p95 开始恶化的拐点"，docs 建议怎么扫？固定什么、变什么？

**答案：**

固定 prompt/output 长度，分别扫 `request_rate ∈ {低,中,inf}` 和 `max_concurrency ∈ {低,中,高}`，每组记一行。随并发上升，TPS 先升后平、而 TTFT/TPOT 的 p95 在某点开始陡升——那个点就是延迟-吞吐权衡的拐点。`high_throughput.yaml` 注释明确说"复制这个文件、改这两个值"来扫。

### 练习 3.4：static 与 continuous 的命令差异

`batching_compare.yaml` 设 `batching_mode: static`，它实际跑的命令和 continuous 场景差在哪？为什么 static 不能配 `--engine sglang`？

**答案：**

static 走 `vllm bench throughput`（离线、无 live server、固定 batch、`--output-json`）；continuous 走 `vllm bench serve`（在线、打活 server、`--save-result`）。`_apply_engine_override` 里有断言：`batching_mode=static` 且 engine≠vllm 直接报错——因为离线吞吐基线是 vLLM 专有的 `bench throughput`，SGLang 没有等价离线入口。

---

## Day 4：prompt / output length 矩阵

### 练习 4.1：prefill-heavy vs decode-heavy

`long_context.yaml` 用 `fixed_3072` prompt + `fixed_128` output；某个 decode-heavy 场景则相反。哪个是 prefill-heavy？它主要压高哪个指标？

**答案：**

长 prompt + 短 output（`3072/128`）是 **prefill-heavy**：大量计算花在处理 prompt 上，主要压高 **TTFT**；输出短，decode 少，TPOT 影响小。反过来短 prompt + 长 output 是 decode-heavy，TPOT/TPS 才是主角。smoke 数据印证：smoke_long_context 的 TTFT p95 达 1478.6 ms，但 TPOT p95 仅 6.9 ms。

### 练习 4.2：为什么长 prompt 推高 TTFT 却几乎不动 TPOT

从 prefill / decode 机制解释这个现象。

**答案：**

prefill 要一次性处理完整个 prompt 才能产出首 token，计算量随 prompt 长度线性增长 → TTFT 随 prompt 变长而升高。而 decode 阶段每步只在已有 KV cache 基础上生成一个新 token，单步成本主要取决于模型大小和当前序列长度增量，受**初始 prompt 长度**影响小 → TPOT 基本稳定。所以长上下文是"首 token 慢、后续打字不慢"。

### 练习 4.3：`parse_dist` 怎么把 `uniform_256_768` 翻成 vLLM 参数

vLLM random 数据集按"均值 ± 幅度比"采样。`parse_dist("uniform_256_768")` 返回什么？

**答案：**

`mean = (256+768)//2 = 512`，`range_ratio = 1 - 256/512 = 0.5`，返回 `(512, 0.5)`。vLLM 在 `[512×0.5, 512×1.5] = [256, 768]` 采样长度。`fixed_N` 则返回 `(N, 0.0)`，幅度为 0 即定长。这层翻译让配置用人类友好的 `fixed_/uniform_` 写法，而不必直接写 vLLM 的均值+幅度。

### 练习 4.4：长上下文场景为什么要把并发压到 2

`long_context.yaml` 配 `max_concurrency: 2`，注释提到 6 GB GPU。为什么长上下文必须低并发？

**答案：**

每个长 prompt 都要在 KV cache 里存 3072 token 的 key/value，显存占用随 `并发数 × prompt 长度`增长。6 GB 显存下，高并发 + 长 prompt 极易 OOM 或被 vLLM 拒绝请求（failed）。压到并发 2 是为了在小显存上跑通，并把 KV cache 压力控制住——长上下文场景的瓶颈信号正是 KV cache / 显存。

---

## Day 5：共享 prefix

### 练习 5.1：`shared_prefix_ratio: 0.86` 怎么算

`shared_prefix.yaml` 里 `shared_prefix_tokens: 800`、`prompt_length_distribution: fixed_128`、`shared_prefix_ratio: 0.86`。这个 0.86 怎么来的？它映射成 flag 吗？

**答案：**

`800 / (800 + 128) ≈ 0.86`：共享前缀 token 占整条 prompt 的比例。它**不**映射成任何 flag，是纯描述性字段，只为写进报告/CSV，让"这个场景共享前缀占多少"可量化（对应 plan 验收"避免把共享 prefix 场景只写成文字"）。真正生效的是 `shared_prefix_tokens: 800` → `--random-prefix-len 800`。

### 练习 5.2：两种共享前缀的造法

要构造共享前缀，仓库给了两条路：`shared_prefix_tokens`（random）和 `datasets.py` 生成 custom JSONL。它们分别更适合测什么？

**答案：**

- `--random-prefix-len N`：给每个请求加**相同的 N 个随机 token** 前缀，简单、纯压 prefix cache 机制，前缀无语义。
- `datasets.py` 的 custom JSONL：生成**真实结构**的前缀（system prompt + tools schema / 长文档）+ 各异的用户问题，更接近 Agent / RAG / 编码助手流量。

前者验证"缓存命中机制"，后者验证"真实 workload 下的收益"。

### 练习 5.3：怎么量出 prefix cache 对 TTFT 的收益

`shared_prefix.yaml` 注释说要"翻转 prefix_caching 跑两次"。具体怎么操作？预期哪个指标变化？

**答案：**

同一场景跑两次：`prefix_caching: false`（`--label nocache`）和 `true`（`--label cache`），对比两份结果的 **TTFT**。命中共享前缀时跳过该段 prefill，所以 cache-on 的 TTFT 应明显低于 cache-off；而 TPOT（纯 decode）几乎不变。引擎侧由 `serve_vllm.sh` 的 `--enable-prefix-caching` / SGLang `--disable-radix-cache` 控制。

### 练习 5.4：datasets.py 为什么不用 `random.Random()`

`datasets.py` 用固定 `_WORDS` 词池 + `salt` 生成 filler，而不是 `random` 模块。为什么？

**答案：**

可复现。`random` 不固定种子会让每次生成的数据集不同，破坏"同一配置→同一实验"的前提。确定性生成保证任何机器、任何时间跑出**逐字节相同**的 JSONL，benchmark 才能公平对比。注意它仍给每请求加 `(request #i, vary=...)` 微扰，避免请求完全相同。

---

## Day 6-7：聚合、报告与复盘

### 练习 6.1：summarize 怎么兼容两种 JSON

`summarize.py` 要处理 `vllm bench serve` 和 `vllm bench throughput` 两种输出。它靠什么字段判断、各取哪些指标？

**答案：**

靠 `is_serve = "request_throughput" in d or "mean_ttft_ms" in d` 分流。serve（在线）JSON 取 TTFT/TPOT/ITL/E2E 的 p50/p95/p99 + output/request throughput；throughput（离线）JSON 只有 `tokens_per_second` / `requests_per_second`，连 output token 数都要用 config 的 `output_length_distribution` 反推。最后落进**同一套 CSV 列**，缺的留空。

### 练习 6.2：cost/1M output tokens 的推导

`summarize.py` 怎么从 `gpu_hourly_usd` 算出 `cost_per_1m_output_usd`？写出公式。

**答案：**

```text
usd_per_sec = gpu_hourly_usd / 3600
cost_per_1m_output = usd_per_sec / output_tps × 1e6
```

直觉：每秒花多少钱 ÷ 每秒产出多少 token = 每 token 成本，再 ×1e6 得每百万 token 成本。**吞吐越高、单 token 越便宜**——这把延迟-吞吐权衡直接换算成钱。smoke 数据里 static batching 73 TPS 对应 4.56 美元/1M，远低于低并发场景的 10+ 美元。

### 练习 6.3：为什么 harness 坚决不自己算指标

plan 反复强调 harness 只 wrap `vllm bench`、不自算指标。这对"benchmark 可信"意味着什么？

**答案：**

把指标计算交给经过验证的官方实现，"公平可信"就归约成"配置是否固定、命令是否记录"，而不依赖一段易错的自研统计代码。一旦自己写延迟/吞吐统计，别人就得相信"你这段代码没 bug、口径和业界一致"。这也是 Agent 友好的边界：Agent 能生成脚本/表格，但 `results/raw/*.json` 原样不改、下结论的是人。

### 练习 6.4：用真实 smoke 数据解释两个现象

用 [[9.培养方案/90.素材库-GPU与推理方向/04.项目分析/Week05/profiling|profiling]] 里的 smoke 表，挑数据解释"TTFT 低但 TPS 不高"和"TPS 上升但 TPOT/p95 变差"。

**答案：**

- **TTFT 低但 TPS 不高**：smoke_low_latency TTFT p95 仅 131 ms、TPOT 7.3 ms，但并发只有 4，output TPS 仅 30.66——单请求快，但同时在跑的少，系统吞吐自然不高。
- **TPS 上升但 TPOT/p95 变差**：从 low_latency 到 high_throughput（rate=inf），TTFT p95 飙到 1046 ms、TPOT p95 到 133 ms——更多请求争 GPU，单请求每 token 等得更久。注意此例 TPS 没显著升（受 smoke 规模和并发=4 限制），正式矩阵需更大并发才能完整复现"TPS↑ 伴随 p95↑"。
