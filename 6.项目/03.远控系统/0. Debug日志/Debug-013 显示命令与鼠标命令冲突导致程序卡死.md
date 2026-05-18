---
tags:
  - 项目/远控系统
git: "0893fd8"
---

# Debug-013 显示命令与鼠标命令冲突导致程序卡死

> **现象**：截图监控线程（`cmd=6`）与鼠标控制命令并发发送时，程序出现卡死（线程互相死等，队列无法推进）。
> **根因**：`SendPacket()` 只用 `m_sock == INVALID_SOCKET` 判断是否启动网络线程，而每次请求完成后 socket 都会被关闭并重建，导致多个业务线程同时启动多条网络线程，且共享队列/映射没有互斥保护。

---

## Bug 基本信息

| 项目 | 内容 |
|------|------|
| **编号** | Debug-013 |
| **严重程度** | 🔴 卡死（程序无响应） |
| **分类** | 多线程竞争 + 无互斥保护 |
| **commit** | `0893fd8413fb94ff06ad1c561b01f3a15b186d0b` |
| **出现场景** | 截图监控与鼠标命令同时发送 |

---

## 现象描述

1. 控制端启动屏幕监控，`threadWatchScreen()` 周期性发送 `cmd=6` 截图请求
2. 同时用户操作鼠标，发送鼠标控制命令
3. 程序卡死，既不刷新画面，也不响应操作
4. `TRACE` 输出显示 `WaitForSingleObject` 不再返回，命令积压在队列中

---

## 根因分析

### 1. 每次请求完成后 socket 被主动关闭

`threadFunc()` 在每次处理完一个请求后，会关闭连接并重新建立：

```cpp
// threadFunc 末尾
m_lstSend.pop_front();
if (InitSocket() == false)
{
    InitSocket();
}
```

`InitSocket()` 的第一步就是 `CloseSocket()`：

```cpp
bool CClientSocket::InitSocket()
{
    if (m_sock != INVALID_SOCKET)
        CloseSocket();  // 先关掉旧连接
    m_sock = socket(PF_INET, SOCK_STREAM, 0);
    // ...
}
```

这意味着：**每次处理完一个命令后，`m_sock` 会短暂地变成 `INVALID_SOCKET`，然后再重建。**

### 2. `SendPacket()` 只检查 socket，不检查线程是否已存在

旧代码：

```cpp
bool CClientSocket::SendPacket(const CPacket& pack, ...)
{
    if (m_sock == INVALID_SOCKET)              // ← 只判断 socket
    {
        _beginthread(&CClientSocket::threadEntry, 0, this);  // ← 启动线程但不保存句柄
    }
    m_lstSend.push_back(pack);
    m_lstSend.push_back(pack);  // ← BUG: 重复 push_back，每个命令入队两次！
    WaitForSingleObject(pack.hEvent, INFINITE);
    ...
}
```

**竞争场景**：

```text
t0: 截图线程完成上次请求 → threadFunc 调用 InitSocket() → m_sock = INVALID_SOCKET（正在重建）
t1: 截图线程再次调用 SendPacket(cmd=6)
    → 检查 m_sock == INVALID_SOCKET → 启动新的网络线程 A
t2: 鼠标线程也调用 SendPacket(鼠标命令)
    → 检查 m_sock == INVALID_SOCKET（还在重建中）→ 又启动网络线程 B

结果：线程 A 和线程 B 同时运行
  → 两个线程都在操作 m_lstSend、m_mapAck
  → 无锁竞争 → 迭代器失效、数据被重复消费 → WaitForSingleObject 永不返回
```

### 3. 重复 push_back 导致命令被处理两次

```cpp
m_lstSend.push_back(pack);  // 第一次入队
m_lstSend.push_back(pack);  // 第二次入队（复制粘贴遗留的 BUG）
```

同一个 `hEvent` 关联的请求被入队两次，网络线程处理第一个时调用 `SetEvent(hEvent)` 唤醒业务线程，业务线程继续清理 `m_mapAck`，网络线程再处理第二个时 `m_mapAck.find(hEvent)` 已找不到对应条目，行为未定义。

### 4. 共享容器没有互斥保护

旧代码中，`m_lstSend` 和 `m_mapAck` 可被多个线程并发写入，没有任何 `CRITICAL_SECTION`/`mutex` 保护：

