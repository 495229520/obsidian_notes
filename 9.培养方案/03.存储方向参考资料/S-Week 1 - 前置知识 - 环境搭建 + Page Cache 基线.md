---
title: S-Week 1 - 前置知识 - 环境搭建 + Page Cache 基线
date: 2026-07-10
tags:
  - 高性能存储
  - 存储方向参考资料
  - 计划
aliases:
  - 存储 Week 1 前置知识
  - Page Cache 基线前置知识
status: active
---

# S-Week 1 - 前置知识 - 环境搭建 + Page Cache 基线

## 索引

- [[#0. 先建立直觉：为什么第二次读同一个文件快这么多]]
- [[#1. 一次 read 的完整路径（总览）]]
- [[#2. Page cache 是什么]]
  - [[#2.1 free -h 里的 buff/cache]]
  - [[#2.2 dirty page 与 writeback（本周只需概念）]]
- [[#3. 冷/热 cache 与 drop_caches]]
  - [[#3.1 为什么 drop_caches 前必须 sync]]
  - [[#3.2 实验纪律]]
- [[#4. readahead：为什么冷顺序读比冷随机读快]]
- [[#5. pread 与延迟测量程序要点]]
  - [[#5.1 为什么用 pread 而不是 read]]
  - [[#5.2 计时与随机偏移]]
- [[#6. 延迟数据怎么记录才可信]]
- [[#7. 观测工具：iostat 与 free]]
- [[#8. 环境验证命令逐条解释]]
- [[#9. 常见错误]]
- [[#10. 学习检查清单]]
- [[#11. 关键要点总结]]
- [[#关联知识]]
- [[#参考]]

## 阅读说明

这篇是 [[S-Week 1 - 环境搭建 + Page Cache 基线]] 的总前置知识：动手前先通读 0-4 节建立"read 路径 + page cache"的整体图景，写测量程序前看 5-6 节，跑实验时对照 7-8 节。某一层想挖得特别细时，转到专题：[[S-Week 1 - Linux I O 路径专题 - VFS 到 NVMe 全路径]]、[[S-Week 1 - Page Cache 与 readahead 专题]]。

---

> 本周实验只做一件事：用自己写的程序测出"page cache 命中"和"不命中"的读延迟差几个数量级，并用 iostat 证明差距来自"是否真的碰了设备"。这篇笔记提供做这件事所需的全部背景。

---

## 0. 先建立直觉：为什么第二次读同一个文件快这么多

在任何 Linux 机器上做一次最朴素的观察：

```bash
# 第一次读（冷）：明显耗时
time cat bigfile > /dev/null
# 第二次读（热）：几乎瞬间完成
time cat bigfile > /dev/null
```

第二次快的原因是：第一次读的时候，内核已经把文件内容以 page（通常 4 KiB）为单位缓存在了内存里——这块缓存就是 **page cache**。第二次读根本没有到 SSD，只是从内存里拷贝。

```text
第一次读：用户程序 → 内核 → NVMe 设备 → 内存 → 用户程序   （数十微秒/次起步）
第二次读：用户程序 → 内核 → 内存 → 用户程序               （约一微秒/次）
```

![[图片/9.培养方案/03.存储方向参考资料/9_3_4_1_1.svg|880]]

> [!important] 第一性原理
> 存储性能分析的第一个问题永远是：**这次 I/O 到底有没有碰设备？** 同一行代码，命中 page cache 和不命中，延迟可以差 1-2 个数量级。不先控制这个变量，任何存储 benchmark 都不可信。本周所有实验设计（预热、drop_caches、iostat 佐证）都是围绕这一点。

---

## 1. 一次 read 的完整路径（总览）

用户程序调用 `read` / `pread` 后，请求自上而下穿过这些层：

| 层 | 做什么 | 本周关注点 |
|---|---|---|
| 用户态 → 系统调用 | 陷入内核，参数检查 | 每次 syscall 有固定开销（约百纳秒级） |
| VFS | 统一文件模型，分发到具体文件系统 | 只需知道它是"接口层" |
| 文件系统（ext4/xfs） | 文件偏移 → 磁盘块号的映射 | `df -T` 记录你的文件系统类型 |
| **page cache** | 检查目标 page 是否已缓存 | **本周主角：命中则到此为止** |
| 块层（blk-mq） | 把未命中的读组装成 bio 请求、排队、调度 | `iostat` 观察的就是这一层 |
| NVMe 驱动 + 设备 | 命令写入队列，SSD 执行，中断返回 | 冷读延迟的大头 |

判断路径走到哪一层的方法：**热读时 `iostat -x 1` 里块设备完全没有流量（r/s 为 0），冷读时 r/s 大量出现**——这是 page cache 命中与否最直接的证据。

每层往下延迟量级（标准直觉，精确数字以你自己的实测为准）：

```text
page cache 命中的 4K 读：       ~1 µs（syscall + 内存拷贝）
NVMe 冷读 4K：                 ~20-100 µs
（对照）HDD 随机读：            ~ms 级，这就是为什么 OSTEP 先讲 HDD 寻道模型
```

逐层细节（bio、blk-mq、NVMe queue pair）本周不需要，深入见 [[S-Week 1 - Linux I O 路径专题 - VFS 到 NVMe 全路径]]。

---

## 2. Page cache 是什么

page cache 是内核用**空闲内存**做的文件内容缓存：

- 以 page（4 KiB）为单位，缓存的是"文件的某一段内容"。
- 它是全局的、按需增长的：内存空着也是空着，内核会尽量多缓存；应用需要内存时再回收。
- 读命中：直接从 page cache 拷贝到用户缓冲区，不产生设备 I/O。
- 读未命中：先从设备读进 page cache，再拷贝给用户——所以冷读 = 设备延迟 + 内存拷贝。

### 2.1 free -h 里的 buff/cache

```text
              total   used   free   shared  buff/cache   available
Mem:           15Gi  1.2Gi  8.0Gi     10Mi       6.5Gi        14Gi
```

- `buff/cache`：绝大部分就是 page cache（加上少量块设备 buffer 与可回收内核结构）。
- `free` 很小**不代表**内存不够——看 `available`：它估计的是"应用还能拿到多少"，page cache 大部分可以随时回收让出来。
- 本周实验的直接观察：读完 8 GiB 测试文件后 `buff/cache` 明显上涨；drop_caches 后回落。

### 2.2 dirty page 与 writeback（本周只需概念）

写入时数据先进 page cache，对应 page 被标记为 **dirty**（脏页），由内核在后台异步刷回设备（writeback）。`write` 返回 ≠ 数据落盘。本周只读不写，这个话题在 [[S-Week 2 - O_DIRECT + 持久化语义]] 展开；但它解释了下一节"为什么 drop_caches 前要 sync"。

---

## 3. 冷/热 cache 与 drop_caches

实验要对比"冷 cache"（数据不在 page cache）和"热 cache"（数据都在），所以必须能人为制造这两种状态：

- **制造热**：实验前把整个文件顺序读一遍（预热）。
- **制造冷**：

```bash
sync && echo 3 | sudo tee /proc/sys/vm/drop_caches
```

`drop_caches` 的取值含义：

| 值 | 丢弃什么 |
|---|---|
| 1 | page cache |
| 2 | dentry 和 inode 缓存（目录项/元数据） |
| 3 | 两者都丢 |

### 3.1 为什么 drop_caches 前必须 sync

`drop_caches` **只丢弃干净（clean）的 page**，不会写回脏页——否则就等于丢数据。如果不先 `sync`，脏页还留在 cache 里丢不掉，"冷"就冷得不彻底，实验数据被污染。所以固定搭配是 `sync`（把脏页刷成干净页）之后紧跟 `drop_caches`。

### 3.2 实验纪律

- drop_caches 是 root 操作、影响全机性能，只在专用实验机上做；这也是 `CLAUDE.md` 里要求"涉及 drop_caches 的命令必须先向用户确认"的原因。
- 每组冷实验前都要重新 drop——第一次冷读本身就会把数据重新热起来。
- 冷/热状态用三个证据交叉验证：延迟数量级、`iostat` 有无设备流量、`free -h` 的 buff/cache 变化。

---

## 4. readahead：为什么冷顺序读比冷随机读快

本周实验矩阵里有一个反直觉的预期：**冷 cache 顺序读明显快于冷 cache 随机读**——明明都要到设备。原因是内核的**预读（readahead）**：

- 内核检测到顺序读访问模式后，会提前把后面的内容批量读进 page cache（默认预读窗口 128 KiB，见 `/sys/block/<dev>/queue/read_ahead_kb`）。
- 于是你的第 N+1 次"冷"顺序读，实际上经常命中"刚被预读进来的 page"——单次 syscall 看到的是内存速度。
- 随机读的偏移无规律，预读派不上用场，每次都老老实实到设备。

这带来两个结论：

1. "顺序快随机慢"在 SSD 上依然成立，但主因已从 HDD 的寻道变成了**预读 + 批量化**。
2. benchmark 想测"设备真实随机读延迟"，必须用随机模式（后续周还会加 O_DIRECT 彻底绕过 cache）。

预读的触发条件、窗口增长与 `posix_fadvise` 控制，深入见 [[S-Week 1 - Page Cache 与 readahead 专题]]（S-Week 4 会再用到）。

---

## 5. pread 与延迟测量程序要点

### 5.1 为什么用 pread 而不是 read

```cpp
ssize_t pread(int fd, void* buf, size_t count, off_t offset);
```

- `read` 依赖文件描述符里的"当前偏移"，随机读需要先 `lseek` 再 `read`，两次调用；`pread` 一次调用带上偏移，**不移动、不依赖当前偏移**。
- 偏移是显式参数，多线程共用一个 fd 也不会互相干扰——这也是它面试里常被问到的点。

### 5.2 计时与随机偏移

- 计时用 `std::chrono::steady_clock`（单调时钟，不受系统时间调整影响；不要用 `system_clock`）。
- 每次 `pread` 单独计时、**逐笔写入 CSV**——只有逐笔数据才能事后算任意分位数、画分布图。
- 随机偏移用固定 seed 的 `std::mt19937_64` 生成：固定 seed = 每次运行访问同一串偏移 = 实验可复现。
- 偏移要**对齐到块大小**（`offset = (rand % (filesize / bs)) * bs`），避免一次逻辑读跨两个 page 的歧义。
- fd 用 RAII 封装（构造 open、析构 close），复用 [[4.1 打开、读取、写入、关闭]] 的系统调用知识。

分工（与周计划一致）：`pread` 循环与计时逻辑自己写；CMake、CSV 解析、统计与画图脚本交给 Agent。

---

## 6. 延迟数据怎么记录才可信

沿用推理版 serving benchmark 的方法论（[[Week 5 - Serving Benchmark Harness]]），存储侧原样适用：

| 原则 | 做法 | 为什么 |
|---|---|---|
| 报分位数不报均值 | p50 / p95 / p99 + 均值 | 存储延迟是长尾分布，均值被尾部拉偏，p99 才反映用户可感知的最差体验 |
| 预热与状态控制 | 热实验先整读预热；冷实验先 sync + drop_caches | 控制"是否命中 cache"这个最大变量 |
| 重复 | 每组至少 3 次，报每次结果而非只报最好 | 识别方差与偶发毛刺 |
| 原始数据不动 | 逐笔 CSV 进 `results/`，只读不改 | 结论可以重算，数据不能重来 |
| 环境记录 | `env.md`：机器/内核/盘型号/文件系统/工具版本 | 别人能复现，自己隔月能对账 |

口算换算（面试和读报告都常用）——带宽等于 IOPS 乘以块大小：

$$
Bandwidth = IOPS \times BlockSize
$$

例如 4 KiB 随机读 200k IOPS：

$$
200000 \times 4 KiB = 800 MB/s
$$

反过来拿到带宽和块大小也要能立刻反推 IOPS。

---

## 7. 观测工具：iostat 与 free

实验期间**另开一个终端**常驻：

```bash
iostat -x 1     # 每秒刷新一次扩展指标
```

本周只需要看懂这几列（针对 nvme 设备行）：

| 列 | 含义 | 本周判据 |
|---|---|---|
| r/s | 每秒读请求数（合并后） | 热读 ≈ 0，冷读大量出现 |
| rkB/s | 每秒读的数据量 | 冷顺序读时明显高于冷随机读（预读批量化） |
| r_await | 读请求平均延迟（含排队） | 与自测冷读延迟对得上量级 |
| aqu-sz | 平均队列深度 | 单线程同步读时应当很小（约等于 0-1） |
| %util | 设备忙的时间占比 | 热读时为 0 |

`free -h` 在每组实验前后各记一次，观察 buff/cache 的涨落（第 2.1 节）。

`iostat -x` 更完整的列解读（w_await、wareq-sz 等）留到 [[存储性能分析专题 - fio 与 benchmark matrix]] 和 S-Week 3。

---

## 8. 环境验证命令逐条解释

周计划要求到手先跑这组命令并记入 `env.md`，逐条说明在验证什么：

| 命令 | 验证什么 | 关注点 |
|---|---|---|
| `uname -r` | 内核版本 | ≥ 5.15，保证后续周 io_uring 特性可用 |
| `lsblk -o NAME,SIZE,ROTA,TYPE,MOUNTPOINT` | 盘的拓扑 | `ROTA=0` 表示非旋转介质（SSD）；确认实验目录挂在本地 NVMe 上 |
| `sudo nvme list` | 盘型号与容量 | 报告必须写明型号；这也是"本地 NVMe 而非云盘"的证据 |
| `df -T /data` | 实验目录文件系统类型 | ext4 / xfs 行为有差异，记录在案 |
| `cat /sys/block/nvme0n1/queue/scheduler` | I/O 调度器 | NVMe 默认通常是 `none`（多队列直通），记录即可 |
| `free -h` | 内存总量 | 决定测试文件要多大：**文件要显著大于可用内存的一半**（周计划用 8 GiB），否则整个文件都装进 cache，"冷"实验做不出来 |

> [!warning] 云盘数据不能写进报告
> EBS / 云硬盘类块存储走网络虚拟化，延迟特性与本地 NVMe 完全不同。培养方案的边界：**云盘只能做流程验证，性能结论必须来自本地 NVMe**。`nvme list` 有输出、`lsblk` 确认挂载点，才算环境合格。

---

## 9. 常见错误

- **忘记 sync 就 drop_caches**：脏页丢不掉，冷得不彻底，冷读数据偏快。
- **测试文件太小**：文件整个装进 page cache，第一遍读完后再也做不出冷实验。
- **只跑一次就下结论**：第一次冷读同时承担了预读窗口建立等一次性成本，必须重复。
- **用 system_clock 计时**：NTP 校时会让个别样本出现负延迟或异常值。
- **随机偏移不对齐**：跨 page 的读引入额外变量，冷读延迟解释不清。
- **只报平均值**：长尾被平均抹掉，p99 信息全丢。
- **忘开 iostat**：只有延迟数字、没有设备流量证据，说服力减半——"你怎么知道热读没碰盘？"答不上来。
- **在带宝贵数据的机器上练 drop_caches / dd**：所有写操作只指向实验目录下的测试文件，禁止指向 `/dev/nvme*` 裸设备。

## 10. 学习检查清单

- [ ] 能画出一次 read 从用户态到 NVMe 的分层路径，并指出 page cache 在哪一层短路返回。
- [ ] 能解释 `free -h` 中 buff/cache 与 available 的含义。
- [ ] 能说出 drop_caches 1/2/3 的区别，以及 sync 在前的原因。
- [ ] 能解释冷顺序读比冷随机读快的机制（readahead）。
- [ ] 能说出 pread 相对 read 的两个优势。
- [ ] 能解释为什么报 p99 而不是只报均值。
- [ ] 能看懂 `iostat -x` 的 r/s、r_await、%util，并说出热/冷读各自的预期形态。
- [ ] 能解释测试文件为什么必须显著大于内存。

## 11. 关键要点总结

- 存储 benchmark 第一问：**这次 I/O 有没有碰设备**。page cache 命中与否 = 1-2 个数量级的差距。
- 冷/热状态要主动制造（预热 vs sync + drop_caches），并用延迟、iostat、buff/cache 三个证据交叉验证。
- 冷顺序读快于冷随机读的功臣是 readahead，不是 SSD 本身"喜欢顺序"。
- 测量程序：pread + steady_clock + 固定 seed + 对齐偏移 + 逐笔 CSV。
- 报告纪律：p50/p95/p99、至少 3 次重复、env.md 完整、原始数据只增不改。
- Agent 边界与推理版一致：生成脚手架和脚本，不下结论、不改数据。

## 关联知识

- [[S-Week 1 - 环境搭建 + Page Cache 基线]]（本篇服务的周计划）
- [[S-Week 1 - Linux I O 路径专题 - VFS 到 NVMe 全路径]]（路径逐层深挖）
- [[S-Week 1 - Page Cache 与 readahead 专题]]（cache 与预读深挖）
- [[AI Infra 存储与 GPU 数据路径系统工程师培养方案]]
- [[4.1 打开、读取、写入、关闭]]（open/read/write/close 系统调用基础）
- [[14.3 CMake基础]]（建仓库用）
- [[Week 5 - Serving Benchmark Harness]]（benchmark 方法论的来源）

## 参考

- OSTEP 第 36 章 I/O Devices、第 37 章 Hard Disk Drives：[网站](https://pages.cs.wisc.edu/~remzi/OSTEP/)
- `man 2 pread`、`man 2 readahead`、`man proc`（/proc/sys/vm/drop_caches 条目）
- Linux 内核文档：Documentation/admin-guide/sysctl/vm.rst（drop_caches）
