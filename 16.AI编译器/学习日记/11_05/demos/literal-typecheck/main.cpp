#include <iostream>
#include <string>
#include <map>
#include <windows.h>

// 节点怎么选（左边是 Cool 源码，右边是 C++ 构造写法）：
// 42              -> IntLiteral{42}：整数常量。
// "charge"        -> StringLiteral{"charge"}：字符串常量。
// charge（无引号，引用变量） -> Variable{"charge"}：check 时按名字查 O。
// Variable{"charge"} 中的 C++ 引号只是为了保存名字。
//
// std::string 只负责存文本：字符串内容或变量名称，本身不会查 O。
// let 声明 y : Int：保存 string name = "y"、declaredType = Type::Int；
// 初始化式检查通过且类型相容后，向 innerO 登记 y 的类型，再检查正文。
// 正文中使用 y：用 Variable{"y"} 表示，check 时查询 innerO。
// x <- 42：Assign 用 string name 保存 "x"，check 时查询 O 中 x 的已有类型。
// node.type 保存表达式的类型；check 的 bool 返回值表示检查是否成功。
// check 检查并标注类型，不执行表达式。

struct Expr
{
    Type type = Type::Unknown;
    virtual ~Expr() = default;
};

enum class Type
{
    Unknown,
    String,
    Object,
    Bool,
    Int
};

struct BoolLiteral
{
    bool value;
    Type type = Type::Unknown;
};

struct StringLiteral
{
    std::string value;
    Type type = Type::Unknown;
};

struct IntLiteral
{
    int value;
    Type type = Type::Unknown;
};

struct Variable
{
    std::string name;
    Type type = Type::Unknown;
};

struct Add
{
    Variable left;
    IntLiteral right;
    Type type = Type::Unknown;
};

struct Let
{
    std::string name;
    Type declaredType;
    IntLiteral init;
    Add body;
    Type type = Type::Unknown;
};

struct LessThan
{
    Variable left;
    IntLiteral right;
    Type type = Type::Unknown;
};

struct Not
{
    Variable operand;
    Type type = Type::Unknown;
};

struct Assign
{
    std::string name;
    IntLiteral right;
    Type type = Type::Unknown;
};

struct Loop
{
    Variable condition;
    Assign body;
    Type type = Type::Unknown;
};

struct LetNoInit
{
    std::string name;
    Type declaredType;
    LessThan body;
    Type type = Type::Unknown;
};

struct If
{
    Variable condition;
    Variable thenBranch;
    Variable elseBranch;
    Type type = Type::Unknown;
};

//        O(x) = T
// ──────────────────── [Var]
//   O, M, C ⊢ x : T
bool check(Variable &node, const std::map<std::string, Type> &O)
{
    auto it = O.find(node.name);

    if (it == O.end())
    {
        std::cerr << "变量未声明：" << node.name << '\n';
        return false;
    }

    node.type = it->second;

    return true;
}

// i is an integer literal
// ───────────────────────── [Int]
//   O, M, C ⊢ i : Int
void check(IntLiteral &node)
{
    node.type = Type::Int;
}

// s is a string literal
// ───────────────────────── [String]
//   O, M, C ⊢ s : String
void check(StringLiteral &node)
{
    node.type = Type::String;
}

// ─────────────────────── [True]  |   ──────────────────────── [False]
//  O, M, C ⊢ true : Bool          |     O, M, C ⊢ false : Bool
void check(BoolLiteral &node)
{
    node.type = Type::Bool;
}

// O, M, C ⊢ e₀ : Int    O, M, C ⊢ e₁ : Int
// ───────────────────────────────────────── [Arith]
//        O, M, C ⊢ e₀ + e₁ : Int
bool check(Add &node, const std::map<std::string, Type> &O)
{
    if (!check(node.left, O))
    {
        return false;
    }
    check(node.right);

    if (node.left.type != Type::Int || node.right.type != Type::Int)
    {
        std::cerr << "加法两侧需要是 Int \n";
        return false;
    }

    node.type = Type::Int;
    return true;
}

