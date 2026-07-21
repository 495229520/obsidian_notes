---
title: AI Infra 职业赛道与公司调研
updated: 2026-07-21
tags:
  - infra
  - 职业调研
  - 参考资料
status: reference
---

# AI Infra 职业赛道与公司调研（参考）

> [!note] 文档角色
> 这里保留网络、GPU、存储和云基础设施的公司与赛道对比，用于选公司和读岗位描述，不负责当前学习顺序。当前主线是 [[00.当前执行 - C++ 到 AI Infra 存储]]，对外定位以 AI Infra 存储 / 数据路径 C++ 工程师为准。

## 一句话结论

如果把目标压缩成一句话，我仍然建议你不要把自己定位成“纯业务层大模型应用工程师”，而要定位成：

**AI 基础设施里的推理系统工程师。**

**会 C++、Linux、网络、存储、GPU runtime、推理 Serving、性能分析，能进入 NVIDIA / AMD / Intel / Arista / Broadcom / Pure Storage / VAST / WEKA / Dell / HPE / Cisco 这类公司做系统工程的人。**

现在推理系统的问题已经非常底层。vLLM 已经把 TTFT、ITL、prefix cache、KV cache usage、请求队列、并发压测等指标暴露为生产级观测对象；SGLang 和 NVIDIA Dynamo 等系统也把 prefill/decode 分离、KV cache transfer、RDMA、NVLink、路由调度作为核心工程问题。这说明真正有壁垒的岗位，不是“会调用一个 LLM API”，而是能把 **C++、Linux、网络、缓存、并发、GPU、I/O、性能剖析** 串起来做工程落地的人。[^vllm-metrics][^sglang-pd][^nvidia-dynamo]

---
## 赛道优先级：企业版

| 赛道                                 | 稳定性 | 成长曲线 | 与你当前基础的匹配度 | 建议 |
|---|---:|---:|---:|---|
| **AI 数据中心网络与算存分离基础设施**             | 高 | 很陡 | 很高 | **第一主攻**。重点看 Arista、Broadcom、Cisco、HPE Juniper、Marvell、NVIDIA Networking。 |
| **GPU / AI accelerator 系统软件与推理性能** | 中高 | 很陡 | 很高 | **强主攻**。重点看 NVIDIA、AMD、Intel；再看 Groq、Cerebras、Tenstorrent。 |
| **企业级 AI 存储、并行文件系统与 NVMe SSD**     | 高 | 稳定上升 | 高 | **务实主攻**。重点看 Pure Storage、VAST Data、WEKA、Dell、HPE、NetApp、Solidigm、Micron、Western Digital。 |
| **CXL / DPU / SmartNIC / IPU**     | 中 | 很陡 | 中高 | **前瞻储备**。重点看 NVIDIA BlueField、Intel IPU、Astera Labs、Broadcom、Marvell。 |
| ** GPU cloud / AI cloud 基础设施**     | 中 | 很陡 | 中高 | **补充投递**。重点看 CoreWeave、Lambda、Crusoe、Nebius、Nscale、OVHcloud。 |

---

## 主攻 AI 基础设施，而不是普通互联网应用岗

你的技术基础明显偏向 **系统、底层、性能、并发、网络和 C++ 工程**。这类能力在公司里最容易对应到以下岗位：

- Systems Software Engineer
- GPU Runtime Engineer
- CUDA / HIP Kernel Engineer
- AI Infrastructure Engineer
- Distributed Systems Engineer
- Storage Systems Engineer
- Network Systems Engineer
- Performance Engineer
- Inference Platform Engineer
- Data Center Software Engineer

这些岗位与“普通后端 CRUD + LLM API 调用”不是一个壁垒层级。AI 基础设施公司正在把模型服务变成一个完整的分布式系统问题：请求进入网关后，要考虑排队、batching、prefill/decode 分离、KV cache、prefix cache、GPU memory、GPU-to-GPU 通信、RDMA、NVLink、NVMe-oF、GDS、p99 latency、吞吐和成本。vLLM、SGLang、TensorRT-LLM、NVIDIA Dynamo、AMD ROCm、Intel Gaudi software 这些生态都在证明：**推理系统不是一个模型问题，而是系统工程问题。**[^vllm][^vllm-metrics][^sglang-pd][^tensorrt-llm][^rocm][^intel-gaudi]

