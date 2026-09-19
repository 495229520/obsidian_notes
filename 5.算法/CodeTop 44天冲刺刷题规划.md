---
tags:
  - 算法
  - 力扣
  - 刷题计划
  - CodeTop
创建日期: 2026-09-17
目标节点: 11月中旬日常/秋招实习
总天数: 44
每日题量: 7
总题量: 308（覆盖 CodeTop Top 300 全部 300 题 + 8 核心压轴复盘）
每日预算: 4-5 小时
---

# CodeTop 44 天全量冲刺刷题规划（每天 7 题 · 300 题全覆盖）

> 目标：面向 11 月中旬日常/秋招实习面试，利用 44 天时间（每日 4-5 小时，每天 7 题），彻底攻克 **CodeTop 高频题 Top 300 全部 300 道题目**。
> 重点攻坚：当前 0 刷的 175 题与 1-5 刷的 102 题（合计 277 题薄弱项），同时将刷过 10 次以上的过熟基石题穿插作为限时热身，形成完整无盲区的面试肌肉记忆。

---

## 44 天阶段大图（Roadmap）

```text
阶段一：链表与缓存架构（Day 01 - Day 05，共 35 题）
  基础反转 ──> 倒数/删除 ──> 环检测/重排 ──> K个一组/合并K个 ──> LRU/LFU 设计
       │
       ▼
阶段二：双指针、滑动窗口与数组核心（Day 06 - Day 11，共 42 题）
  对撞双指针 ──> 原地修改/区间 ──> 变长滑窗 ──> 定长滑窗 ──> 单调队列与接雨水
       │
       ▼
阶段三：二分查找与排序/Top-K（Day 12 - Day 15，共 28 题）
  二分上下界 ──> 旋转数组极值 ──> 二分答案/矩阵 ──> 手撕快排/堆排/Top-K
       │
       ▼
阶段四：栈、队列与单调结构（Day 16 - Day 18，共 21 题）
  括号匹配/最小栈 ──> 单调栈模板 ──> 柱状图面积/表达式计算器
       │
       ▼
阶段五：二叉树与树形递归（Day 19 - Day 24，共 42 题）
  前中后遍历 ──> 层序/锯齿 ──> 结构判定/合并 ──> 递归构造 ──> BST 专题 ──> 树上路径/LCA
       │
       ▼
阶段六：回溯算法与穷举搜索（Day 25 - Day 27，共 21 题）
  全排列/下一个排列 ──> 子集/组合总和 ──> 字符串切分/N皇后
       │
       ▼
阶段七：图论、拓扑排序、并查集与最短路径（Day 28 - Day 30，共 21 题）
  网格连通块 ──> DAG拓扑排序/判环 ──> 并查集 ──> 最短路径专题（BFS/Dijkstra）
       │
       ▼
阶段八：动态规划全景（Day 31 - Day 36，共 42 题）
  线性爬楼梯/打家劫舍 ──> 01与完全背包 ──> LIS子序列 ──> 二维网格 ──> 编辑距离 ──> 股票状态机
       │
       ▼
阶段九：字符串模拟、高精度与矩阵操作（Day 37 - Day 39，共 21 题）
  高精度加乘/atoi ──> 单词翻转/模式匹配 ──> 螺旋矩阵/旋转置换
       │
       ▼
阶段十：哈希表、贪心、数学、位运算与巅峰押题（Day 40 - Day 44，共 35 题）
  哈希补数 ──> 贪心跳跃/区间 ──> 位运算/快速幂 ──> 综合扫盲 ──> 大厂必考巅峰手撕
```

---

## 每日 4-5 小时时间分配模型

- **热身期（30-40 分钟，2 题）**：快速过掉熟题或简单题，注重代码规范、边界条件（如 `nullptr`、空数组、溢出）一次写对。
- **主力攻坚期（150-180 分钟，4 题）**：攻克中等核心题。每题给 20-30 分钟独立思考与编码，卡壳 15 分钟立即对照专题笔记与动画，彻底理清原理后再手写 AC。
- **高难攻坚期（45-60 分钟，1 题）**：攻克困难压轴题或复杂设计题（如 LRU、K个一组、接雨水、编辑距离）。画图推演状态转移或指针流向。
- **复盘与记录（20-30 分钟）**：在当题记录踩坑点，并在 [[CodeTop 高频题 Top300]] 递增刷题次数。

---

## Day 01：链表基础：指针反转与双指针

> 对应专题：[[5.2.9 链表常见技巧]] ｜ 核心要点：掌握双指针与三指针原地反转，虚拟头节点 dummy 统一首节点边界

| 进度 | CodeTop 排名 | 题号 | 题目 | 难度 | 历史通过次数 | 状态建议 |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| [ ] | #2 | 146 | [LRU缓存机制](https://leetcode.cn/problems/lru-cache) | 中等 | 1 | 仅 1 刷需巩固 |
| [ ] | #3 | 206 | [反转链表](https://leetcode.cn/problems/reverse-linked-list) | 简单 | 13 | 已过 13 次（限时手撕） |
| [ ] | #5 | 25 | [K 个一组翻转链表](https://leetcode.cn/problems/reverse-nodes-in-k-group) | 困难 | 3 | 已过 3 次（限时手撕） |
| [ ] | #10 | 21 | [合并两个有序链表](https://leetcode.cn/problems/merge-two-sorted-lists) | 简单 | 3 | 已过 3 次（限时手撕） |
| [ ] | #19 | 92 | [反转链表 II](https://leetcode.cn/problems/reverse-linked-list-ii) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #23 | 23 | [合并K个排序链表](https://leetcode.cn/problems/merge-k-sorted-lists) | 困难 | 0 | 0 刷重点攻坚 |
| [ ] | #25 | 143 | [重排链表](https://leetcode.cn/problems/reorder-list) | 中等 | 4 | 已过 4 次（限时手撕） |

---

## Day 02：链表操作：区间截断与倒数节点

> 对应专题：[[5.2.9 链表常见技巧]] ｜ 核心要点：快慢指针保持固定距离定位前驱，连续排重跳过重复值

| 进度 | CodeTop 排名 | 题号 | 题目 | 难度 | 历史通过次数 | 状态建议 |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| [ ] | #26 | 141 | [环形链表](https://leetcode.cn/problems/linked-list-cycle) | 简单 | 3 | 已过 3 次（限时手撕） |
| [ ] | #29 | 160 | [相交链表](https://leetcode.cn/problems/intersection-of-two-linked-lists) | 简单 | 3 | 已过 3 次（限时手撕） |
| [ ] | #33 | 82 | [删除排序链表中的重复元素 II](https://leetcode.cn/problems/remove-duplicates-from-sorted-list-ii) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #36 | 19 | [删除链表的倒数第N个节点](https://leetcode.cn/problems/remove-nth-node-from-end-of-list) | 中等 | 4 | 已过 4 次（限时手撕） |
| [ ] | #38 | 142 | [环形链表 II](https://leetcode.cn/problems/linked-list-cycle-ii) | 中等 | 4 | 已过 4 次（限时手撕） |
| [ ] | #45 | 148 | [排序链表](https://leetcode.cn/problems/sort-list) | 中等 | 3 | 已过 3 次（限时手撕） |
| [ ] | #51 | 2 | [两数相加](https://leetcode.cn/problems/add-two-numbers) | 中等 | 2 | 已过 2 次（限时手撕） |

---

## Day 03：链表环检测与重排排序

> 对应专题：[[5.2.9 链表常见技巧]] ｜ 核心要点：Floyd 判圈求入环点，快慢找中点 + 局部反转 + 交叉合并

| 进度 | CodeTop 排名 | 题号 | 题目 | 难度 | 历史通过次数 | 状态建议 |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| [ ] | #61 | 剑指 Offer 22 | [链表中倒数第k个节点](https://leetcode.cn/problems/lian-biao-zhong-dao-shu-di-kge-jie-dian-lcof) | 简单 | 0 | 0 刷重点攻坚 |
| [ ] | #75 | 234 | [回文链表](https://leetcode.cn/problems/palindrome-linked-list) | 简单 | 0 | 0 刷重点攻坚 |
| [ ] | #91 | 24 | [两两交换链表中的节点](https://leetcode.cn/problems/swap-nodes-in-pairs) | 中等 | 4 | 已过 4 次（限时手撕） |
| [ ] | #93 | 83 | [删除排序链表中的重复元素](https://leetcode.cn/problems/remove-duplicates-from-sorted-list) | 简单 | 0 | 0 刷重点攻坚 |
| [ ] | #107 | 138 | [复制带随机指针的链表](https://leetcode.cn/problems/copy-list-with-random-pointer) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #112 | 460 | [LFU缓存](https://leetcode.cn/problems/lfu-cache) | 困难 | 0 | 0 刷重点攻坚 |
| [ ] | #119 | 61 | [旋转链表](https://leetcode.cn/problems/rotate-list) | 中等 | 0 | 0 刷重点攻坚 |

