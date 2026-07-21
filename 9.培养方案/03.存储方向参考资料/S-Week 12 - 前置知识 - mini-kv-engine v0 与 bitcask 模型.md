---
title: S-Week 12 - 前置知识 - mini-kv-engine v0 与 bitcask 模型
date: 2026-07-12
tags:
  - 高性能存储/存储方向参考资料/计划
aliases:
  - 存储 Week 12 前置知识
  - bitcask 前置知识
status: active
---

# S-Week 12 - 前置知识 - mini-kv-engine v0 与 bitcask 模型

## 索引

- [[#0. 先建立直觉：最朴素的数据库只有两行 shell]]
- [[#1. bitcask 三件套与一次 put/get 的完整路径]]
- [[#2. record 格式：每个字段为什么存在]]
- [[#3. KeyDir：索引选型与内存口算]]
- [[#4. tombstone 与 overwrite：append-only 世界的删改]]
- [[#5. 重启重放 v0：实现要点]]
- [[#6. Modern C++ 工程骨架]]
- [[#7. 单测设计：五个必测场景]]
- [[#8. 常见错误]]
- [[#9. 学习检查清单]]
- [[#10. 关键要点总结]]
- [[#关联知识]]
- [[#参考]]

## 阅读说明

这篇是 [[S-Week 12 - mini-kv-engine v0 与 bitcask 模型]] 的总前置知识：动手前通读 0-2 节建立模型图景，设计 record 格式前精读 2 节，写代码前看 3-6 节，写测试时对照 7 节。深挖版见 [[存储引擎专题 - bitcask 与哈希索引]]。

---

> 阶段 0/1 你一直在**测量**别人的 I/O 路径；本周开始**设计**自己的写路径。角色转变带来一个新问题：过去问"这个延迟花在哪一层"，现在要问"我把数据放在哪、崩溃后还能不能找回来"。bitcask 是回答这个问题的最小可行模型——DDIA 第 3 章开篇讲的就是它。

---

## 0. 先建立直觉：最朴素的数据库只有两行 shell

DDIA 第 3 章的开场实验，值得先亲手跑一遍：

```bash
db_set()  { echo "$1,$2" >> database; }
db_get()  { grep "^$1," database | sed -e "s/^$1,//" | tail -n 1; }
```

- `db_set` 是**追加**——所以它快（顺序写），而且天然保留历史版本（`tail -n 1` 取最新）。
- `db_get` 是**全文件扫描**——所以它慢，O(n)。

bitcask 对这个模型只做了一处升级：**给 get 加一张内存哈希表**，记录每个 key 最新一条记录的文件偏移。写路径原封不动地保持"只追加"。这一处升级就是本周全部工作的核心。

> [!important] 第一性原理
> 磁盘擅长顺序访问（S-Week 1/3 的实测差距是数量级级别的）。存储引擎设计的第一问永远是：**怎么把用户的随机写变成磁盘的顺序写**。bitcask 的答案最直白——所有写都是 append，"哪个是最新版本"这个问题交给内存索引回答。

## 1. bitcask 三件套与一次 put/get 的完整路径

| 组件 | 位置 | 职责 |
|---|---|---|
| active log | 磁盘 | 唯一数据本体，append-only |
| KeyDir | 内存 | key → (file_id, offset, size)，指向最新版本 |
| merge | 后台（本周不做） | 回收旧版本垃圾 |

三条操作路径：

- **put(k, v)**：序列化 record → append 到 log 尾部 → KeyDir[k] = 新 (offset, size)。两步，磁盘侧纯顺序。
- **get(k)**：查 KeyDir → 命中则按 (offset, size) 一次 `pread` → 反序列化返回。未命中直接返回不存在——**不碰盘**。
- **del(k)**：append 一条 tombstone → KeyDir.erase(k)。

注意 get 用 `pread` 而不是 `lseek + read`：pread 带偏移原子读，不动文件游标，天然线程安全（[[4.1 打开、读取、写入、关闭]]）。写端用单 fd append，读端可以共享同一个 fd 用 pread 并发读——这个"一写多读"结构 S-Week 15 会正式展开。

## 2. record 格式：每个字段为什么存在

![[图片/9.培养方案/03.存储方向参考资料/9_3_12_1.svg|940]]

| 字段 | 大小 | 为什么存在 |
|---|---|---|
| crc32c | 4 B | 完整性校验（S-Week 13 启用，格式里从第一天就留位） |
| tstamp | 8 B | 版本时间戳；merge 时判新旧、调试时定位 |
| key_size | 4 B | 变长 key 的长度 |
| value_size | 4 B | 变长 value 的长度；特殊值兼作墓碑标记 |
| key | 变长 | — |
| value | 变长 | 墓碑记录无此段 |

设计纪律：

- **定长头在前**：恢复扫描先读 20 B 头才知道这条记录总长，扫描才能逐条前进。变长字段绝不能放在头里。
- **显式定义字节序**：格式文档写死（本机开发用小端即可，但要写下来）；跨平台不是本周目标，声明即可。
- **长度上限**：key_size / value_size 各设硬上限（比如 4 KiB / 16 MiB），恢复时超上限即判损坏——没有上限，一个坏长度字段能让你 `malloc` 出 4 GB。
- **对齐不填充**：记录首尾不做扇区对齐（空间换鲁棒性不划算，小 value 场景浪费严重），半条记录的问题 S-Week 13 用 CRC + 截断解决。

序列化建议手写：固定头用一个 POD struct + `memcpy` 进出 buffer（注意不要直接把 struct 指针 reinterpret 到文件 buffer 上跨平台读，编译器 padding 会坑人——用逐字段 memcpy 或 `#pragma pack` 明确声明）。

## 3. KeyDir：索引选型与内存口算

选型直接用 `std::unordered_map<std::string, IndexEntry>`：

```cpp
struct IndexEntry {
    uint32_t file_id;
    uint64_t offset;
    uint32_t size;      // 整条 record 的长度，pread 一次读全
};
```

- **为什么不是 std::map**：不需要有序遍历，O(1) 优于 O(log n)；真需要范围扫描时该换的是整个模型（LSM），不是换 map。
- **为什么 size 存整条记录长**：get 时一次 pread 拿回完整 record 再校验解析，不用两次 I/O（先头后体）。
- **内存口算**（面试必问）：每个条目 ≈ key 字节数 + IndexEntry 16 B + unordered_map 节点/桶开销（约 30-50 B）+ std::string 头 32 B。粗算每 key 100 B 量级：1000 万 key ≈ 1 GB。这就是"bitcask 要求 key 装进内存"的定量版本。
- 迭代器失效规则顺手复习：unordered_map rehash 后迭代器全失效、但**元素指针/引用不失效**——本周面试保底的 [[12.3 迭代器的失效问题]] 正好对上。

## 4. tombstone 与 overwrite：append-only 世界的删改

- **overwrite**：就是再 put 一次。新记录 append，KeyDir 改指新 offset，旧记录成为**垃圾**但物理还在。log 大小只增不减——空间放大的来源，本周先接受它，merge 是后续可选项。
- **delete**：append 一条 tombstone（value_size 用特殊标记值，无 value 段），然后 KeyDir.erase。**为什么必须写墓碑而不是只删索引**：索引是易失的！只删内存索引，重启重放后这个 key 会从旧记录里"复活"。墓碑是持久化的删除凭证。
- 重放遇到墓碑：从正在重建的 KeyDir 里 erase。重放顺序天然保证"后写覆盖先写"。

## 5. 重启重放 v0：实现要点

```text
offset = 0
while offset + HEADER_SIZE <= file_size:
    读 20 B 头
    if 长度字段超上限或越界: break        # v0 先 break，S-Week 13 升级为截断
    读 key（和需要的话 value 的存在性）
    if 是墓碑: keydir.erase(key)
    else:      keydir[key] = {file_id, offset, record_size}
    offset += record_size
```

v0 的简化：不做 CRC（字段留空）、遇到解析不动的尾部直接停止。这周的目标是**格式与重放逻辑正确**，容错升级完整地留给 S-Week 13——一次只引入一个复杂度。

## 6. Modern C++ 工程骨架

- **RAII FileHandle**：构造 `open`（`O_CREAT | O_APPEND | O_WRONLY` 写端 / `O_RDONLY` 读端）、析构 `close`、禁拷贝、可移动。裸 fd 不许在业务代码里出现。
- **错误处理约定**（写进 design_note，二选一并坚持）：
  - 错误码风格：`enum class KvError` + `std::expected<T, KvError>`（C++23）或自写轻量 Result；
  - 异常风格：只在构造失败等不可恢复处抛 `std::system_error`。
  - 推荐前者：存储引擎的错误（NotFound / Corruption / IOError）是控制流的一部分，不是异常事件——RocksDB 的 `Status` 就是这个思路。
- **目录结构**：`src/`（engine.h/cpp、record.h/cpp、file_handle.h）、`tests/`、`docs/`、CMake + GoogleTest（`FetchContent` 拉取即可）。
- 风格红线（仓库规范）：无裸 `new/delete`、无 C 数组、`static_cast` 家族、`std::` 算法优先。

## 7. 单测设计：五个必测场景

| 场景 | 断言 |
|---|---|
| 基本 put/get | 写后读回逐字节相等 |
| overwrite | get 返回最新值；旧 offset 仍可（内部接口）读到旧值——证明 append 语义 |
| delete | get 返回 NotFound；重启后仍是 NotFound（墓碑生效） |
| 重启可见性 | put 若干 → 正常关闭 → 重开 → 全部可读且值正确 |
| 空库/边界 | 空文件启动不崩；空 key/最大长度 key/空 value 各一条 |

测试基础设施：每个用例用独立临时目录（`std::filesystem::temp_directory_path()` + 随机子目录），fixture 里自动清理——测试之间零共享状态。

## 8. 常见错误

- **变长字段放进头部**：恢复扫描无法逐条前进。定长头是铁律。
- **只删索引不写墓碑**：重启后被删的 key 复活——本周最典型的语义 bug。
- **长度字段不设上限**：一个损坏的 length 让恢复代码申请海量内存或越界读。
- **用 `struct` 直接 `write` 落盘**：编译器 padding 让格式隐式依赖编译器；逐字段序列化或显式 pack。
- **get 用 `lseek + read`**：游标是共享状态，并发读互相踩；用 `pread`。
- **单测共享同一个数据目录**：上一个用例的残留 log 污染下一个；每用例独立临时目录。
- **过早做 merge/hint file**：v0 的验收是"四操作 + 重放正确"，把复杂度留给对应周次。

## 9. 学习检查清单

- [ ] 能画出 put/get/del 三条路径（哪步碰内存、哪步碰盘、盘上是顺序还是随机）。
- [ ] 能默写 record 六字段并说出每个字段存在的理由。
- [ ] 能口算给定 key 规模下 KeyDir 的内存占用。
- [ ] 能解释为什么 delete 必须写墓碑而不能只删索引。
- [ ] 能说出恢复扫描循环的终止条件和 v0 的简化边界。
- [ ] 能讲清 pread 相对 lseek+read 的并发优势。
- [ ] RAII FileHandle 与错误处理约定已成文并在代码里贯彻。

## 10. 关键要点总结

- bitcask = "append-only log + 内存 hash 索引"：把用户的随机写变成磁盘的顺序写，把"找最新版本"交给内存。
- 定长头在前、长度设上限、字节序写死——record 格式的三条纪律都是为恢复扫描服务的。
- 删除是一条持久化的墓碑记录，不是一次内存操作。
- KeyDir 的内存占用要会口算，它是"能不能用 bitcask"的第一道判断。
- 工程骨架先行：RAII、错误码约定、独立临时目录的单测——后面五周全部建在这上面。

## 关联知识

- [[S-Week 12 - mini-kv-engine v0 与 bitcask 模型]]（本篇服务的周计划）
- [[存储引擎专题 - bitcask 与哈希索引]]（深挖版与面试口述）
- [[S-Week 1 - 前置知识 - 环境搭建 + Page Cache 基线]]（顺序 vs 随机的测量地基）
- [[4.1 打开、读取、写入、关闭]]（open/pread/append 语义）
- [[12.3 迭代器的失效问题]]（unordered_map 失效规则，本周面试保底）
- [[存储引擎专题 - WAL 与 crash consistency]]（下周开始的主线）

## 参考

- DDIA 第 3 章上半（哈希索引一节）：`书籍/` 目录
- Bitcask 论文：*Bitcask - A Log-Structured Hash Table for Fast Key/Value Data*（Basho，很短，一小时能读完）
- `man 2 pread`、`man 2 open`（O_APPEND 语义）
- GoogleTest 文档（FetchContent 集成）
