---
tags:
  - 项目/远控系统
  - debug
git: "8d2e3c3"
---

# Debug-018 CWatchDialog ACK 条件误判与 isFull 状态断链

**严重程度**：🔴 功能失效（远程画面黑屏）
**分类**：消息分发 + 状态机断链 + 资源管理
**发现时间**：2026-03-25

---

## 现象

打开远程监控窗口（`CWatchDialog`），画面全黑。截图请求正常发出，服务端正常响应，消息也正常到达 `CWatchDialog::OnSendPackAck`，但图像始终不渲染。

---

## 根因

`CWatchDialog::OnSendPackAck` 里存在四处同时生效的错误：

### 错误 1：switch 条件是布尔值

```cpp
// ❌
switch (pPacket != NULL)   // 布尔值，只有 case 0 / case 1 有效
{
case 5:   // 永远命中不到
case 6:   // 永远命中不到
```

### 错误 2：case 5 无 break

```cpp
// ❌
case 5:    // 没有 break，fall-through 进 case 6
case 6:
{
    // 鼠标 ACK 和截图 ACK 逻辑混用
}
```

### 错误 3：m_isFull 永远是 false（主因）

```cpp
// ❌ OnSendPackAck
case 6:
{
    if (m_isFull)   // 永远是 false，渲染代码永远跳过
    {
        // 渲染截图
        m_isFull = false;
    }
}

// ❌ threadWatchScreen
int ret = SendCommandPacket(...);   // 返回 bool（0 或 1）
if (ret == 6)                       // bool 最大值是 1，永假
{
    m_watchDlg.SetImageStatus(true);   // 从未执行
}
// 结果：m_isFull 永远是 false，渲染代码永远跳过
```

**状态机断链**：设置 `m_isFull = true` 的路径因为类型判断错误从来没有走到，`m_isFull` 一直是构造函数初始化的 `false`。

### 错误 4：CPacket 堆对象泄漏

```cpp
// ❌
CPacket* pPacket = (CPacket*)wParam;
// 用完之后没有 delete pPacket，每个成功响应泄漏一个 CPacket
```

---

## 修复

```cpp
// ✅ 修复后
CPacket head = *(CPacket*)wParam;   // 拷贝到栈
delete (CPacket*)wParam;            // 入口处统一回收
switch (head.sCmd)                  // 命令号分发
{
case 5:
    TRACE("mouse event ack\r\n");
    break;
case 6:
{
    // 直接渲染，移除 if (m_isFull) 条件
    CEdoyunTool::Bytes2Image(m_image, head.strData);
    ...
    break;
}
```

同时在 `threadWatchScreen` 里移除旧的 `if (ret == 6)` 图像处理逻辑，改为纯定时发请求 + 200ms 帧率控制。

---

## 教训

| 教训 | 说明 |
|------|------|
| 修 bug 后要搜索同名函数 | `OnSendPackAck` 在两个类里各有一份，只修了 `RemoteClientDlg` |
| bool 不要和大整数比较 | `ret == 6` 永假但编译不报错 |
| 状态机调试要追踪每个写入路径 | `m_isFull` 的写入路径断了，静默失效，无报错 |

---

## 关联

- [[6.10 远程显示修复：ACK 陷阱复现与帧率控制]]
- [[Bug目录/6.10-Bug-01 CWatchDialog ACK三连错与isFull状态陷阱]]
- [[Debug-016 消息应答接线错误与分发条件误写导致机制失效]] — 同款 bug 在 RemoteClientDlg 的日志