---

## Day 04：链表高阶：K个一组与合并K个

> 对应专题：[[5.2.9 链表常见技巧]] ｜ 核心要点：小顶堆合并 K 个有序链表，K 个一组局部翻转接回主链

| 进度 | CodeTop 排名 | 题号 | 题目 | 难度 | 历史通过次数 | 状态建议 |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| [ ] | #127 | 剑指 Offer 36 | [二叉搜索树与双向链表](https://leetcode.cn/problems/er-cha-sou-suo-shu-yu-shuang-xiang-lian-biao-lcof) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #132 | 114 | [二叉树展开为链表](https://leetcode.cn/problems/flatten-binary-tree-to-linked-list) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #133 | 补充题1 | [排序奇升偶降链表](https://leetcode.cn/problems/https://mp.weixin.qq.com/s/0WVa2wIAeG0nYnVndZiEXQ) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #148 | 445 | [两数相加 II](https://leetcode.cn/problems/add-two-numbers-ii) | 中等 | 1 | 仅 1 刷需巩固 |
| [ ] | #160 | 328 | [奇偶链表](https://leetcode.cn/problems/odd-even-linked-list) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #200 | 面试题 02.05 | [链表求和](https://leetcode.cn/problems/sum-lists-lcci) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #212 | 86 | [分隔链表](https://leetcode.cn/problems/partition-list) | 中等 | 0 | 0 刷重点攻坚 |

---

## Day 05：链表设计与复杂结构模拟

> 对应专题：[[5.2.16 面试常考数据结构设计]] ｜ 核心要点：哈希表 + 双向链表 LRU/LFU 架构，循环队列数组实现

| 进度 | CodeTop 排名 | 题号 | 题目 | 难度 | 历史通过次数 | 状态建议 |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| [ ] | #217 | 剑指 Offer 52 | [两个链表的第一个公共节点](https://leetcode.cn/problems/liang-ge-lian-biao-de-di-yi-ge-gong-gong-jie-dian-lcof) | 简单 | 0 | 0 刷重点攻坚 |
| [ ] | #247 | 426 | [将二叉搜索树转化为排序的双向链表](https://leetcode.cn/problems/convert-binary-search-tree-to-sorted-doubly-linked-list) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #284 | 876 | [链表的中间结点](https://leetcode.cn/problems/middle-of-the-linked-list) | 简单 | 0 | 0 刷重点攻坚 |
| [ ] | #289 | 109 | [有序链表转换二叉搜索树](https://leetcode.cn/problems/convert-sorted-list-to-binary-search-tree) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #291 | 剑指 Offer 24 | [反转链表](https://leetcode.cn/problems/fan-zhuan-lian-biao-lcof) | 简单 | 0 | 0 刷重点攻坚 |
| [ ] | #295 | 剑指 Offer 35 | [复杂链表的复制](https://leetcode.cn/problems/fu-za-lian-biao-de-fu-zhi-lcof) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #2 | 146 | [LRU缓存机制](https://leetcode.cn/problems/lru-cache) | 中等 | 1 | 仅 1 刷需巩固 |

---

## Day 06：栈基础：括号匹配与辅助最小栈

> 对应专题：[[5.1.12 stack]] ｜ 核心要点：辅助同步栈存历史极值，双栈出入队模拟，括号生成剪枝

| 进度 | CodeTop 排名 | 题号 | 题目 | 难度 | 历史通过次数 | 状态建议 |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| [ ] | #16 | 20 | [有效的括号](https://leetcode.cn/problems/valid-parentheses) | 简单 | 11 | 已过 11 次（限时手撕） |
| [ ] | #43 | 22 | [括号生成](https://leetcode.cn/problems/generate-parentheses) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #44 | 32 | [最长有效括号](https://leetcode.cn/problems/longest-valid-parentheses) | 困难 | 0 | 0 刷重点攻坚 |
| [ ] | #48 | 232 | [用栈实现队列](https://leetcode.cn/problems/implement-queue-using-stacks) | 简单 | 1 | 仅 1 刷需巩固 |
| [ ] | #62 | 394 | [字符串解码](https://leetcode.cn/problems/decode-string) | 中等 | 2 | 已过 2 次（限时手撕） |
| [ ] | #64 | 155 | [最小栈](https://leetcode.cn/problems/min-stack) | 简单 | 1 | 仅 1 刷需巩固 |
| [ ] | #95 | 227 | [基本计算器 II](https://leetcode.cn/problems/basic-calculator-ii) | 中等 | 2 | 已过 2 次（限时手撕） |

---

## Day 07：单调栈入门：下一个更大元素

> 对应专题：[[5.2.10 栈与单调结构]] ｜ 核心要点：单调递减栈维护未结算元素，出栈时更新答案数组

| 进度 | CodeTop 排名 | 题号 | 题目 | 难度 | 历史通过次数 | 状态建议 |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| [ ] | #102 | 739 | [每日温度](https://leetcode.cn/problems/daily-temperatures) | 中等 | 1 | 仅 1 刷需巩固 |
| [ ] | #109 | 224 | [基本计算器](https://leetcode.cn/problems/basic-calculator) | 困难 | 0 | 0 刷重点攻坚 |
| [ ] | #116 | 402 | [移掉K位数字](https://leetcode.cn/problems/remove-k-digits) | 中等 | 1 | 仅 1 刷需巩固 |
| [ ] | #129 | 剑指 Offer 09 | [用两个栈实现队列](https://leetcode.cn/problems/yong-liang-ge-zhan-shi-xian-dui-lie-lcof) | 简单 | 0 | 0 刷重点攻坚 |
| [ ] | #159 | 225 | [用队列实现栈](https://leetcode.cn/problems/implement-stack-using-queues) | 简单 | 2 | 已过 2 次（限时手撕） |
| [ ] | #161 | 678 | [有效的括号字符串](https://leetcode.cn/problems/valid-parenthesis-string) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #189 | 84 | [柱状图中最大的矩形](https://leetcode.cn/problems/largest-rectangle-in-histogram) | 困难 | 0 | 0 刷重点攻坚 |

---

## Day 08：单调栈进阶与复杂表达式解析

> 对应专题：[[5.2.10 栈与单调结构]] ｜ 核心要点：柱状图最大矩形哨兵边界，字符串解码嵌套与计算器优先级

| 进度 | CodeTop 排名 | 题号 | 题目 | 难度 | 历史通过次数 | 状态建议 |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| [ ] | #198 | 71 | [简化路径](https://leetcode.cn/problems/simplify-path) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #226 | 1047 | [删除字符串中的所有相邻重复项](https://leetcode.cn/problems/remove-all-adjacent-duplicates-in-string) | 简单 | 0 | 0 刷重点攻坚 |
| [ ] | #244 | 622 | [设计循环队列](https://leetcode.cn/problems/design-circular-queue) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #246 | 301 | [删除无效的括号](https://leetcode.cn/problems/remove-invalid-parentheses) | 困难 | 0 | 0 刷重点攻坚 |
| [ ] | #263 | 150 | [逆波兰表达式求值](https://leetcode.cn/problems/evaluate-reverse-polish-notation) | 中等 | 3 | 已过 3 次（限时手撕） |
| [ ] | #271 | 946 | [验证栈序列](https://leetcode.cn/problems/validate-stack-sequences) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #298 | 735 | [行星碰撞](https://leetcode.cn/problems/asteroid-collision) | 中等 | 0 | 0 刷重点攻坚 |

---

## Day 09：对撞双指针与多元组求和

> 对应专题：[[5.2.7 双指针与滑动窗口]] ｜ 核心要点：排序后左右指针夹逼，严格跳过重复值避免重复三元组

| 进度 | CodeTop 排名 | 题号 | 题目 | 难度 | 历史通过次数 | 状态建议 |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| [ ] | #1 | 3 | [无重复字符的最长子串](https://leetcode.cn/problems/longest-substring-without-repeating-characters) | 中等 | 13 | 已过 13 次（限时手撕） |
| [ ] | #6 | 15 | [三数之和](https://leetcode.cn/problems/3sum) | 中等 | 7 | 已过 7 次（限时手撕） |
| [ ] | #17 | 88 | [合并两个有序数组](https://leetcode.cn/problems/merge-sorted-array) | 简单 | 3 | 已过 3 次（限时手撕） |
| [ ] | #28 | 56 | [合并区间](https://leetcode.cn/problems/merge-intervals) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #31 | 42 | [接雨水](https://leetcode.cn/problems/trapping-rain-water) | 困难 | 0 | 0 刷重点攻坚 |
| [ ] | #41 | 239 | [滑动窗口最大值](https://leetcode.cn/problems/sliding-window-maximum) | 困难 | 3 | 已过 3 次（限时手撕） |
| [ ] | #49 | 31 | [下一个排列](https://leetcode.cn/problems/next-permutation) | 中等 | 0 | 0 刷重点攻坚 |