// let y : Int <- 42 in y + 1
// O, M, C ⊢ e₀ : T₀    T₀ ≤ T    O[T/x], M, C ⊢ e₁ : T₁
// ─────────────────────────────────────────────────────── [Let-Init]
//       O, M, C ⊢ let x : T <- e₀ in e₁ : T₁
// 本例：x = y，T = Int，e₀ = 42，e₁ = y + 1。
bool check(Let &node, const std::map<std::string, Type> &O)
{
    // 先检查初始化式；当前 demo 用类型相等判断 T₀ ≤ T。
    check(node.init);
    if (node.declaredType != node.init.type)
    {
        std::cerr << "let 初始化式与声明类型不相容\n";
        return false;
    }

    // 用局部副本表示 O[T/x]，供正文查询，原 O 保持不变。
    auto innerO = O;
    innerO[node.name] = node.declaredType;
    if (!check(node.body, innerO))
    {
        return false;
    }

    // 正文得到的 T₁ 也是整个 let 表达式的类型。
    node.type = node.body.type;
    return true;
}

// O, M, C ⊢ e₀ : Int    O, M, C ⊢ e₁ : Int
// ───────────────────────────────────────── [Compare]
//        O, M, C ⊢ e₀ < e₁ : Bool
bool check(LessThan &node, const std::map<std::string, Type> &O)
{
    check(node.right);
    if (!check(node.left, O))
    {
        return false;
    }

    if (node.left.type != node.right.type || node.left.type != Type::Int)
    {
        /* code */
        std::cerr << "比较两侧需要是 Int\n";
        return false;
    }

    // Bool 是表达式的类型；返回 true 表示类型检查通过。
    node.type = Type::Bool;
    return true;
}

//       O, M, C ⊢ e : Bool
// ─────────────────────────── [Not]
//     O, M, C ⊢ not e : Bool
bool check(Not &node, const std::map<std::string, Type> &O)
{
    if (!check(node.operand, O))
    {
        return false;
    }

    if (node.operand.type != Type::Bool)
    {
        std::cerr << "变量声明不是bool。\n";
        return false;
    }

    node.type = Type::Bool;
    return true;
}

// O(x) = T    O, M, C ⊢ e : T′    T′ ≤ T
// ─────────────────────────────────────── [Assign]
//          O, M, C ⊢ x <- e : T′
bool check(Assign &node, const std::map<std::string, Type> &O)
{
    auto it = O.find(node.name);
    // TODO 1：查询目标变量；未声明则报错并返回 false
    if (it == O.end())
    {
        std::cerr << "变量未声明。\n";
        return false;
    }

    // TODO 2：检查右侧表达式；类型不相容则报错并返回 false
    check(node.right);
    if (it->second != node.right.type)
    {
        std::cerr << "右侧表达式与左侧变量类型不一致。\n";
        return false;
    }

    // TODO 3：把右侧表达式的类型标注给当前节点，返回 true
    node.type = node.right.type;
    return true;
}

// O, M, C ⊢ e₀ : Bool    O, M, C ⊢ e₁ : T
// ─────────────────────────────────────────── [Loop]
// O, M, C ⊢ while e₀ loop e₁ pool : Object
bool check(Loop &node, const std::map<std::string, Type> &O)
{
    // TODO 1：检查条件；检查失败或类型不符合规则时返回 false
    if(!check(node.condition, O) || node.condition.type != Type::Bool)
    {
        return false;
    }
     
    // TODO 2：检查循环体；检查失败时返回 false
    if(!check(node.body,O))
    {
        return false;
    }

    // TODO 3：根据规则标注整个循环的类型，返回 true
    node.type = Type::Object;
    return true;
}

// let y : Int in y < 1
//          O[T₀/x], M, C ⊢ e₁ : T₁
// ───────────────────────────────────────── [Let-No-Init]
//   O, M, C ⊢ let x : T₀ in e₁ : T₁
// 本例：公式中的 x 对应变量名 y；T₀ 对应 Int；e₁ 对应正文 y < 1,T₁ = Bool。
bool check(LetNoInit &node, const std::map<std::string, Type> &O)
{
    // TODO 1：复制外层环境 O，得到正文使用的 innerO。
    auto innerO = O;
    // TODO 2：构造 O[T₀/x]。
    // 在 innerO 中登记 node.name 对应的 node.declaredType；
    // 如果存在同名变量，覆盖局部副本中的绑定。
    auto it = innerO.find(node.name);
    if(it == innerO.end())
    {
        innerO.insert({node.name,node.declaredType});
    }
    else
    {
        innerO[node.name] = node.declaredType;
    }
    // TODO 3：验证前提 O[T₀/x], M, C ⊢ e₁ : T₁。
    // 使用 innerO 检查 node.body，失败则返回 false。
    if(!check(node.body,innerO))
    {
        return false;
    }
    // TODO 4：根据结论，将正文类型 T₁ 标注给整个 let 节点。
    node.type = node.body.type;
    return true;
}