所以，企业版的最优方向不是“去 OpenAI/Anthropic 做应用层开发”这种极难且偏研究/产品的路线，而是：

**进入 AI 基础设施供应链，在 GPU、网络、存储、调度、推理 Serving、性能工具链这些环节里做系统工程。**

---

## 主攻赛道一：AI 数据中心网络与算存分离基础设施

这是企业版里我最建议你作为第一职业标签去打的方向。原因是：

1. 它与你的 C++、Linux、网络、epoll、并发、性能排障基础高度匹配。
2. 它不是单一模型周期的热点，而是 AI 数据中心长期扩张的底层需求。
3. 它能迁移到云、存储、数据库、GPU 集群、网络设备、HPC 等多个领域。

在企业里，这条线对应的代表公司是：

- **Arista**：AI data center Ethernet fabric、RoCE、EOS、CloudVision。
- **Broadcom**：Tomahawk / Jericho switch ASIC、Thor Ultra 800G AI NIC、RoCE / UEC 相关网络芯片。
- **Cisco**：Nexus、AI data center networking、Nexus HyperFabric AI clusters。
- **HPE Juniper**：HPE 已完成对 Juniper 的收购，组合后更偏 AI-native networking 与完整企业网络栈。
- **Marvell**：PAM4 / coherent DSP、光互连、custom silicon、AI data center interconnect。
- **NVIDIA Networking**：Spectrum-X Ethernet、ConnectX SuperNIC、NVLink、BlueField DPU、InfiniBand。

Broadcom 在 2025 年发布了 Thor Ultra 800G AI Ethernet NIC，强调 UEC 与增强 RoCE 支持；Arista 的 AI networking 页面也明确把 Ethernet-based AI/ML workloads 作为核心方案；Cisco 则把 AI 网络问题描述为不仅需要带宽，还需要自动化、可观测性、安全与确定性性能。[^broadcom-thor][^arista-ai][^cisco-ai-networking]

对你来说，最适合切入的不是交换芯片 RTL，而是下面这些系统软件方向：

- RDMA / RoCE 通信库与调优
- NIC / SmartNIC / DPU 相关软件
- 网络拥塞控制、ECN、PFC、lossless Ethernet 诊断
- AI cluster 网络观测与压测
- NVMe-oF over TCP / RoCE / InfiniBand
- GPU cluster 通信链路分析
- 推理系统中的 KV cache transfer 与网络瓶颈定位
- 高并发服务中的 p50 / p99 / tail latency 排障

如果你想做一个能放进简历的项目，可以做：

**“LLM 推理集群网络压测与诊断工具”**

功能包括：

- 对 vLLM / SGLang 服务发起可控并发请求；
- 记录 TTFT、ITL、TPOT、p95/p99 延迟、吞吐；
- 模拟 bursty traffic；
- 对比普通 TCP、RDMA/RoCE 环境下的通信延迟；
- 记录 GPU memory、KV cache 命中率、队列长度；
- 输出性能诊断报告。

这个项目比“写一个聊天机器人”更能打动 AI 基础设施团队。

---

## 主攻赛道二：GPU / AI Accelerator 系统软件与推理性能

** GPU / AI accelerator 系统软件与推理性能。**

核心目标公司如下：

### 1. NVIDIA：第一优先级

NVIDIA 是这条线最强目标。它不只是 GPU 公司，而是覆盖了 GPU、NVLink、InfiniBand、Spectrum-X Ethernet、BlueField DPU、CUDA、TensorRT-LLM、NIM、Dynamo、GPUDirect Storage 等完整 AI 基础设施栈。TensorRT-LLM 官方文档明确面向 LLM 推理加速和优化；NVIDIA NIM 是面向加速推理部署的微服务；Dynamo 则进一步把 disaggregated inference、prefill/decode 分离和集群级推理调度做成系统栈的一部分。[^tensorrt-llm][^nvidia-nim][^nvidia-dynamo]

