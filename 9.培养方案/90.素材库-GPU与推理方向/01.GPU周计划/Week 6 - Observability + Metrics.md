---
title: Week 6 - Observability + Metrics
date: 2026-05-14
tags:
  - AI-infra
  - 素材库-GPU与推理方向
  - GPU周计划
  - 计划
status: active
---

# Week 6 - Observability + Metrics

> [!goal] 本周目标
> 从“跑 benchmark 的人”升级到“能定位线上 serving 问题的人”：部署 vLLM OpenAI-compatible server，接入 Prometheus / Grafana，观察 queueing、running / waiting requests、running batch、prefill queue、decode step、TTFT、TPOT、KV cache usage、tokens/s，并写出一份 serving runbook。

## 学习目标

完成这一周后，应该能回答七个问题：

1. **server-level metrics 和 request-level metrics 有什么区别？**
2. **TTFT 变高时如何判断是 queueing、prefill、网络还是 KV cache 压力？**
3. **TPOT 变高时如何判断是 decode 慢、batch 太大，还是 GPU / KV cache 读带宽受限？**
4. **running requests 和 waiting requests 如何反映排队压力？**
5. **GPU cache usage / KV cache usage 接近满时应该怎么处理？**
6. **dashboard 应该服务谁？** 能区分开发调试、SRE 监控、benchmark 报告三种视角。
7. **runbook 应该怎么写？** 能把“看到现象 -> 判断原因 -> 下一步动作”写清楚。

## 1. 为什么 Week 6 补 Observability

Week 5 的 benchmark 能告诉你“结果变了”，但线上 Infra 更关心：

```text
为什么变了？
现在瓶颈在哪？
要调哪个参数？
是不是已经接近 SLO 风险？
```

没有 observability，benchmark 报告很容易停留在表格；有 metrics 和 dashboard，才能把 TTFT / TPOT / TPS 的变化接到 queue、KV cache、GPU cache utilization 和失败请求上。

## 2. 必做实验场景

| 场景 | 负载 | 观察重点 |
|---|---|---|
| 低并发短 prompt | request rate 低、短输入短输出 | baseline latency 和 tokens/s |
| 高并发短 prompt | request rate 高、并发高 | waiting requests、queue time、p95 latency |
| 低并发长 prompt | 长输入短输出 | prefill time、TTFT、GPU memory |
| 高并发长 prompt | 长输入 + 高并发 | backlog、failed requests、KV cache pressure |
| 共享 prefix | system prompt / tools schema 相同 | prefix cache hit rate，如果框架暴露 |
| KV cache 接近满 | 长上下文 + 高并发 | GPU cache usage、eviction / failure |
| 人为 backlog | 请求到达率超过服务能力 | queue time、waiting requests、SLO 风险 |

## 3. 最小部署结构

```text
benchmark client
    |
    v
vLLM OpenAI-compatible server  --->  /metrics
    |                                  |
    v                                  v
GPU / KV cache                  Prometheus
                                      |
                                      v
                                   Grafana
```

产出文件建议：

```text
observability/
├── docker-compose.yaml
├── prometheus.yml
├── grafana-dashboard.json
├── runbook.md
└── observability_report.md
```

## 4. 必须记录指标

| 类别 | 指标 |
|---|---|
| Request state | running requests、waiting requests、finished requests、failed requests |
| Scheduler / batching | waiting queue、running batch、prefill queue、decode step、batch size，如果框架暴露 |
| Latency | TTFT、TPOT / ITL、E2E latency、queue time |
| Token throughput | prompt tokens/s、generation tokens/s、output TPS |
| Cache | GPU cache usage、KV cache blocks、prefix cache hit rate、cache hit rate，如果暴露 |
| Resource | GPU utilization、GPU memory、CPU memory、network latency，可选 |
| Reliability | error rate、timeout、OOM、server restart |

> [!warning] 线上观测不是只看 GPU utilization
> GPU utilization 高但 TPS 不涨，可能是 batch 太大、KV cache 读带宽受限、queue 已经堆积、decode 阶段 tail latency 变差，或者请求失败被吞掉。必须结合 request-level metrics 判断。

## 5. 实现顺序

### Day 1：启动 server + 暴露 metrics

- 启动 vLLM OpenAI-compatible server。
- 确认 `/metrics` 可以访问。
- 记录 vLLM、CUDA、PyTorch、Triton、GPU、模型和 dtype。

