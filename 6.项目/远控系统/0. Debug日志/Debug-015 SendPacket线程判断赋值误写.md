---
tags:
  - 项目/远控系统
  - debug
---

# Debug-015 SendPacket 线程判断赋值误写

## 基本信息

| 项目 | 内容 |
|------|------|
| **发现时间** | 2026-03-24 |
| **所在提交** | `34aee5f`（重构网络模块） |
| **所在文件** | `RemoteClient/CClientSocket.cpp` |
| **所在函数** | `CClientSocket::SendPacket` |
| **Bug 类型** | 运算符误用（赋值 vs 比较） |
| **影响** | 每次调用 `SendPacket` 都重新创建网络线程 |

---

## Bug 现象

每次调用 `CClientController::SendCommandPacket()` 发包，都会创建一个新的网络线程（`threadFunc2`），而不是复用已有的线程。多次发包后系统中存在多个网络线程同时运行，争抢同一 socket。

---

## 根因分析

```cpp
bool CClientSocket::SendPacket(HWND hWnd, const CPacket& pack, bool isAutoClosed)
{
    // ❌ Bug：= 是赋值，不是比较
    if (m_hThread = INVALID_HANDLE_VALUE)
    {
        m_hThread = (HANDLE)_beginthreadex(...);
    }
    ...
}
```

**执行过程**：

1. `m_hThread = INVALID_HANDLE_VALUE` 把 `m_hThread` 赋值为 `INVALID_HANDLE_VALUE`（即 `(HANDLE)-1`）
2. `if` 对赋值结果（`-1`，非零）求布尔值 → **始终为 `true`**
3. 每次调用 `SendPacket` 都执行 `_beginthreadex`，创建新线程

**正确写法**：

```cpp
// ✅ 正确：== 才是比较
if (m_hThread == INVALID_HANDLE_VALUE)
{
    m_hThread = (HANDLE)_beginthreadex(...);
}
```

---

## 后果

| 影响 | 描述 |
|------|------|
| 多线程竞争 | 多条 `threadFunc2` 同时运行，竞争取同一消息队列的消息 |
| 多个 socket | 每个线程都调用 `InitSocket()`，创建多个并发 socket |
| 泄漏 | 旧线程的 `m_hThread` 句柄被覆盖，无法 `CloseHandle`，线程句柄泄漏 |

---

## 经验总结

**C/C++ 常见陷阱**：

- `if (x = value)` 是赋值表达式的布尔值，几乎永远为 `true`（`value` 非零时）
- 应写为 `if (x == value)`
- 开启编译器警告 `/W4` 或 `-Wall` 可以在编译阶段捕获此类赋值在条件中出现的警告

**预防方法**：

```cpp
// 防御性写法（Yoda Condition）：把常量放左侧，编译器会对 = 报错
if (INVALID_HANDLE_VALUE == m_hThread)  // 如果误写为 = ，编译器报错
```

---

## 关联

- [[6.5 重构网络模块（线程事件机制→消息机制）]] — Bug 所在提交的完整笔记
- [[6.8 消息机制补强：线程启动同步与ACK泄漏修复]] — 后续版本中通过“构造阶段提前启动线程”把这条错误路径整体移除
