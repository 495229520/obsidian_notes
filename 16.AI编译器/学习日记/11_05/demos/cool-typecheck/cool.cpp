#include "cool.hpp"

#include <set>

namespace cool {
namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw TypeError(message);
    }
}

bool basic(const Type& type) {
    return type == "Int" || type == "String" || type == "Bool";
}

Type lookup(const Environment& env, const std::string& name) {
    const auto it = env.find(name);
    require(it != env.end(), "[Var] undefined variable: " + name);
    return it->second;
}

void bindable(const std::string& name) {
    require(name != "self", "cannot bind or assign self");
}

} // namespace

ClassTable::ClassTable(const std::vector<Class>& program) {
    // 内建方法只需要签名；它们的实现由运行库提供。
    classes_.emplace("Object", Class{"Object", "", {}, {
        {"abort", {}, "Object", nullptr},
        {"type_name", {}, "String", nullptr},
        {"copy", {}, "SELF_TYPE", nullptr}
    }});
    classes_.emplace("IO", Class{"IO", "Object", {}, {
        {"out_string", {{"x", "String"}}, "SELF_TYPE", nullptr},
        {"out_int", {{"x", "Int"}}, "SELF_TYPE", nullptr},
        {"in_string", {}, "String", nullptr},
        {"in_int", {}, "Int", nullptr}
    }});
    classes_.emplace("Int", Class{"Int", "Object", {}, {}});
    classes_.emplace("Bool", Class{"Bool", "Object", {}, {}});
    classes_.emplace("String", Class{"String", "Object", {}, {
        {"length", {}, "Int", nullptr},
        {"concat", {{"s", "String"}}, "String", nullptr},
        {"substr", {{"i", "Int"}, {"l", "Int"}}, "String", nullptr}
    }});

    for (const auto& cls : program) {
        require(!cls.name.empty() && cls.name != "SELF_TYPE", "invalid class name");
        require(classes_.emplace(cls.name, cls).second,
                "duplicate class: " + cls.name);
        users_.push_back(cls.name);
    }
    validate();
}

const Class& ClassTable::get(const Type& name) const {
    const auto it = classes_.find(name);
    require(it != classes_.end(), "undefined class: " + name);
    return it->second;
}

void ClassTable::requireType(const Type& type, bool allowSelf) const {
    if (type == "SELF_TYPE" && allowSelf) {
        return;
    }
    get(type);
}

void ClassTable::validate() const {
    // 先验证整个继承图，再使用沿父类查找的方法，避免环导致死循环。
    for (const auto& name : users_) {
        const auto& cls = get(name);
        require(!cls.parent.empty() && cls.parent != "SELF_TYPE" && !basic(cls.parent),
                "illegal parent of " + name + ": " + cls.parent);
        std::set<Type> seen;
        for (Type t = name; !t.empty(); t = get(t).parent) {
            require(seen.insert(t).second, "inheritance cycle: " + t);
            get(t);
        }
    }

    for (const auto& name : users_) {
        const auto& cls = get(name);
        auto inherited = attributes(cls.parent);
        std::set<std::string> attrs;
        for (const auto& attr : cls.attributes) {
            bindable(attr.name);
            requireType(attr.type);
            require(attrs.insert(attr.name).second && !inherited.count(attr.name),
                    "duplicate or inherited attribute: " + attr.name);
        }

        std::set<std::string> methods;
        for (const auto& m : cls.methods) {
            require(methods.insert(m.name).second, "duplicate method: " + m.name);
            requireType(m.result);
            require(m.body != nullptr, "missing method body: " + m.name);
            std::set<std::string> formals;
            for (const auto& f : m.formals) {
                bindable(f.name);
                requireType(f.type, false); // 形参不能声明为 SELF_TYPE
                require(formals.insert(f.name).second, "duplicate formal: " + f.name);
            }
            if (const auto* parent = findMethod(cls.parent, m.name)) {
                require(m.result == parent->result && m.formals.size() == parent->formals.size(),
                        "invalid override signature: " + m.name);
                for (std::size_t i = 0; i < m.formals.size(); ++i) {
                    require(m.formals[i].type == parent->formals[i].type,
                            "invalid override parameter: " + m.name);
                }
            }
        }
    }
}

bool ClassTable::conforms(Type actual, const Type& expected, const Type& current) const {
    requireType(actual);
    requireType(expected);
    get(current);
    if (actual == expected) {
        return true;
    }
    // C <= SELF_TYPE_C 不成立；不能把 new C 冒充成当前对象的动态类型。
    if (expected == "SELF_TYPE") {
        return false;
    }
    if (actual == "SELF_TYPE") {
        actual = current;
    }
    for (Type t = actual; !t.empty(); t = get(t).parent) {
        if (t == expected) {
            return true;
        }
    }
    return false;
}