```
业务线程 A: m_lstSend.push_back(pack)
业务线程 B: m_lstSend.push_back(pack)   ← 同时写入
网络线程:   m_lstSend.front() + pop_front()  ← 同时读取/删除
```

`std::list` 本身不是线程安全的，并发修改会造成链表节点损坏。

---

## 修复方案

### 修复 1：保存线程句柄，同时检查 socket 和线程

```cpp
// ✅ 修复后
bool CClientSocket::SendPacket(const CPacket& pack, std::list<CPacket>& lstPacks, bool isAutoClosed)
{
    // 只有 socket 无效 AND 线程也不存在时，才启动新线程
    if (m_sock == INVALID_SOCKET && m_hThread == INVALID_HANDLE_VALUE)
    {
        m_hThread = (HANDLE)_beginthread(&CClientSocket::threadEntry, 0, this);
        TRACE("start thread\r\n");
    }
    // ...
}
```

**关键点**：
- `m_hThread` 保存线程句柄，初始化为 `INVALID_HANDLE_VALUE`
- 双重判断：socket 无效**且**线程不存在，才启动新线程
- 重建 socket 期间（`m_sock == INVALID_SOCKET`），只要线程还存在（`m_hThread != INVALID_HANDLE_VALUE`），就不会重复启动

### 修复 2：用互斥锁保护共享容器

```cpp
bool CClientSocket::SendPacket(const CPacket& pack, std::list<CPacket>& lstPacks, bool isAutoClosed)
{
    if (m_sock == INVALID_SOCKET && m_hThread == INVALID_HANDLE_VALUE)
    {
        m_hThread = (HANDLE)_beginthread(&CClientSocket::threadEntry, 0, this);
    }
    m_lock.lock();   // ← 加锁
    auto pr = m_mapAck.insert(std::pair<HANDLE, std::list<CPacket>&>(pack.hEvent, lstPacks));
    m_mapAutoClosed.insert(std::pair<HANDLE, bool>(pack.hEvent, isAutoClosed));
    m_lstSend.push_back(pack);   // ← 只入队一次（修复了重复 push_back）
    m_lock.unlock(); // ← 解锁
    WaitForSingleObject(pack.hEvent, INFINITE);
    ...
}
```

`threadFunc()` 中也加锁保护队列操作：

```cpp
m_lock.lock();
CPacket& head = m_lstSend.front();  // 读取队首
m_lock.unlock();

// ... send + recv ...

m_lock.lock();
m_lstSend.pop_front();  // 出队
m_lock.unlock();
```

### 修复 3：删除重复入队

```cpp
// ❌ 修复前（两次 push_back）
m_lstSend.push_back(pack);
m_lstSend.push_back(pack);

// ✅ 修复后（只入队一次，在锁内）
m_lock.lock();
m_lstSend.push_back(pack);
m_lock.unlock();
```

---

## 修复效果

```text
修复前：
  截图线程 + 鼠标线程同时发包
    → 两者都看到 m_sock == INVALID_SOCKET
    → 各自启动一条网络线程
    → 两个线程并发操作无锁队列
    → 程序卡死

修复后：
  截图线程发包 → 启动线程 A，保存 m_hThread
  鼠标线程发包 → m_hThread != INVALID_HANDLE_VALUE → 不启动新线程
    → 只有线程 A 在跑，顺序消费队列
    → 不会卡死
```

---

## 经验总结

| 教训 | 说明 |
|------|------|
| **线程"存不存在"不等于"socket 是否有效"** | socket 在两次请求间会被关闭重建，不能用 socket 状态代替线程状态判断 |
| **保存线程句柄是必须的** | `_beginthread` 的返回值必须保存，否则无法判断线程是否还在运行 |
| **任何被多线程共享的容器都需要锁** | `std::list` / `std::map` 不是线程安全的，并发读写必须加互斥保护 |
| **重复操作要靠代码审查发现** | 重复的 `push_back` 在编译时没有任何提示，只能靠 code review 和日志排查 |

---

## 关联笔记

- [[6.4 网络模型线程完善(3)]] — 本次修复所在的版本记录
- [[6.4 网络模型线程完善(2)]] — SendPacket 接口初次出现，但没有锁保护
- [[Debug-014 显示内容变化过大导致接收程序卡死]] — 同一 commit 的另一个 Bug

---

## 更新记录

| 日期 | 变更 |
|------|------|
| 2026-03-23 | 初始版本：基于提交 `0893fd8` 记录多线程竞争导致卡死的根因与修复 |
