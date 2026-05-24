---
title: AI Infra 岗算法笔试保底清单
date: 2026-05-24
tags:
  - infra
  - 面试
  - 算法
roadmap_week: "求职全程"
sort_order: "99.00"
status: active
---

# AI Infra 岗算法笔试保底清单

> [!info] 所属路线
> - 总纲 Week：求职全程
> - 排序：99.00
> - 用途：不改变 AI Infra 主线，只作为笔试通过率保底。

> [!goal] 目标
> 这篇笔记不是改变 [[AI Agent Native AI Infra GPU Performance Engineer 培养方案]] 的主线，而是补求职落地风险：AI Infra 方向面试可以靠 CUDA / 推理系统拉开差距，但笔试不过就没有后续。

---

## 1. 保底原则

不要把算法训练变成主线，但要保证：

```text
常见 DP / 二分 / 树 DFS / 图搜索 / 堆 / 双指针 / 前缀和
```

遇到时能快速识别模型，并写出稳定代码。

每周最低投入：

- 2 次，每次 45-60 分钟。
- 每次只练一个模型。
- 做完必须写“识别信号”和“模板”。

---

## 2. DP 保底

常见识别信号：

- “最小代价 / 最大收益”。
- “相邻不能相同”。
- “前 i 个状态”。
- “选或不选”。
- “两个机器 / 两个位置 / 另一台设备状态”。

最低模型：

| 类型 | 状态设计 |
|---|---|
| 线性 DP | `dp[i]` 表示前 i 个的最优值 |
| 颜色 DP | `dp[i][color]` 表示第 i 个选 color 的最小代价 |
| 背包 DP | `dp[j]` 表示容量 / 目标为 j 的最优值 |
| 区间 DP | `dp[l][r]` 表示区间最优 |
| 记忆化搜索 | `dfs(state)` + cache |

颜色 DP 模板：

```python
colors = range(k)
dp = [0] * k
for i in range(n):
    ndp = [10**30] * k
    for c in colors:
        cost = get_cost(i, c)
        ndp[c] = min(dp[p] for p in colors if p != c) + cost
    dp = ndp
ans = min(dp)
```

---

## 3. 二分保底

识别信号：

- “第 k 个”。
- “最小满足”。
- “最大可行”。
- “答案具有单调性”。
- “暴力枚举答案会超时”。

答案二分模板：

```python
def check(x):
    return True  # x 是否满足条件

l, r = 0, upper
while l < r:
    mid = (l + r) // 2
    if check(mid):
        r = mid
    else:
        l = mid + 1
print(l)
```

第 k 个缺失正整数常见判断：

```text
missing_count(x) = x - count(a_i <= x)
找到最小 x，使 missing_count(x) >= k
```

---

## 4. 树 DFS 保底

常见任务：

- 子树大小。
- 子树最大值 / 最小值。
- 子树是否满足某条件。
- 树形 DP。
- 后序合并子节点信息。

递归模板：

```python
import sys
sys.setrecursionlimit(300000)

def dfs(u, parent):
    size = 1
    mn = mx = value[u]
    for v in g[u]:
        if v == parent:
            continue
        s, a, b = dfs(v, u)
        size += s
        mn = min(mn, a)
        mx = max(mx, b)
    return size, mn, mx
```

后序合并直觉：

```text
先拿子树信息
-> 合并到当前节点
-> 判断当前子树是否满足条件
-> 返回给父节点
```

---

## 5. Python 递归爆栈处理

优先顺序：

1. 先加：

```python
import sys
sys.setrecursionlimit(300000)
```

2. 如果还是不稳，改 C++。
3. 如果必须 Python，再考虑迭代 DFS。

迭代后序 DFS 模板：

```python
parent = [-1] * n
order = [0]
for u in order:
    for v in g[u]:
        if v == parent[u]:
            continue
        parent[v] = u
        order.append(v)

for u in reversed(order):
    # 在这里合并子节点到 u
    pass
```

这比临场写复杂 generator bootstrap 更稳。

---

## 6. 图搜索保底

| 场景 | 方法 |
|---|---|
| 无权最短路 | BFS |
| 连通块 | DFS / BFS |
| 拓扑依赖 | topological sort |
| 正权最短路 | Dijkstra |
| 网格搜索 | BFS / DFS + dirs |

网格方向模板：

```python
dirs = (-1, 0, 1, 0, -1)
for d in range(4):
    ni = i + dirs[d]
    nj = j + dirs[d + 1]
```

---

## 7. 堆 / 前缀和 / 双指针保底

| 模型 | 识别信号 |
|---|---|
| heap | 动态最大/最小、top-k、合并多个有序流 |
| prefix sum | 区间和、频繁查询、子数组和 |
| two pointers | 排序数组、滑动窗口、最长/最短连续区间 |
| monotonic stack | 下一个更大/更小、矩形面积 |
| union-find | 连通性、合并集合 |

---

## 8. 竞赛输入输出保底

```python
import sys
input = sys.stdin.readline
```

多组测试：

```python
T = int(input())
for _ in range(T):
    solve()
```

常见风险：

- 忘记去重。
- `mid = l + r >> 1` 可读性差，建议写 `(l + r) // 2`。
- 递归没设深度。
- `lru_cache` 用完多组测试没清。
- Python 常数过大，树/图大数据应考虑 C++。

---

## 9. 一周保底训练安排

| 次数 | 主题 | 产出 |
|---|---|---|
| 第 1 次 | DP + 二分 | 各做 2 题，写识别信号 |
| 第 2 次 | 树 DFS + 图 BFS | 各做 2 题，写模板 |
| 第 3 次，可选 | 堆 / 前缀和 / 双指针 | 做 3 题，补边界 case |

---

## 10. 自测问题

1. 看到“相邻不能相同 + 最小代价”能否立刻想到颜色 DP？
2. 看到“第 k 个不在数组中的数”能否想到答案二分？
3. 树上子树问题能否用后序 DFS 合并信息？
4. Python DFS 会不会爆栈？如何改成迭代后序？
5. 多组测试下 cache、全局变量、输入输出是否会污染？
