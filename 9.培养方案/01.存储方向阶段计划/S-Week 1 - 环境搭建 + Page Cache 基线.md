---
title: S-Week 1 - 环境搭建 + Page Cache 基线
date: 2026-07-08
tags:
  - infra
  - 存储
  - 阶段计划
status: active
---

# S-Week 1 - 环境搭建 + Page Cache 基线

> [!goal] 本周目标
> 建立存储实验的最小闭环：一台带本地 NVMe 的 Linux 环境、一个带 `CLAUDE.md` 的 `linux-io-lab` 仓库、一组能证明 page cache 存在的冷/热读延迟数据。本周结束时，你要能用自己的数据回答"page cache 命中和不命中差几个数量级"。

## 学习目标

完成这一周后，应该能回答五个问题：

1. **一次 read 走了哪些层？** 用户态 → 系统调用 → VFS → page cache → 块层 → NVMe 设备。
2. **page cache 是什么？** 为什么第二次读同一个文件快这么多？`free -h` 里的 buff/cache 是什么？
3. **怎么制造"冷 cache"？** `drop_caches` 做了什么，为什么实验前要 `sync`？
4. **延迟数据怎么记录才可信？** 逐笔记录、报 p50/p95/p99 而不是只报平均值。
5. **Agent 在存储项目里的边界是什么？** 和推理版一致：生成脚手架和脚本，不下结论、不改数据。

## 1. 环境搭建（Day 1-2）

### 1.1 云主机要求

- 带**本地 NVMe 盘**（instance store / local SSD），不是云盘——云盘数据不能写进报告。
- Ubuntu 22.04+ 或同代发行版，内核 5.15+（保证 io_uring 可用性，后续周要用）。
- 按小时计费，实验做完即释放；代码保留在 Git，数据下载到本地。

到手后先验证并记录（写进 `env.md`，以后每次实验都附）：

```bash
uname -r                 # 内核版本
lsblk -o NAME,SIZE,ROTA,TYPE,MOUNTPOINT
sudo nvme list           # 盘型号（需 apt install nvme-cli）
df -T /data              # 实验目录的文件系统类型
cat /sys/block/nvme0n1/queue/scheduler
free -h
```

安装工具链：

```bash
sudo apt update
sudo apt install -y fio sysstat nvme-cli build-essential cmake git
```

### 1.2 建仓库

创建两个仓库：

- `linux-io-lab`：本阶段主项目。目录结构参考推理版 [[Week 1 - CUDA + Agent workflow]] 的模板思路：`src/`、`scripts/`、`results/`、`docs/`，加 `CMakeLists.txt`（CMake 语法回看 [[14.3 CMake基础]]）。
- `storage-ai-infra-portfolio`：索引仓库，本周只放 `README.md` 和 `env-template.md` 初稿。

`CLAUDE.md` 沿用推理版的写法，存储版核心约束：

```markdown
## Safety
- 绝不修改 results/ 下的原始数据。
- 涉及 drop_caches、mkfs、dd 写盘的命令必须先向用户确认。
- benchmark 结论由用户人工判断，Agent 只生成脚本和图表。
```

> [!warning] 破坏性命令边界
> 本项目会频繁使用 root 权限（drop_caches）。`dd`、`mkfs`、`fio` 写裸设备都可能毁数据——所有写操作只允许指向实验目录下的测试文件，禁止指向 `/dev/nvme*` 裸设备（阶段 0 用不到裸设备）。

## 2. 实验：冷/热 page cache 读延迟（Day 3-5）

### 2.1 准备测试文件

```bash
mkdir -p /data/iolab && cd /data/iolab
# 8 GiB 测试文件，超过多数实例内存的一半，避免全部驻留 cache
dd if=/dev/urandom of=testfile bs=1M count=8192 status=progress
sync
```

### 2.2 自写 C++ 读程序

写一个最小的 `pread` 延迟测量程序（Modern C++，RAII 管理 fd）：

- 参数：文件路径、块大小（默认 4096）、读取次数、顺序 / 随机模式。
- 每次 `pread` 用 `std::chrono::steady_clock` 记录耗时，逐笔写入 CSV。
- 随机偏移用固定 seed 的 `std::mt19937_64` 生成（可复现），偏移对齐到块大小。
- 结束时输出 p50 / p95 / p99 / 平均值 / IOPS。

Agent 生成 CMake、CSV 解析和统计脚本；`pread` 循环与计时逻辑自己写。