---

## Day 10：变长滑动窗口与无重复子串

> 对应专题：[[5.2.7 双指针与滑动窗口]] ｜ 核心要点：变长窗口右扩入表、不满足时左缩，双指针接雨水短板效应

| 进度 | CodeTop 排名 | 题号 | 题目 | 难度 | 历史通过次数 | 状态建议 |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| [ ] | #50 | 76 | [最小覆盖子串](https://leetcode.cn/problems/minimum-window-substring) | 困难 | 0 | 0 刷重点攻坚 |
| [ ] | #57 | 41 | [缺失的第一个正数](https://leetcode.cn/problems/first-missing-positive) | 困难 | 4 | 已过 4 次（限时手撕） |
| [ ] | #98 | 169 | [多数元素](https://leetcode.cn/problems/majority-element) | 简单 | 2 | 已过 2 次（限时手撕） |
| [ ] | #99 | 283 | [移动零](https://leetcode.cn/problems/move-zeroes) | 简单 | 5 | 已过 5 次（限时手撕） |
| [ ] | #110 | 11 | [盛最多水的容器](https://leetcode.cn/problems/container-with-most-water) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #114 | 16 | [最接近的三数之和](https://leetcode.cn/problems/3sum-closest) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #121 | 75 | [颜色分类](https://leetcode.cn/problems/sort-colors) | 中等 | 1 | 仅 1 刷需巩固 |

---

## Day 11：定长滑窗、异位词与单调队列

> 对应专题：[[5.2.7 双指针与滑动窗口]] ｜ 核心要点：定长窗口维护 26 字符频次差，单调队列维护滑动窗口最大值

| 进度 | CodeTop 排名 | 题号 | 题目 | 难度 | 历史通过次数 | 状态建议 |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| [ ] | #126 | 26 | [删除排序数组中的重复项](https://leetcode.cn/problems/remove-duplicates-from-sorted-array) | 简单 | 1 | 仅 1 刷需巩固 |
| [ ] | #164 | 189 | [轮转数组](https://leetcode.cn/problems/rotate-array) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #213 | 18 | [四数之和](https://leetcode.cn/problems/4sum) | 中等 | 7 | 已过 7 次（限时手撕） |
| [ ] | #241 | 242 | [有效的字母异位词](https://leetcode.cn/problems/valid-anagram) | 简单 | 5 | 已过 5 次（限时手撕） |
| [ ] | #242 | 80 | [删除排序数组中的重复项 II](https://leetcode.cn/problems/remove-duplicates-from-sorted-array-ii) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #251 | 49 | [字母异位词分组](https://leetcode.cn/problems/group-anagrams) | 中等 | 3 | 已过 3 次（限时手撕） |
| [ ] | #282 | 438 | [找到字符串中所有字母异位词](https://leetcode.cn/problems/find-all-anagrams-in-a-string) | 中等 | 0 | 0 刷重点攻坚 |

---

## Day 12：二分查找核心边界与上下界

> 对应专题：[[5.2.6 二分查找与二分答案]] ｜ 核心要点：闭区间二分模板，lower_bound 与 upper_bound 边界夹逼

| 进度 | CodeTop 排名 | 题号 | 题目 | 难度 | 历史通过次数 | 状态建议 |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| [ ] | #4 | 215 | [数组中的第K个最大元素](https://leetcode.cn/problems/kth-largest-element-in-an-array) | 中等 | 8 | 已过 8 次（限时手撕） |
| [ ] | #8 | 补充题4 | [手撕快速排序](https://leetcode.cn/problems/sort-an-array) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #13 | 33 | [搜索旋转排序数组](https://leetcode.cn/problems/search-in-rotated-sorted-array) | 中等 | 2 | 已过 2 次（限时手撕） |
| [ ] | #37 | 4 | [寻找两个正序数组的中位数](https://leetcode.cn/problems/median-of-two-sorted-arrays) | 困难 | 0 | 0 刷重点攻坚 |
| [ ] | #42 | 704 | [二分查找](https://leetcode.cn/problems/binary-search) | 简单 | 4 | 已过 4 次（限时手撕） |
| [ ] | #46 | 69 | [x 的平方根](https://leetcode.cn/problems/sqrtx) | 简单 | 0 | 0 刷重点攻坚 |
| [ ] | #60 | 34 | [在排序数组中查找元素的第一个和最后一个位置](https://leetcode.cn/problems/find-first-and-last-position-of-element-in-sorted-array) | 中等 | 2 | 已过 2 次（限时手撕） |

---

## Day 13：旋转排序数组专题与极值查找

> 对应专题：[[5.2.6 二分查找与二分答案]] ｜ 核心要点：二分切分后必有一半有序，按有序段范围判定目标所在半区

| 进度 | CodeTop 排名 | 题号 | 题目 | 难度 | 历史通过次数 | 状态建议 |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| [ ] | #77 | 240 | [搜索二维矩阵 II](https://leetcode.cn/problems/search-a-2d-matrix-ii) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #80 | 179 | [最大数](https://leetcode.cn/problems/largest-number) | 中等 | 1 | 仅 1 刷需巩固 |
| [ ] | #85 | 162 | [寻找峰值](https://leetcode.cn/problems/find-peak-element) | 中等 | 1 | 仅 1 刷需巩固 |
| [ ] | #101 | 补充题6 | [手撕堆排序](https://leetcode.cn/problems/sort-an-array) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #106 | 153 | [寻找旋转排序数组中的最小值](https://leetcode.cn/problems/find-minimum-in-rotated-sorted-array) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #118 | 剑指 Offer 51 | [数组中的逆序对](https://leetcode.cn/problems/shu-zu-zhong-de-ni-xu-dui-lcof) | 困难 | 0 | 0 刷重点攻坚 |
| [ ] | #120 | 补充题5 | [手撕归并排序](https://leetcode.cn/problems/sort-an-array) | 中等 | 0 | 0 刷重点攻坚 |

---

## Day 14：二分答案与单调矩阵折半

> 对应专题：[[5.2.6 二分查找与二分答案]] ｜ 核心要点：答案空间单调性判定，二维矩阵从右上角单调折半扫描

| 进度 | CodeTop 排名 | 题号 | 题目 | 难度 | 历史通过次数 | 状态建议 |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| [ ] | #131 | 74 | [搜索二维矩阵](https://leetcode.cn/problems/search-a-2d-matrix) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #157 | 295 | [数据流的中位数](https://leetcode.cn/problems/find-median-from-data-stream) | 困难 | 0 | 0 刷重点攻坚 |
| [ ] | #158 | 287 | [寻找重复数](https://leetcode.cn/problems/find-the-duplicate-number) | 中等 | 1 | 仅 1 刷需巩固 |
| [ ] | #210 | 面试题 17.14 | [最小K个数](https://leetcode.cn/problems/smallest-k-lcci) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #238 | 410 | [分割数组的最大值](https://leetcode.cn/problems/split-array-largest-sum) | 困难 | 0 | 0 刷重点攻坚 |
| [ ] | #239 | 154 | [寻找旋转排序数组中的最小值 II](https://leetcode.cn/problems/find-minimum-in-rotated-sorted-array-ii) | 困难 | 0 | 0 刷重点攻坚 |
| [ ] | #250 | 剑指 Offer 53 - I | [在排序数组中查找数字 I](https://leetcode.cn/problems/zai-pai-xu-shu-zu-zhong-cha-zhao-shu-zi-lcof) | 简单 | 0 | 0 刷重点攻坚 |

---

## Day 15：大厂手撕排序与 Top-K 堆模式

> 对应专题：[[5.2.5 常见排序算法与STL排序]] ｜ 核心要点：快速选择 Partition 划分，小顶堆维护前 K 个最大，手撕快排堆排

| 进度 | CodeTop 排名 | 题号 | 题目 | 难度 | 历史通过次数 | 状态建议 |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| [ ] | #276 | 540 | [有序数组中的单一元素](https://leetcode.cn/problems/single-element-in-a-sorted-array) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #294 | 1095 | [山脉数组中查找目标值](https://leetcode.cn/problems/find-in-mountain-array) | 困难 | 0 | 0 刷重点攻坚 |
| [ ] | #296 | 912 | [排序数组](https://leetcode.cn/problems/sort-an-array) | 中等 | 14 | 已过 14 次（限时手撕） |
| [ ] | #300 | 164 | [最大间距](https://leetcode.cn/problems/maximum-gap) | 困难 | 0 | 0 刷重点攻坚 |
| [ ] | #4 | 215 | [数组中的第K个最大元素](https://leetcode.cn/problems/kth-largest-element-in-an-array) | 中等 | 8 | 已过 8 次（限时手撕） |
| [ ] | #8 | 补充题4 | [手撕快速排序](https://leetcode.cn/problems/sort-an-array) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #101 | 补充题6 | [手撕堆排序](https://leetcode.cn/problems/sort-an-array) | 中等 | 0 | 0 刷重点攻坚 |

