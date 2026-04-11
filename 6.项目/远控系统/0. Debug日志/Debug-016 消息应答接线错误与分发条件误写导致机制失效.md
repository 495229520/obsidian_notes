---
tags:
  - 项目/远控系统
  - debug
---

# Debug-016 消息应答接线错误与分发条件误写导致机制失效

> **现象**：提交 `9b1f6ea` 虽然新增了 `WM_SEND_PACK_ACK` 的窗口处理函数，但盘符、文件树、下载、截图等 ACK 仍然不能按命令正确落到各自逻辑，消息机制在窗口消费层继续失效。  
> **根因**：ACK 回调层同时出现了两类接线错误：`RemoteClientDlg` 的 `ON_MESSAGE` 绑定到了错误的类成员函数；`OnSendPackAck()` 内又把 `switch` 条件写成了 `pPacket != NULL`，导致分发依据退化成布尔值。

---

## Bug 基本信息

| 项目 | 内容 |
|------|------|
| **编号** | Debug-016 |
| **严重程度** | 🔴 功能失效 |
| **分类** | 消息分发 + 语法陷阱 |
| **commit** | `9b1f6ea` |
| **出现场景** | 将网络通讯从事件机制继续迁移到消息机制，开始在 `CWatchDialog` / `CRemoteClientDlg` 中处理 `WM_SEND_PACK_ACK` |

---

## 现象描述

1. 这次提交已经补上了 `OnSendPackAck()`，按设计应当由窗口直接消费 ACK。
2. 但从代码审查可以看出，盘符、文件树、下载、截图这几条消息分支并不会按预期命中。
3. 更迷惑的是：主窗口中的 `case 1` 看起来“像是有机会工作”，而 `case 2/4/1981`、监视窗口的 `case 5/6` 则明显跑不通，属于一种**部分表面正常、整体逻辑失效**的典型重构陷阱。

---

## 调试过程

### 第一步：先看消息有没有被“声明出来”

从 diff 看，这次提交确实做了两件“看上去已经闭环”的事：

- `CWatchDialog` 增加了 `ON_MESSAGE(WM_SEND_PACK_ACK, &CWatchDialog::OnSendPackAck)`
- `RemoteClientDlg` 也新增了 `OnSendPackAck()`

所以问题不再是“窗口层没有 handler”，而是“handler 有没有接对、分派条件有没有写对”。

### 第二步：检查主窗口的消息映射

```cpp
BEGIN_MESSAGE_MAP(CRemoteClientDlg, CDialogEx)
    ...
    ON_MESSAGE(WM_SEND_PACK_ACK, &CWatchDialog::OnSendPackAck)
END_MESSAGE_MAP()
```

这里立刻能看到第一个问题：

- 当前类是 `CRemoteClientDlg`
- 但 `ON_MESSAGE` 却绑定到了 `CWatchDialog::OnSendPackAck`

也就是说，主窗口明明自己写了 `OnSendPackAck()`，消息却没有接到自己这里。

### 第三步：检查真正的分发条件

主窗口和监视窗口都写了类似代码：

```cpp
CPacket* pPacket = (CPacket*)wParam;
if (pPacket != NULL)
{
    switch (pPacket != NULL)
    {
    case 1:
    case 2:
    case 4:
        ...
    }
}
```

这里的 `switch (pPacket != NULL)` 只可能得到：

- `0`：空指针
- `1`：非空指针

这意味着：

- `RemoteClientDlg` 里只有 `case 1` 有可能误打误撞被执行
- `case 2`、`case 4`、`case 1981` 等分支永远不会命中
- `CWatchDialog` 里的 `case 5` / `case 6` 也永远不会命中

到这里，根因已经完全锁定：**ACK 不是没回来，而是回到窗口后根本没有按命令号被正确分发。**

---

## 根因分析

### 根因 1：主窗口消息映射接错类

**问题代码**：

```cpp
// ❌ 当前类是 CRemoteClientDlg，却绑定到了 CWatchDialog 的 handler
ON_MESSAGE(WM_SEND_PACK_ACK, &CWatchDialog::OnSendPackAck)
```

**正确写法**：

