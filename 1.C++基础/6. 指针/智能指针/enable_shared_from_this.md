---
tags:
  - Cpp基础
  - 指针
  - 智能指针
阅读次数: 0
---

> [!abstract] enable_shared_from_this 速记卡片
> **一句话**：让类对象在成员函数内部能够安全获取管理自身的 `std::shared_ptr`，避免由裸指针 `this` 构造产生第二份控制块导致 Double Free。
>
> | 维度 | 关键要点 |
> | --- | --- |
> | **核心机理** | 采用 CRTP 继承；基类内置一个 `mutable std::weak_ptr<T> _M_weak_this`，初次被 `shared_ptr` 托管时挂载同一个控制块 |
> | **调用本质** | `shared_from_this()` 本质上调用 `_M_weak_this.lock()`，使原控制块的强引用计数原子递增 |
> | **前置约束** | 调用前对象**必须已经被外部 shared_ptr 托管**；在构造函数内部或对栈对象调用会直接抛出 `std::bad_weak_ptr` |
> | **异步经典范式** | 将 `shared_from_this()` 按值捕获进异步操作的 Lambda 闭包中，实现“I/O 未完成前对象绝不析构”的生命周期续命 |
> | **防踩坑设计** | 将构造函数设为 `private` / `protected`，仅暴露返回 `std::shared_ptr<T>` 的静态工厂方法 `create()` |
> | **C++17 补充** | 提供 `weak_from_this()`，在观察者或长周期定时器中直接获取弱指针，防止无意延长生命周期或形成循环引用 |

![[图片/SVG/enable_shared_from_this-机制与双重控制块.svg|900]]

---

## 1. 核心问题：为什么严禁 shared_ptr(this)

### 裸指针缺乏控制块元数据

在类成员函数内部，`this` 只是一个普通的 8 字节裸指针（内存地址）。C++ 运行时并不会在全局维护“哪个内存地址当前归哪个控制块管”的映射表。

当外部已经使用 `std::shared_ptr` 管理该对象时，堆上已经存在一份控制块 A：

```cpp
auto sp1 = std::make_shared<Session>(); // 控制块 A 产生，shared_count = 1
```

如果此时在成员函数内部直接用 `this` 构造新的智能指针：

```cpp
void Session::doSomething() {
    auto sp2 = std::shared_ptr<Session>(this); // 严重错误
}
```

`std::shared_ptr` 的裸指针构造函数 `explicit shared_ptr(Y* p)` 无法感知到外部 `sp1` 与控制块 A 的存在。它只能把 `this` 当作一个刚刚分配、未经管理的全新对象，在堆上分配**第二份独立的控制块 B**（`shared_count = 1`）。

### 双重控制块的崩溃时序（Double Free）

两个控制块彼此完全隔离，却指向同一块内存地址。其生命周期交错会导致致命的二次释放：

```cpp
#include <iostream>
#include <memory>

class Session {
public:
    void registerSelf() {
        // sp2 接管 this，创建独立的控制块 B（计数 = 1）
        std::shared_ptr<Session> sp2(this);
        std::cout << "sp2 use_count: " << sp2.use_count() << "\n";
    } // 1. 函数结束，sp2 析构：控制块 B 归零，触发 delete this。对象内存已被释放！

    ~Session() {
        std::cout << "~Session() 析构\n";
    }
};

int main() {
    auto sp1 = std::make_shared<Session>(); // 控制块 A 计数 = 1
    sp1->registerSelf();

    // 2. 此时底层对象已经被 sp2 提前销毁！
    // 外部 sp1 变成了悬空指针（Dangling Pointer），处于 Use-After-Free 状态。

    std::cout << "main 准备退出\n";
    return 0;
    // 3. main 结束，sp1 析构：控制块 A 计数降为 0，再次对已释放的地址执行 delete this。
    // 运行时直接崩溃：free(): double free detected / SIGABRT
}
```

---

## 2. enable_shared_from_this 的底层工作机制