---

## Day 16：二叉树基础遍历与递归分治

> 对应专题：[[5.2.12 二叉树与遍历算法]] ｜ 核心要点：前中后序递归与显式栈迭代，翻转二叉树与镜像对比

| 进度 | CodeTop 排名 | 题号 | 题目 | 难度 | 历史通过次数 | 状态建议 |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| [ ] | #11 | 102 | [二叉树的层序遍历](https://leetcode.cn/problems/binary-tree-level-order-traversal) | 中等 | 15 | 已过 15 次（限时手撕） |
| [ ] | #21 | 103 | [二叉树的锯齿形层次遍历](https://leetcode.cn/problems/binary-tree-zigzag-level-order-traversal) | 中等 | 1 | 仅 1 刷需巩固 |
| [ ] | #22 | 236 | [二叉树的最近公共祖先](https://leetcode.cn/problems/lowest-common-ancestor-of-a-binary-tree) | 中等 | 4 | 已过 4 次（限时手撕） |
| [ ] | #35 | 124 | [二叉树中的最大路径和](https://leetcode.cn/problems/binary-tree-maximum-path-sum) | 困难 | 2 | 已过 2 次（限时手撕） |
| [ ] | #40 | 199 | [二叉树的右视图](https://leetcode.cn/problems/binary-tree-right-side-view) | 中等 | 1 | 仅 1 刷需巩固 |
| [ ] | #47 | 94 | [二叉树的中序遍历](https://leetcode.cn/problems/binary-tree-inorder-traversal) | 简单 | 6 | 已过 6 次（限时手撕） |
| [ ] | #56 | 105 | [从前序与中序遍历序列构造二叉树](https://leetcode.cn/problems/construct-binary-tree-from-preorder-and-inorder-traversal) | 中等 | 0 | 0 刷重点攻坚 |

---

## Day 17：二叉树层序遍历与形态属性

> 对应专题：[[5.2.12 二叉树与遍历算法]] ｜ 核心要点：队列 BFS 按层输出，锯齿形交替遍历，二叉树最大宽度与深度

| 进度 | CodeTop 排名 | 题号 | 题目 | 难度 | 历史通过次数 | 状态建议 |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| [ ] | #63 | 129 | [求根到叶子节点数字之和](https://leetcode.cn/problems/sum-root-to-leaf-numbers) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #66 | 101 | [对称二叉树](https://leetcode.cn/problems/symmetric-tree) | 简单 | 4 | 已过 4 次（限时手撕） |
| [ ] | #72 | 104 | [二叉树的最大深度](https://leetcode.cn/problems/maximum-depth-of-binary-tree) | 简单 | 3 | 已过 3 次（限时手撕） |
| [ ] | #74 | 110 | [平衡二叉树](https://leetcode.cn/problems/balanced-binary-tree) | 简单 | 3 | 已过 3 次（限时手撕） |
| [ ] | #78 | 98 | [验证二叉搜索树](https://leetcode.cn/problems/validate-binary-search-tree) | 中等 | 1 | 仅 1 刷需巩固 |
| [ ] | #79 | 144 | [二叉树的前序遍历](https://leetcode.cn/problems/binary-tree-preorder-traversal) | 简单 | 8 | 已过 8 次（限时手撕） |
| [ ] | #81 | 662 | [二叉树最大宽度](https://leetcode.cn/problems/maximum-width-of-binary-tree) | 中等 | 0 | 0 刷重点攻坚 |

---

## Day 18：二叉树结构判定与子树匹配

> 对应专题：[[5.2.12 二叉树与遍历算法]] ｜ 核心要点：完全二叉树节点编号检验，子树匹配与两树合并

| 进度 | CodeTop 排名 | 题号 | 题目 | 难度 | 历史通过次数 | 状态建议 |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| [ ] | #84 | 543 | [二叉树的直径](https://leetcode.cn/problems/diameter-of-binary-tree) | 简单 | 0 | 0 刷重点攻坚 |
| [ ] | #97 | 226 | [翻转二叉树](https://leetcode.cn/problems/invert-binary-tree) | 简单 | 6 | 已过 6 次（限时手撕） |
| [ ] | #105 | 297 | [二叉树的序列化与反序列化](https://leetcode.cn/problems/serialize-and-deserialize-binary-tree) | 困难 | 0 | 0 刷重点攻坚 |
| [ ] | #124 | 剑指 Offer 26 | [树的子结构](https://leetcode.cn/problems/shu-de-zi-jie-gou-lcof) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #128 | 958 | [二叉树的完全性检验](https://leetcode.cn/problems/check-completeness-of-a-binary-tree) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #140 | 572 | [另一个树的子树](https://leetcode.cn/problems/subtree-of-another-tree) | 简单 | 0 | 0 刷重点攻坚 |
| [ ] | #141 | 145 | [二叉树的后序遍历](https://leetcode.cn/problems/binary-tree-postorder-traversal) | 中等 | 2 | 已过 2 次（限时手撕） |

---

## Day 19：二叉树递归构造与展开

> 对应专题：[[5.2.12 二叉树与遍历算法]] ｜ 核心要点：前中序/中后序定位根节点切分区间，二叉树展开为链表

| 进度 | CodeTop 排名 | 题号 | 题目 | 难度 | 历史通过次数 | 状态建议 |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| [ ] | #145 | 208 | [实现 Trie (前缀树)](https://leetcode.cn/problems/implement-trie-prefix-tree) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #150 | 450 | [删除二叉搜索树中的节点](https://leetcode.cn/problems/delete-node-in-a-bst) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #151 | 剑指 Offer 54 | [二叉搜索树的第k大节点](https://leetcode.cn/problems/er-cha-sou-suo-shu-de-di-kda-jie-dian-lcof) | 简单 | 0 | 0 刷重点攻坚 |
| [ ] | #166 | 106 | [从中序与后序遍历序列构造二叉树](https://leetcode.cn/problems/construct-binary-tree-from-inorder-and-postorder-traversal) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #169 | 230 | [二叉搜索树中第K小的元素](https://leetcode.cn/problems/kth-smallest-element-in-a-bst) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #170 | 96 | [不同的二叉搜索树](https://leetcode.cn/problems/unique-binary-search-trees) | 中等 | 2 | 已过 2 次（限时手撕） |
| [ ] | #194 | 剑指 Offer 27 | [二叉树的镜像](https://leetcode.cn/problems/er-cha-shu-de-jing-xiang-lcof) | 简单 | 0 | 0 刷重点攻坚 |

---

## Day 20：二叉搜索树（BST）特性与操作

> 对应专题：[[5.2.12 二叉树与遍历算法]] ｜ 核心要点：中序单调递增性，BST 第 K 小元素，删除与插入节点

| 进度 | CodeTop 排名 | 题号 | 题目 | 难度 | 历史通过次数 | 状态建议 |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| [ ] | #207 | 111 | [二叉树的最小深度](https://leetcode.cn/problems/minimum-depth-of-binary-tree) | 简单 | 5 | 已过 5 次（限时手撕） |
| [ ] | #232 | 剑指 Offer 34 | [二叉树中和为某一值的路径](https://leetcode.cn/problems/er-cha-shu-zhong-he-wei-mou-yi-zhi-de-lu-jing-lcof) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #248 | 222 | [完全二叉树的节点个数](https://leetcode.cn/problems/count-complete-tree-nodes) | 中等 | 1 | 仅 1 刷需巩固 |
| [ ] | #252 | 257 | [二叉树的所有路径](https://leetcode.cn/problems/binary-tree-paths) | 简单 | 3 | 已过 3 次（限时手撕） |
| [ ] | #254 | 108 | [将有序数组转换为二叉搜索树](https://leetcode.cn/problems/convert-sorted-array-to-binary-search-tree) | 简单 | 0 | 0 刷重点攻坚 |
| [ ] | #257 | 剑指 Offer 33 | [二叉搜索树的后序遍历序列](https://leetcode.cn/problems/er-cha-sou-suo-shu-de-hou-xu-bian-li-xu-lie-lcof) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #258 | 863 | [二叉树中所有距离为 K 的结点](https://leetcode.cn/problems/all-nodes-distance-k-in-binary-tree) | 中等 | 0 | 0 刷重点攻坚 |

---

## Day 21：树上路径总和、LCA 与最大路径和

> 对应专题：[[5.2.12 二叉树与遍历算法]] ｜ 核心要点：后序自底向上汇聚 LCA，回溯收集树上路径，单边贡献树形 DP

