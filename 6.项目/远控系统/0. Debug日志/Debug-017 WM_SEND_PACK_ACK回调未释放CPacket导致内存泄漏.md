---
tags:
  - 项目/远控系统
  - debug
---

# Debug-017 WM_SEND_PACK_ACK回调未释放CPacket导致内存泄漏

> **现象**：消息机制改造后，网络线程每成功收到一个响应包，就会 `new CPacket(pack)` 再通过 `WM_SEND_PACK_ACK` 发送给窗口；如果窗口处理完不释放，这个对象就会按“每包一次”的速度持续泄漏。  
> **根因**：跨线程消息只传递了数据，却没有把“谁负责 delete”这件事收口，导致 `new` 出来的 `CPacket` 在成功路径上无人回收。

---

## Bug 基本信息

| 项目 | 内容 |
|------|------|
| **编号** | Debug-017 |
| **严重程度** | 🟡 内存泄漏 |
| **分类** | 资源管理 / 所有权约定 |
| **commit** | `f8d695b` |
| **出现场景** | `CClientSocket::SendPack()` 成功收包后，通过 `SendMessage(hWnd, WM_SEND_PACK_ACK, new CPacket(pack), ...)` 把数据交给窗口 |

---

## 现象描述

1. 消息机制改造后，窗口已经不再从共享 `m_packet` 里取结果，而是直接接收 `WM_SEND_PACK_ACK`。
2. 网络线程在成功路径上每收到一个完整包，都会执行一次 `new CPacket(pack)`。
3. 如果窗口处理函数只用这个指针，不释放它，那么文件树、下载、监控这类多包/高频场景下，内存会一点点往上涨。

这类问题最麻烦的地方在于：

- 短时间测试不一定能看出来
- 功能表面可能完全正常
- 但长时间运行后，内存占用会不断变大

---

## 调试过程

### 第一步：先看对象是谁创建的

成功路径在 `CClientSocket::SendPack()` 里：

```cpp
::SendMessage(hWnd, WM_SEND_PACK_ACK, (WPARAM)new CPacket(pack), data.wParam);
```

这说明 `CPacket` 的堆对象来自网络线程。

### 第二步：再看接收方有没有释放

上一版主窗口的写法大致是：

```cpp
CPacket* pPacket = (CPacket*)wParam;
if (pPacket != NULL)
{
    CPacket& head = *pPacket;
    ...
}
```

这里只有“取出并使用”，没有任何 `delete pPacket`。

### 第三步：确认这不是单次小问题，而是“按包泄漏”

因为 `WM_SEND_PACK_ACK` 的成功路径是按包触发的，所以泄漏速度不是“每次打开窗口一次”，而是：

```text
收到 1 个包 -> 泄漏 1 个 CPacket
收到 N 个包 -> 泄漏 N 个 CPacket
```

所以文件树遍历、下载、截图这种连续收包场景，都会把这个问题迅速放大。

---

## 根因分析

**问题代码**：

```cpp
// 网络线程
::SendMessage(hWnd, WM_SEND_PACK_ACK, (WPARAM)new CPacket(pack), data.wParam);

// 窗口线程
CPacket* pPacket = (CPacket*)wParam;
if (pPacket != NULL)
{
    CPacket& head = *pPacket;
    // ... 只使用，不释放
}
```

**为什么这会泄漏**：

```text
网络线程 new CPacket
  -> 把指针塞进 wParam
  -> 窗口线程收到后使用
  -> 没有人 delete
  -> 成功路径每来一包，泄漏一包
```

这本质上不是“忘了写一行 delete”那么简单，而是一个**所有权约定缺失**的问题：

- 网络线程以为：我把对象交出去了
- 窗口线程以为：我只是拿来看看

当双方都没有明确承担“最终释放者”的角色时，泄漏就发生了。

---

## 修复方案

```cpp
LRESULT CRemoteClientDlg::OnSendPackAck(WPARAM wParam, LPARAM lParam)
{
    ...
    if (wParam != NULL)
    {
        // 先拷贝到栈对象，马上释放堆对象
        CPacket head = *(CPacket*)wParam;
        delete (CPacket*)wParam;

        switch (head.sCmd)
        {
        case 1:
            ...
            break;
        case 2:
            ...
            break;
        case 4:
            ...
            break;
        }
    }
    return 0;
}
```

**关键点**：

- 先把内容拷贝出来，再 `delete`
- 后续逻辑只操作栈上的 `head`
- 这样不需要在每个 `case` 里都记着释放一次，最不容易漏

---

## 修复效果

```text
修复前：
  网络线程 new CPacket
    -> 窗口只读不删
    -> 每个成功包都泄漏一份堆对象

修复后：
  网络线程 new CPacket
    -> 窗口入口立即复制到栈对象
    -> 立刻 delete 原始堆对象
    -> 后续业务逻辑不再持有悬空所有权
```

---

## 底层原理

### Win32 消息传指针，本质上是在“手动传递所有权”

像下面这种写法：

```cpp
SendMessage(hWnd, WM_SEND_PACK_ACK, (WPARAM)new CPacket(pack), ...);
```

从语法上看只是“把一个指针塞进 `WPARAM`”，但从资源管理角度看，它已经隐含了一个协议：

- 发送方创建对象
- 接收方成为新所有者
- 接收方负责释放

如果这个协议没有在代码里写清楚，问题不会体现在编译期，而是会体现在运行一段时间后的内存曲线上。

---

## 调试经验

| 经验 | 说明 |
|------|------|
| **教训 1** | 通过 `WPARAM/LPARAM` 传指针时，必须第一时间想清楚对象最后由谁释放 |
| **教训 2** | 对于多分支消息处理函数，最好在入口统一回收资源，不要分散到各个 `case` 里赌自己不会漏 |
| **教训 3** | “功能能跑”不等于“资源正确”，高频路径尤其要检查 `new/delete` 是否闭环 |

---

## 关联笔记

- [[6.8 消息机制补强：线程启动同步与ACK泄漏修复]] — 本次修复所在的版本记录
- [[Debug-016 消息应答接线错误与分发条件误写导致机制失效]] — 同一阶段一起修补的 ACK 回调问题

---

## 更新记录

| 日期 | 变更 |
|------|------|
| 2026-03-24 | 初始版本：基于提交 `f8d695b` 记录 `WM_SEND_PACK_ACK` 成功路径上的 `CPacket` 堆对象泄漏 |
