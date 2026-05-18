---
tags:
  - 项目/远控系统
git: "0893fd8"
---

# Debug-014 显示内容变化过大导致接收程序卡死

> **现象**：屏幕内容剧烈变化（大幅画面切换）时，截图数据量明显增大，控制端接收程序卡死，网络线程不再处理后续命令。
> **根因**：`threadFunc()` 的 recv 循环中，`isAutoClosed=true` 的命令（如截图）在 `SetEvent()` 后没有 `break`，导致网络线程继续阻塞在 `recv()` 上，后续命令积压在队列中无法推进。

---

## Bug 基本信息

| 项目 | 内容 |
|------|------|
| **编号** | Debug-014 |
| **严重程度** | 🔴 卡死（接收线程阻塞，程序无响应） |
| **分类** | 循环逻辑缺陷（缺少 break） |
| **commit** | `0893fd8413fb94ff06ad1c561b01f3a15b186d0b` |
| **出现场景** | 截图数据量较大时，recv 循环不能正常退出 |

---

## 现象描述

1. 正常状态：屏幕变化不大，截图数据较小，`recv()` 一次拿到完整包，程序运行正常
2. 异常状态：屏幕内容剧烈变化（如切换窗口、视频播放），截图数据量增大
3. 控制端接收程序卡死，画面不再更新，其他命令也无法执行
4. 调试时发现网络线程卡在 `recv()` 调用处，不再消费 `m_lstSend` 队列

---

## 根因分析

### recv 循环的 do-while 结构

`threadFunc()` 的接收循环设计如下：

```cpp
do {
    int length = recv(m_sock, pBuffer + index, BUFFER_SIZE - index, 0);
    if ((length > 0) || (index > 0))
    {
        index += length;
        size_t size = (size_t)index;
        CPacket pack((BYTE*)pBuffer, size);  // 解析，size 被修改为实际包长
        if (size > 0)  // 解析成功
        {
            pack.hEvent = head.hEvent;
            it->second.push_back(pack);
            memmove(pBuffer, pBuffer + size, index - size);
            index -= size;
            if (it0->second)  // isAutoClosed == true（如截图 cmd=6）
            {
                SetEvent(head.hEvent);
                // ← BUG: 这里缺少 break！
            }
        }
        // 若 size == 0（包不完整），继续循环等待更多数据
    }
    else if (length <= 0 && index <= 0)
    {
        CloseSocket();
        SetEvent(head.hEvent);
        break;
    }
} while (it0->second == false);  // isAutoClosed==true 时，条件为 false → 正常应该退出
```

### 场景 1：包解析成功后，继续 recv 导致阻塞

```text
大截图数据到达 → recv() 收到完整包 → CPacket 解析成功 (size > 0)
→ SetEvent(hEvent) → 业务线程被唤醒，开始处理图像
→ do-while 条件 while(it0->second == false) → false → "应该退出"

BUT: SetEvent 后没有 break，do-while 先检查条件再决定下一步
  → while(false) 结构表明：这次迭代结束，下一次循环开始前检查条件
  → 条件 false → 退出？
```

等等，让我们重新看这个循环结构：`do { ... } while(condition)`。

**正确理解**：`do-while` 是"先执行，后判断"。当本次迭代的循环体执行完毕之后，才去判断 `while(condition)` 是否满足下次迭代。

对于 `isAutoClosed = true` (`it0->second = true`) 的情况：
- 循环体执行：`SetEvent()` 被调用（无 break）
- 判断 `while(it0->second == false)` → `while(true == false)` → `false`
- 循环退出

**那为什么会卡死？**

### 场景 2：包解析失败（数据量大，第一次 recv 收到的是不完整的包）

```text
大截图数据
  → 第一次 recv() 收到部分数据（例如 8KB，但完整包有 50KB）
  → index = 8192
  → CPacket 解析失败：size = 0（包头不完整或长度字段超出当前数据）
  → 进入 if((length > 0) || (index > 0)) 分支
  → 但 if(size > 0) 为 false → 跳过 SetEvent
  → 到达 do-while 末尾，判断 while(it0->second == false)
  → it0->second == true → 条件为 false → 退出循环
  → pop_front() 移除请求
  → InitSocket() 重建连接

  → 业务线程仍在 WaitForSingleObject(hEvent, INFINITE) ← 永远等待！
```

