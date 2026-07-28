---
title: 存储面试问题清单 - Linux I O
date: 2026-07-12
tags:
  - 高性能存储
  - 存储方向专题清单
  - 清单
roadmap_week: 求职全程（S-Week 7 首次沉淀，阶段 1 后补毛刺定位题）
sort_order: "99.00"
status: active
---

# 存储面试问题清单 - Linux I O

> [!info] 所属路线
> - 培养方案第十四节 Linux I/O 板块的沉淀版：每题给"可脱稿回答"的答案要点 + 自己的实验证据 + 追问预案
> - 排序：99.00
> - 用法：S-Week 7 起面试前扫一遍；每句答案都能指到自己的数据，答不出证据的题回炉对应周次。答案控制在 5 句内，**第一句永远是结论**。

---

## Q1 一次 read 的完整路径是什么？page cache 命中和不命中差在哪？

**答案要点**：路径是 syscall → VFS → page cache → 块层（blk-mq）→ NVMe 驱动 → 设备；命中在 page cache 层短路返回，只是一次内存拷贝，不命中要同步等设备。我的实测：热读约 1 µs，冷读几十µs，差 1-2 个数量级。冷顺序读明显快于冷随机读，功臣是 readahead 把设备 I/O 批量化，我用 iostat 的平均请求大小验证过。

**证据**：[[S-Week 1 - 环境搭建 + Page Cache 基线]] 四组实验；机制底稿 [[S-Week 1 - Linux I O 路径专题 - VFS 到 NVMe 全路径]]。

**追问预案**：
- "怎么证明热读没碰盘？" → iostat 同窗口 r/s 为 0 + buff/cache 不变，三重证据交叉。
- "readahead 什么时候有害？" → 随机大文件负载，白读浪费带宽还污染 cache，用 POSIX_FADV_RANDOM 关。

## Q2 write 返回后数据一定落盘了吗？fsync / fdatasync / O_SYNC 区别？

**答案要点**：不一定——write 返回只是进了 page cache 标脏页，掉电全丢。持久化是阶梯：fdatasync 刷数据加读回必需的元数据，fsync 全刷，两者都含设备 cache flush；O_SYNC 是每笔 write 自带 fsync 语义；sync_file_range 只是回写调度，不是保证。我的五组对比：仅 write 与每笔 fsync 吞吐差 1-2 个数量级。append 场景 size 每笔都变，fdatasync 和 fsync 几乎一样贵，覆盖写才省。

**证据**：[[S-Week 2 - O_DIRECT + 持久化语义]] 五策略实验；深挖 [[O_DIRECT 与持久化语义专题]]。

**追问预案**：
- "每笔 fsync 的上限由什么决定？" → 设备 flush 时间；消费盘毫秒级，带 PLP 的企业盘近似免费，差几十倍。
- "group commit 换的是什么？" → 用最近一组的丢失窗口换吞吐，两个数字必须一起报。

## Q3 O_DIRECT 的对齐要求和适用场景？

**答案要点**：buffer 地址、文件偏移、I/O 长度三者都要对齐到设备逻辑块大小（常见 512/4096），违反报 EINVAL——我故意触发并记录过。适用场景是自管缓存的数据库（避免双重缓存、掌控刷盘节奏）和要测设备本身的 benchmark。它不等于更快：放弃 readahead 和写合并，换延迟确定性和控制权。还有一个常被忽略的点：O_DIRECT 不等于持久化，数据可能停在盘的易失缓存，要保证照样 fsync。

**证据**：[[S-Week 2 - O_DIRECT + 持久化语义]]（EINVAL 案例 + "预热无效"实验）。

**追问预案**：
- "O_DIRECT 读为什么预热无效？" → 根本不进 page cache，没有"热"状态。
- "怎么拿逻辑块大小？" → `/sys/block/<dev>/queue/logical_block_size`；工程上统一按 4096 对齐最稳。

## Q4 io_uring 的 SQ / CQ 模型是什么？相比 epoll + read、AIO 解决了什么？