要解决这个问题，必须在对象内部建立一条通往已有控制块的“反向通道”。标准库通过 CRTP（奇异递归模板模式）实现了这一机制。

### 内部结构与成员变量

在主流标准库实现（如 GCC libstdc++、LLVM libc++、MSVC STL）中，`std::enable_shared_from_this<T>` 的核心骨架如下：

```cpp
template <typename T>
class enable_shared_from_this {
    // 内部持有一个弱指针，指向管理自身控制块
    mutable std::weak_ptr<T> _M_weak_this;

protected:
    constexpr enable_shared_from_this() noexcept {}
    enable_shared_from_this(const enable_shared_from_this&) noexcept {}
    enable_shared_from_this& operator=(const enable_shared_from_this&) noexcept { return *this; }
    ~enable_shared_from_this() = default;

public:
    std::shared_ptr<T> shared_from_this() {
        // 本质是通过内置的 weak_ptr 升格为 shared_ptr
        return std::shared_ptr<T>(_M_weak_this);
    }

    std::shared_ptr<const T> shared_from_this() const {
        return std::shared_ptr<const T>(_M_weak_this);
    }

    // C++17 引入：直接返回 weak_ptr，避免无谓的强引用原子递增
    std::weak_ptr<T> weak_from_this() noexcept {
        return _M_weak_this;
    }

    std::weak_ptr<const T> weak_from_this() const noexcept {
        return _M_weak_this;
    }

    // 声明 shared_ptr 为友元，允许其在构造时初始化 _M_weak_this
    template <typename U> friend class std::shared_ptr;
};
```

### 控制块的挂载时机（Mounting）

对象刚通过 `new` 被分配出来时，基类子对象中的 `_M_weak_this` 是空的。

当第一任 `std::shared_ptr` 接管该裸指针时（无论通过 `make_shared` 还是 `shared_ptr(raw)` 构造）：

1. `std::shared_ptr` 的构造函数内部包含编译期类型检测（通过 SFINAE、`if constexpr` 或辅助模板函数 `__enable_shared_from_this_helper`）。
2. 检测目标类型 `T` 是否公有继承自 `std::enable_shared_from_this<T>`。
3. 如果满足条件，并且 `_M_weak_this.expired()` 为真，构造函数会自动将新生成的控制块与指针绑定到 `_M_weak_this` 上。

这一步将内部的弱指针悄悄“锚定”到了首个控制块上，**且不增加强引用计数**。

### shared_from_this() 的调用成本

当后续在类成员函数中调用 `shared_from_this()` 时：
- 它直接调用 `_M_weak_this.lock()`。
- `lock()` 检查既有控制块的强引用计数是否大于 0。
- 若存活，原子递增该控制块的 `shared_count` 并返回一个新的 `shared_ptr`。
- **整个过程零额外堆内存分配**，仅产生一次 CAS 原子递增操作，所有返回的智能指针完全共享同一个控制块。

---

## 3. 异步网络工程中的生命周期接力

在异步系统（如 Reactor 模式、Boost.Asio、muduo、RPC 框架）中，`enable_shared_from_this` 是处理对象生命周期最关键的基础设施。

### 核心矛盾：时空分离

在同步代码中，调用者阻塞等待函数返回，调用栈天然保证了对象的生命周期：

```cpp
// 同步模型：函数未返回前，this 指向的对象必定有效
void Session::syncRead() {
    auto data = socket_.read();
    handleData(data);
}
```

但在异步模型中，操作的发起与回调的执行在时间与空间上彻底解耦：

```cpp
void TcpConnection::doAsyncRead() {
    // 发起异步调用后，doAsyncRead() 瞬间返回退出
    async_read_some([this](const std::string& data) {
        handleData(data); // 严重隐患！
    });
}
```