| 进度 | CodeTop 排名 | 题号 | 题目 | 难度 | 历史通过次数 | 状态建议 |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| [ ] | #261 | 100 | [相同的树](https://leetcode.cn/problems/same-tree) | 简单 | 0 | 0 刷重点攻坚 |
| [ ] | #266 | 235 | [二叉搜索树的最近公共祖先](https://leetcode.cn/problems/lowest-common-ancestor-of-a-binary-search-tree) | 简单 | 0 | 0 刷重点攻坚 |
| [ ] | #272 | 107 | [二叉树的层次遍历 II](https://leetcode.cn/problems/binary-tree-level-order-traversal-ii) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #283 | 剑指 Offer 32 - III | [从上到下打印二叉树 III](https://leetcode.cn/problems/cong-shang-dao-xia-da-yin-er-cha-shu-iii-lcof) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #290 | 617 | [合并二叉树](https://leetcode.cn/problems/merge-two-binary-trees) | 简单 | 0 | 0 刷重点攻坚 |
| [ ] | #11 | 102 | [二叉树的层序遍历](https://leetcode.cn/problems/binary-tree-level-order-traversal) | 中等 | 15 | 已过 15 次（限时手撕） |
| [ ] | #236 | 1262 | [可被三整除的最大和](https://leetcode.cn/problems/greatest-sum-divisible-by-three) | 中等 | 0 | 0 刷重点攻坚 |

---

## Day 22：回溯算法：全排列与下一个排列

> 对应专题：[[5.2.18 回溯算法]] ｜ 核心要点：used 数组标记状态，排序后同层去重，下一个排列字典序推进

| 进度 | CodeTop 排名 | 题号 | 题目 | 难度 | 历史通过次数 | 状态建议 |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| [ ] | #15 | 46 | [全排列](https://leetcode.cn/problems/permutations) | 中等 | 3 | 已过 3 次（限时手撕） |
| [ ] | #34 | 93 | [复原IP地址](https://leetcode.cn/problems/restore-ip-addresses) | 中等 | 1 | 仅 1 刷需巩固 |
| [ ] | #58 | 78 | [子集](https://leetcode.cn/problems/subsets) | 中等 | 3 | 已过 3 次（限时手撕） |
| [ ] | #65 | 39 | [组合总和](https://leetcode.cn/problems/combination-sum) | 中等 | 2 | 已过 2 次（限时手撕） |
| [ ] | #103 | 79 | [单词搜索](https://leetcode.cn/problems/word-search) | 中等 | 1 | 仅 1 刷需巩固 |
| [ ] | #108 | 47 | [全排列 II](https://leetcode.cn/problems/permutations-ii) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #113 | 40 | [组合总和 II](https://leetcode.cn/problems/combination-sum-ii) | 中等 | 0 | 0 刷重点攻坚 |

---

## Day 23：回溯算法：组合与子集搜集

> 对应专题：[[5.2.18 回溯算法]] ｜ 核心要点：startIndex 控制搜索起点，剪枝减扣剩余目标值

| 进度 | CodeTop 排名 | 题号 | 题目 | 难度 | 历史通过次数 | 状态建议 |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| [ ] | #174 | 416 | [分割等和子集](https://leetcode.cn/problems/partition-equal-subset-sum) | 中等 | 1 | 仅 1 刷需巩固 |
| [ ] | #187 | 51 | [N皇后](https://leetcode.cn/problems/n-queens) | 困难 | 0 | 0 刷重点攻坚 |
| [ ] | #204 | 17 | [电话号码的字母组合](https://leetcode.cn/problems/letter-combinations-of-a-phone-number) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #233 | 37 | [解数独](https://leetcode.cn/problems/sudoku-solver) | 困难 | 0 | 0 刷重点攻坚 |
| [ ] | #240 | 902 | [最大为 N 的数字组合](https://leetcode.cn/problems/numbers-at-most-n-given-digit-set) | 困难 | 0 | 0 刷重点攻坚 |
| [ ] | #268 | 77 | [组合](https://leetcode.cn/problems/combinations) | 中等 | 1 | 仅 1 刷需巩固 |
| [ ] | #281 | 131 | [分割回文串](https://leetcode.cn/problems/palindrome-partitioning) | 中等 | 0 | 0 刷重点攻坚 |

---

## Day 24：回溯算法：分割、棋盘与图搜索

> 对应专题：[[5.2.18 回溯算法]] ｜ 核心要点：字符串合法切分，二维网格 DFS 回溯踏板，N 皇后对角线掩码

| 进度 | CodeTop 排名 | 题号 | 题目 | 难度 | 历史通过次数 | 状态建议 |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| [ ] | #292 | 面试题 08.06 | [汉诺塔问题](https://leetcode.cn/problems/hanota-lcci) | 简单 | 0 | 0 刷重点攻坚 |
| [ ] | #14 | 1 | [两数之和](https://leetcode.cn/problems/two-sum) | 简单 | 5 | 已过 5 次（限时手撕） |
| [ ] | #39 | 165 | [比较版本号](https://leetcode.cn/problems/compare-version-numbers) | 中等 | 1 | 仅 1 刷需巩固 |
| [ ] | #68 | 470 | [用 Rand7() 实现 Rand10()](https://leetcode.cn/problems/implement-rand10-using-rand7) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #70 | 128 | [最长连续序列](https://leetcode.cn/problems/longest-consecutive-sequence) | 中等 | 3 | 已过 3 次（限时手撕） |
| [ ] | #115 | 136 | [只出现一次的数字](https://leetcode.cn/problems/single-number) | 简单 | 4 | 已过 4 次（限时手撕） |
| [ ] | #130 | 补充题23 | [检测循环依赖](https://leetcode.cn/problems/https://mp.weixin.qq.com/s/pCRscwKqQdYYN7M1Sia7xA) | 中等 | 0 | 0 刷重点攻坚 |

---

## Day 25：图论基础：网格连通块与沉岛搜索

> 对应专题：[[5.2.24 图论基础]] ｜ 核心要点：网格图 DFS 沉岛法，回溯染色统计连通块面积与边界飞地

| 进度 | CodeTop 排名 | 题号 | 题目 | 难度 | 历史通过次数 | 状态建议 |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| [ ] | #12 | 200 | [岛屿数量](https://leetcode.cn/problems/number-of-islands) | 中等 | 1 | 仅 1 刷需巩固 |
| [ ] | #67 | 64 | [最小路径和](https://leetcode.cn/problems/minimum-path-sum) | 中等 | 1 | 仅 1 刷需巩固 |
| [ ] | #69 | 695 | [岛屿的最大面积](https://leetcode.cn/problems/max-area-of-island) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #83 | 48 | [旋转图像](https://leetcode.cn/problems/rotate-image) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #86 | 62 | [不同路径](https://leetcode.cn/problems/unique-paths) | 中等 | 2 | 已过 2 次（限时手撕） |
| [ ] | #88 | 113 | [路径总和 II](https://leetcode.cn/problems/path-sum-ii) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #92 | 112 | [路径总和](https://leetcode.cn/problems/path-sum) | 简单 | 1 | 仅 1 刷需巩固 |

---

## Day 26：DAG 拓扑排序与依赖判环

> 对应专题：[[5.2.27 拓扑排序与有向无环图]] ｜ 核心要点：Kahn 算法入度表消减队列，出队数小于 N 判环，Trie 前缀树

| 进度 | CodeTop 排名 | 题号 | 题目 | 难度 | 历史通过次数 | 状态建议 |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| [ ] | #100 | 207 | [课程表](https://leetcode.cn/problems/course-schedule) | 中等 | 2 | 已过 2 次（限时手撕） |
| [ ] | #125 | 329 | [矩阵中的最长递增路径](https://leetcode.cn/problems/longest-increasing-path-in-a-matrix) | 困难 | 0 | 0 刷重点攻坚 |
| [ ] | #165 | 120 | [三角形最小路径和](https://leetcode.cn/problems/triangle) | 中等 | 1 | 仅 1 刷需巩固 |
| [ ] | #167 | 210 | [课程表 II](https://leetcode.cn/problems/course-schedule-ii) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #186 | 63 | [不同路径 II](https://leetcode.cn/problems/unique-paths-ii) | 中等 | 1 | 仅 1 刷需巩固 |
| [ ] | #275 | 994 | [腐烂的橘子](https://leetcode.cn/problems/rotting-oranges) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #299 | 437 | [路径总和 III](https://leetcode.cn/problems/path-sum-iii) | 中等 | 1 | 仅 1 刷需巩固 |

---

## Day 27：并查集连通性与最短路径专题

> 对应专题：[[5.2.28 最短路径算法专题]] ｜ 核心要点：父节点数组森林路径压缩，多源 BFS 扩散，Dijkstra 堆优化松弛