**答案要点**：提交/完成两个共享内存环，应用写 SQE 内核写 CQE，批量提交批量收割，syscall 数和 I/O 数解耦。语义是完成通知——CQE 出现时数据已就位；epoll 是就绪通知，且对普通文件无意义（永远就绪）。相比 AIO：不限 O_DIRECT、提交路径便宜、支持批量和零 syscall 模式。我的 QD 扫描：单线程堆到 QD32 把 NVMe 喂饱，IOPS 饱和后继续加深，p99 按排队论恶化。

**证据**：[[S-Week 5 - io_uring 异步 IO]] QD 扫描 + `io_models.md`；消融数据 [[S-Week 10 - io_uring 深入]]；体系版 [[io_uring 异步 IO 专题]]。

**追问预案**：
- "为什么网络服务器很多还用 epoll？" → 就绪模型配 Reactor 生态成熟，海量低频连接下 io_uring 预挂 buffer 的内存代价不划算。
- "SQPOLL/registered buffers 省什么？" → 分别省提交 syscall 和每笔页固定，高 IOPS 下才显形——有消融数据。

## Q5 mmap 读文件什么时候比 read 快、什么时候更慢？

**答案要点**：mmap 把 I/O 藏进缺页：省 syscall 和一次拷贝，代价是缺页、页表和 TLB。热数据随机点查 mmap 明显赢（纯内存访问）；冷数据随机点查 mmap 输——每页一次 major fault 同步阻塞且堆不了队列深度。我的实验用 perf 的 major/minor fault 计数证实了这个分界。所以 RocksDB 默认 pread + 自管 block cache（冷点查是常态、SST 压缩块用不上零拷贝、SIGBUS 难处理），而 LMDB 全 mmap 成立是因为假设数据常驻内存。

**证据**：[[S-Week 4 - mmap 与读路径对比]] 四组实验 + fault 计数；选型矩阵 [[mmap 与读路径对比专题]]。

**追问预案**：
- "mmap 拷贝几次？" → 命中 0 次（直接访问 cache 页），read 是 1 次；不命中都先 DMA 进 cache。
- "为什么 mmap 堆不了 QD？" → 缺页在访问指令处同步发生，单线程同一时刻只有一个在途。

## Q6 p99 延迟毛刺怎么定位到具体层？

**答案要点**：分层定位——先用应用侧逐笔延迟确认现象，再 fileslower/biolatency 一上一下分清文件系统层还是块层；biolatency 直方图看形态（整体右移是全局变慢，双峰是混入异类 I/O）；biosnoop 按时间戳抓肇事进程；需要单笔证据时 blktrace 短窗口取证，btt 把延迟拆成 Q2D（排队）和 D2C（设备）——两者的处理路径完全不同。我自己注入 writeback 突发复现过全流程，并用调 dirty 阈值量化了缓解效果。

**证据**：[[S-Week 9 - eBPF 观测]] 的 `p99_hunt.md`（阶段 1 落成后把具体数字填进本节）；方法论 [[块层观测专题 - iostat blktrace eBPF]]。

**追问预案**：
- "找不到外部肇事者但 D2C 变大？" → 怀疑 SSD 内部 GC，稳态盘尤其明显。
- "%util 100% 能说明饱和吗？" → 并行设备上不能，看 IOPS 增量和 await 拐点。

---

## 口述纪律

- 每题第一句是结论，第二句开始给机制，第三句亮自己的数据——**没有第三句的答案在存储岗面试里只值一半分**。
- 数字记量级和倍数（"差 1-2 个数量级"），不背精确值；被追问时再给出处。
- Q6 在阶段 1 完成前只答方法论框架，完成后回来填实测数字。
- 配套自测：随机抽 3 题录音脱稿答，回听检查是否超 5 句、第一句是否结论。

## 关联知识

- [[S-Week 7 - 简历化与投递启动]]（interview_qa.md 的汇总周）
- [[AI Infra 存储与 GPU 数据路径系统工程师培养方案]]（第十四节问题总源）
- [[存储面试问题清单 - NVMe 与 NVMe-oF]]（下一板块，阶段 2 沉淀）
- [[00.存储方向专题清单索引]]
