---
tags:
  - 项目/远控系统
git: "7278af3"
aliases:
  - Debug-024 File list refresh blocked when a directory node has no placeholder child
---

# Debug-024 File list refresh blocked when a directory node has no placeholder child

> **Symptom**: clicking a valid directory node sometimes does not refresh the file list at all.  
> **Root cause**: `LoadFileInfo()` used the current tree-widget child structure as an early-return condition, but in an async loading model `GetChildItem(...) == NULL` is only temporary UI state, not a valid reason to block `cmd=2`.  
> Related main note: [[6.13 客户端ACK分发收口与下载状态回归主对话框]].

---

## Bug basics

| Item | Content |
|------|------|
| Number | Debug-024 |
| Severity | Functional failure |
| Category | MFC control semantics + stale async guard |
| Commit | `7278af3ddbea3a061f2a20c6dbf1e503797c38fe` |
| Trigger scene | Clicking a tree node after the async file-tree path has already started deleting and rebuilding placeholder children |

---

## Symptom description

1. The user clicks a valid item in the directory tree.
2. `HitTest` successfully finds the tree item.
3. The UI does not send a new `cmd=2` request.
4. The file list stays stale or empty, even though the selected item is still a valid refresh target.

From the user's point of view, it looks like “the node can be clicked, but the right-side file panel does not move.”

---

## Debugging process

### Step 1: confirm that the click really reaches the tree item

The problem is not that the click misses the control. `HitTest` returns a valid `HTREEITEM`, so the event path is alive.

### Step 2: check whether the request is sent after the click

The next suspicion is whether `LoadFileInfo()` actually reaches:

```cpp
CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 2, false,
    (BYTE*)(LPCTSTR)strPath, strPath.GetLength(), (WPARAM)hTreeSelected);
```

It turns out that in the failure case the function returns earlier, so the network request is never launched.

### Step 3: narrow the failure to the stale early-return guard

The blocking line is the old condition:

```cpp
if (m_Tree.GetChildItem(hTreeSelected) == NULL)
    return;
```

This condition assumes that “no current child item” means “do not load this node”. That assumption was only safe for an older UI expectation. It is not safe after the file tree became async and placeholder-based.

---

## Root-cause analysis

**Problematic code**:

```cpp
HTREEITEM hTreeSelected = m_Tree.HitTest(ptMouse, 0);
if (hTreeSelected == NULL)
    return;
if (m_Tree.GetChildItem(hTreeSelected) == NULL)
    return;
```

Why is this wrong in the current design?

1. A directory node may be valid even when it has no child item **at this exact moment**.
2. `LoadFileInfo()` itself later deletes children before rebuilding them, so the control structure is intentionally transient.
3. Async insertion means the next valid child item may only appear after the remote reply comes back.

So the code is mixing up two different things:

- **UI structure right now**
- **whether the remote directory is eligible for refresh**

Those are not the same question.

A useful way to remember the failure is:

```text
current child item exists?  -> widget state
should send cmd=2?          -> protocol / refresh decision
```

The bug happened because the first question was incorrectly used to answer the second one.

---

## Fix

The fix is simply to remove the stale gate and keep the real refresh path:

```cpp
HTREEITEM hTreeSelected = m_Tree.HitTest(ptMouse, 0);
if (hTreeSelected == NULL)
    return;

m_Tree.SelectItem(hTreeSelected);
DeleteTreeChildrenItem(hTreeSelected);
m_List.DeleteAllItems();
CString strPath = GetPath(hTreeSelected);
CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 2, false,
    (BYTE*)(LPCTSTR)strPath, strPath.GetLength(), (WPARAM)hTreeSelected);
```

Why this is correct:

- If the selected node is valid, the refresh request should be sent.
- Whether the directory contains files or subdirectories should be decided by the remote reply, not by placeholder children in the widget.
- The async callback path (`UpdateFileInfo`) is already the place where the tree is rebuilt item by item.

---

## Result after the fix

```text
Before:
  click node
  -> LoadFileInfo sees no current child item
  -> returns early
  -> no cmd=2
  -> file list looks frozen

After:
  click node
  -> LoadFileInfo sends cmd=2
  -> ACK packets return asynchronously
  -> tree/list are rebuilt from real remote data
```

---

## Lessons learned

| Lesson | Explanation |
|------|------|
| UI placeholders are not source-of-truth state | A dummy child item only helps the widget render or expand; it does not define whether a remote refresh is legal |
| Async refactors often leave behind stale guards | Early-return checks that were harmless in older synchronous code can become blockers after callback-based loading |
| Tree-control APIs need precise interpretation | `HitTest`, `SelectItem`, `GetChildItem`, and `Expand` each describe different things; treating them as interchangeable creates subtle UI bugs |

The reusable debugging lesson here is:

> **When a UI action should launch a network refresh, audit every early return and ask whether it is checking protocol truth or only temporary widget shape.**

---

## Related notes

- [[6.13 客户端ACK分发收口与下载状态回归主对话框]]
- [[6.12 文件树展示与下载缓冲区双 Bug 修复]]
- [[Debug-009 树控件未设置选中状态导致路径错误]]
- [[Debug-022 文件树目录节点插入后未自动展开]]

---

## Update record

| Date | Change |
|------|------|
| 2026-03-28 | Initial version: recorded the stale `GetChildItem(...) == NULL` guard that blocked valid async file-list refresh requests |
