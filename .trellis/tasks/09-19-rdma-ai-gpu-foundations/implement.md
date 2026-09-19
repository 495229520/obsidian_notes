# 学习顺序与验收记录

资料入口：[[8.高性能存储/00.总纲]]。以下勾选表示用户学习验收，不由笔记搬迁自动勾选。跨阶段依赖按顺序推进，测量从第一个实际实验开始使用。

## A. CUDA 数据搬运与同步

阅读：[[8.高性能存储/8. AI GPU 数据路径/学习补充/3.2 CUDA Runtime 进阶]] 第 1–6、8 节；[[8.高性能存储/8. AI GPU 数据路径/学习补充/3.3 CUDA Host-Device 数据传输瓶颈与进程线程角色]] 第 3–8 节。

- [ ] 记录可用 Linux/CUDA/GPU/RNIC 环境与不可验证项；不购买或租用资源。
- [ ] 画出 CPU 提交 → Host 内存 → DMA → GPU 消费，标注所有权与可复用时点。
- [ ] 解释调用返回、Stream/Event 完成、跨设备可见性三个边界。
- [ ] 有 GPU 时做 pageable/pinned、同步/异步传输对照并保存原始数据；无 GPU 时仅完成代码与依赖分析，注明待实测。

## B. KV 数据模型与 PD 交接

阅读：[[8.高性能存储/8. AI GPU 数据路径/学习补充/3.11 LLM Serving Week 7 前置知识 - KV Cache + Prefix Cache + Paged KV]]、[[8.高性能存储/8. AI GPU 数据路径/学习补充/LLM 推理面试公式速算清单]]、[[8.高性能存储/8. AI GPU 数据路径/学习补充/3.12 LLM Serving Week 8 前置知识 - Prefill Decode + Open Source Repro]]；接 [[8.16 PD 分离（Prefill-Decode Disaggregation）下的 KV Cache 网络传输架构]]。

- [ ] 给定模型层数、KV head、维度、dtype 和序列长度，计算逻辑 KV 字节量并注明分片假设。
- [ ] 分开描述 Prefix 取回、PD handoff、Decode HBM 读取及各自计量口径。
- [ ] 画出目标容量准入 → 分块传输 → 就绪发布 → GPU 消费；说明超时与部分完成后的处理。

## C. 测量与性能归因

阅读：[[8.高性能存储/8. AI GPU 数据路径/学习补充/3.9 LLM Serving Week 5 前置知识 - Benchmark Harness + Metrics]]、[[8.高性能存储/8. AI GPU 数据路径/学习补充/3.10 LLM Serving Week 6 前置知识 - Observability + Metrics + Runbook]]；Nsight Systems 参考 [[8.21 NCCL 异步执行与性能：CUDA Stream、Overlap、nccl-tests 与故障诊断]]。

- [ ] 固定版本、负载、到达率/并发、warm-up 与测量窗口，列出成功和失败的统计口径。
- [ ] 用一条时间线区分排队、计算、传输与同步；没有 trace 时标注为设计图。
- [ ] 保存实验命令、原始结果和复现说明；区分 smoke、合成负载、真实推理及未完成项。

## D. 分布式通信接口

阅读：[[8.高性能存储/8. AI GPU 数据路径/学习补充/分布式推理通信与 TP 切分速记]]；配合 8.高性能存储的 NCCL 8.18–8.22。

- [ ] 画出 TP 的规约和 EP 的 All-to-All，解释与 KV handoff 的不同通信模式。
- [ ] 说明 PCIe、NVLink、RDMA 的适用路径，以及通信库与底层 verbs 的边界。
- [ ] 形成一份差距记录，再选择 Mooncake 或 NIXL 的一个传输路径作为后续任务，不同时开多个库的实现项目。

## 结果记录

目前没有学习验收或新增硬件实验结果。资料恢复日期：2026-09-19。后续每项完成时记录证据路径、环境与仍待验证的限制。