对你最匹配的 NVIDIA 岗位不是泛泛的 AI 应用，而是：

- CUDA / GPU kernel engineer
- TensorRT-LLM / inference runtime engineer
- Systems software engineer
- Distributed inference engineer
- Networking / NCCL / NVLink / GPUDirect 相关岗位
- Storage / GPUDirect Storage / data path 优化岗位
- Performance analysis / profiling engineer

你的项目最好能体现：

- CUDA kernel 基础；
- Nsight / profiling；
- vLLM / TensorRT-LLM 部署与性能对比；
- TTFT、ITL、throughput、p99 latency 分析；
- GPU memory 与 KV cache 诊断；
- CPU-to-GPU / GPU-to-storage 数据路径理解。

### 2. AMD：第二优先级，适合走 ROCm / HIP / Instinct 路线

AMD 的优势在于 ROCm 是公开的 GPU software platform，面向 AMD Instinct 与 Radeon，支持 HIP、OpenCL、OpenMP 等编程接口。对于想做系统软件的人，AMD 的价值在于你可以围绕 ROCm、HIP、编译器、runtime、driver、profiling、vLLM-on-ROCm 做工程积累。[^rocm]

对你来说，AMD 适合准备这些关键词：

- ROCm
- HIP
- AMD Instinct
- vLLM on ROCm
- GPU kernel optimization
- AITER / attention kernel / GEMM kernel
- rocprof / profiling
- distributed inference on AMD GPUs

如果你想和 NVIDIA 路线区分开，可以做一个：

**CUDA kernel 到 HIP kernel 的迁移与性能对比项目。**

这个项目能说明你不是只绑定 CUDA，而是理解 GPU programming model、memory hierarchy、kernel launch、profiling 和跨平台性能差异。

### 3. Intel：适合 Gaudi / oneAPI / OpenVINO / IPU / Xeon 系统侧

Intel 的价值不只是 CPU。Intel Gaudi software 官方页面明确提供模型参考、库、容器、工具，用于 GenAI 和 LLM 的训练与部署；Intel 还有 oneAPI、OpenVINO、Xeon、Ethernet、IPU 等系统侧资产。[^intel-gaudi][^intel-ai-tools][^intel-ipu]

对你来说，Intel 更适合以下岗位方向：

- Gaudi software engineer
- oneAPI / compiler / runtime engineer
- OpenVINO inference engineer
- Xeon + accelerator inference optimization
- Ethernet / IPU / infrastructure offload engineer
- data center systems software engineer

Intel 的成长性可能不如 NVIDIA 那么陡，但岗位类型更广，系统软件、编译器、runtime、驱动、网络、存储、CPU 性能优化都有机会。

### 4. Groq / Cerebras / Tenstorrent：高成长但波动更高

这些公司可以作为“高成长补充主攻”：

- **Groq**：偏 LPU / inference accelerator 与低延迟推理。
- **Cerebras**：偏 wafer-scale AI system、大模型训练/推理基础设施。
- **Tenstorrent**：强调从硬件到软件到部署的 full-stack AI solution，并且强调开源框架集成与模型迁移。[^tenstorrent]

这类公司技术密度高、岗位更贴近 AI accelerator 系统栈，但招聘规模和稳定性通常不如 NVIDIA / AMD / Intel。适合你作为“冲刺型目标”，不要作为唯一安全垫。

---

## 主攻赛道三：企业级 AI 存储、并行文件系统与 NVMe SSD

- **Pure Storage**
- **VAST Data**
- **WEKA**
- **Dell Technologies**
- **HPE Storage**
- **NetApp**
- **Solidigm**
- **Micron**
- **Western Digital**

这条线非常适合你，因为它同时吃你的 C++、Linux、I/O、并发、性能分析、网络与系统工程能力。