// if flag then a else b fi
// O, M, C ⊢ e₀ : Bool    O, M, C ⊢ e₁ : T₁    O, M, C ⊢ e₂ : T₂
// ─────────────────────────────────────────────────────────────── [If]
//       O, M, C ⊢ if e₀ then e₁ else e₂ fi : lub(T₁, T₂)
//
// 本例：e₀ 对应 flag，e₁ 对应 a，e₂ 对应 b。
// lub(T₁, T₂)：能够同时容纳两个分支结果的最小公共父类型。
bool check(If &node, const std::map<std::string, Type> &O)
{
    // TODO 1：检查条件 node.condition，失败则返回 false。
    if(!check(node.condition,O))
    {
        return false;
    }
    // TODO 2：验证条件的类型必须为 Bool，否则报错并返回 false。
    if(node.condition.type != Type::Bool)
    {
        std::cerr << "condition 类型错误\n";
        return false;
    }
    // TODO 3：分别检查 thenBranch 和 elseBranch。
    // 两个分支都需要通过类型检查，任何一个失败都返回 false。
    if(!check(node.thenBranch,O) || !check(node.elseBranch,O))
    {
        return false;
    }
    // TODO 4：计算两个分支类型的 lub，并标注给整个 if 节点。
    //
    // 当前练习只考虑 Int、Bool、String、Object：
    // Int、Bool、String 都直接继承 Object。
    //
    // 两个合法类型相同：lub(T, T) = T。
    // 两个合法类型不同：lub(T₁, T₂) = Object。
    // Unknown 表示尚未得到合法类型，遇到时应返回 false。
    Type result = lub(node.thenBranch.type, node.elseBranch.type);

    if (result == Type::Unknown)
    {
        return false;
    }
    
    // 将 lub(T₁, T₂) 标注给整个 if 表达式。
    node.type = result;
    return true;
}

// 合并两个已经检查完成的分支类型。
// 当前练习中，Int、Bool、String 都直接继承 Object。
Type lub(Type left, Type right)
{
    // Unknown 表示尚未得到合法类型，不能参与正常合并。
    if (left == Type::Unknown || right == Type::Unknown)
    {
        return Type::Unknown;
    }

    // lub(T, T) = T。
    if (left == right)
    {
        return left;
    }

    // 当前简化类型层次中，不同合法类型的最小公共父类型为 Object。
    return Type::Object;
}

