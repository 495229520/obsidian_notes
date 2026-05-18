---
tags:
  - 项目/远控系统
---

# Debug-008：盘符显示遗漏最后一个驱动器

> **日期**：2026-02-10
> **影响范围**：Cmd 1（获取磁盘分区信息）→ 客户端 UI 显示
> **严重程度**：🟡 显示不全
> **状态**：✅ 已修复
> **修复版本**：commit `174aee7`

---

## 现象

客户端获取盘符信息后，树控件中缺少最后一个盘符。例如服务端有 C 盘和 D 盘，客户端只显示 C 盘。

---

## 调试过程

### 第一步：分析服务端返回数据

服务端 `MakeDriverInfo` 返回的字符串格式：

```
"C,D"     ← 逗号分隔，末尾无逗号
```

### 第二步：分析客户端解析逻辑

```cpp
std::string drivers = pClient->GetPacket().strData;  // "C,D"
drivers.push_back(',');  // 旧修复：添加尾部逗号 → "C,D,"
std::string dr;
for (size_t i = 0; i < drivers.size(); i++)
{
    if (drivers[i] == ',')
    {
        dr += ":";
        HTREEITEM hTemp = m_Tree.InsertItem(dr.c_str(), TVI_ROOT, TVI_LAST);
        m_Tree.InsertItem(NULL, hTemp, TVI_LAST);
        dr = "";
        continue;
    }
    dr += drivers[i];
}
```

旧代码通过 `push_back(',')` 在末尾添加逗号来确保最后一个盘符被处理。但这种方法有一个问题：**如果服务端返回空字符串（没有盘符），`push_back(',')` 会生成 `","`, 导致一个空项被插入树控件**。

### 第三步：重新设计解析逻辑

去掉 `push_back(',')` 的 hack，改为在循环结束后处理剩余字符串：

```cpp
// 循环结束后，dr 中可能还有最后一个盘符
if (dr.size() > 0)
{
    dr += ":";
    HTREEITEM hTemp = m_Tree.InsertItem(dr.c_str(), TVI_ROOT, TVI_LAST);
    m_Tree.InsertItem(NULL, hTemp, TVI_LAST);
}
```

---

## 根因

字符串解析时只在遇到逗号分隔符时处理数据，但最后一个元素后面没有逗号，导致遗漏。旧的 `push_back(',')` 修复虽然解决了遗漏问题，但引入了空字符串处理的边界问题。

---

## 修复

```cpp
// [原代码] drivers.push_back(',');  // 添加尾部逗号
// [问题] 空字符串时会插入空项
// [修复] 删除 push_back，在循环后处理剩余数据
for (size_t i = 0; i < drivers.size(); i++)
{
    if (drivers[i] == ',')
    {
        dr += ":";
        HTREEITEM hTemp = m_Tree.InsertItem(dr.c_str(), TVI_ROOT, TVI_LAST);
        m_Tree.InsertItem(NULL, hTemp, TVI_LAST);
        dr = "";
        continue;
    }
    dr += drivers[i];
}
// [新代码] 处理最后一个盘符
if (dr.size() > 0)
{
    dr += ":";
    HTREEITEM hTemp = m_Tree.InsertItem(dr.c_str(), TVI_ROOT, TVI_LAST);
    m_Tree.InsertItem(NULL, hTemp, TVI_LAST);
}
```

---

## 调试经验

| 经验 | 说明 |
|------|------|
| **字符串分割的边界处理** | 以分隔符分割字符串时，必须考虑最后一个元素后无分隔符的情况 |
| **hack 修复 vs 正确修复** | `push_back(',')` 是 hack，会引入新的边界问题；正确做法是循环后处理剩余数据 |

---

## 关联笔记

- [[5.5 Bug 修复与冗余代码清理]] — Bug 修复的主笔记
- [[2.4 获取磁盘分区信息]] — MakeDriverInfo 服务端实现
- [[Debug-002 TCP粘包导致文件列表数据丢失]] — 同一 UI 控件的另一个数据丢失问题
