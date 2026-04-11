---
tags:
  - 项目/远控系统
---

# Debug-009：树控件未设置选中状态导致路径错误

> **日期**：2026-02-10
> **影响范围**：文件浏览（树控件 → 列表展示 → 下载/打开）
> **严重程度**：🔴 功能失效（下载/打开文件时路径错误）
> **状态**：✅ 已修复
> **修复版本**：commit `174aee7`

---

## 现象

在文件浏览界面中：
1. 点击树控件中的某个目录，列表正确显示该目录下的文件
2. 右键选择"下载文件"或"打开文件"，操作失败或操作了错误的文件
3. 路径拼接结果指向之前选中的目录，而非当前点击的目录

---

## 调试过程

### 第一步：检查下载路径

在 `OnDownloadFile` 中打断点，查看路径拼接：

```cpp
HTREEITEM hSelected = m_Tree.GetSelectedItem();  // ← 返回的不是刚点击的项！
CString strPath = GetPath(hSelected);
```

发现 `hSelected` 返回的是之前的选中项，而非当前点击的目录。

### 第二步：追踪 LoadFileInfo 中的选中逻辑

```cpp
void CRemoteClientDlg::LoadFileInfo()
{
    // HitTest 获取当前鼠标点击位置对应的树节点
    CPoint ptMouse;
    GetCursorPos(&ptMouse);
    m_Tree.ScreenToClient(&ptMouse);
    HTREEITEM hTreeSelected = m_Tree.HitTest(ptMouse);

    // ← 这里获取了正确的节点，但没有设置为"选中"状态！

    // ... 后续用 hTreeSelected 加载目录内容（正确）
    // 但 GetSelectedItem() 返回的仍是旧的选中项
}
```

### 第三步：理解 HitTest vs SelectItem 的区别

| API | 作用 | 是否改变选中状态 |
|-----|------|----------------|
| `HitTest(pt)` | 返回指定坐标处的树节点 | ❌ 不改变 |
| `SelectItem(hItem)` | 设置指定节点为选中状态 | ✅ 改变 |
| `GetSelectedItem()` | 返回当前选中的节点 | — |

`HitTest` 只是"查询"点击位置对应的节点，**不会自动设置选中状态**。后续的 `GetSelectedItem()` 仍返回上一次 `SelectItem` 设置的节点。

---

## 根因

`LoadFileInfo` 中通过 `HitTest` 获取点击节点后，直接用该节点加载目录内容，但**没有调用 `SelectItem` 设置选中状态**。导致后续 `OnDownloadFile` 等函数调用 `GetSelectedItem()` 时，返回的是旧的选中项而非当前点击的项。

---

## 修复

在 `HitTest` 获取节点后，立即设置选中状态：

```cpp
HTREEITEM hTreeSelected = m_Tree.HitTest(ptMouse);
// ... 空检查 ...

// [新代码] 显式设置选中状态，确保后续 GetSelectedItem() 返回正确的项
m_Tree.SelectItem(hTreeSelected);
// [新代码结束]

DeleteTreeChildrenItem(hTreeSelected);
m_List.DeleteAllItems();
CString strPath = GetPath(hTreeSelected);
```

---

## 调试经验

| 经验 | 说明 |
|------|------|
| **区分 "查询" 和 "操作" API** | `HitTest` 只查询不改状态，`SelectItem` 才改状态；很多控件 API 都有这种区分 |
| **MFC 树控件的选中模型** | 树控件的"选中"和"点击"是两个独立概念，手动点击会自动选中，但代码中的 `HitTest` 不会 |
| **跟踪 GetSelectedItem 返回值** | 打断点确认返回的 HTREEITEM 是否是预期的节点 |

---

## 关联笔记

- [[5.5 Bug 修复与冗余代码清理]] — Bug 修复的主笔记
- [[3.5 文件树实现]] — 树控件的基本实现
- [[3.4 驱动信息获取与UI控件集成]] — 树控件和列表控件的初始化