如果在发起读取到数据真正到达的 100 毫秒间隙内，外部客户端主动断开、或者心跳超时导致上层管理容器将该 `TcpConnection` 移除：
- 外部所有持有该连接的 `shared_ptr` 全部析构。
- `TcpConnection` 内存被系统释放。
- 100 毫秒后内核事件触发，回调函数执行 `this->handleData(data)`，访问已被回收的内存，进程直接崩溃。

### 解决方案：Lambda 按值捕获 self 续命

```cpp
class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
public:
    void doAsyncRead() {
        // 1. 取得自身的 shared_ptr，强引用计数原子 +1
        auto self = shared_from_this();

        async_read_some([self](const std::string& data) {
            // 2. self 作为闭包成员变量存活于事件循环队列中
            // 只要回调未执行，TcpConnection 就绝对不会被析构
            self->handleData(data);
        });
    }

    void handleData(const std::string& data) {
        // 安全处理业务数据
    }
};
```

#### 闭包底层的生命周期流转

编译器将 `[self](const std::string& data) { ... }` 展开为一个仿函数类，内部保存了 `self` 的副本：

```text
外部连接断开 ──► 外部 shared_ptr 析构 ──► shared_count 从 2 降为 1（仍 > 0）
                                              │
                                              ▼ (对象依然完好存活在堆上)
网络数据到达 ──► 事件循环执行 Lambda ──► 安全调用 self->handleData()
                                              │
                                              ▼
Lambda 执行完毕 ──► 事件循环丢弃该闭包 ──► 闭包析构，内部 self 析构
                                              │
                                              ▼
                                   shared_count 归零 ──► 执行 delete this 平稳释放
```

这就是经典的**生命周期延长（Lifetime Extension）模式**：只要异步操作未完结，闭包就持有对象的强所有权；操作一旦完结，对象顺手完成自我回收。

### 避免延长过头：使用 weak_from_this()

若异步任务是长周期定时器或只读监控，强行捕获 `shared_from_this()` 会导致即便连接已经关闭，对象依然被迫活着直到定时器触发（伪内存泄漏）。

此时应使用 C++17 的 `weak_from_this()`：

```cpp
void TcpConnection::startHeartbeatTimer() {
    // 捕获弱指针，不增加强引用计数，不阻碍对象正常销毁
    auto weak_self = weak_from_this();

    timer_.async_wait([weak_self](const boost::system::error_code& ec) {
        if (auto self = weak_self.lock()) {
            // 对象仍活着，安全执行心跳
            self->sendHeartbeat();
        } else {
            // 外部已销毁该连接，无需做任何事，静默退出
        }
    });
}
```

---

## 4. 典型陷阱与防御型设计

### 陷阱 1：在构造函数中调用 shared_from_this()

在对象构造函数执行期间，外部的 `std::shared_ptr` 根本还没构造完成，底层控制块与 `_M_weak_this` 的挂载动作尚未发生：

```cpp
class BadSession : public std::enable_shared_from_this<BadSession> {
public:
    BadSession() {
        // 抛出 std::bad_weak_ptr 异常！
        auto self = shared_from_this();
    }
};

int main() {
    auto p = std::make_shared<BadSession>(); // 直接抛出异常
}
```

**对策**：将注册逻辑剥离到独立的 `init()` 或 `start()` 成员函数中，在对象完全构造并被 `shared_ptr` 包裹后显式调用。

### 陷阱 2：栈对象或裸 new 对象调用

如果对象定义在栈上，或者直接通过裸指针 `new` 出来而未交由 `shared_ptr` 管理：

```cpp
Session s;
s.registerSelf(); // 抛出 std::bad_weak_ptr

Session* p = new Session();
p->registerSelf(); // 抛出 std::bad_weak_ptr
```

### 防御范式：私有化构造函数 + 工厂方法

在工程上，如果一个类依赖 `enable_shared_from_this`，应当从语法层面杜绝使用者在栈上或用裸指针创建对象：

