---
title: S-Week 9 - eBPF 观测
date: 2026-07-11
tags:
  - 高性能存储
  - 存储方向阶段计划
  - 计划
status: active
---

# S-Week 9 - eBPF 观测

> [!goal] 本周目标
> 用 bcc 工具集（biolatency / biosnoop / fileslower / ext4slower）建立"常驻低开销"的观测能力，然后自己注入干扰、制造一次真实的 p99 毛刺，并把定位全过程写成 runbook——这份"毛刺定位记录"是总纲阶段 1 验收的硬性产出，也是面试里最值钱的故事。

## 学习目标

1. **eBPF 为什么开销低？** 程序在内核内事件点（kprobe/tracepoint）执行，聚合（直方图/计数）在内核态完成，只把汇总结果送到用户态——对比 blktrace 全量导出每个事件。
2. **biolatency 和 blktrace 各自什么时候用？** biolatency 常驻看分布形态（双峰=两类来源），blktrace 短窗口取证看单笔事件链。
3. **怎么区分"文件系统慢"和"设备慢"？** fileslower / ext4slower 在 VFS/文件系统层，biolatency 在块层——上面慢下面不慢，问题在中间（锁、日志、writeback）。
4. **毛刺的常见来源有哪些？** writeback 突发、readahead 误伤、带宽挤占、CPU 饱和、cgroup 限速、SSD 内部 GC。
5. **一份可信的毛刺定位记录长什么样？** 现象 → 假设清单 → 逐层工具 → 证据 → 定位 → 缓解措施，每步有数据。

## 1. 工具跑通（Day 1）

```bash
sudo apt install -y bpfcc-tools linux-headers-$(uname -r)
# Ubuntu 包名带后缀：biolatency-bpfcc / biosnoop-bpfcc / ext4slower-bpfcc
sudo biolatency-bpfcc 5 3        # 每 5 秒一张直方图，共 3 张
sudo biosnoop-bpfcc | head -30   # 逐笔：时间、进程、盘、扇区、字节、延迟
sudo ext4slower-bpfcc 1          # 超过 1ms 的 ext4 读/写/open/fsync
```

对着 fio 稳定负载跑一遍，确认三件事：biolatency 直方图峰值与 fio clat 对得上；biosnoop 能看到 fio 进程名；ext4slower 在 O_DIRECT 负载下基本安静。

> 注意 biolatency 默认统计"下发到完成"（≈ D2C），加 `-Q` 才包含内核排队时间（≈ Q2C）。对账时要和 iostat await 用同口径。

## 2. 毛刺注入与定位（Day 2-3）

前台负载：io_uring 4K 随机读，QD4，持续记录逐笔延迟（复用 S-Week 5 程序），先测出干净基线的 p99。

依次注入两种干扰（每次只开一种）：

1. **writeback 突发**：另一进程大量 buffered 写 + 周期性 `sync`，观察前台 p99 何时起飞。
2. **带宽挤占**：fio 顺序读大块（1M、QD8）抢设备带宽。

对每种干扰回答：

- 前台 p99 从多少涨到多少？毛刺是持续的还是脉冲的？
- biosnoop 抓到的毛刺时刻，盘上还有谁在发 I/O？请求多大？
- biolatency 直方图是整体右移还是长出第二个峰？两种形态分别说明什么？

## 3. blktrace 交叉验证 + runbook（Day 4-5）

- 挑一次毛刺，用 blktrace 短窗口取证，btt 分解毛刺样本的 Q2D / D2C：排队变长还是设备变慢？与 [[S-Week 8 - 块层与 blktrace]] 的方法闭环。
- 尝试一种缓解并量化效果（三选一）：调低 `vm.dirty_ratio` / `vm.dirty_background_ratio`、干扰进程 `ionice`、cgroup v2 `io.max` 限速。
- 写 `docs/p99_hunt.md`：完整 runbook。格式沿用推理版 observability 的事故记录风格——别人照着能复现出同样的毛刺和同样的定位路径。

## 4. 推理保温（约 25%）

- 维护态第 1 周：serving benchmark harness 完整复跑一遍，确认环境漂移后仍可复现；KV cache 显存占用口算复习（每 token 的 KV 字节数公式）。

## 5. 面试保底（约 15%）

> 阶段 1 网络补强第 2 讲。

- 算法（5-8 题）：图论。参考 [[5.2.24 图论基础]]、[[5.2.26 图与图算法]]（BFS/DFS/岛屿/拓扑），配 [[CodeTop 高频题 Top300]] 图类题。
- 八股（1 章）：TCP 调优与拥塞控制。过 [[8.13 TCP内核队列与参数调优]]（半连接/全连接队列、backlog）、[[8.10 拥塞控制]]（慢启动/拥塞避免/快重传）、[[8.8 流控制]]、[[10.3 Nagle算法]]。验收：能讲清"listen backlog 满了会发生什么"和"拥塞窗口和接收窗口谁说了算"。
- 项目问答：10 个 Q&A（本周素材：毛刺定位链路、观测工具选型、writeback 干扰）。

## 6. 本周产出文件

| 文件 | 内容 | 验收方式 |
|---|---|---|
| `results/bpf/*` | biolatency 直方图、biosnoop 抓包片段 | 标注负载与干扰条件 |
| `scripts/inject_*.sh` | 两种干扰注入脚本 | 可复跑、有安全边界注释 |
| `docs/p99_hunt.md` | 毛刺定位 runbook | 新读者能照着复现定位过程 |

## 7. 验收标准

- [ ] 四个 bcc 工具跑通，biolatency 与 fio clat 对账一致。
- [ ] 两种干扰各完成一轮"注入 → 观测 → 定位"，肇事者有 biosnoop 证据。
- [ ] 一次毛刺完成 blktrace 交叉验证，Q2D/D2C 结论明确。
- [ ] 至少一种缓解措施实施并量化了效果。
- [ ] `p99_hunt.md` 完成，能 5 分钟脱稿讲完整定位故事。
- [ ] 推理保温和面试保底完成本周额度。

## 面试问题

- 线上存储 p99 突然涨了 10 倍，你的排查顺序是什么？
- biolatency 直方图出现双峰说明什么？
- 怎么证明毛刺是别的进程干扰而不是盘本身变慢？
- eBPF 观测的开销为什么比 blktrace 低？代价是丢了什么信息？
- dirty_ratio 和 dirty_background_ratio 的区别？调它们在换什么？

## 关联知识

- [[S-Week 8 - 块层与 blktrace]]
- [[S-Week 10 - io_uring 深入]]
- [[S-Week 9 - 前置知识 - eBPF 观测]]
- [[块层观测专题 - iostat blktrace eBPF]]
- [[Week 6 - Observability + Metrics]]（runbook 方法论同源）
- bcc（iovisor/bcc）tools 文档
