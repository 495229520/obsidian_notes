---
tags:
  - 指针/智能指针
  - 基础语法
阅读次数: 0
---

# 弱引用智能指针 (weak_ptr)

> `std::weak_ptr` 是 C++ 标准库中的一个智能指针，它用于解决 [[共享智能指针|shared_ptr]] 的**循环引用**问题。

---

## 简介

`weak_ptr` 是一种**弱引用**，它指向一个由 `shared_ptr` 管理的对象，但**不增加对象的引用计数**，也**不拥有资源的所有权**。

---

## 核心特点

### 不参与引用计数

- `weak_ptr` 的存在不会影响其指向的对象的生命周期
- 即使有很多 `weak_ptr` 指向一个对象，只要没有 `shared_ptr` 指向它，该对象仍然会被销毁

### 安全性

- 由于 `weak_ptr` 指向的对象可能在任何时候被销毁，`weak_ptr` 本身**不能直接访问**对象
- 要使用它指向的对象，你必须先调用 `lock()` 方法来获取一个 `std::shared_ptr`

### lock() 方法

- 如果 `weak_ptr` 指向的对象**仍然存在**，`lock()` 会返回一个有效的 `std::shared_ptr`
- 如果 `weak_ptr` 指向的对象**已经被销毁**，`lock()` 会返回一个空的 `std::shared_ptr`

### expired() 方法

- 用于判断 `weak_ptr` 所指向的对象是否已经被销毁
- 如果已销毁，`expired()` 返回 `true`

> **因为这种特性，`std::weak_ptr` 常用于打破 `shared_ptr` 的循环引用。**

---

## 案例：解决循环引用

### 问题场景

```cpp
#include <iostream>
#include <memory>

class Parent;  // 前置声明

class Child {
public:
    std::weak_ptr<Parent> parent_ptr;  // 使用 weak_ptr 避免循环引用
    ~Child() {
        std::cout << "Child destroyed." << std::endl;
    }
    void use_parent() {
        if (auto p = parent_ptr.lock()) {  // 尝试获取 shared_ptr
            std::cout << "Child successfully accessed parent." << std::endl;
        } else {
            std::cout << "Parent has been destroyed." << std::endl;
        }
    }
};

class Parent {
public:
    std::shared_ptr<Child> child_ptr;  // 使用 shared_ptr
    ~Parent() {
        std::cout << "Parent destroyed." << std::endl;
    }
};

void create_family() {
    auto parent = std::make_shared<Parent>();  // parent 引用计数: 1
    auto child = std::make_shared<Child>();    // child 引用计数: 1

    // 建立父子关系
    parent->child_ptr = child;       // child 引用计数: 2
    child->parent_ptr = parent;      // weak_ptr 不增加引用计数

    // 此时，parent 引用计数为 1，child 引用计数为 2。
}  // 作用域结束

int main() {
    create_family();
    std::cout << "-----------------------------------------" << std::endl;
    std::cout << "End of main. Objects should have been destroyed." << std::endl;
    return 0;
}
```

### 输出结果

```
Child destroyed.
Parent destroyed.
-----------------------------------------
End of main. Objects should have been destroyed.
```

### 分析

1. `create_family()` 函数结束时，`parent` 和 `child` 这两个 `shared_ptr` 会被销毁
2. `parent` 被销毁，它所持有的 `child_ptr` 引用计数从 2 降到 1
3. `child` 被销毁，它所持有的 `parent_ptr` 引用计数从 1 降到 0，`parent` 对象被销毁
4. `parent` 销毁后，其内部的 `child_ptr` 也被销毁，`child` 的引用计数从 1 降到 0，`child` 对象被销毁

**通过使用 `weak_ptr`，我们成功地避免了循环引用，并确保了两个对象都能被正确地销毁，防止了内存泄漏。**

---

## 如果不使用 weak_ptr（循环引用问题）

```cpp
class Child {
public:
    std::shared_ptr<Parent> parent_ptr;  // 两边都用 shared_ptr
    // ...
};

class Parent {
public:
    std::shared_ptr<Child> child_ptr;
    // ...
};
```

**问题**：
- `parent` 持有 `child` 的 `shared_ptr`
- `child` 持有 `parent` 的 `shared_ptr`
- 两者引用计数都不会归零
- **内存永远不会被释放 → 内存泄漏！**

---

## weak_ptr 的常用方法

| 方法 | 说明 |
|------|------|
| `lock()` | 尝试获取一个指向对象的 `shared_ptr`，如果对象已销毁则返回空 |
| `expired()` | 检查对象是否已被销毁，返回 `true` 表示已销毁 |
| `use_count()` | 返回指向对象的 `shared_ptr` 数量（用于调试） |
| `reset()` | 释放对对象的引用 |

---

## 使用示例

```cpp
#include <iostream>
#include <memory>

int main() {
    std::shared_ptr<int> sp = std::make_shared<int>(42);
    std::weak_ptr<int> wp = sp;  // weak_ptr 指向 sp 管理的对象

    std::cout << "use_count: " << wp.use_count() << std::endl;  // 1
    std::cout << "expired: " << wp.expired() << std::endl;      // 0 (false)

    if (auto locked = wp.lock()) {
        std::cout << "Value: " << *locked << std::endl;  // 42
    }

    sp.reset();  // 释放 shared_ptr

    std::cout << "After reset:" << std::endl;
    std::cout << "expired: " << wp.expired() << std::endl;  // 1 (true)

    if (auto locked = wp.lock()) {
        std::cout << "Value: " << *locked << std::endl;
    } else {
        std::cout << "Object has been destroyed." << std::endl;
    }

    return 0;
}
```

---

## 关键要点

1. **不增加引用计数**：`weak_ptr` 不影响对象生命周期
2. **不能直接访问对象**：必须通过 `lock()` 获取 `shared_ptr`
3. **主要用途**：打破 `shared_ptr` 的循环引用
4. **安全检查**：使用 `expired()` 或 `lock()` 检查对象是否存在
5. **常见模式**：父对象用 `shared_ptr` 指向子对象，子对象用 `weak_ptr` 指向父对象

---

## 相关链接

- [[共享智能指针]] - `shared_ptr`：共享所有权的智能指针
- [[独享智能指针]] - `unique_ptr`：独占所有权的智能指针
- [[6.8 指针安全]] - 智能指针概述和循环引用问题
- [[6.3 动态内存分配的风险]] - 内存泄漏等问题