验收：

- `curl /metrics` 或等价命令能看到 Prometheus 格式指标。
- `serving_metrics.md` 写出关键指标含义。

### Day 2：Prometheus

- 写 `prometheus.yml`。
- 配置 scrape interval。
- 确认 Prometheus 能拉取 vLLM metrics。

验收：

- Prometheus UI 能查询 running / waiting requests、tokens/s、latency 相关指标。

### Day 3：Grafana dashboard

- 创建 dashboard。
- 至少包含 latency、queue、requests、scheduler / batching、tokens/s、cache、GPU memory 七类面板。

验收：

- `grafana-dashboard.json` 可导入。
- 每个面板都有明确用途，不只堆指标。

### Day 4：复跑 Week 5 场景

- 用 Week 5 的低延迟、高吞吐、长上下文、共享 prefix 场景复跑。
- 观察 dashboard 随负载变化。

验收：

- 能指出至少 2 个 dashboard 上的指标拐点。

### Day 5：制造 backlog

- 把 request rate 提到服务能力之上。
- 观察 waiting requests、queue time、failed requests 和 p95 latency。

验收：

- 能解释 backlog 为什么会推高 TTFT。

### Day 6：runbook

写 `runbook.md`：

- 症状：TTFT 变高。
- 症状：TPOT 变高。
- 症状：GPU utilization 高但 TPS 不涨。
- 症状：KV cache usage 接近满。
- 症状：failed requests 增多。
- 症状：continuous batching 下 tail latency 恶化。
- 症状：chunked prefill 开启后 TTFT / TPOT 同时变化。
- 症状：prefix cache hit rate 或 cache hit rate 异常。

每个症状都写：

```text
先看哪些指标 -> 可能原因 -> 下一步实验 -> 临时缓解 -> 长期修复
```

### Day 7：报告和面试表达

形成一句项目表达：

```text
搭建 vLLM serving observability lab，接入 Prometheus / Grafana，围绕 queue time、running / waiting requests、TTFT、TPOT、GPU cache usage 和 tokens/s 编写 runbook，用于定位高并发、长上下文、shared prefix 和 request backlog 场景下的 serving 瓶颈。
```

## 6. 本周产出文件

| 文件 | 内容 | 验收方式 |
|---|---|---|
| `docker-compose.yaml` | Prometheus / Grafana 最小部署 | 能启动 |
| `prometheus.yml` | scrape vLLM metrics | 能拉取 metrics |
| `grafana-dashboard.json` | latency / queue / cache / tokens/s 面板 | 能导入 |
| `observability_report.md` | 场景、截图、指标解释 | 结论能被图支持 |
| `runbook.md` | 故障定位步骤 | 能按症状排查 |

## 7. 验收标准

- vLLM server 暴露 metrics。
- Prometheus 能采集指标。
- Grafana 至少有 7 类面板，包含 scheduler / batching 视角。
- 至少复跑 Week 5 的 4 类 benchmark 场景。
- runbook 能回答 TTFT、TPOT、GPU utilization、KV cache usage、failed requests、batching tail latency、prefix cache 命中异常七类问题。
- 报告不只贴图，必须解释指标变化和下一步动作。

## 面试问题

- server-level metrics 和 request-level metrics 有什么区别？
- TTFT 变高时如何判断是不是 queueing？
- TPOT 变高时为什么可能是 decode 阶段慢？
- running requests 和 waiting requests 分别说明什么？
- GPU utilization 高但 TPS 不涨，可能是什么原因？
- KV cache usage 接近满时有哪些处理手段？
- 如何从指标判断瓶颈在 scheduler、prefill、decode、KV cache 还是通信？
- continuous batching 为什么可能改善吞吐但恶化 tail latency？
- chunked prefill 开启后，TTFT 和 TPOT 为什么可能一起变化？
- 为什么 observability 对 AI Infra 面试加分？
- Agent 能不能自动写 runbook？为什么最终结论仍要人工判断？

## 关联知识

- [[Week 5 - Serving Benchmark Harness]]
- [[Week 7 - KV Cache + Prefix Cache + Paged KV]]
- [[AI Agent Native AI Infra GPU Performance Engineer 培养方案]]
- [vLLM metrics](https://docs.vllm.ai/en/latest/design/metrics/)