Type ClassTable::join(Type left, Type right, const Type& current) const {
    requireType(left);
    requireType(right);
    get(current);
    if (left == right) {
        return left; // 两边都是 SELF_TYPE_C 时必须保留它
    }
    if (left == "SELF_TYPE") left = current;
    if (right == "SELF_TYPE") right = current;
    for (Type t = left; !t.empty(); t = get(t).parent) {
        if (conforms(right, t, current)) {
            return t;
        }
    }
    throw TypeError("no common ancestor");
}

const Method* ClassTable::findMethod(Type start, const std::string& name) const {
    for (Type t = start; !t.empty(); t = get(t).parent) {
        for (const auto& m : get(t).methods) {
            if (m.name == name) return &m;
        }
    }
    return nullptr;
}

const Method& ClassTable::method(Type start, const std::string& name) const {
    const auto* found = findMethod(start, name);
    require(found != nullptr, "undefined method: " + start + "." + name);
    return *found;
}

Environment ClassTable::attributes(Type current) const {
    Environment env;
    for (Type t = current; !t.empty(); t = get(t).parent) {
        for (const auto& attr : get(t).attributes) {
            env.emplace(attr.name, attr.type);
        }
    }
    return env;
}

TypeChecker::TypeChecker(const ClassTable& table, Type current)
    : table_(table), current_(std::move(current)), base_(table_.attributes(current_)) {
    base_["self"] = "SELF_TYPE";
}

Type TypeChecker::check(const E& expression) {
    return check(expression, base_);
}

Type TypeChecker::check(const E& expression, const Environment& env) {
    require(expression != nullptr, "missing expression");
    expression->inferred.reset();
    Type result = std::visit([&](const auto& node) { return infer(node, env); }, expression->node);
    expression->inferred = result;
    return result;
}

void TypeChecker::requireConforms(const Type& actual, const Type& expected,
                                 const std::string& where) const {
    require(table_.conforms(actual, expected, current_),
            where + ": " + actual + " does not conform to " + expected);
}

Type TypeChecker::infer(const IntLiteral&, const Environment&) { return "Int"; } // [Int]
Type TypeChecker::infer(const StringLiteral&, const Environment&) { return "String"; } // [String]
Type TypeChecker::infer(const BoolLiteral&, const Environment&) { return "Bool"; } // [True/False]

Type TypeChecker::infer(const Variable& node, const Environment& env) { // [Var]
    return lookup(env, node.name);
}

Type TypeChecker::infer(const New& node, const Environment&) { // [New]
    table_.requireType(node.type);
    return node.type;
}

Type TypeChecker::infer(const Unary& node, const Environment& env) {
    const Type operand = check(node.operand, env);
    switch (node.op) {
    case UnaryOp::Not: // [Not]
        require(operand == "Bool", "[Not] operand must be Bool");
        return "Bool";
    case UnaryOp::Negate: // [Neg]
        require(operand == "Int", "[Neg] operand must be Int");
        return "Int";
    case UnaryOp::IsVoid: // [Isvoid]
        return "Bool";
    }
    throw TypeError("unknown unary operator");
}

Type TypeChecker::infer(const Binary& node, const Environment& env) {
    const Type left = check(node.left, env);
    const Type right = check(node.right, env);
    if (node.op == BinaryOp::Equal) { // [Equal]
        require(!(basic(left) || basic(right)) || left == right,
                "[Equal] Int/String/Bool must be compared with the same type");
        return "Bool";
    }
    require(left == "Int" && right == "Int", "[Arith/Compare] operands must be Int");
    switch (node.op) {
    case BinaryOp::Less:
    case BinaryOp::LessEqual:
        return "Bool"; // [Compare]
    case BinaryOp::Add:
    case BinaryOp::Subtract:
    case BinaryOp::Multiply:
    case BinaryOp::Divide:
        return "Int"; // [Arith]
    case BinaryOp::Equal:
        break;
    }
    throw TypeError("unknown binary operator");
}

Type TypeChecker::infer(const Assign& node, const Environment& env) { // [Assign]
    bindable(node.name);
    const Type declared = lookup(env, node.name);
    const Type value = check(node.value, env);
    requireConforms(value, declared, "[Assign] " + node.name);
    return value; // 赋值表达式的类型是 RHS 类型；变量声明类型不变
}

