---
title: S-Week 3 - 前置知识 - fio 对照与 Benchmark Matrix
date: 2026-07-12
tags:
  - 高性能存储/存储方向参考资料/计划
aliases:
  - 存储 Week 3 前置知识
  - fio 前置知识
status: active
---

# S-Week 3 - 前置知识 - fio 对照与 Benchmark Matrix

## 索引

- [[#0. 先建立直觉：为什么需要一个工业级测量工具]]
- [[#1. fio 的执行模型：job、引擎、并发]]
- [[#2. job file 参数逐条解释]]
- [[#3. 输出怎么读：slat / clat / lat 与分位数]]
- [[#4. iodepth 与 Little's law：曲线的形状是注定的]]
- [[#5. 与自写程序对账：差异从哪来]]
- [[#6. 矩阵实验的工程化：JSON 输出与脚本]]
- [[#7. iostat 同窗口观察什么]]
- [[#8. 常见错误]]
- [[#9. 学习检查清单]]
- [[#10. 关键要点总结]]
- [[#关联知识]]
- [[#参考]]

## 阅读说明

这篇是 [[S-Week 3 - fio 对照与 Benchmark Matrix]] 的总前置知识：写第一个 job file 前通读 0-2 节，读输出前看 3-4 节，做对账和矩阵前看 5-7 节。方法论深挖版见 [[存储性能分析专题 - fio 与 benchmark matrix]]。

---

> 前两周你自己写了测量程序，这周引入行业标准工具 fio 做两件事：**校准**（证明自写工具可信）和**扩展**（36 组矩阵靠手写程序要写到猴年马月）。fio 在存储线的地位 = Nsight 在推理线的地位：主力测量仪器，必须精通到"每个参数知道它在改什么"。

---

## 0. 先建立直觉：为什么需要一个工业级测量工具

自写程序的价值是"我知道每一行在干什么"；它的短板是覆盖面——异步引擎、多线程、写混合、限速、稳态检测……每个都自己实现不现实。fio 用一个 job file 描述负载，几十个引擎几百个参数都是现成的。

但工具越强大越容易"跑出一个数就信了"。本周的核心纪律：**每个 fio 数字都要能回答"它测的是什么路径、什么状态"**——direct 开没开、文件预分配没有、iodepth 真的生效没有。上周的知识全部用得上。

## 1. fio 的执行模型：job、引擎、并发

一个 fio 运行 = 若干 **job**（并发的 worker），每个 job 用指定 **ioengine** 发 I/O：

| ioengine | 类型 | 对应你写过的 |
|---|---|---|
| psync | 同步 pread/pwrite | S-Week 1/2 的自写程序 |
| libaio | Linux AIO（需 direct） | 老式数据库路径 |
| io_uring | 异步双环 | S-Week 5 将要写的 |

并发的两个来源，语义完全不同：

- **iodepth**：单个 job 维持的在途请求数。**只对异步引擎有效**——psync 下设成 128 也恒为 1，这是新手第一坑。
- **numjobs**：克隆 N 个相同 job（各自独立发 I/O）。总在途 ≈ iodepth × numjobs。

测设备队列行为用 iodepth（单提交者，变量干净）；模拟多客户端用 numjobs（混入调度与 CPU 因素）。本周矩阵只动 iodepth。

## 2. job file 参数逐条解释

```ini
[global]
directory=/data/iolab   ; 测试文件目录（挂在本地 NVMe 上，上周已验证）
filename=fiotest        ; 固定文件名：所有 job 复用同一文件
size=8g                 ; 文件大小：显著大于内存一半（同上周纪律）
direct=1                ; O_DIRECT，绕过 page cache——测设备本身
ioengine=io_uring
runtime=60              ; 每组跑 60 秒
time_based=1            ; 时间驱动（否则读完 size 就停）
group_reporting=1       ; 多 job 时合并报告
randrepeat=1            ; 随机序列可复现（默认即 1，显式写出表达意图）

[randread-4k-qd1]
rw=randread             ; randread/read/randwrite/write
bs=4k                   ; 块大小
iodepth=1
```

几个容易忽略的行为：

- **首跑会先 layout 文件**：fio 发现文件不存在或不够大会先写满它——这一跑的数据不能要。先单独跑一次建文件，或用上周的 8 GiB 测试文件。
- `runtime + time_based`：保证每组时长一致，分位数才有可比性。
- `ramp_time=5` 可选：丢掉前 5 秒预热期数据（队列未满、缓存未稳）。

## 3. 输出怎么读：slat / clat / lat 与分位数

```text
lat = slat + clat
slat（submission latency）：发起 → 提交完成。异步引擎才有意义；
                           同步引擎提交即完成，slat ≈ 0。
clat（completion latency）：提交完成 → I/O 完成。p99 看这里。
```

重点读三段：

1. `IOPS / BW` 行：吞吐主数字。
2. `clat percentiles`：p50 / p95 / p99 / p99.9——和上周自算的分位数同口径。
3. 结尾 `disk stats`：确认 I/O 真落在目标盘上（util、ios）——防止测到别的路径。

口径对照（三方对账时用）：fio 的 clat ≈ iostat 的 await（都含内核排队）；biolatency 默认只测下发到完成（D2C）——阶段 1 会正式展开，本周先记住"对账先对口径"。

## 4. iodepth 与 Little's law：曲线的形状是注定的

排队论的基本关系锁死了三个量：

$$
QD \approx IOPS \times Latency
$$

推论：QD 扫描曲线必然分三段——

1. **线性段**（QD 小）：设备没喂饱，IOPS 随 QD 线性涨，延迟基本不动。
2. **饱和点**：设备并行度到顶，IOPS 封顶。
3. **排队段**：继续加 QD，IOPS 不涨，**增量全部变成排队延迟**，p99 按比例恶化。

这就是"iodepth 提高吞吐但恶化 p99"的完整答案，也是给定 p99 SLO 反推 QD 上限的依据。本周矩阵的 1 / 8 / 32 三档就是想让你亲眼看到三段形态。

口算换算（必须条件反射）：

$$
Bandwidth = IOPS \times BlockSize
$$

4K 随机 200k IOPS = 800 MB/s；1M 顺序 3 GB/s 只需 3000 IOPS。**小块负载看 IOPS，大块负载看带宽**——拿错指标是报告里最常见的笑话。

## 5. 与自写程序对账：差异从哪来

对账配置必须逐项对齐：`ioengine=psync, rw=randread, bs=4k, direct=1, iodepth=1`，同一文件、同一量级的采样数。p50/p99 同量级即通过；差异超 20% 时按这张单子排查：

| 差异来源 | 说明 |
|---|---|
| 计时口径 | 自写程序计整个 pread；fio 也是——先确认没把打开/关闭算进去 |
| offset 分布 | 你固定 seed 全文件均匀；fio randrepeat 的序列不同，碰上文件碎片分布差异就显形 |
| 对齐 | 你按 4096 对齐；fio 默认也按 bs 对齐——确认 bs 与你的块大小一致 |
| 统计方式 | 分位数算法（最近秩 vs 插值）在样本少时有出入，加大样本 |

对账的意义写进报告：**自写工具证明理解，工业工具证明可信，两者互相校准**。

## 6. 矩阵实验的工程化：JSON 输出与脚本

36 组 × 3 次重复 = 108 跑，必须脚本化：

```bash
fio --output-format=json --output=results/matrix/randread_4k_qd8_r1.json jobs/randread_4k_qd8.fio
```

- job file 由脚本从模板生成（Agent 写生成器，你审模板）——命名规范 `<rw>_<bs>_qd<depth>`。
- JSON 里取数的路径：`jobs[0].read.iops`、`jobs[0].read.clat_ns.percentile["99.000000"]`——解析脚本一次写对，全矩阵通用。
- 原始 JSON 全部保留不改；汇总表由脚本重算生成。
- 跑批期间机器不做别的事（上周纪律），组间 sleep 几秒让设备喘气。

## 7. iostat 同窗口观察什么

每组实验并行开 `iostat -x 1`，三个对应关系是本周的观测重点：

| iostat 列 | 应与 fio 对应 | 对不上时查什么 |
|---|---|---|
| r/s | ≈ fio IOPS | direct 没开（读被 cache 挡了）、别的进程混流量 |
| aqu-sz | ≈ iodepth | 同步引擎 iodepth 没生效、饱和后排队溢出 |
| r_await | ≈ fio clat 均值 | 口径都含排队，应吻合；差太多查采样窗口 |

顺带记录一个现象不深究：iodepth=32 时 %util 早到 100% 但 IOPS 仍在涨——**%util 在多队列 SSD 上不等于饱和**。阶段 1（[[S-Week 8 - 块层与 blktrace]]）会给出机制解释，本周报告里如实记录即可。

## 8. 常见错误

- **psync 引擎配 iodepth=32**：以为在测高并发，实际恒为 QD1——矩阵里最隐蔽的废数据来源。
- **首跑 layout 的数据进了结果**：第一次跑包含文件分配，延迟畸高。
- **direct=0 却以为在测盘**：热 cache 下测出百万 IOPS 的"神盘"。
- **不同组用不同 size/文件**：文件碎片与 LBA 范围不同，组间不可比。
- **只看均值行不看 percentiles**：上周的教训换个工具再犯一遍。
- **JSON 解析取错字段**：clat_ns 单位是纳秒，除错单位差三个量级。
- **跑批时开着别的下载/编译**：iostat 里混入无关流量，对账全乱。
- **randwrite 矩阵直接跑在宝贵数据的盘上**：写负载只指向测试文件，绝不碰裸设备——纪律与上周相同。

## 9. 学习检查清单

- [ ] 能说出 iodepth 与 numjobs 的语义差别，以及 psync 下 iodepth 失效的原因。
- [ ] 能解释 job file 里每个 global 参数在控制什么。
- [ ] 能说出 slat / clat / lat 的构成与同步引擎下的形态。
- [ ] 能用 Little's law 解释 QD 曲线三段形态，并反推 QD 上限。
- [ ] 能列出自写程序与 fio 对账的四类差异来源。
- [ ] 知道首跑 layout、randrepeat、ramp_time 各自的坑与用法。
- [ ] 能说出 iostat 三列与 fio 参数/输出的对应关系。

## 10. 关键要点总结

- fio 的角色 = 存储线的 Nsight：校准自写工具 + 扩展实验覆盖面。
- 并发两来源：iodepth（单提交者在途数，异步引擎限定）、numjobs（提交者个数）；本周只动前者。
- 曲线形状由 Little's law 注定：线性段 → 饱和点 → 排队段；p99 恶化是排队的数学必然。
- 对账先对口径：fio clat ≈ iostat await（含排队），差异超 20% 必须解释。
- 工程化纪律：JSON 落盘、脚本重算、原始数据不改、跑批环境干净。

## 关联知识

- [[S-Week 3 - fio 对照与 Benchmark Matrix]]（本篇服务的周计划）
- [[存储性能分析专题 - fio 与 benchmark matrix]]（方法论深挖版）
- [[S-Week 2 - 前置知识 - O_DIRECT + 持久化语义]]（direct=1 的语义基础）
- [[S-Week 1 - 前置知识 - 环境搭建 + Page Cache 基线]]（iostat 入门与实验纪律）
- [[块层观测专题 - iostat blktrace eBPF]]（%util 谜题的阶段 1 解答）
- [[Week 5 - Serving Benchmark Harness]]（分位数与 warmup 方法论同源）

## 参考

- fio 官方文档与 HOWTO：[GitHub](https://github.com/axboe/fio)（doc 目录）
- `man fio`（参数权威解释）
- Systems Performance（Brendan Gregg）ch 9 Disks（workload characterization 小节）
- OSTEP 第 44 章 Flash-based SSDs：[网站](https://pages.cs.wisc.edu/~remzi/OSTEP/)