int main()
{
    SetConsoleOutputCP(CP_UTF8);

    // 字面量：[Int]、[String]、[True]、[False]
    // O, M, C ⊢ 42 : Int    O, M, C ⊢ "123" : String
    // O, M, C ⊢ true : Bool
    IntLiteral node{42};
    StringLiteral node_s{"123"};
    BoolLiteral node_b{true};
    check(node);
    check(node_s);
    check(node_b);

    std::cout << node.value << " : "
              << (node.type == Type::Int ? "Int" : "Unknown") << '\n';

    std::cout << node_s.value << " : "
              << (node_s.type == /* TODO：预期的类型标签 */
                          Type::String
                      ? "String"
                      : "Unknown")
              << '\n';

    std::cout << node_b.value << " : "
              << (node_b.type == /* TODO：预期的类型标签 */
                          Type::Bool
                      ? "Bool"
                      : "Unknown")
              << '\n';

    //        O(x) = T
    // ────────────────────  [Var]
    //   O, M, C ⊢ x : T
    std::map<std::string, Type> O{
        {"x", Type::Int}
        // {"y",Type::String}
    };

    auto it = O.find("x");

    if (it != O.end())
    {
        Type result = it->second;

        std::cout << "x : "
                  << (result == Type::Int ? "Int" : "Unknow") << "\n";
    }
    else
    {
        std::cerr << "变量未声明";
    }

    Variable var{"x"};
    Variable var_2{"y"};

    if (check(var, O) && check(var_2, O))
    {
        std::cout << var.name << " : "
                  << (var.type == Type::Int ? "Int" : "Unknown")
                  << '\n';

        std::cout << var_2.name << " : "
                  << (var_2.type == Type::String ? "String" : "Unknown")
                  << '\n';
    }

    // x + 100
    // O, M, C ⊢ e₀ : Int    O, M, C ⊢ e₁ : Int
    // ───────────────────────────────────────── [Arith]
    //        O, M, C ⊢ e₀ + e₁ : Int
    Add expr{
        Variable{"x"},
        IntLiteral{100}};

    if (check(expr, O))
    {
        std::cout << "x + " << expr.right.value << " : "
                  << (expr.type == Type::Int ? "Int" : "Unknown")
                  << '\n';
    }

    // 失败检测：
    std::map<std::string, Type> O_bad{
        {"x", Type::String}};

    if (check(expr, O_bad))
    {
        std::cout << "x + " << expr.right.value << " : "
                  << (expr.type == Type::Int ? "Int" : "Unknown")
                  << '\n';
    }

    // let y : Int <- 42 in y + 1
    // O, M, C ⊢ 42 : Int    Int ≤ Int    O[Int/y], M, C ⊢ y + 1 : Int
    // ───────────────────────────────────────────────────────────── [Let-Init]
    //        O, M, C ⊢ let y : Int <- 42 in y + 1 : Int
    Let letExpr{
        "y",
        Type::Int,
        IntLiteral{42},
        Add{
            Variable{"y"},
            IntLiteral{1},
        },
    };

    bool ok = check(letExpr, O);

    std::cout << "let check: " << ok << '\n';

    std::cout << "let type: "
              << (/* TODO：判断整个 let 节点的类型是否为 Int */
                  letExpr.type == Type::Int ? "Int" : "Unknown")
              << '\n';

    // x < 10
    // O, M, C ⊢ e₀ : Int    O, M, C ⊢ e₁ : Int
    // ───────────────────────────────────────── [Compare]
    //        O, M, C ⊢ e₀ < e₁ : Bool
    LessThan cmpExpr{
        Variable{"x"},
        IntLiteral{10}};

    bool cmpOk = check(cmpExpr, O);

    std::cout << "compare check: " << cmpOk << '\n';
    std::cout << "compare type: "
              << (cmpExpr.type == Type::Bool ? "Bool" : "Unknown")
              << '\n';

    LessThan badCmp{
        Variable{"x"},
        IntLiteral{10}};

    bool badCmpOk = check(badCmp, O_bad);

    std::cout << "bad compare check: " << badCmpOk << '\n';

    //       O, M, C ⊢ e : Bool
    // ─────────────────────────── [Not]
    //     O, M, C ⊢ not e : Bool
    std::map<std::string, Type> O_not{
        {"flag", Type::Bool}};

    Not notExpr{
        Variable{"flag"}};

    bool notOk = check(notExpr, O_not);

    std::cout << "not check: " << notOk << '\n';
    std::cout << "not type: "
              << (notExpr.type == Type::Bool ? "Bool" : "Unknown")
              << '\n';

    Not badNot{
        Variable{"x"}};

    bool badNotOk = check(badNot, O);

    std::cout << "bad not check: " << badNotOk << '\n';

    // O(x) = T    O, M, C ⊢ e : T′    T′ ≤ T
    // ─────────────────────────────────────── [Assign]
    //          O, M, C ⊢ x <- e : T′
    Assign ass_Expr{
        "x",
        IntLiteral{42}};
    bool assignOk = check(ass_Expr, O);

    std::cout << "assign check: " << assignOk << '\n';
    std::cout << "assign type: "
              << (ass_Expr.type == Type::Int ? "Int" : "Unknown")
              << '\n';

    Assign badAssign{"x", IntLiteral{42}};

    bool badAssignOk = check(badAssign, O_bad);

    std::cout << "bad assign check: " << badAssignOk << '\n';

    // O, M, C ⊢ e₀ : Bool    O, M, C ⊢ e₁ : T
    // ─────────────────────────────────────────── [Loop]
    // O, M, C ⊢ while e₀ loop e₁ pool : Object
    
    std::map<std::string,Type> looMp
    {
        {"e_0",Type::Bool},
        {"e_1",Type::Int}
    };

    Loop looExpr
    {
       Variable{"e_0"},
       Assign{"e_1",IntLiteral{41}} 
    };

    bool loopOk = check(looExpr,looMp);

    std::cout << "loop check: " << loopOk << '\n';
    std::cout << "loop type: "
              << (looExpr.type == Type::Object ? "Object" : "Unknown")
              << '\n';

    auto badLoopO = looMp;
    badLoopO["e_0"] = Type::Int;
              
    Loop badLoop{
        Variable{"e_0"},
        Assign{"e_1", IntLiteral{41}}
    };
              
    bool badLoopOk = check(badLoop, badLoopO);
              
    std::cout << "bad loop check: " << badLoopOk << '\n';

    // let y : Int in y < 1
    // 外层环境 O = {y : String}。
    // 正文环境 O[Int/y] = {y : Int}，内层绑定遮蔽外层同名绑定。
    //
    //           O[Int/y], M, C ⊢ y < 1 : Bool
    // ───────────────────────────────────────────────── [Let-No-Init]
    //       O, M, C ⊢ let y : Int in y < 1 : Bool
    std::map<std::string, Type> outerO{
        {"y", Type::String}
    };

    LetNoInit noInitExpr{
        "y",
        Type::Int,
        LessThan{
            Variable{"y"},
            IntLiteral{1}
        }
    };

    bool noInitOk = check(noInitExpr, outerO);

    // 预期：检查成功，输出 1。
    std::cout << "let no-init check: " << noInitOk << '\n';

    // 结论中的类型来自正文 y < 1，因此预期为 Bool。
    std::cout << "let no-init type: "
            << (noInitExpr.type == Type::Bool ? "Bool" : "Unknown")
            << '\n';

    // O[Int/y] 只用于正文；外层 O 中的 y 仍为 String，预期输出 1。
    std::cout << "outer y remains String: "
            << (outerO.at("y") == Type::String)
            << '\n';
    
    // 正例一：if flag then a else b fi
    // 环境 O_if = {flag : Bool, a : Int, b : Int}。
    //
    // O_if, M, C ⊢ flag : Bool
    // O_if, M, C ⊢ a : Int       O_if, M, C ⊢ b : Int
    // ─────────────────────────────────────────────────── [If]
    // O_if, M, C ⊢ if flag then a else b fi : lub(Int, Int)
    //
    // lub(Int, Int) = Int，因此整个 if 的类型为 Int。
    std::map<std::string, Type> O_if{
        {"flag", Type::Bool},
        {"a", Type::Int},
        {"b", Type::Int}
    };

    If ifExpr{
        Variable{"flag"},
        Variable{"a"},
        Variable{"b"}
    };

    bool ifOk = check(ifExpr, O_if);

    // 预期：1、Int。
    std::cout << "if check: " << ifOk << '\n';
    std::cout << "if type: "
            << (ifExpr.type == Type::Int ? "Int" : "Unknown")
            << '\n';


    // 正例二：两个分支的类型不同。
    // 环境 O_mixed = {flag : Bool, a : Int, b : String}。
    //
    // O_mixed, M, C ⊢ flag : Bool
    // O_mixed, M, C ⊢ a : Int    O_mixed, M, C ⊢ b : String
    // ─────────────────────────────────────────────────────── [If]
    // O_mixed, M, C ⊢ if flag then a else b fi : lub(Int, String)
    //
    // lub(Int, String) = Object。
    auto O_mixed = O_if;
    O_mixed["b"] = Type::String;

    If mixedIf{
        Variable{"flag"},
        Variable{"a"},
        Variable{"b"}
    };

    bool mixedIfOk = check(mixedIf, O_mixed);

    // 预期：1、Object。
    std::cout << "mixed if check: " << mixedIfOk << '\n';
    std::cout << "mixed if type: "
            << (mixedIf.type == Type::Object ? "Object" : "Unknown")
            << '\n';

    // 反例：条件类型错误。
    // O_bad_if(flag) = Int，不满足规则中的 e₀ : Bool。
    // 因此前提不成立，整个 if 应检查失败。
    auto O_bad_if = O_if;
    O_bad_if["flag"] = Type::Int;

    If badIf{
        Variable{"flag"},
        Variable{"a"},
        Variable{"b"}
    };

    bool badIfOk = check(badIf, O_bad_if);

    // 预期：0。
    std::cout << "bad if check: " << badIfOk << '\n';

    return 0;
}