AI 不是只消耗 GPU。长上下文推理、RAG、向量数据库、embedding、checkpoint、训练数据加载、KV cache offload 都会把存储推到关键路径。NVIDIA GPUDirect Storage 官方文档明确说明，它允许本地或远程 NVMe / NVMe-oF 与 GPU memory 之间建立直接路径，减少 CPU 参与；Dell AI Factory with NVIDIA 也强调 compute、storage、networking 可以模块化独立扩展。[^gds][^dell-ai-factory]

### 1. Pure Storage

Pure Storage 适合走企业级 AI storage、FlashBlade、AI data platform、NVIDIA reference architecture 这条线。Pure 与 NVIDIA 的 AI-ready infrastructure / AI Factory 参考架构说明它不是普通存储公司，而是在 AI 数据路径里做高性能存储系统。[^pure-nvidia]

适合岗位：

- Storage systems engineer
- Distributed storage engineer
- Filesystem engineer
- Performance engineer
- C++ backend / systems engineer
- Kernel / user-space I/O engineer

### 2. VAST Data

VAST Data 很适合关注。它的定位已经从“存储”扩展到 AI Operating System / AI data platform，覆盖 storage、database、compute，一体化服务 agentic computing 与数据密集型负载。Reuters 2026 年报道也提到 VAST Data 在新一轮融资后估值达到 300 亿美元，并且客户包括 xAI、CoreWeave 等 AI 基础设施客户。[^vast][^vast-reuters]

适合岗位：

- distributed systems engineer
- storage engine engineer
- metadata / database / filesystem engineer
- AI data platform engineer
- performance / scalability engineer

### 3. WEKA

WEKA 更偏高性能并行文件系统和 AI storage。它长期强调 GPUDirect Storage、AI/ML workload、high-performance data platform。WEKA 的优势在于非常贴近“GPU 等数据”的问题，适合你把 NVMe、RDMA、GDS、并行 I/O、缓存、metadata path 串起来。[^weka-gds]

适合岗位：

- distributed filesystem engineer
- storage performance engineer
- Linux systems engineer
- RDMA / NVMe / data path engineer

### 4. Dell / HPE / NetApp

这三类更偏成熟平台，稳定性更高。Dell AI Factory with NVIDIA 明确把计算、存储、网络作为模块化架构来扩展；HPE 完成 Juniper 收购后，AI networking 与 hybrid cloud infrastructure 组合更完整；NetApp 则长期在企业级数据管理、AI data pipeline、NVIDIA 生态里有存在感。[^dell-ai-factory][^hpe-juniper]

适合你投递的岗位：

- AI infrastructure engineer
- storage systems engineer
- server platform engineer
- networking systems engineer
- performance engineer
- cloud infrastructure engineer

### 5. Solidigm / Micron / Western Digital

这类更偏 SSD、NAND、firmware、controller、NVMe、PCIe、存储介质。它们不一定都直接写“AI 推理系统”，但 AI 对高性能 NVMe 与大容量存储的需求会持续增强。Solidigm 官方 AI storage 页面也明确把 GDS 与 GPU-NVMe 数据路径作为 AI workload 的关键方向之一。[^solidigm-ai]

适合你投递的岗位：

- SSD firmware engineer
- NVMe driver / validation engineer
- storage performance engineer
- systems software engineer
- PCIe / controller / firmware engineer

---

## 主攻赛道四：CXL / DPU / SmartNIC / IPU 作为前瞻能力

这条线建议继续学，但不要作为唯一求职标签。

对应公司是：

- **NVIDIA BlueField**：DPU，负责 networking、storage、security offload。
- **Intel IPU**：基础设施处理器，强调 tenant isolation、infrastructure offload、virtual storage。
- **Astera Labs**：AI connectivity、CXL / PCIe retimer、fabric switch、connectivity chips。
- **Broadcom / Marvell**：AI networking、NIC、switch ASIC、SerDes、光互连。
- **CXL Consortium 生态**：CXL memory expansion、memory pooling、accelerator coherent interconnect。