| 进度 | CodeTop 排名 | 题号 | 题目 | 难度 | 历史通过次数 | 状态建议 |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| [ ] | #136 | 442 | [数组中重复的数据](https://leetcode.cn/problems/find-all-duplicates-in-an-array) | 中等 | 1 | 仅 1 刷需巩固 |
| [ ] | #138 | 347 | [前 K 个高频元素](https://leetcode.cn/problems/top-k-frequent-elements) | 中等 | 3 | 已过 3 次（限时手撕） |
| [ ] | #139 | 50 | [Pow(x, n)](https://leetcode.cn/problems/powx-n) | 中等 | 6 | 已过 6 次（限时手撕） |
| [ ] | #146 | 剑指 Offer 40 | [最小的k个数](https://leetcode.cn/problems/zui-xiao-de-kge-shu-lcof) | 简单 | 0 | 0 刷重点攻坚 |
| [ ] | #147 | 440 | [字典序的第K小数字](https://leetcode.cn/problems/k-th-smallest-in-lexicographical-order) | 困难 | 0 | 0 刷重点攻坚 |
| [ ] | #156 | 剑指 Offer 10- II | [青蛙跳台阶问题](https://leetcode.cn/problems/qing-wa-tiao-tai-jie-wen-ti-lcof) | 简单 | 0 | 0 刷重点攻坚 |
| [ ] | #162 | 补充题2 | [圆环回原点问题](https://leetcode.cn/problems/https://mp.weixin.qq.com/s/NZPaFsFrTybO3K3s7p7EVg) | 中等 | 0 | 0 刷重点攻坚 |

---

## Day 28：线性动态规划：爬楼梯与打家劫舍

> 对应专题：[[5.2.19 动态规划]] ｜ 核心要点：Kadane 最大子数组和，打家劫舍间隔选取与首尾环形拆解

| 进度 | CodeTop 排名 | 题号 | 题目 | 难度 | 历史通过次数 | 状态建议 |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| [ ] | #7 | 53 | [最大子数组和](https://leetcode.cn/problems/maximum-subarray) | 中等 | 6 | 已过 6 次（限时手撕） |
| [ ] | #9 | 5 | [最长回文子串](https://leetcode.cn/problems/longest-palindromic-substring) | 中等 | 3 | 已过 3 次（限时手撕） |
| [ ] | #18 | 121 | [买卖股票的最佳时机](https://leetcode.cn/problems/best-time-to-buy-and-sell-stock) | 简单 | 1 | 仅 1 刷需巩固 |
| [ ] | #20 | 300 | [最长上升子序列](https://leetcode.cn/problems/longest-increasing-subsequence) | 中等 | 1 | 仅 1 刷需巩固 |
| [ ] | #30 | 72 | [编辑距离](https://leetcode.cn/problems/edit-distance) | 困难 | 1 | 仅 1 刷需巩固 |
| [ ] | #32 | 1143 | [最长公共子序列](https://leetcode.cn/problems/longest-common-subsequence) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #52 | 322 | [零钱兑换](https://leetcode.cn/problems/coin-change) | 中等 | 1 | 仅 1 刷需巩固 |

---

## Day 29：背包问题与凑零钱模型

> 对应专题：[[5.2.20 背包问题]] ｜ 核心要点：01 背包倒序容量，完全背包正序容量求最少硬币数与组合方案

| 进度 | CodeTop 排名 | 题号 | 题目 | 难度 | 历史通过次数 | 状态建议 |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| [ ] | #55 | 70 | [爬楼梯](https://leetcode.cn/problems/climbing-stairs) | 简单 | 2 | 已过 2 次（限时手撕） |
| [ ] | #71 | 122 | [买卖股票的最佳时机 II](https://leetcode.cn/problems/best-time-to-buy-and-sell-stock-ii) | 简单 | 0 | 0 刷重点攻坚 |
| [ ] | #73 | 221 | [最大正方形](https://leetcode.cn/problems/maximal-square) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #76 | 152 | [乘积最大子数组](https://leetcode.cn/problems/maximum-product-subarray) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #87 | 560 | [和为K的子数组](https://leetcode.cn/problems/subarray-sum-equals-k) | 中等 | 6 | 已过 6 次（限时手撕） |
| [ ] | #89 | 209 | [长度最小的子数组](https://leetcode.cn/problems/minimum-size-subarray-sum) | 中等 | 20 | 已过 20 次（限时手撕） |
| [ ] | #90 | 198 | [打家劫舍](https://leetcode.cn/problems/house-robber) | 中等 | 3 | 已过 3 次（限时手撕） |

---

## Day 30：子序列 DP：最长上升与连续最值

> 对应专题：[[5.2.21 子序列动态规划]] ｜ 核心要点：LIS 动态规划与贪心二分 tails 数组，乘积最大正负极值状态转移

| 进度 | CodeTop 排名 | 题号 | 题目 | 难度 | 历史通过次数 | 状态建议 |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| [ ] | #94 | 139 | [单词拆分](https://leetcode.cn/problems/word-break) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #96 | 718 | [最长重复子数组](https://leetcode.cn/problems/maximum-length-of-repeated-subarray) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #111 | 55 | [跳跃游戏](https://leetcode.cn/problems/jump-game) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #117 | 123 | [买卖股票的最佳时机 III](https://leetcode.cn/problems/best-time-to-buy-and-sell-stock-iii) | 困难 | 0 | 0 刷重点攻坚 |
| [ ] | #122 | 518 | [零钱兑换 II](https://leetcode.cn/problems/coin-change-ii) | 中等 | 1 | 仅 1 刷需巩固 |
| [ ] | #135 | 135 | [分发糖果](https://leetcode.cn/problems/candy) | 困难 | 0 | 0 刷重点攻坚 |
| [ ] | #137 | 125 | [验证回文串](https://leetcode.cn/problems/valid-palindrome) | 简单 | 0 | 0 刷重点攻坚 |

---

## Day 31：二维网格动态规划与正方形

> 对应专题：[[5.2.19 动态规划]] ｜ 核心要点：木桶短板效应求最大正方形，网格路径加法递推与障碍绕行

| 进度 | CodeTop 排名 | 题号 | 题目 | 难度 | 历史通过次数 | 状态建议 |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| [ ] | #142 | 91 | [解码方法](https://leetcode.cn/problems/decode-ways) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #144 | 10 | [正则表达式匹配](https://leetcode.cn/problems/regular-expression-matching) | 困难 | 0 | 0 刷重点攻坚 |
| [ ] | #149 | 213 | [打家劫舍 II](https://leetcode.cn/problems/house-robber-ii) | 中等 | 4 | 已过 4 次（限时手撕） |
| [ ] | #152 | 剑指 Offer 42 | [连续子数组的最大和](https://leetcode.cn/problems/lian-xu-zi-shu-zu-de-zui-da-he-lcof) | 简单 | 0 | 0 刷重点攻坚 |
| [ ] | #153 | 45 | [跳跃游戏 II](https://leetcode.cn/problems/jump-game-ii) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #154 | 剑指 Offer 10- I | [斐波那契数列](https://leetcode.cn/problems/fei-bo-na-qi-shu-lie-lcof) | 简单 | 0 | 0 刷重点攻坚 |
| [ ] | #155 | 516 | [最长回文子序列](https://leetcode.cn/problems/longest-palindromic-subsequence) | 中等 | 0 | 0 刷重点攻坚 |

---

## Day 32：字符串双串 DP 与编辑距离

> 对应专题：[[5.2.22 区间动态规划]] ｜ 核心要点：最长公共子序列 LCS，回文串中心扩展，编辑距离增删改操作转移

| 进度 | CodeTop 排名 | 题号 | 题目 | 难度 | 历史通过次数 | 状态建议 |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| [ ] | #168 | 9 | [回文数](https://leetcode.cn/problems/palindrome-number) | 简单 | 8 | 已过 8 次（限时手撕） |
| [ ] | #173 | 44 | [通配符匹配](https://leetcode.cn/problems/wildcard-matching) | 困难 | 0 | 0 刷重点攻坚 |
| [ ] | #176 | 85 | [最大矩形](https://leetcode.cn/problems/maximal-rectangle) | 困难 | 1 | 仅 1 刷需巩固 |
| [ ] | #183 | 673 | [最长递增子序列的个数](https://leetcode.cn/problems/number-of-longest-increasing-subsequence) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #196 | 647 | [回文子串](https://leetcode.cn/problems/palindromic-substrings) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #223 | 509 | [斐波那契数](https://leetcode.cn/problems/fibonacci-number) | 简单 | 7 | 已过 7 次（限时手撕） |
| [ ] | #229 | 337 | [打家劫舍 III](https://leetcode.cn/problems/house-robber-iii) | 中等 | 0 | 0 刷重点攻坚 |

---

## Day 33：高阶区间 DP 与多状态机股票

> 对应专题：[[5.2.19 动态规划]] ｜ 核心要点：买卖股票含手续费/冷冻期，大区间合并小区间递推