```cpp
// ✅ 应绑定当前类自己的消息处理函数
ON_MESSAGE(WM_SEND_PACK_ACK, &CRemoteClientDlg::OnSendPackAck)
```

**为什么这会出问题**：

1. `WM_SEND_PACK_ACK` 是发给主窗口 `HWND` 的
2. MFC 要靠 `ON_MESSAGE` 找到当前窗口的成员函数
3. 这里把消息接到了错误的类上，主窗口自己写的 `OnSendPackAck()` 形同虚设

### 根因 2：按布尔值而不是命令号做分发

**问题代码**：

```cpp
// ❌ 这里分发依据不是命令号，而是“指针是否非空”
switch (pPacket != NULL)
{
case 1:
    ...
case 2:
    ...
case 4:
    ...
}
```

**正确写法**：

```cpp
// ✅ 应该按 CPacket 的命令号分发
switch (pPacket->sCmd)
{
case 1:
    ...
case 2:
    ...
case 4:
    ...
}
```

**为什么这段代码会出问题**：

```text
ACK 到达窗口
  -> pPacket 非空
  -> (pPacket != NULL) 等于 1
  -> switch(1)
     -> 主窗口只有 case 1 可能被命中
     -> 监视窗口的 case 5/6 永远不会命中
```

也就是说，这个 Bug 非常“狡猾”：

- `cmd=1` 可能看起来像是“部分工作”
- 但实际上只是因为 `true == 1` 恰好撞上了 `case 1`
- 一旦命令号不是 1，整个消息机制就会失效

---

## 修复方案

```cpp
// ✅ RemoteClientDlg 的消息映射
BEGIN_MESSAGE_MAP(CRemoteClientDlg, CDialogEx)
    ...
    ON_MESSAGE(WM_SEND_PACK_ACK, &CRemoteClientDlg::OnSendPackAck)
END_MESSAGE_MAP()

// ✅ 正确按命令号分发
LRESULT CRemoteClientDlg::OnSendPackAck(WPARAM wParam, LPARAM lParam)
{
    CPacket* pPacket = (CPacket*)wParam;
    if (pPacket == NULL)
        return 0;

    switch (pPacket->sCmd)
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

    delete pPacket;
    return 0;
}
```

**关键点**：

- `ON_MESSAGE` 必须接当前窗口自己的成员函数
- 分发依据必须是 `CPacket::sCmd`
- 处理完 `new CPacket(pack)` 之后要记得 `delete`

---

## 修复效果

```text
修复前：
  ACK 到达窗口
    -> 主窗口消息接错 handler
    -> 或 switch(true) 只命中 case 1
    -> 文件树/下载/截图等分支失效

修复后：
  ACK 到达窗口
    -> 正确进入当前窗口的 OnSendPackAck
    -> 按 sCmd 分发到 cmd=1/2/4/5/6/...
    -> Tree/List/Image/下载状态各自走到正确逻辑
```

---

## 调试经验

| 经验 | 说明 |
|------|------|
| **教训 1** | MFC 中“写了 handler”不等于“消息真的接到了 handler”，必须先检查 `BEGIN_MESSAGE_MAP` 是否接到正确类 |
| **教训 2** | `switch(expr)` 里的 `expr` 如果退化成布尔值，往往会制造“只有某个 case 偶然可用”的假象，比完全报错更难发现 |
| **教训 3** | 消息机制重构时，最容易错的不是 send/recv，而是 ACK 回到窗口之后的接线、分派条件和对象生命周期 |

---

## 关联笔记

- [[6.7 消息机制闭环：窗口回调与上下文透传]] — 本次提交的主版本笔记
- [[Debug-015 SendPacket线程判断赋值误写]] — 同一阶段仍然残留的另一处消息机制问题
- [[6.8 消息机制补强：线程启动同步与ACK泄漏修复]] — 后续提交中修正了 `ON_MESSAGE` 接线与 `switch(head.sCmd)` 分发逻辑

---

## 更新记录

| 日期 | 变更 |
|------|------|
| 2026-03-24 | 初始版本：基于提交 `9b1f6ea` 记录 `WM_SEND_PACK_ACK` 接线错误与命令分发条件误写 |