CXL Consortium 对 CXL 的定义是面向处理器、内存扩展和加速器的 cache-coherent interconnect；NVIDIA BlueField 官方定位则是为 networking、storage、security 提供软件定义硬件加速；Intel IPU 则强调 provider service 与 tenant application 隔离、基础设施 offload 与 virtual storage。[^cxl][^bluefield][^intel-ipu]

对你最合理的策略是：

**主简历写 C++ / Linux / RDMA / NVMe / 推理 Serving / GPU performance；项目或面试里补充 CXL、DPU、SmartNIC、IPU 的理解。**

也就是说，不要把自己包装成“只懂 CXL 概念的人”，而要包装成：

**已经能做 RDMA / NVMe / GPU / 推理性能项目，同时理解 CXL 与 DPU 为什么是下一阶段资源池化和 offload 的方向。**

---

## 企业清单：按投递优先级

### 第一梯队：强主攻

#### NVIDIA

最值得主攻。它覆盖 GPU、CUDA、TensorRT-LLM、NIM、Dynamo、NVLink、Spectrum-X、BlueField、GPUDirect Storage、InfiniBand。你可以从推理 runtime、GPU kernel、网络、存储、分布式推理、性能工具链多个角度切入。NVIDIA 也有官方 university recruiting / internship 页面。[^nvidia-university]

最匹配岗位关键词：

- Systems Software Engineer Intern
- GPU Computing Intern
- CUDA Kernel Engineer
- Deep Learning Performance Engineer
- AI Infrastructure Engineer
- Distributed Inference Engineer
- Networking Software Engineer
- Storage / GPUDirect Engineer

#### AMD

适合走 ROCm / HIP / Instinct / GPU runtime 路线。AMD 官方 student programs 面向 AI 与工程实习，ROCm 生态适合你做跨平台 GPU 系统软件积累。[^amd-students][^rocm]

最匹配岗位关键词：

- GPU Software Engineer Intern
- ROCm Engineer
- HIP Runtime Engineer
- Kernel Optimization Engineer
- AI Performance Engineer
- Systems Software Engineer

#### Intel

适合 Gaudi、oneAPI、OpenVINO、Xeon、Ethernet、IPU、编译器和 runtime。Intel 的系统侧岗位广，适合你从 C++ / Linux / 性能优化进入。[^intel-gaudi][^intel-ai-tools][^intel-ipu]

最匹配岗位关键词：

- AI Software Engineer Intern
- Compiler / Runtime Engineer
- Gaudi Software Engineer
- OpenVINO Engineer
- IPU / Ethernet Software Engineer
- Data Center Systems Engineer

#### Arista

如果你想把“AI 数据中心网络”作为主线，Arista 是非常合适的公司。它的 AI networking 方案明确面向 Ethernet-based AI/ML workloads。Arista 也有 university recruiting 页面，招 interns 和 new grads。[^arista-ai][^arista-university]

最匹配岗位关键词：

- Network Systems Software Engineer
- EOS Software Engineer
- Distributed Systems Engineer
- Platform Software Engineer
- Performance / Reliability Engineer

#### Broadcom

Broadcom 适合你关注 AI Ethernet NIC、switch ASIC、RoCE、UEC、Tomahawk、Jericho、SerDes、storage/networking silicon。它不是传统意义上的“写业务代码”公司，更偏底层网络芯片和系统软件生态。[^broadcom-thor][^broadcom-jericho]

最匹配岗位关键词：

- Ethernet / NIC Software Engineer
- Switch SDK Engineer
- Firmware Engineer
- Systems Software Engineer
- Performance / Validation Engineer

#### Pure Storage / VAST Data / WEKA

这三家是 AI storage 与分布式存储软件里非常值得你主攻的组合。Pure 更成熟，VAST 成长性强，WEKA 技术路线贴近高性能 AI 文件系统。[^pure-nvidia][^vast][^weka-gds]

最匹配岗位关键词：

- Distributed Storage Engineer
- Filesystem Engineer
- C++ Systems Engineer
- Linux Kernel / User-space I/O Engineer
- Performance Engineer
- AI Data Platform Engineer

