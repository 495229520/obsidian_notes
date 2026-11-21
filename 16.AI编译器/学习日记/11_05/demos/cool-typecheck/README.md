# COOL 类型规则的 C++17 实现

把截图中的推导规则写成能运行的类型检查器。输入是 AST，输出是表达式的静态类型；违反规则时抛出 `TypeError`。每个成功检查的表达式还会把类型记录到 `Expr::inferred`。

## 文件与运行

| 文件 | 内容 |
| --- | --- |
| [cool.hpp](cool.hpp) | AST、类和方法声明、检查器接口 |
| [cool.cpp](cool.cpp) | 类型环境、子类型、公共祖先、各条规则的实现 |
| [main.cpp](main.cpp) | 截图示例、正反例检查、case 分支选择演示 |

在本机 WSL / Ubuntu 中运行：

```bash
cd '/mnt/d/obsidian/C++/16.AI编译器/11_05/demos/cool-typecheck'
g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror cool.cpp main.cpp -o /tmp/cool-typecheck
/tmp/cool-typecheck
```

Windows 安装了 GCC 并配置好 PATH 时，也可以在本目录的 PowerShell 中运行：

```powershell
g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror cool.cpp main.cpp -o cool-typecheck.exe
if ($LASTEXITCODE -eq 0) { ./cool-typecheck.exe }
```

2026-11-12 使用 WSL Ubuntu 的 GCC 13.3.0 实测：无编译警告，`108 passed, 0 failed`。程序先打印截图示例，再运行检查；有失败项时退出码为 1。Windows 原生编译未实测。

## 公式中的 O、M、C 对应什么

```text
ClassTable：先收集所有类、属性、方法签名，验证继承和声明
    |
    +-- M(C, f) -> ClassTable::method(C, f)
    +-- 子类型  -> ClassTable::conforms(actual, expected, C)
    +-- lub     -> ClassTable::join(left, right, C)

TypeChecker：固定当前类 C，递归检查 AST
    |
    +-- O -> Environment，变量名到声明类型的映射
    +-- C -> current_
    +-- O, M, C |- e : T -> check(e, env)，返回 T
```

为便于阅读，`Environment` 使用 `std::map`。进入 let 或 case 分支时复制旧环境，再覆盖局部绑定，直接表达 `O[T/x]`。因此异常退出也不会污染外层作用域。大型编译器可以改为作用域栈；这里不需要额外的栈管理代码。

`SELF_TYPE` 在一个检查器内表示 `SELF_TYPE_C`，不是普通类名。检查器的当前类固定，因此不另建带下标的类型对象。方法签名中的 `SELF_TYPE` 则要在调用位置替换为接收者的静态类型。

## 截图规则与实现对照

| 截图中的规则 | cool.cpp 中的位置 | 检查重点 |
| --- | --- | --- |
| Int、String、True、False | 对应字面量的 `infer` 重载 | 字面量类型 |
| Var | `infer(Variable, ...)` | 从当前环境查声明 |
| New | `infer(New, ...)` | 类型存在；允许 SELF_TYPE |
| Arith | `infer(Binary, ...)` | 两边都是 Int，结果为 Int |
| Not、Compare、Equal | `infer(Unary, ...)`、`infer(Binary, ...)` | not 检查 Bool；比较检查 Int；相等检查基本类型限制 |
| Loop | `infer(Loop, ...)` | 检查条件和循环体，结果为 Object |
| Let-No-Init、Let-Init、同名绑定 | `infer(Let, ...)` | 初始值使用旧环境，主体使用扩展环境 |
| Assign | `infer(Assign, ...)` | RHS 符合声明类型，表达式返回 RHS 类型 |
| Attr-Init、Attr-No-Init | `checkClass()` | 属性表预先收集；有初始化式时检查兼容性 |
| If | `infer(If, ...)` | 两个分支主体类型的 join |
| Case、分支主体类型不同 | `infer(Case, ...)` | 分支作用域隔离，合并所有主体类型 |
| case 执行过程 | `selectCaseBranch()` | 从对象动态类向上查找最近的分支类型 |
| Dispatch、Static Dispatch | `infer(Dispatch, ...)` | 方法查找、参数数量和类型、接收者兼容性 |
| Method、self、SELF_TYPE | `checkClass()`、`conforms()` | 方法体符合返回声明，保持 SELF_TYPE 的约束 |
| 返回 SELF_TYPE 的调用 | `infer(Dispatch, ...)` | 返回接收者静态类型 |

还实现了方法体常用的 Block、整数取负 `~` 和 `isvoid`，以及 Object、IO、String 的内建方法签名。

## 添加自己的表达式

下面是 `let x : Bool <- x < 3 in x`。外层 `x : Int` 来自 Main 的属性；初始化式中的 x 必须读取外层绑定。

```cpp
ClassTable classes({
    {"Main", "Object", {{"x", "Int", nullptr}}, {}}
});
TypeChecker checker(classes, "Main");

auto initializer = expr(Binary{
    BinaryOp::Less,
    expr(Variable{"x"}),
    expr(IntLiteral{3})
});
auto body = expr(Variable{"x"});
auto expression = expr(Let{"x", "Bool", initializer, body});

Type result = checker.check(expression); // Bool
// initializer 的 x : Int；body 的 x : Bool；外层 x 仍然是 Int。
```

这些类型是检查得出的，不是把预期答案写进 AST。`main.cpp` 的预期类型仅用于核验检查结果。

若要检查类中的属性初始化式和方法体，构造 `ClassTable` 后调用 `checkProgram(classes)`。单独调用 `checker.check(expression)` 只检查这个表达式。

## 静态检查与执行边界

- 不含词法分析器、语法分析器或代码生成器，不能把 `.cl` 文件直接作为输入。AST 的结构由调用者或未来的解析器提供；标识符拼写等词法规则不在这里检查。
- `checkProgram` 检查提供的类片段，不要求存在完整可执行程序的 `Main.main()` 入口。
- `case` 的运行示例只实现分支选择，返回下标，不求值分支主体。调用前应先通过静态检查；void 或无匹配分支会抛出运行时异常。
- 未初始化的属性和 let 变量具有声明的静态类型。本例不分配对象或执行默认值初始化，也不执行 while 和方法调用。
- 使用遇错即停的异常诊断，不实现多错误恢复。失败检查后的 AST 可能只带有部分类型标注；只有检查成功的树才能交给后续阶段。
- 每个语法位置应创建独立的 `Expr` 节点。`inferred` 中的 SELF_TYPE 需要结合所属类解释，不能脱离检查上下文当成普通类使用。

规则依据：[Cool Reference Manual](https://theory.stanford.edu/~aiken/software/cool/cool-manual.pdf) §12.2；case 分支选择见 §7.9。特别留意 §12.2 的静态派发公式：返回声明为 SELF_TYPE 时，结果取接收者类型 T0，不能直接取 `@T` 中的 T。