| 进度 | CodeTop 排名 | 题号 | 题目 | 难度 | 历史通过次数 | 状态建议 |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| [ ] | #237 | 115 | [不同的子序列](https://leetcode.cn/problems/distinct-subsequences) | 困难 | 0 | 0 刷重点攻坚 |
| [ ] | #255 | 188 | [买卖股票的最佳时机 IV](https://leetcode.cn/problems/best-time-to-buy-and-sell-stock-iv) | 困难 | 0 | 0 刷重点攻坚 |
| [ ] | #265 | 862 | [和至少为 K 的最短子数组](https://leetcode.cn/problems/shortest-subarray-with-sum-at-least-k) | 困难 | 1 | 仅 1 刷需巩固 |
| [ ] | #274 | 523 | [连续的子数组和](https://leetcode.cn/problems/continuous-subarray-sum) | 中等 | 8 | 已过 8 次（限时手撕） |
| [ ] | #287 | 680 | [验证回文字符串 Ⅱ](https://leetcode.cn/problems/valid-palindrome-ii) | 简单 | 0 | 0 刷重点攻坚 |
| [ ] | #53 | 43 | [字符串相乘](https://leetcode.cn/problems/multiply-strings) | 中等 | 3 | 已过 3 次（限时手撕） |
| [ ] | #300 | 164 | [最大间距](https://leetcode.cn/problems/maximum-gap) | 困难 | 0 | 0 刷重点攻坚 |

---

## Day 34：字符串模拟与高精度竖式运算

> 对应专题：[[5.2.3 高精度]] ｜ 核心要点：竖式高精度加法乘法模拟，字符串转换整型 atoi 溢出防御

| 进度 | CodeTop 排名 | 题号 | 题目 | 难度 | 历史通过次数 | 状态建议 |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| [ ] | #24 | 54 | [螺旋矩阵](https://leetcode.cn/problems/spiral-matrix) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #27 | 415 | [字符串相加](https://leetcode.cn/problems/add-strings) | 简单 | 7 | 已过 7 次（限时手撕） |
| [ ] | #53 | 43 | [字符串相乘](https://leetcode.cn/problems/multiply-strings) | 中等 | 3 | 已过 3 次（限时手撕） |
| [ ] | #54 | 8 | [字符串转换整数 (atoi)](https://leetcode.cn/problems/string-to-integer-atoi) | 中等 | 2 | 已过 2 次（限时手撕） |
| [ ] | #59 | 151 | [翻转字符串里的单词](https://leetcode.cn/problems/reverse-words-in-a-string) | 中等 | 6 | 已过 6 次（限时手撕） |
| [ ] | #82 | 14 | [最长公共前缀](https://leetcode.cn/problems/longest-common-prefix) | 简单 | 1 | 仅 1 刷需巩固 |
| [ ] | #104 | 468 | [验证IP地址](https://leetcode.cn/problems/validate-ip-address) | 中等 | 1 | 仅 1 刷需巩固 |

---

## Day 35：字符串双指针翻转与模式校验

> 对应专题：[[5.2.15 字符串匹配算法]] ｜ 核心要点：双指针翻转单词，同向双指针压缩，回文中心扩展校验

| 进度 | CodeTop 排名 | 题号 | 题目 | 难度 | 历史通过次数 | 状态建议 |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| [ ] | #123 | 498 | [对角线遍历](https://leetcode.cn/problems/diagonal-traverse) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #134 | 7 | [整数反转](https://leetcode.cn/problems/reverse-integer) | 简单 | 3 | 已过 3 次（限时手撕） |
| [ ] | #143 | 59 | [螺旋矩阵 II](https://leetcode.cn/problems/spiral-matrix-ii) | 中等 | 15 | 已过 15 次（限时手撕） |
| [ ] | #171 | 384 | [打乱数组](https://leetcode.cn/problems/shuffle-an-array) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #177 | 97 | [交错字符串](https://leetcode.cn/problems/interleaving-string) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #193 | 剑指 Offer 29 | [顺时针打印矩阵](https://leetcode.cn/problems/shun-shi-zhen-da-yin-ju-zhen-lcof) | 简单 | 0 | 0 刷重点攻坚 |
| [ ] | #208 | 443 | [压缩字符串](https://leetcode.cn/problems/string-compression) | 中等 | 0 | 0 刷重点攻坚 |

---

## Day 36：矩阵模拟：螺旋收缩与旋转置换

> 对应专题：[[5.2.26 图与图算法]] ｜ 核心要点：矩阵螺旋遍历四边界收缩，顺时针旋转转置翻转，对角线遍历

| 进度 | CodeTop 排名 | 题号 | 题目 | 难度 | 历史通过次数 | 状态建议 |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| [ ] | #209 | 344 | [反转字符串](https://leetcode.cn/problems/reverse-string) | 简单 | 1 | 仅 1 刷需巩固 |
| [ ] | #219 | 378 | [有序矩阵中第K小的元素](https://leetcode.cn/problems/kth-smallest-element-in-a-sorted-matrix) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #224 | 459 | [重复的子字符串](https://leetcode.cn/problems/repeated-substring-pattern) | 简单 | 9 | 已过 9 次（限时手撕） |
| [ ] | #234 | 567 | [字符串的排列](https://leetcode.cn/problems/permutation-in-string) | 中等 | 1 | 仅 1 刷需巩固 |
| [ ] | #245 | 剑指 Offer 48 | [最长不含重复字符的子字符串](https://leetcode.cn/problems/zui-chang-bu-han-zhong-fu-zi-fu-de-zi-zi-fu-chuan-lcof) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #249 | 剑指 Offer 11 | [旋转数组的最小数字](https://leetcode.cn/problems/xuan-zhuan-shu-zu-de-zui-xiao-shu-zi-lcof) | 简单 | 0 | 0 刷重点攻坚 |
| [ ] | #262 | 557 | [反转字符串中的单词 III](https://leetcode.cn/problems/reverse-words-in-a-string-iii) | 简单 | 0 | 0 刷重点攻坚 |

---

## Day 37：哈希表查补数与频次统计

> 对应专题：[[5.2.8 哈希表与计数]] ｜ 核心要点：哈希边查边插，前缀和哈希查余数，连续序列 O(1) 判起点

| 进度 | CodeTop 排名 | 题号 | 题目 | 难度 | 历史通过次数 | 状态建议 |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| [ ] | #270 | 面试题 17.24 | [最大子矩阵](https://leetcode.cn/problems/max-submatrix-lcci) | 困难 | 0 | 0 刷重点攻坚 |
| [ ] | #293 | 面试题 10.03 | [搜索旋转数组](https://leetcode.cn/problems/search-rotate-array-lcci) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #163 | 剑指 Offer 04 | [二维数组中的查找](https://leetcode.cn/problems/er-wei-shu-zu-zhong-de-cha-zhao-lcof) | 简单 | 0 | 0 刷重点攻坚 |
| [ ] | #172 | 剑指 Offer 62 | [圆圈中最后剩下的数字](https://leetcode.cn/problems/yuan-quan-zhong-zui-hou-sheng-xia-de-shu-zi-lcof) | 简单 | 0 | 0 刷重点攻坚 |
| [ ] | #175 | 887 | [鸡蛋掉落](https://leetcode.cn/problems/super-egg-drop) | 困难 | 0 | 0 刷重点攻坚 |
| [ ] | #178 | 剑指 Offer 21 | [调整数组顺序使奇数位于偶数前面](https://leetcode.cn/problems/diao-zheng-shu-zu-shun-xu-shi-qi-shu-wei-yu-ou-shu-qian-mian-lcof) | 简单 | 0 | 0 刷重点攻坚 |
| [ ] | #179 | 400 | [第N个数字](https://leetcode.cn/problems/nth-digit) | 中等 | 0 | 0 刷重点攻坚 |

---

## Day 38：贪心区间调度与边界合并

> 对应专题：[[5.2.17 贪心算法]] ｜ 核心要点：区间按右端点排序求最少重叠，自定义排序拼接字符串最大数

| 进度 | CodeTop 排名 | 题号 | 题目 | 难度 | 历史通过次数 | 状态建议 |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| [ ] | #180 | 611 | [有效三角形的个数](https://leetcode.cn/problems/valid-triangle-number) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #181 | 679 | [24 点游戏](https://leetcode.cn/problems/24-game) | 困难 | 0 | 0 刷重点攻坚 |
| [ ] | #182 | 1004 | [最大连续1的个数 III](https://leetcode.cn/problems/max-consecutive-ones-iii) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #184 | 134 | [加油站](https://leetcode.cn/problems/gas-station) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #185 | 395 | [至少有K个重复字符的最长子串](https://leetcode.cn/problems/longest-substring-with-at-least-k-repeating-characters) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #188 | 253 | [会议室 II](https://leetcode.cn/problems/meeting-rooms-ii) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #190 | 316 | [去除重复字母](https://leetcode.cn/problems/remove-duplicate-letters) | 中等 | 0 | 0 刷重点攻坚 |

