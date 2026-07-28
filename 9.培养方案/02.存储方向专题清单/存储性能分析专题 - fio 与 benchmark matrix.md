---
title: 存储性能分析专题 - fio 与 benchmark matrix
date: 2026-07-12
tags:
  - 高性能存储
  - 存储方向专题清单
roadmap_week: 阶段 0-1（S-Week 3 主线；之后所有 benchmark 沿用）
sort_order: "05.00"
status: active
---

# 存储性能分析专题 - fio 与 benchmark matrix

> [!info] 所属路线
> - 培养方案阶段：阶段 0（S-Week 3 fio matrix）；此后每个项目的 benchmark（io_uring 消融、mini-kv-engine、NVMe-oF 延迟分解）都沿用本专题的设计纪律
> - 排序：05.00
> - 用途：把"会跑 fio"升级成"会设计公平可复现的存储 benchmark"。工具是 fio，资产是方法论。

> [!goal] 目标
> 讲清三件事：fio 的参数模型（iodepth / numjobs / ioengine 到底控制什么）、延迟三段（slat / clat / lat）的口径、一张可信 benchmark matrix 的设计与解读。支撑"你怎么证明你的 benchmark 可信"这道必答题。

---

## 1. fio 参数模型：并发从哪来

```ini
[global]
directory=/data/iolab
size=8g
direct=1            ; 绕过 page cache，测设备本身
ioengine=io_uring   ; psync（同步）/ libaio / io_uring（异步）
runtime=60
time_based=1
group_reporting=1

[randread-4k-qd8]
rw=randread
bs=4k
iodepth=8
```

最常被问混的三个参数：

| 参数 | 控制什么 | 陷阱 |
|---|---|---|
| ioengine | 发 I/O 的方式 | **psync 等同步引擎下 iodepth 不生效**，恒为 1 |
| iodepth | 单个 job 的在途请求数 | 只有异步引擎才真堆得起来 |
| numjobs | 克隆几个并发 worker | 总在途 ≈ iodepth × numjobs；多 job 报告要 group_reporting |

iodepth 与 numjobs 的语义差别：iodepth 是"一个提交者维持 N 个在途"（单线程异步），numjobs 是"N 个提交者各自为战"（多线程/多进程）——前者考察设备队列，后者还混入了 CPU 与调度的因素。测设备用 iodepth，模拟多租户用 numjobs。

## 2. 延迟三段：slat / clat / lat

```text
lat（总延迟） = slat（提交延迟） + clat（完成延迟）
```

- **slat**：从发起到提交进内核完成——异步引擎才有意义；同步引擎提交即完成，slat ≈ 0。
- **clat**：从提交完成到 I/O 完成——p99 看的就是它（`clat percentiles` 段）。
- 与其他工具对口径：fio 的 clat ≈ 块层以下全程 ≈ iostat 的 await 口径（含排队）；biolatency 默认只测 D2C——三方对账前先统一口径（深入见 [[块层观测专题 - iostat blktrace eBPF]]）。

读 fio 输出的最小三段：`IOPS/BW` 行、`clat percentiles`、末尾的 `disk stats`（确认 I/O 真落到了目标盘）。

## 3. matrix 设计：维度怎么选、什么必须固定

S-Week 3 的 36 组矩阵是模板：

| 维度 | 取值 | 为什么是这几个 |
|---|---|---|
| rw | randread / read / randwrite / write | 四象限覆盖 |
| bs | 4k / 64k / 1m | IOPS 型 → 过渡 → 带宽型 |
| iodepth | 1 / 8 / 32 | 无排队 → 甜点区 → 饱和区 |

固定项与纪律：

- `direct=1` 固定：不固定则测的是 page cache 不是盘。
- 同一测试文件、预先 layout（首跑 fio 会先分配文件，这一跑的数据丢弃）。
- 每组 3 次重复；随机负载固定 seed（`randrepeat=1` 默认即可复现）。
- 结果用 `--output-format=json` 落盘，解析交给脚本，原始 JSON 不改。
- 写负载只指向测试文件，绝不碰裸设备。