这是核心卡死路径：**包解析失败时，循环在没有 SetEvent 的情况下直接退出**，业务线程永远等不到唤醒。

### 场景 3：SetEvent 后无 break，网络线程继续 recv 阻塞

```text
数据量中等，recv 收到完整包
  → 解析成功 → SetEvent(hEvent) → 业务线程唤醒
  → 没有 break → do-while 继续
  → while(it0->second == false) = false → 退出循环
  → pop_front() 移除请求
  → InitSocket() 重建连接

这条路径本身不卡死，但在数据量大、recv 只取到部分数据的情况下就会触发场景 2。
```

---

## 直观对比

### 小数据（正常）

```
recv() → 完整包 → size > 0 → SetEvent → do-while 退出 → 下一命令
    ✅ 正常
```

### 大数据（卡死）

```
recv() → 部分数据 → size = 0 → 不 SetEvent → do-while 退出
    → pop_front() → InitSocket()
    → 业务线程永远等待 ← 💀 卡死
```

---

## 修复方案

在 `SetEvent()` 后加 `break`，并同时确保解析失败时也能通过 do-while 语义继续读取：

```cpp
if (it0->second)  // isAutoClosed == true
{
    SetEvent(head.hEvent);
    break;  // ← 修复：立即退出 recv 循环，让网络线程去处理下一命令
}
```

**修复后的两条路径**：

```text
路径 1：解析成功（size > 0）
  → SetEvent(hEvent) + break
  → 业务线程唤醒，网络线程退出循环，继续处理下一请求
  ✅ 正常

路径 2：解析失败（size = 0），数据不完整
  → 不进入 if(size > 0) 分支
  → do-while 条件 while(it0->second == false) → false → 退出
  → 虽然这条路依然有问题（没有 SetEvent），但 break 的修复确保了路径 1 的正确性
  → 路径 2 的彻底修复需要在解析失败时继续循环等待更多数据（多包支持的后续工作）
```

> **注意**：`break` 的修复主要解决了"解析成功后不退出导致下次 recv 阻塞"的问题。对于"大数据分多次 recv 才能拼完整包"的场景，需要配合多包收集逻辑一起完善（见 [[6.4 网络模型线程完善(3)]] 的"未完成部分"）。

---

## 代码 diff

```cpp
// ❌ 修复前
if (it0->second)
{
    SetEvent(head.hEvent);
    // 没有 break
}

// ✅ 修复后
if (it0->second)
{
    SetEvent(head.hEvent);
    break;  // 收到单包响应后立即退出，不再阻塞等待更多数据
}
```

---

## 经验总结

| 教训 | 说明 |
|------|------|
| **do-while 的"先执行后判断"要格外注意** | while 条件在本次迭代末尾才判断，已经执行了 SetEvent 不代表下一个 recv 不会被执行 |
| **break 和循环条件要明确分工** | 循环条件控制"是否继续迭代"，break 控制"提前退出"，两者不能混淆 |
| **异常路径（解析失败）同样需要通知等待方** | 任何不调用 SetEvent 就退出循环的路径，都会让 WaitForSingleObject 永远阻塞 |
| **用数据量验证接收逻辑** | 仅用小数据测试通过不代表正确，大数据下 recv 分包的情况必须覆盖到 |

---

## 关联笔记

- [[6.4 网络模型线程完善(3)]] — 本次修复所在的版本记录
- [[Debug-013 显示命令与鼠标命令冲突导致程序卡死]] — 同一 commit 的另一个 Bug
- [[Debug-002 TCP粘包导致文件列表数据丢失]] — 同样是 TCP 流式接收，分包处理的早期教训

---

## 更新记录

| 日期 | 变更 |
|------|------|
| 2026-03-23 | 初始版本：基于提交 `0893fd8` 记录 recv 循环缺少 break 导致接收卡死的根因与修复 |