---

## Day 39：贪心跳跃与覆盖范围更新

> 对应专题：[[5.2.17 贪心算法]] ｜ 核心要点：维护当前覆盖最远边界，到达边界即触发步数递增

| 进度 | CodeTop 排名 | 题号 | 题目 | 难度 | 历史通过次数 | 状态建议 |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| [ ] | #191 | 264 | [丑数 II](https://leetcode.cn/problems/ugly-number-ii) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #192 | 补充题9 | [36进制加法](https://leetcode.cn/problems/https://mp.weixin.qq.com/s/XcKQwnwCh5nZsz-DLHJwzQ) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #195 | 692 | [前K个高频单词](https://leetcode.cn/problems/top-k-frequent-words) | 中等 | 2 | 已过 2 次（限时手撕） |
| [ ] | #197 | 279 | [完全平方数](https://leetcode.cn/problems/perfect-squares) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #199 | 670 | [最大交换](https://leetcode.cn/problems/maximum-swap) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #201 | 349 | [两个数组的交集](https://leetcode.cn/problems/intersection-of-two-arrays) | 简单 | 2 | 已过 2 次（限时手撕） |
| [ ] | #202 | 剑指 Offer 39 | [数组中出现次数超过一半的数字](https://leetcode.cn/problems/shu-zu-zhong-chu-xian-ci-shu-chao-guo-yi-ban-de-shu-zi-lcof) | 简单 | 0 | 0 刷重点攻坚 |

---

## Day 40：位运算技巧与状态压缩

> 对应专题：[[5.2.2 位运算]] ｜ 核心要点：异或消消乐找单数，Lowbit 取最低位 1，掩码状态压缩

| 进度 | CodeTop 排名 | 题号 | 题目 | 难度 | 历史通过次数 | 状态建议 |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| [ ] | #203 | 补充题14 | [阿拉伯数字转中文数字]() | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #205 | 343 | [整数拆分](https://leetcode.cn/problems/integer-break) | 中等 | 5 | 已过 5 次（限时手撕） |
| [ ] | #206 | 12 | [整数转罗马数字](https://leetcode.cn/problems/integer-to-roman) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #211 | 1044 | [最长重复子串](https://leetcode.cn/problems/longest-duplicate-substring) | 困难 | 0 | 0 刷重点攻坚 |
| [ ] | #214 | 763 | [划分字母区间](https://leetcode.cn/problems/partition-labels) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #215 | 494 | [目标和](https://leetcode.cn/problems/target-sum) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #216 | 剑指 Offer 61 | [扑克牌中的顺子](https://leetcode.cn/problems/bu-ke-pai-zhong-de-shun-zi-lcof) | 简单 | 0 | 0 刷重点攻坚 |

---

## Day 41：数学与随机化采样算法

> 对应专题：[[5.2.1 数学与数论基础]] ｜ 核心要点：Rand7 实现 Rand10 拒绝采样，快速幂与大数取模

| 进度 | CodeTop 排名 | 题号 | 题目 | 难度 | 历史通过次数 | 状态建议 |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| [ ] | #218 | 剑指 Offer 03 | [数组中重复的数字](https://leetcode.cn/problems/shu-zu-zhong-zhong-fu-de-shu-zi-lcof) | 简单 | 0 | 0 刷重点攻坚 |
| [ ] | #220 | 380 | [常数时间插入、删除和获取随机元素](https://leetcode.cn/problems/insert-delete-getrandom-o1) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #221 | 503 | [下一个更大元素 II](https://leetcode.cn/problems/next-greater-element-ii) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #222 | 1444 | [切披萨的方案数](https://leetcode.cn/problems/number-of-ways-of-cutting-a-pizza) | 困难 | 0 | 0 刷重点攻坚 |
| [ ] | #225 | 168 | [Excel表列名称](https://leetcode.cn/problems/excel-sheet-column-title) | 简单 | 1 | 仅 1 刷需巩固 |
| [ ] | #227 | 260 | [只出现一次的数字 III](https://leetcode.cn/problems/single-number-iii) | 中等 | 5 | 已过 5 次（限时手撕） |
| [ ] | #228 | 191 | [位1的个数](https://leetcode.cn/problems/number-of-1-bits) | 简单 | 5 | 已过 5 次（限时手撕） |

---

## Day 42：综合模拟与大厂中档题扫盲

> 对应专题：[[5.1 STL]] ｜ 核心要点：罗马数字转整数、有效数字模拟、旋转数组综合突破

| 进度 | CodeTop 排名 | 题号 | 题目 | 难度 | 历史通过次数 | 状态建议 |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| [ ] | #230 | 剑指 Offer 45 | [把数组排成最小的数](https://leetcode.cn/problems/ba-shu-zu-pai-cheng-zui-xiao-de-shu-lcof) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #231 | 6 | [Z 字形变换](https://leetcode.cn/problems/zigzag-conversion) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #235 | 556 | [下一个更大元素 III](https://leetcode.cn/problems/next-greater-element-iii) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #236 | 1262 | [可被三整除的最大和](https://leetcode.cn/problems/greatest-sum-divisible-by-three) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #243 | 528 | [按权重随机选择](https://leetcode.cn/problems/random-pick-with-weight) | 中等 | 2 | 已过 2 次（限时手撕） |
| [ ] | #253 | 补充题22 | [IP地址与整数的转换](https://leetcode.cn/problems/https://mp.weixin.qq.com/s/UWCuEtNS2kuAuDY-eIbghg) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #256 | 130 | [被围绕的区域](https://leetcode.cn/problems/surrounded-regions) | 中等 | 0 | 0 刷重点攻坚 |

---

## Day 43：高频盲区攻坚与边界扫尾

> 对应专题：[[5.1 STL]] ｜ 核心要点：打乱数组洗牌算法、整数反转溢出判断、综合拔高

| 进度 | CodeTop 排名 | 题号 | 题目 | 难度 | 历史通过次数 | 状态建议 |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| [ ] | #259 | 674 | [最长连续递增序列](https://leetcode.cn/problems/longest-continuous-increasing-subsequence) | 简单 | 0 | 0 刷重点攻坚 |
| [ ] | #260 | 167 | [两数之和 II - 输入有序数组](https://leetcode.cn/problems/two-sum-ii-input-array-is-sorted) | 简单 | 0 | 0 刷重点攻坚 |
| [ ] | #264 | 525 | [连续数组](https://leetcode.cn/problems/contiguous-array) | 中等 | 2 | 已过 2 次（限时手撕） |
| [ ] | #267 | 977 | [有序数组的平方](https://leetcode.cn/problems/squares-of-a-sorted-array) | 简单 | 11 | 已过 11 次（限时手撕） |
| [ ] | #269 | 268 | [缺失数字](https://leetcode.cn/problems/missing-number) | 简单 | 3 | 已过 3 次（限时手撕） |
| [ ] | #273 | 386 | [字典序排数](https://leetcode.cn/problems/lexicographical-numbers) | 中等 | 0 | 0 刷重点攻坚 |
| [ ] | #277 | 238 | [除自身以外数组的乘积](https://leetcode.cn/problems/product-of-array-except-self) | 中等 | 3 | 已过 3 次（限时手撕） |

---

## Day 44：大厂必考巅峰手撕押题复盘

> 对应专题：[[5.1 STL]] ｜ 核心要点：全真白板冲刺：手撕快排、手撕堆排、LRU、K个一组、接雨水、拓扑排序、合并K个链表

| 进度 | CodeTop 排名 | 题号 | 题目 | 难度 | 历史通过次数 | 状态建议 |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| [ ] | #278 | 706 | [设计哈希映射](https://leetcode.cn/problems/design-hashmap) | 简单 | 0 | 0 刷重点攻坚 |
| [ ] | #279 | 204 | [计数质数](https://leetcode.cn/problems/count-primes) | 简单 | 2 | 已过 2 次（限时手撕） |
| [ ] | #280 | 13 | [罗马数字转整数](https://leetcode.cn/problems/roman-to-integer) | 简单 | 2 | 已过 2 次（限时手撕） |
| [ ] | #285 | 554 | [砖墙](https://leetcode.cn/problems/brick-wall) | 中等 | 1 | 仅 1 刷需巩固 |
| [ ] | #286 | 340 | [至多包含 K 个不同字符的最长子串](https://leetcode.cn/problems/longest-substring-with-at-most-k-distinct-characters) | 困难 | 0 | 0 刷重点攻坚 |
| [ ] | #288 | 233 | [数字 1 的个数](https://leetcode.cn/problems/number-of-digit-one) | 困难 | 1 | 仅 1 刷需巩固 |
| [ ] | #297 | 166 | [分数到小数](https://leetcode.cn/problems/fraction-to-recurring-decimal) | 中等 | 3 | 已过 3 次（限时手撕） |

---