Type TypeChecker::infer(const Let& node, const Environment& env) {
    bindable(node.name);
    table_.requireType(node.type);
    if (node.init) { // [Let-Init]：先用旧环境检查初始化式
        requireConforms(check(node.init, env), node.type, "[Let-Init] " + node.name);
    } // [Let-No-Init]：省略初始化约束，不引入 void 静态类型
    Environment inner = env;
    inner[node.name] = node.type;
    return check(node.body, inner); // 复制环境实现 O[T/x]，离开后自动恢复
}

Type TypeChecker::infer(const If& node, const Environment& env) { // [If]
    require(check(node.condition, env) == "Bool", "[If] condition must be Bool");
    const Type thenType = check(node.thenBranch, env);
    const Type elseType = check(node.elseBranch, env);
    return table_.join(thenType, elseType, current_);
}

Type TypeChecker::infer(const Loop& node, const Environment& env) { // [Loop]
    require(check(node.condition, env) == "Bool", "[Loop] condition must be Bool");
    check(node.body, env); // 即使条件为 false，也必须静态检查循环体
    return "Object";
}

Type TypeChecker::infer(const Block& node, const Environment& env) { // [Sequence]
    require(!node.expressions.empty(), "[Sequence] empty block");
    Type result;
    for (const auto& e : node.expressions) result = check(e, env);
    return result;
}

Type TypeChecker::infer(const Case& node, const Environment& env) { // [Case]
    check(node.value, env);
    require(!node.branches.empty(), "[Case] no branches");
    std::set<Type> seen;
    std::optional<Type> result;
    for (const auto& branch : node.branches) {
        bindable(branch.name);
        table_.requireType(branch.type, false);
        require(seen.insert(branch.type).second, "[Case] duplicate branch type: " + branch.type);
        Environment inner = env; // 每条分支都从原环境开始
        inner[branch.name] = branch.type;
        const Type body = check(branch.body, inner);
        result = result ? table_.join(*result, body, current_) : body;
    }
    return *result; // 合并分支主体类型，不是分支声明类型，也不是被匹配对象的类型
}

Type TypeChecker::infer(const Dispatch& node, const Environment& env) {
    const Type receiver = check(node.receiver, env);
    Type start = receiver == "SELF_TYPE" ? current_ : receiver;
    if (node.staticType) { // [Static Dispatch]
        table_.requireType(*node.staticType, false);
        requireConforms(receiver, *node.staticType, "[Static Dispatch] receiver");
        start = *node.staticType;
    }
    const Method& m = table_.method(start, node.method);
    require(node.arguments.size() == m.formals.size(), "[Dispatch] wrong argument count");
    for (std::size_t i = 0; i < node.arguments.size(); ++i) {
        requireConforms(check(node.arguments[i], env), m.formals[i].type,
                        "[Dispatch] argument " + std::to_string(i + 1));
    }
    // 两种派发都如此。@T 只改变方法查找起点，不把 SELF_TYPE 返回值改为 T。
    return m.result == "SELF_TYPE" ? receiver : m.result;
}

void TypeChecker::checkClass() {
    const Class& cls = table_.get(current_);
    // base_ 已含所有属性；属性初始化式可引用自己、后声明属性以及 self。
    for (const auto& attr : cls.attributes) {
        if (attr.init) { // [Attr-Init]
            requireConforms(check(attr.init, base_), attr.type, "[Attr-Init] " + attr.name);
        } // [Attr-No-Init]：声明类型已在 ClassTable 中验证
    }
    for (const auto& m : cls.methods) { // [Method]
        Environment env = base_;
        for (const auto& formal : m.formals) env[formal.name] = formal.type;
        requireConforms(check(m.body, env), m.result, "[Method] " + m.name);
    }
}

void checkProgram(const ClassTable& table) {
    for (const auto& name : table.userClasses()) {
        TypeChecker(table, name).checkClass();
    }
}

std::size_t selectCaseBranch(const ClassTable& table,
                            const std::optional<Type>& dynamicType,
                            const std::vector<Branch>& branches) {
    if (!dynamicType) throw std::runtime_error("case on void");
    table.requireType(*dynamicType, false);
    // 从动态类向 Object 走，遇到的第一种分支类型就是最近匹配；与源码顺序无关。
    for (Type t = *dynamicType; !t.empty(); t = table.get(t).parent) {
        for (std::size_t i = 0; i < branches.size(); ++i) {
            if (branches[i].type == t) return i;
        }
    }
    throw std::runtime_error("case has no matching branch for " + *dynamicType);
}

} // namespace cool
