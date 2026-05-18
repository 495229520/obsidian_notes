---
tags:
  - 项目/远控系统
---

# Debug-022 文件树目录节点插入后未自动展开

> 归属提交：`71203ae`（2026-03-27）  
> 关联笔记：[[6.12 文件树展示与下载缓冲区双 Bug 修复]]

---

## 现象

点击文件树中的目录节点展开后，远端文件夹列表通过 ACK 正常返回，`TRACE` 也能看到 `hasnext=1 isdirectory=1 dirname`——但目录节点的**子节点就是不出现**，界面上节点依然是折叠的小三角。

---

## 根本原因

```cpp
// ❌ 旧代码：只插入子节点，从不展开父节点
HTREEITEM hTemp = m_Tree.InsertItem(pInfo->szFileName, (HTREEITEM)lParam, TVI_LAST);
m_Tree.InsertItem("", hTemp, TVI_LAST);
// 没有 Expand！
```

**MFC `CTreeCtrl` 的展开行为不是自动的**：向一个节点插入子项，节点本身的展开状态不会改变。用户必须手动点击那个小三角，才能触发展开。  
程序异步收包、异步插入节点，整个过程 UI 不感知"刚插入了东西"，父节点永远保持折叠。

---

## 修复方案

```cpp
// ✅ 新代码：插入子节点后立即展开父节点
HTREEITEM hTemp = m_Tree.InsertItem(pInfo->szFileName, (HTREEITEM)lParam, TVI_LAST);
m_Tree.InsertItem("", hTemp, TVI_LAST);
m_Tree.Expand((HTREEITEM)lParam, TVE_EXPAND);  // ← 每收到一条目录条目就刷新一次展开状态
```

> 📁 `RemoteClient/RemoteClientDlg.cpp` : `OnSendPackAck` case 2（行 456-458）

`CTreeCtrl::Expand` 的签名：

```cpp
BOOL CTreeCtrl::Expand(HTREEITEM hItem, UINT nCode);
```

| 参数 | 说明 |
|------|------|
| `hItem` | 要展开的父节点句柄；这里是 `lParam`（`SendCommandPacket` 时传入的 `HTREEITEM`） |
| `nCode` | `TVE_EXPAND`：展开；`TVE_COLLAPSE`：折叠；`TVE_TOGGLE`：切换 |

---

## 为什么每个 ACK 都调一次 Expand？

文件列表是**多包分批**返回的，每个 ACK 携带一条 `FILEINFO`。只在第一条或最后一条调用 `Expand` 都可能出现时序问题（第一条来了就展开，用户还没加载完；最后一条不知道是哪条）。

每收到一条目录条目就调一次 `Expand` 是最安全的写法：
- 第一条到来 → 展开（子节点出现在界面上）
- 后续条目到来 → 重复展开同一节点（已展开状态下 `TVE_EXPAND` 是幂等的，无副作用）

---

## 易错点

> [!warning] `InsertItem` 不等于"显示出来"

- `InsertItem` 只是把节点加入树的数据结构
- **只有父节点处于展开状态，子节点才会在 UI 上可见**
- 异步插入场景（收 ACK → 插入）必须手动调 `Expand`，不能假设 UI 会自动刷新

---

## 更新记录

| 日期 | 变更 |
|------|------|
| 2026-03-27 | 初始版本 |