---

### 第二梯队：稳定型主投

#### Cisco

Cisco 适合网络系统、Nexus、AI data center networking、observability、security。它比 Arista 更企业级、更平台化。对你来说，它适合网络系统软件、数据中心网络、AI fabric 运维与自动化方向。[^cisco-ai-networking]

#### HPE Juniper

HPE 已完成对 Juniper 的收购，形成更完整的 AI-native networking 与 hybrid cloud 基础设施组合。适合你投 networking、server、storage、AI infrastructure、systems software 相关岗位。[^hpe-juniper]

#### Dell Technologies

Dell AI Factory with NVIDIA 覆盖服务器、GPU、网络、存储、软件栈和企业部署，适合做 AI infrastructure、server platform、storage、networking、deployment automation。[^dell-ai-factory]

#### Marvell

Marvell 更偏数据中心互连、custom silicon、optical DSP、PAM4 / coherent DSP、AI fabric。如果你想往网络芯片、光互连、SerDes、data center interconnect 方向走，可以重点关注。[^marvell-ai]

#### NetApp / HPE Storage / Solidigm / Micron / Western Digital

这些公司更偏成熟稳定，适合长期做存储系统、SSD、NVMe、firmware、数据管理平台。它们可能没有 NVIDIA 那么“热”，但技术积累更厚，简历迁移性好。

---

### 第三梯队：高成长冲刺 / 欧洲补充

#### Groq / Cerebras / Tenstorrent

适合作为 AI accelerator 系统软件方向的冲刺型目标。技术密度高，但岗位数量、融资节奏、组织稳定性通常比 NVIDIA / AMD / Intel 更不确定。Tenstorrent 已公开强调 full-stack AI、硬件到软件到部署，以及开源框架集成。[^tenstorrent]

#### ARM

ARM 是英国公司，适合 CPU architecture、compiler、runtime、performance、embedded / edge AI、server CPU ecosystem。它不是最直接的“LLM 推理集群”公司，但如果你对体系结构、编译器、低层系统软件感兴趣，ARM 值得投。

#### Nebius / Nscale / OVHcloud / Scaleway

这些是欧洲 AI cloud / GPU cloud / HPC / sovereign cloud 方向的补充目标。它们更适合你投：

- cloud infrastructure engineer
- GPU platform engineer
- distributed systems engineer
- storage / networking engineer
- Kubernetes / cluster scheduling / observability engineer

相比 NVIDIA / AMD / Arista / Pure，这类公司更偏“运营 AI 基础设施”，而不是“制造 AI 基础设施核心部件”。但如果你想在欧洲找 AI infra 实习或全职，它们值得关注。

#### ASML / SiPearl / Graphcore

这些可以跟踪，但不建议作为你当前“推理系统工程”的第一主线。

- **ASML**：欧洲半导体设备巨头，系统软件、控制、嵌入式、计算光刻很强，但与 LLM 推理系统距离较远。
- **SiPearl**：欧洲 HPC processor 方向，偏高性能计算 CPU 生态。
- **Graphcore**：英国 AI accelerator 背景，但生态和岗位稳定性需要单独核查，不建议重仓。

---

## 从现在到 2027 年春季实习的准备路线：企业版

你现在的目标应该从“泛泛学 C++/Linux/AI”收敛成 3 个能讲清楚的项目。

### 项目一：推理 Serving 性能平台

目标：做一个接近生产观测逻辑的推理 benchmark / gateway。

建议功能：

- 支持 vLLM 或 SGLang 后端；
- 记录 TTFT、ITL、TPOT、吞吐、p95/p99；
- 支持固定并发、请求速率、突发流量；
- 记录 prefix cache hit rate、KV cache usage、GPU memory；
- 支持输出 CSV / JSON / Grafana dashboard；
- 对比不同 batch size、max tokens、prefix 重复率、并发度下的性能。

这个项目对应公司：