### 2.3 实验矩阵

| 实验 | 操作 | 预期观察 |
|---|---|---|
| 热 cache 顺序读 | 先整读一遍文件预热，再顺序读 | 微秒级，接近内存拷贝 |
| 热 cache 随机读 | 预热后随机读 | 仍是微秒级 |
| 冷 cache 随机读 | `sync && echo 3 \| sudo tee /proc/sys/vm/drop_caches` 后随机读 | 到设备，数十至上百微秒 |
| 冷 cache 顺序读 | drop_caches 后顺序读 | 比冷随机读快——思考为什么（readahead） |

每组至少 3 次重复；实验期间另开终端跑 `iostat -x 1`，观察冷读时 `r/s` 出现、热读时块设备完全没有流量——这是 page cache 命中与否最直观的证据。

### 2.4 必须回答

- 冷/热延迟差了几倍？属于哪两个存储层次的差距？
- 冷 cache 顺序读为什么明显快于冷 cache 随机读？（readahead 预读了什么？）
- `free -h` 的 buff/cache 在实验前后怎么变化？

## 3. 理论配套（穿插在 Day 1-5）

- OSTEP：第 36 章 I/O Devices、第 37 章 Hard Disk Drives（HDD 的寻道模型是理解"随机 vs 顺序"的原点，SSD 章后置到 S-Week 3 之后）。
- 复习本仓库 [[4.1 打开、读取、写入、关闭]]，把 open/read/write/close 与本周实验对上。
- 初步浏览 `man 2 pread`、`man 2 readahead`。

## 4. 推理保温（约 25%）

- 继续推进 [[Week 5 - Serving Benchmark Harness]] 当前进度（若已完成则进入 [[Week 6 - Observability + Metrics]]）。
- 本周至少产出：一组新的 serving benchmark 数据或一节 `benchmark_report.md`。
- 注意迁移：serving 里的 p99 / warmup / 可复现原则，本周存储实验原样在用——两边报告格式尽量统一。

## 5. 面试保底（约 15%）

- 算法（5-8 题）：数组 / 哈希主题。参考 [[5.2.8 哈希表与计数]]，做 [[3. 无重复字符的最长子串]]、[[15. 三数之和]] 及 [[CodeTop 高频题 Top300]] 同类题。
- 八股（1 章）：智能指针。过 [[独享智能指针]]、[[共享智能指针]]、[[weak_ptr]]，用 [[智能指针总结与练习题]] 自测。
- 项目问答：把本周实验整理成 10 个 Q&A 写入 `interview_qa.md`。

## 6. 本周产出文件

| 文件 | 内容 | 验收方式 |
|---|---|---|
| `env.md` | 机器、内核、盘型号、文件系统、工具版本 | 别人能据此复现环境 |
| `src/read_latency.cpp` | pread 延迟测量程序 | 顺序/随机、冷/热四种模式可跑 |
| `results/*.csv` | 逐笔延迟原始数据 | 不手动修改 |
| `docs/benchmark.md` | 四组实验对比表 + 结论 | 每个结论有数据支撑 |
| `CLAUDE.md` | 构建命令、安全边界 | Agent 能按说明执行 |

## 7. 验收标准

- [ ] 云主机环境就绪，`env.md` 完整。
- [ ] 两个仓库创建并有首次提交。
- [ ] 四组冷/热实验完成，各 3 次重复。
- [ ] 能报出冷/热 p99 的具体数字和倍数关系。
- [ ] 能解释冷顺序读为什么比冷随机读快。
- [ ] iostat 证据（冷读有设备流量、热读没有）截图或记录在案。
- [ ] 推理保温和面试保底完成本周额度。

## 面试问题

- 一次 read 从系统调用到数据返回经过哪些层？
- page cache 命中和不命中延迟差多少？你的数据是多少？
- drop_caches 前为什么要先 sync？
- 为什么报告 p99 而不是只报平均延迟？
- readahead 是什么时候触发的？对哪类负载有效？

## 关联知识

- [[AI Infra 存储与 GPU 数据路径系统工程师培养方案]]
- [[00.存储方向阶段计划索引]]
- [[S-Week 2 - O_DIRECT + 持久化语义]]
- [[4.1 打开、读取、写入、关闭]]
- [[14.3 CMake基础]]
- OSTEP Ch.36-37（I/O Devices / HDD）
