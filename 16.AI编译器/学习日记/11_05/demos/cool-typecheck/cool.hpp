#pragma once

#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace cool {

using Type = std::string;
using Environment = std::map<std::string, Type>; // 规则中的 O

struct TypeError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct Expr;
using E = std::shared_ptr<Expr>;

struct IntLiteral { int value; };
struct StringLiteral { std::string value; };
struct BoolLiteral { bool value; };
struct Variable { std::string name; };
struct New { Type type; };
enum class UnaryOp { Not, Negate, IsVoid };
struct Unary { UnaryOp op; E operand; };
enum class BinaryOp { Add, Subtract, Multiply, Divide, Less, LessEqual, Equal };
struct Binary { BinaryOp op; E left; E right; };
struct Assign { std::string name; E value; };
struct Let { std::string name; Type type; E init; E body; }; // init 可为空
struct If { E condition; E thenBranch; E elseBranch; };
struct Loop { E condition; E body; };
struct Block { std::vector<E> expressions; };
struct Branch { std::string name; Type type; E body; };
struct Case { E value; std::vector<Branch> branches; };
struct Dispatch {
    E receiver;
    std::string method;
    std::vector<E> arguments;
    std::optional<Type> staticType; // 空：e.f()；有值：e@T.f()
};

using Node = std::variant<IntLiteral, StringLiteral, BoolLiteral, Variable, New,
                          Unary, Binary, Assign, Let, If, Loop, Block, Case, Dispatch>;
struct Expr {
    Node node;
    std::optional<Type> inferred; // 成功检查后写入静态类型；不是运行时类型
};

template<class T>
E expr(T node) {
    return std::make_shared<Expr>(Expr{Node{std::move(node)}, std::nullopt});
}

struct Formal { std::string name; Type type; };
struct Method {
    std::string name;
    std::vector<Formal> formals;
    Type result;
    E body;
};
struct Attribute { std::string name; Type type; E init; };
struct Class {
    Type name;
    Type parent = "Object";
    std::vector<Attribute> attributes;
    std::vector<Method> methods;
};

// 收集类、属性、方法签名，再检查继承图与声明。允许引用后声明的类/方法。
class ClassTable {
public:
    explicit ClassTable(const std::vector<Class>& program);
    const Class& get(const Type& name) const;
    void requireType(const Type& type, bool allowSelf = true) const;
    bool conforms(Type actual, const Type& expected, const Type& current) const;
    Type join(Type left, Type right, const Type& current) const;
    const Method& method(Type start, const std::string& name) const; // 规则中的 M
    Environment attributes(Type current) const;
    const std::vector<Type>& userClasses() const { return users_; }

private:
    std::map<Type, Class> classes_;
    std::vector<Type> users_;
    const Method* findMethod(Type start, const std::string& name) const;
    void validate() const;
};

// 一个检查器固定当前类 C。SELF_TYPE 字符串在这里代表 SELF_TYPE_C。
class TypeChecker {
public:
    TypeChecker(const ClassTable& table, Type current);
    Type check(const E& expression);
    void checkClass();

private:
    const ClassTable& table_;
    Type current_;
    Environment base_;

    Type check(const E& expression, const Environment& env);
    void requireConforms(const Type& actual, const Type& expected,
                         const std::string& where) const;
    Type infer(const IntLiteral&, const Environment&);
    Type infer(const StringLiteral&, const Environment&);
    Type infer(const BoolLiteral&, const Environment&);
    Type infer(const Variable&, const Environment&);
    Type infer(const New&, const Environment&);
    Type infer(const Unary&, const Environment&);
    Type infer(const Binary&, const Environment&);
    Type infer(const Assign&, const Environment&);
    Type infer(const Let&, const Environment&);
    Type infer(const If&, const Environment&);
    Type infer(const Loop&, const Environment&);
    Type infer(const Block&, const Environment&);
    Type infer(const Case&, const Environment&);
    Type infer(const Dispatch&, const Environment&);
};

void checkProgram(const ClassTable& table);

// 仅实现 case 的运行时分支选择：输入对象动态类，返回分支下标。
// 调用前应通过静态检查；nullopt 表示 void。这里不执行分支主体。
std::size_t selectCaseBranch(const ClassTable& table,
                            const std::optional<Type>& dynamicType,
                            const std::vector<Branch>& branches);

} // namespace cool