- NVIDIA
- AMD
- Intel
- Groq
- Cerebras
- Tenstorrent
- CoreWeave
- Lambda
- Nebius

### 项目二：高性能 I/O / NVMe-oF / GPUDirect Storage 实验

目标：证明你理解 AI 系统里的数据路径，而不是只会模型 API。

建议功能：

- 用 SPDK 或 Linux NVMe 工具做本地 NVMe benchmark；
- 对比本地 NVMe、网络存储、缓存命中/不命中的延迟；
- 学习 NVMe-oF over TCP / RDMA 的基本路径；
- 理解 GPUDirect Storage 的“存储到 GPU memory 直接路径”；
- 输出 CPU 占用、吞吐、IOPS、p99 latency 分析。

这个项目对应公司：

- Pure Storage
- VAST Data
- WEKA
- Dell
- HPE
- NetApp
- Solidigm
- Micron
- Western Digital
- NVIDIA GPUDirect Storage 团队

### 项目三：GPU kernel / CUDA-HIP 迁移与 profiling

目标：证明你能进入 GPU system software，而不是只会部署模型。

建议功能：

- 写几个 CUDA kernel：vector add、matmul tile、attention-like kernel、reduction；
- 用 Nsight Compute / Nsight Systems 做 profiling；
- 分析 memory bandwidth、occupancy、warp divergence、shared memory；
- 尝试把 CUDA kernel 改写成 HIP，在 ROCm 环境跑；
- 对比 NVIDIA 和 AMD 路线的工具链差异。

这个项目对应公司：

- NVIDIA
- AMD
- Intel
- Tenstorrent
- Groq
- Cerebras

---

你的简历主标题可以写成：

**C++ / Linux Systems Engineer focused on AI Infrastructure and LLM Inference Systems**

或者：

**AI Infrastructure Systems Engineer: C++, Linux, RDMA, NVMe, GPU Runtime, LLM Serving Performance**

技能栏建议这样组织：

**Languages**：C++17/20, Python, Bash  
**Systems**：Linux, pthread, epoll, io_uring, mmap, perf, flamegraph  
**Networking**：TCP/IP, RDMA/RoCE, InfiniBand concepts, congestion control, RPC  
**Storage**：NVMe, NVMe-oF, SPDK basics, filesystem, object storage, cache  
**GPU / AI Infra**：CUDA basics, ROCm/HIP basics, vLLM, SGLang, TensorRT-LLM concepts, KV cache, prefix cache  
**Performance**：TTFT, ITL, TPOT, throughput, p99 latency, profiling, benchmarking  
**Distributed Systems**：queueing, load balancing, batching, backpressure, observability

---

## 面试准备重点

不要只刷普通互联网八股。 AI 基础设施岗位更看重你能不能讲清楚系统问题。

你应该准备这些问题：

1. TTFT、ITL、TPOT 分别是什么？为什么要分开优化？
2. prefill 和 decode 为什么会互相干扰？
3. prefix cache 为什么能降低 TTFT？它什么时候无效？
4. KV cache 为什么会成为显存瓶颈？为什么会出现 SSD-backed KV cache？
5. RDMA 和 TCP 的数据路径差异是什么？
6. RoCE 为什么需要关注 PFC、ECN、拥塞控制？
7. NVMe-oF 解决什么问题？TCP / RDMA / InfiniBand 传输各有什么代价？
8. GPUDirect Storage 为什么可以降低 CPU 参与？
9. CUDA kernel 性能瓶颈如何定位？memory bandwidth、occupancy、warp divergence 分别怎么看？
10. CXL memory pooling 和 RDMA/NVMe-oF 的定位有什么区别？
11. DPU / IPU / SmartNIC 为什么适合做 networking、storage、security offload？
12. 一个 LLM inference gateway 如何做 admission control、batching、queueing 和 backpressure？

这些问题比“会不会调用 LangChain”更符合你要去的公司。

---

## 最终投递顺序

如果按“最适合你 + 企业相关性 + 2027 实习/全职可落地”排序，我建议：

### 第一优先级