## 4. QD 曲线怎么解读：Little's law 直觉

并发、吞吐、延迟三者锁死：

$$
QD \approx IOPS \times Latency
$$

于是 QD 扫描曲线必然长这样：低 QD 段 IOPS 随 QD 近似线性涨（延迟平）；到设备并行度上限后 IOPS 封顶；**继续加 QD，多出来的请求全在排队，延迟按比例涨**——这就是"iodepth 提高吞吐但恶化 p99"的排队论答案，也是"给定 p99 SLO 反推 QD 上限"的依据：

$$
QD_{max} \approx IOPS_{sat} \times p99_{target}
$$

配套口算（面试高频）：

$$
Bandwidth = IOPS \times BlockSize
$$

例如 4K 随机读 200k IOPS 即 800 MB/s；反过来 1M 顺序读 3 GB/s 只需要 3000 IOPS——**小块看 IOPS，大块看带宽**，两个指标在两端各自失真。

## 5. 公平可复现 benchmark 检查单

面试问"你怎么保证 benchmark 可信"，按这张单子答：

```text
1. 测什么说清楚：设备？文件系统？还是 page cache？（direct 与预热策略随之定）
2. 状态受控：冷热 cache 主动制造；写测试考虑 SSD 稳态 vs 空盘（阶段 2 深化）
3. 工具互证：自写程序与 fio 同参数对账，差异 < 20% 或有解释
4. 重复与分位数：≥3 次，报 p50/p95/p99 不报单均值
5. 环境快照：env.md（内核/盘型号/文件系统/调度器/工具版本）
6. 原始数据只增不改，结论可从 JSON 重算
7. 观测旁证：iostat 同窗口记录，aqu-sz 与 iodepth 对得上
8. 边界声明：云盘/虚拟化/单盘等局限写在报告里
```

第 3 条是双向的：fio 校准自写工具的可信度，自写工具验证你读懂了 fio 在干什么——S-Week 3 的对账实验就是这个意义。

## 6. 面试口述模板

```text
我设计存储 benchmark 先定"测什么"：测设备就 direct=1 加预分配文件，
测缓存就控制冷热状态。矩阵按 rw、块大小、iodepth 三维展开，小块看
IOPS 大块看带宽，QD 从 1 扫到饱和。解读靠 Little's law：并发约等
于吞吐乘延迟，IOPS 封顶后继续加深队列，增量全变成排队延迟，所以
p99 恶化——反过来给定 p99 SLO 就能反推 QD 上限。可信度靠三件事：
自写工具和 fio 同参数对账、每组三次重复报分位数、iostat 同窗口做
旁证。fio 的延迟我看 clat 分位段，它和 iostat await 是同口径、和
biolatency 默认口径差一个排队段，对账时先统一。
```

追问预案：

- "iodepth 和 numjobs 的区别？" → 单提交者的在途数 vs 提交者个数；同步引擎 iodepth 无效是经典陷阱。
- "%util 100% 说明饱和吗？" → 并行设备上不说明，看 IOPS 增量和 await 拐点（详见 [[块层观测专题 - iostat blktrace eBPF]]）。
- "随机写先快后慢？" → SSD 空盘写到稳态的转变（FTL/GC），严谨的写测试要先预写到稳态——阶段 2 展开。
- "ramp_time 有什么用？" → 丢弃起步阶段（cache 未稳、预读窗口未建立）的数据，只统计稳态窗口。

## 关联知识

- [[S-Week 3 - fio 对照与 Benchmark Matrix]]（本专题服务的周计划）
- [[S-Week 3 - 前置知识 - fio 对照与 Benchmark Matrix]]（入门版与参数逐条解释）
- [[块层观测专题 - iostat blktrace eBPF]]（观测侧姊妹篇，口径对账）
- [[S-Week 5 - io_uring 异步 IO]]、[[S-Week 10 - io_uring 深入]]（QD 扫描与消融沿用本纪律）
- [[Week 5 - Serving Benchmark Harness]]（推理侧同源方法论：warmup、分位数、可复现）
- [[00.存储方向专题清单索引]]
