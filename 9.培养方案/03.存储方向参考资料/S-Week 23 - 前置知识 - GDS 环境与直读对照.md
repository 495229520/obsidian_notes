---
title: S-Week 23 - 前置知识 - GDS 环境与直读对照
date: 2026-07-12
tags:
  - 高性能存储/存储方向参考资料/计划
aliases:
  - 存储 Week 23 前置知识
  - GDS 前置知识
status: active
---

# S-Week 23 - 前置知识 - GDS 环境与直读对照

## 索引

- [[#0. 先建立直觉：数据到显存的最后一公里]]
- [[#1. 三条路径的逐跳账]]
- [[#2. cuFile API：五步骨架与两个纪律]]
- [[#3. 环境预检：gdscheck 与拓扑]]
- [[#4. 兼容模式：本周最大的假实验陷阱]]
- [[#5. 实验设计：矩阵、指标与预期形态]]
- [[#6. 租期纪律：4-6 小时怎么花]]
- [[#7. 常见错误]]
- [[#8. 学习检查清单]]
- [[#9. 关键要点总结]]
- [[#关联知识]]
- [[#参考]]

## 阅读说明

这篇是 [[S-Week 23 - GDS 环境与直读对照]] 的总前置知识：租机前必须读完 3-4 节和 6 节（租期内没有学习时间，只有执行时间），写程序前看 2 节，跑矩阵前对照 5 节。机制深挖版见 [[GPUDirect Storage 专题 - cuFile 与 bounce buffer]]。

---

> S1 到 S3 的所有实验，数据的终点都是主机内存；本周终点第一次变成显存。这也是整个培养方案"两条线交汇"的动手时刻：S-Week 2 的 O_DIRECT 对齐、S-Week 10 的 registered buffers、S-Week 20 的"注册 + DMA 直达"——三样旧知识拼出一个新能力。GPU 机时贵，本周的一切准备都为了让租期内每一分钟都在采数。

---

## 0. 先建立直觉：数据到显存的最后一公里

训练数据、模型权重、offload 的 KV cache——AI 负载的数据都在盘上，都要进显存。默认路径是"盘 → 主机内存 → 显存"两段接力；GDS 问的问题是：**中间那一站能不能不停？** PCIe 是一棵树，NVMe 和 GPU 都挂在上面——理论上 NVMe 的 DMA 引擎可以把数据直接写进 GPU 暴露在 PCIe 上的显存窗口（P2P），不必绕道主机内存。GDS = 让这件事在文件系统语义下可用的那套软件（nvidia-fs 内核模块 + libcufile 用户态库）。

> [!important] 第一性原理
> GDS 消掉的是**一跳搬运和一条被占用的总线**，不是引入更快的硬件。所以判断它值不值，永远看"被消掉的那一跳原本是不是瓶颈"——主机内存带宽富余、CPU 空闲、单盘小块时，它注定平淡；多盘聚合、大块吞吐、主机吃紧时，它才是主角。

## 1. 三条路径的逐跳账

| 路径 | 跳数 | 主机内存穿透 | CPU 参与 | 备注 |
|---|---|---|---|---|
| A：pageable | 盘→page cache→用户 buffer→（隐藏 pinned 中转）→显存 | 2-3 次 | 高（含隐藏拷贝） | 最差基线，任何教程代码的默认形态 |
| B：pinned + O_DIRECT | 盘→pinned buffer→显存 | 2 次（进 + 出） | 中（编排 + memcpy 提交） | **传统路径的最优形态，真正的对照组** |
| C：GDS | 盘→显存 | 0 次 | 低（只发命令） | P2P 直达 |

为什么 B 才是公平基线：拿 A 当对照会把"pageable 的隐藏拷贝"错记到 GDS 的功劳簿上。三组都测，但结论里的"GDS 收益"必须相对 B 说——这是本周实验设计的第一条纪律。

## 2. cuFile API：五步骨架与两个纪律

```cpp
cuFileDriverOpen();                                   // 1. 驱动会话
int fd = open(path, O_RDONLY | O_DIRECT);             // 2. O_DIRECT 是硬要求
CUfileDescr_t descr{}; CUfileHandle_t fh;
descr.handle.fd = fd; descr.type = CU_FILE_HANDLE_TYPE_OPAQUE_FD;
cuFileHandleRegister(&fh, &descr);                    // 3. fd 升级为 cuFile 句柄
cudaMalloc(&gpu_buf, size);
cuFileBufRegister(gpu_buf, size, 0);                  // 4. 注册显存（推荐）
ssize_t n = cuFileRead(fh, gpu_buf, size, file_off, 0); // 5. 直读
// 逆序清理：BufDeregister → HandleDeregister → close → DriverClose
```

两个纪律：

- **注册池化**：`cuFileBufRegister` 与 RDMA 的 MR、io_uring 的 registered buffer 一样是贵操作——buffer 复用、注册一次用整场，绝不进热路径。不注册也能跑（库内部代注册），但性能与可控性都差。
- **对齐三件套**：O_DIRECT 的 offset/长度对齐要求原样生效（S-Week 2 的 EINVAL 实验记忆犹新）；4 KiB 对齐起步，实验里的 io_size 全部取 4 KiB 的倍数。

正确性 gate（沿用 S-Week 5/10 纪律）：C 路径读进显存后 `cudaMemcpy` 拷回主机，与纯 `pread` 的结果逐字节/校验和对账——三条路径先全部对账通过，再谈性能。

## 3. 环境预检：gdscheck 与拓扑

租到机器的前 30 分钟按清单过：

```bash
nvidia-smi                                   # GPU 型号、驱动版本
nvidia-smi topo -m                           # GPU 与 NVMe 的 PCIe 距离（PIX/PXB/PHB/NODE/SYS）
lsmod | grep nvidia_fs                       # nvidia-fs 模块
/usr/local/cuda/gds/tools/gdscheck -p        # 平台自检：逐项 Supported / Unsupported
nvme list && mount | grep -E "ext4|xfs"      # 本地盘 + 文件系统
```

- 拓扑读法：GPU 与 NVMe 的连接关系越近（同一 PCIe switch 最好），P2P 效率越高；跨 NUMA/根复合体（SYS）时 P2P 可能退化甚至不可用——`topo -m` 的输出直接决定这台机器值不值得继续。
- gdscheck 的输出全文进 `env.md`：它列出的每一项（文件系统支持、驱动、P2P 能力）就是这台机器的"可信度证书"。

## 4. 兼容模式：本周最大的假实验陷阱

cuFile 的设计哲学是"尽量不失败"：条件不满足（文件系统不支持、拓扑不行、模块缺失）时**静默切换 compat mode**——内部走 bounce buffer，API 全部成功返回。后果：你以为在测 C 路径，实际测的是一个更慢的 B 路径。

防御三件套：

1. `gdscheck -p` 预检先行（上一节）；
2. 每轮实验后查 `gds_stats`（或 cufile 日志）的 P2P/直读计数是否增长——**写进脚本做断言**，不靠人眼；
3. `cufile.json`（libcufile 配置文件）里可以把 compat mode 显式关掉——让"回落"变成"报错"，假实验直接失败暴露。

这个陷阱值得写进报告的方法论一节：它是"每个数字要能回答自己怎么来的"纪律在 GPU 实验里的具体形态。

## 5. 实验设计：矩阵、指标与预期形态

- 矩阵：块大小 4K / 64K / 1M / 16M × worker 1 / 4 / 16 × 路径 A/B/C，每组 3 次；顺序读为主（GDS 的主场），留一组 4K 随机做边界展示。
- 指标三件套：吞吐（GB/s）、CPU（`pidstat -t` 盯搬运线程，B 组的 memcpy 提交线程别漏）、gds_stats 计数断言；主机内存带宽有工具（如 `pcm-memory`）就测，没有就用"B 组两倍穿透"推算并声明。
- 预期形态（先写预注册，再拿数据验证）：
  - 4K 小块：三组都被盘的 IOPS/延迟主导，差距小；
  - 1M-16M 大块：B 组逼近"主机内存带宽 ÷ 2"或 CPU 编排上限，C 组跟着盘的顺序读带宽走；
  - worker 拉高：B 组先饱和（CPU/内存墙），C 组随盘扩展——**拉开差距的正是"被消掉的那一跳"成为瓶颈的时刻**。
- 单盘机器测不出 C 组的招牌优势（盘先饱和）——如实报告"单盘下 B≈C，差异在 CPU 占用"，并声明多盘聚合是外推场景。诚实的平淡结论 + 正确的机制解释，好过夸大的曲线。

## 6. 租期纪律：4-6 小时怎么花

- 租前（本地全部完成）：三路径程序在无 GPU 机器上编译通过（CUDA 部分条件编译或 stub）、脚本参数化、矩阵清单与预期形态写好、gdscheck/gds_stats 断言写进脚本。
- 租期时间表：0.5h 预检进 env.md → 0.5h 三路径正确性 gate → 3h 矩阵采数（脚本自动跑，人盯断言）→ 0.5h 数据回传与抽查 → 剩余缓冲。
- 数据每完成一组立即回传对象存储/本地——**租期结束机器即蒸发**，别把唯一副本留在实例盘上。

## 7. 常见错误

- **拿 pageable 当对照组**：GDS 收益被高估一截——公平基线是 pinned + O_DIRECT。
- **全程 compat mode 不自知**：三件套防御一样没做，整场数据作废。
- **buffer/句柄每次 I/O 现注册**：注册开销进了热路径，C 组被自己拖慢。
- **offset/io_size 不对齐**：O_DIRECT 报 EINVAL 或性能异常——S-Week 2 的老坑新犯。
- **忽略拓扑就开跑**：GPU 与盘跨根复合体，P2P 天生残废，测出的"GDS 不行"是机器不行。
- **租期内现写脚本**：GPU 机时按小时计费，租期内写代码是最贵的写代码方式。
- **数据不回传**：实例释放后数据蒸发，一周白干。

## 8. 学习检查清单

- [ ] 三条路径的逐跳账能背着画出来，说清为什么 B 是公平基线。
- [ ] cuFile 五步骨架能默写，注册池化与对齐两条纪律清楚。
- [ ] 预检清单五条命令与各自看什么，能脱稿。
- [ ] compat mode 的成因与防御三件套齐备且已写进脚本。
- [ ] 矩阵、指标、预期形态在租机前全部成文。
- [ ] 租期时间表与数据回传方案就绪。

## 9. 关键要点总结

- GDS 消掉的是一跳搬运和一条总线：判断收益永远看"那一跳原本是不是瓶颈"。
- 公平对照 = pinned + O_DIRECT；pageable 只做垫底展示。
- compat mode 是静默的假实验制造机：预检 + 计数断言 + 显式关闭，三件套防御。
- 注册池化、对齐三件套——S1/S3 的老纪律在 GPU 上原样生效。
- GPU 机时贵：租前万事俱备，租期只跑不写。

## 关联知识

- [[S-Week 23 - GDS 环境与直读对照]]（本篇服务的周计划）
- [[GPUDirect Storage 专题 - cuFile 与 bounce buffer]]（机制深挖与口述）
- [[S-Week 2 - 前置知识 - O_DIRECT + 持久化语义]]（对齐纪律来源）
- [[S-Week 20 - 前置知识 - RDMA verbs 入门]]（注册 + DMA 直达同源）
- [[S-Week 22 - 前置知识 - nvme-of-lab 收口与阶段 2 复盘]]（租机采购学）

## 参考

- NVIDIA GPUDirect Storage 文档：Overview / cuFile API Reference / Best Practices / Troubleshooting（compat mode 一节）
- gdsio / gdscheck / gds_stats 工具文档（随 CUDA GDS 包分发）
- `man 2 open`（O_DIRECT 段，对齐要求）