**NVIDIA、AMD、Intel、Arista、Broadcom、Pure Storage、VAST Data、WEKA**

这组最贴合你的系统、网络、存储、GPU runtime、推理性能方向。

### 第二优先级

**Cisco、HPE Juniper、Dell Technologies、Marvell、NetApp、Solidigm、Micron、Western Digital**

这组更稳定，适合做网络、服务器、企业存储、数据中心系统软件。

### 第三优先级

**Groq、Cerebras、Tenstorrent、CoreWeave、Lambda、Crusoe、Nebius、Nscale、OVHcloud、Scaleway、ARM**

这组适合作为高成长冲刺或欧洲补充目标。

### 不建议作为第一主线，但可以跟踪

**ASML、SiPearl、Graphcore**

它们不是不好，而是与你当前“LLM 推理系统 + AI 基础设施”的目标没有 NVIDIA / AMD / Arista / Pure / VAST / WEKA 那么直接。

---

## 最终版本一句话

企业版的路线可以压缩成：

**主线选 AI Infrastructure，方向选 LLM Inference Systems，第一批主投 NVIDIA / AMD / Intel / Arista / Broadcom / Pure Storage / VAST / WEKA，第二批投 Cisco / HPE Juniper / Dell / Marvell / NetApp，第三批冲 Groq / Cerebras / Tenstorrent / CoreWeave / Nebius / Nscale。**

你不应该把自己训练成“普通大模型应用工程师”，而应该训练成：

**能做 C++ / Linux / RDMA / NVMe / GPU runtime / LLM Serving / 性能分析 / 分布式推理系统的人。**

这个定位在公司里比单纯业务应用岗更有壁垒，也更符合你现在的基础。

---

## 参考来源

[^vllm]: vLLM official site: “The high-throughput and memory-efficient inference and serving engine for LLMs.”
[^vllm-metrics]: vLLM documentation, Metrics and prefix cache metrics.
[^sglang-pd]: SGLang documentation, Prefill-Decode Disaggregation.
[^nvidia-dynamo]: NVIDIA Developer, Dynamo Inference Framework.
[^tensorrt-llm]: NVIDIA TensorRT-LLM documentation.
[^nvidia-nim]: NVIDIA NIM Microservices for Accelerated AI Inference.
[^rocm]: AMD ROCm documentation.
[^intel-gaudi]: Intel Gaudi AI Accelerator software overview.
[^intel-ai-tools]: Intel AI frameworks and tools documentation.
[^tenstorrent]: Tenstorrent newsroom, full-stack AI hardware/software/deployment positioning.
[^broadcom-thor]: Broadcom press release, Thor Ultra 800G AI Ethernet NIC.
[^broadcom-jericho]: Broadcom press release, Jericho4 distributed AI computing across Ethernet.
[^arista-ai]: Arista AI Networking Center.
[^cisco-ai-networking]: Cisco AI Networking in Data Centers.
[^marvell-ai]: Marvell, Accelerated Infrastructure for the AI Era.
[^pure-nvidia]: NVIDIA and Pure Storage AI-ready infrastructure / AIRI reference architecture.
[^vast]: VAST Data official site, AI Operating System positioning.
[^vast-reuters]: Reuters, VAST Data valued at $30 billion in 2026 funding round.
[^weka-gds]: WEKA, GPUDirect Storage explanation and AI storage materials.
[^dell-ai-factory]: Dell AI Factory with NVIDIA.
[^gds]: NVIDIA GPUDirect Storage official page.
[^hpe-juniper]: HPE press release, completion of Juniper Networks acquisition.
[^cxl]: CXL Consortium, About CXL.
[^bluefield]: NVIDIA BlueField Networking Platform.
[^intel-ipu]: Intel Infrastructure Processing Unit official page.
[^solidigm-ai]: Solidigm, Accelerating AI with High Performance Storage.
[^nvidia-university]: NVIDIA University Recruiting and Early-Talent Programs.
[^amd-students]: AMD Student Programs.
[^arista-university]: Arista University Recruiting.