```cpp
class SafeSession : public std::enable_shared_from_this<SafeSession> {
private:
    // 1. 私有化构造函数
    SafeSession(int id) : id_(id) {}

    // 如果想使用 std::make_shared，需要将 std::make_shared 设为友元，
    // 或定义一个私有的 Token 结构体（Passkey 模式）
    struct PassKey { explicit PassKey() = default; };

public:
    // 允许 make_shared 通过 PassKey 调用公开构造
    SafeSession(PassKey, int id) : id_(id) {}

    // 2. 统一通过静态工厂方法创建，强制返回 shared_ptr
    static std::shared_ptr<SafeSession> create(int id) {
        return std::make_shared<SafeSession>(PassKey{}, id);
    }

    void start() {
        auto self = shared_from_this(); // 100% 安全，绝不可能抛异常
    }

private:
    int id_;
};

int main() {
    // SafeSession s(1); // 编译错误：构造函数私有
    auto session = SafeSession::create(101); // 唯一合法入口
    session->start();
}
```

---

## 5. 完整工程范式代码

以下为可编译运行的完整异步会话模型示范：

```cpp
#include <iostream>
#include <memory>
#include <functional>
#include <vector>

// 模拟简易事件循环 / 异步队列
class EventLoop {
public:
    using Task = std::function<void()>;
    void post(Task task) {
        taskQueue_.push_back(std::move(task));
    }
    void runAll() {
        for (auto& task : taskQueue_) {
            if (task) task();
        }
        taskQueue_.clear();
    }
private:
    std::vector<Task> taskQueue_;
};

EventLoop g_loop;

class ServiceSession : public std::enable_shared_from_this<ServiceSession> {
private:
    struct ConstructToken {};
    int sessionId_;

public:
    // 仅允许工厂方法借助 Token 构造
    ServiceSession(ConstructToken, int id) : sessionId_(id) {
        std::cout << "[构造] Session " << sessionId_ << "\n";
    }

    ~ServiceSession() {
        std::cout << "[析构] Session " << sessionId_ << " 内存释放\n";
    }

    static std::shared_ptr<ServiceSession> create(int id) {
        return std::make_shared<ServiceSession>(ConstructToken{}, id);
    }

    // 强所有权延续：耗时异步 I/O 任务
    void sendAsync(const std::string& msg) {
        g_loop.post([self = shared_from_this(), msg] {
            std::cout << "[执行任务] Session " << self->sessionId_
                      << " 发送数据: " << msg
                      << " (强引用数: " << self.use_count() << ")\n";
        });
    }

    // 弱观察机制：定时状态监控
    void scheduleStatusCheck() {
        g_loop.post([weakSelf = weak_from_this()] {
            if (auto self = weakSelf.lock()) {
                std::cout << "[状态检测] Session " << self->sessionId_ << " 活跃中\n";
            } else {
                std::cout << "[状态检测] 目标 Session 已销毁，忽略操作\n";
            }
        });
    }
};

int main() {
    {
        auto session = ServiceSession::create(1);

        session->sendAsync("Hello Reactor");
        session->scheduleStatusCheck();

        std::cout << "作用域退出前，外部强引用数: " << session.use_count() << "\n";
    } // session 离开作用域，外部引用归零，但任务队列中的闭包维持其存活

    std::cout << "--- 外部强引用已失效，开始驱动事件循环 ---\n";
    g_loop.runAll();
    std::cout << "--- 所有异步任务结束 ---\n";

    return 0;
}
```

**运行输出**：

```text
[构造] Session 1
作用域退出前，外部强引用数: 2
--- 外部强引用已失效，开始驱动事件循环 ---
[执行任务] Session 1 发送数据: Hello Reactor (强引用数: 1)
[状态检测] Session 1 活跃中
[析构] Session 1 内存释放
--- 所有异步任务结束 ---
```

---

## 相关笔记

- [[共享智能指针]]：了解 `shared_ptr` 的引用计数原理与内存布局
- [[弱引用智能指针]]：了解 `weak_ptr` 如何观察对象以及与控制块生命周期的关系
- [[独享智能指针]]：对比独占所有权模型与生命周期语义
