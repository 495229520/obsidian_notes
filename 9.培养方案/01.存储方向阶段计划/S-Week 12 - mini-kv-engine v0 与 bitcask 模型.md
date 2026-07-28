---
title: S-Week 12 - mini-kv-engine v0 与 bitcask 模型
date: 2026-07-12
tags:
  - 高性能存储
  - 存储方向阶段计划
  - 计划
status: active
---

# S-Week 12 - mini-kv-engine v0 与 bitcask 模型

> [!goal] 本周目标
> 阶段 2 开局：从 linux-io-lab 的"测量者"切换成 mini-kv-engine 的"实现者"。读 DDIA 第 3 章上半，把 bitcask 模型（append-only log + 内存 hash index）用 Modern C++ 落成能跑的 v0——put/get/delete/重启重放全部有单测。S1 攒下的 I/O 路径知识从"解释别人的延迟"第一次变成"设计自己的写路径"。

## 学习目标

1. **bitcask 为什么写快？** 所有写都是对 active log 的顺序 append，磁盘只做它最擅长的事（S-Week 1 顺序 vs 随机的结论直接复用）；索引更新在内存完成。
2. **读为什么也只要一次 I/O？** 内存 hash index 存 key → (file_id, offset, size)，get = 查表 + 一次 pread。
3. **代价是什么？** 全部 key 必须装进内存；启动要扫全量 log 重建索引；overwrite/delete 留下的旧记录造成空间放大，要靠 compaction 回收。
4. **delete 为什么写 tombstone？** append-only 不允许原地改；删除是一条特殊记录，重放到它时把 key 从索引移除。
5. **record 格式要哪些字段？** crc | timestamp | key_size | value_size | key | value。定长头在前——恢复时先读头才知道这条记录多长，扫描才能逐条前进。

## 1. DDIA 与设计（Day 1-2）

- 读 DDIA 第 3 章上半（hash 索引、bitcask 模型），带着问题读：它牺牲了什么换写入速度？
- 写 `docs/record_format.md`：字段定义、字节序、key/value 长度上限、tombstone 的表示方式。
- 项目骨架：CMake + 单测框架（GoogleTest 或 doctest 二选一）+ RAII 的 `FileHandle`（构造 open、析构 close、禁拷贝、可移动）。
- 错误处理约定选定一种（错误码 / `std::expected` 风格）并写进 `design_note.md` 草稿——第一批设计决策从这里开始记录。

代码风格按仓库规范：智能指针 / RAII，禁 `new/delete`、C 风格数组与转换。

## 2. v0 实现（Day 3-4）

- `put/get/del` 三个接口；单个 active log 文件，写用 append（`write`），读用 `pread`。
- 索引：`std::unordered_map<std::string, IndexEntry>`，`IndexEntry = {file_id, offset, size}`。
- overwrite 语义 = 追加新记录 + 索引指向新 offset，旧记录变成垃圾（空间放大的伏笔，留给 compaction）。
- delete = 追加 tombstone + 从索引移除。

## 3. 重启重放与单测（Day 5）

- 启动时全量扫描 log 重建索引（本周先不做完整性校验，S-Week 13 加 CRC 后升级）。
- 单测覆盖：基本 put/get、overwrite 取最新、delete 后不可见、重启后可见性一致、空库启动。
- 跑通 `ctest`，全绿才算 v0 完成。

## 4. 推理保温（约 25%）

- 维护态：serving benchmark harness 冒烟复跑一次，确认可用；KV cache 每 token 字节数口算保手感。

## 5. 面试保底（约 15%）

> 阶段 2 八股按章清账第 1 讲：目标是 2027-03 前把 C++ / Linux 八股按章过完、[[CodeTop 高频题 Top300]] 前 150 收口，直接服务 2027 春/夏实习投递。

- 算法（5-8 题）：数据结构设计专题。参考 [[5.2.16 面试常考数据结构设计]]，重点 [[146. LRU 缓存]]——和本周的 hash index 是同一套"哈希表 + 附加结构"思路。
- 八股（1 章）：STL 容器底层。过 [[5.1 STL]]、[[5.1.1 vector]]、[[5.1.5 map]]、[[12.3 迭代器的失效问题]]。验收：能讲清 unordered_map 扩容 rehash 时迭代器/引用的失效规则——正好回答"index 为什么选 unordered_map 而不是 map"。
- 项目问答：10 个 Q&A（本周素材：bitcask 三条取舍、record 格式设计、tombstone）。

## 6. 本周产出文件

| 文件 | 内容 | 验收方式 |
|---|---|---|
| `src/` + `tests/` | v0 引擎与单测 | ctest 全绿 |
| `docs/record_format.md` | record 格式定义 | 仅靠文档能逐条解析 log |
| `design_note.md`（草稿） | 骨架 + 第一批决策（错误处理、格式） | 每个决策有备选方案 |

## 7. 验收标准

- [ ] put/get/delete/overwrite + 重启重放的单测全部通过。
- [ ] record 格式文档化，恢复扫描仅依赖格式定义即可逐条前进。
- [ ] 能脱稿讲 bitcask 三条取舍（写快 / 读一跳 / 内存吃下全部 key）。
- [ ] RAII 与错误处理约定成文，代码无裸 new/delete。
- [ ] 推理保温和面试保底完成本周额度。

## 面试问题

- bitcask 和 B+ 树的读写路径各是什么形态？各适合什么负载？
- 为什么删除不原地删？tombstone 什么时候才真正消失？
- key 的数量超过内存怎么办？（引出 SSTable/LSM——v2 的伏笔）
- overwrite 之后旧数据去哪了？空间怎么回收？
- 你的 record 头为什么设计成定长？变长头会带来什么麻烦？

## 关联知识

- [[S-Week 11 - 完整版收口]]
- [[S-Week 13 - checksum 与崩溃恢复]]
- [[S-Week 12 - 前置知识 - mini-kv-engine v0 与 bitcask 模型]]
- [[存储引擎专题 - bitcask 与哈希索引]]
- [[S-Week 1 - Linux I O 路径专题 - VFS 到 NVMe 全路径]]（顺序写为什么快）
- [[AI Infra 存储与 GPU 数据路径系统工程师培养方案]]（S2 里程碑与验收）
- DDIA 第 3 章（存储与检索）；Bitcask 论文（Riak：A Log-Structured Hash Table）
