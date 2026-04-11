---
tags: [Remote Control System, debug, cpp, windows, IOCP, thread, memory, lifetime]
created: 2026-04-09
git: "7c646042"
---

# Debug-026 IOCP实验内存生命周期与队列增长

> **现象**：原始 IOCP 实验虽然只是概念验证，但调试时暴露出失败路径泄漏、`_endthread()` 可能跳过析构，以及仅 Push 导致队列持续增长。
> 主笔记：[[7.6 RemoteCtrl 线程退出、队列增长与内存生命周期|7.6 RemoteCtrl 线程退出、队列增长与内存生命周期]]

---

## 1. 问题摘要

- `PostQueuedCompletionStatus` 失败时，堆上的 `IOCP_PARAM` 没有回收者
- 如果 `_endthread()` 落在拥有 `lstString` 的同一作用域，局部对象析构可能被跳过
- 即使不存在传统意义上的泄漏，只有 Push 没有 Pop 时，队列也会持续增长

---

## 内存生命周期分析

![[图片/SVG/Debug-026-IOCP实验内存生命周期与队列增长-01.svg|757]]


### 正常路径——无泄漏

在测试过的运行里：
- 每个 `IOCP_PARAM` 都会在工作线程处理完成后被 `delete`
- `lstString` 会在 `threadmain` 返回前显式清空
- 回调拿到的是栈地址，不是堆指针，因此不需要 `delete`

### 风险路径 1：PostQueuedCompletionStatus 失败

```cpp
IOCP_PARAM* pParam = new IOCP_PARAM(IocpListPush, "hello", NULL);
PostQueuedCompletionStatus(hIOCP, sizeof(IOCP_PARAM), (ULONG_PTR)pParam, NULL);
// ⚠ 如果 Post 失败，pParam 永远不会被释放——这是典型的失败路径泄漏
```

修复方式（在 `c6fa8056` 中应用）：检查返回值，失败时 `delete pParam`。

### 风险路径 2：_endthread() 跳过析构

如果 `_endthread()` 是在 `threadmain()` **内部**调用的（就像更早的未提交版本那样）：

```cpp
void threadmain_BAD(HANDLE hIOCP)
{
    std::list<std::string> lstString;   // 带有堆节点的局部对象
    // ... loop ...
    _endthread();   // ⚠ 线程在这里死亡——lstString 的析构函数可能不会运行
                    //    链表节点和 std::string 缓冲区会留在堆上
}
```

提交版本里的双函数拆分避免了这个问题：`threadmain` 正常返回（析构触发），然后 `threadQueueEntry` 再调用 `_endthread()`。

### 无界队列增长

即使没有泄漏，当前版本仍然有一个**增长问题**：

- Push 操作会持续向 `lstString` 里加数据
- 在测试路径里根本不会投递 Pop 操作（因为 `_kbhit` 的 bug）
- 因此只要程序一直运行，`lstString` 就会持续增长

这**不是**传统意义上的泄漏——程序关闭时所有内存都会被释放。但如果程序持续跑上几个小时，内存占用会不断上涨。

---

## 3. 回链

- [[7.6 RemoteCtrl 线程退出、队列增长与内存生命周期|7.6 RemoteCtrl 线程退出、队列增长与内存生命周期]]
