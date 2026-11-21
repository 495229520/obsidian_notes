#include "cool.hpp"

#include <functional>
#include <iostream>

using namespace cool;

namespace {

E integer(int value = 42) { return expr(IntLiteral{value}); }
E boolean(bool value = true) { return expr(BoolLiteral{value}); }
E string(std::string value = "hi") { return expr(StringLiteral{std::move(value)}); }
E var(std::string name) { return expr(Variable{std::move(name)}); }
E object(Type type) { return expr(New{std::move(type)}); }
E binary(BinaryOp op, E a, E b) { return expr(Binary{op, std::move(a), std::move(b)}); }
E call(E receiver, std::string method, std::vector<E> args = {},
       std::optional<Type> staticType = std::nullopt) {
    return expr(Dispatch{std::move(receiver), std::move(method), std::move(args),
                         std::move(staticType)});
}

std::vector<Class> sampleProgram() {
    // 刻意把子类放在父类前面，验证声明收集不依赖源码顺序。
    return {
        {"Dog", "Animal", {}, {{"speak", {{"n", "Int"}}, "String", string("woof")}}},
        {"Cat", "Animal", {}, {}},
        {"Animal", "Object", {}, {{"speak", {{"n", "Int"}}, "String", string()}}},
        {"Stock", "Object", {{"count", "Int", nullptr}}, {
            {"inc", {}, "SELF_TYPE", expr(Block{{
                expr(Assign{"count", binary(BinaryOp::Add, var("count"), integer(1))}),
                var("self")
            }})}
        }},
        {"Main", "IO", {
            {"x", "Int", integer(2)},
            {"pet", "Animal", object("Dog")},
            {"uninitialized", "Int", nullptr},
            {"forward", "Int", var("later")},
            {"later", "Int", integer(9)},
            {"recursive", "Int", var("recursive")},
            {"me", "SELF_TYPE", var("self")}
        }, {{"main", {}, "Object", var("self")}}}
    };
}

struct Tests {
    int passed = 0;
    int failed = 0;

    void run(const std::string& label, const std::function<void()>& test) {
        try {
            test();
            ++passed;
        } catch (const std::exception& e) {
            ++failed;
            std::cerr << "FAIL " << label << ": " << e.what() << '\n';
        }
    }

    void expect(const std::string& actual, const std::string& expected) {
        if (actual != expected) {
            throw std::runtime_error("expected " + expected + ", got " + actual);
        }
    }

    void type(TypeChecker& checker, const std::string& label, E expression,
              const Type& expected, bool print = false) {
        run(label, [&] {
            expect(checker.check(expression), expected);
            expect(expression->inferred.value_or("<missing>"), expected);
            if (print) std::cout << label << " : " << expected << '\n';
        });
    }

    void rejects(const std::string& label, const std::string& message,
                 const std::function<void()>& action) {
        run(label, [&] {
            try {
                action();
            } catch (const TypeError& e) {
                if (std::string(e.what()).find(message) != std::string::npos) return;
                throw std::runtime_error("unexpected diagnostic: " + std::string(e.what()));
            }
            throw std::runtime_error("invalid program was accepted");
        });
    }
};

void examples(Tests& tests, const ClassTable& table, TypeChecker& checker) {
    std::cout << "=== Rules from the screenshot ===\n";
    const auto show = [&](const std::string& label, E e, const Type& type) {
        tests.type(checker, label, std::move(e), type, true);
    };
    show("[Int] 42", integer(), "Int");
    show("[String] \"hi\"", string(), "String");
    show("[True] true", boolean(), "Bool");
    show("[False] false", boolean(false), "Bool");
    show("[Var] x", var("x"), "Int");
    show("[New] new Dog", object("Dog"), "Dog");
    show("[Arith] x * 2", binary(BinaryOp::Multiply, var("x"), integer(2)), "Int");
    show("[Not/Compare] not (x < 3)",
         expr(Unary{UnaryOp::Not, binary(BinaryOp::Less, var("x"), integer(3))}), "Bool");
    show("[Equal] x = 2", binary(BinaryOp::Equal, var("x"), integer(2)), "Bool");
    show("[Loop] while false loop 42 pool", expr(Loop{boolean(false), integer()}), "Object");
    show("[Let-No-Init] let y : Int in y", expr(Let{"y", "Int", nullptr, var("y")}), "Int");
    show("[Let-Init] let a : Animal <- new Dog in a",
         expr(Let{"a", "Animal", object("Dog"), var("a")}), "Animal");
    show("[Let-Init/shadow] let x : Bool <- x < 3 in x",
         expr(Let{"x", "Bool", binary(BinaryOp::Less, var("x"), integer(3)), var("x")}), "Bool");
    show("[Assign] pet <- new Cat", expr(Assign{"pet", object("Cat")}), "Cat");
    tests.run("[Attr-Init/Attr-No-Init/Method] program", [&] {
        checkProgram(table);
        std::cout << "[Attr-Init/Attr-No-Init/Method] all class bodies: OK\n";
    });
    show("[If] if true then new Dog else new Cat fi",
         expr(If{boolean(), object("Dog"), object("Cat")}), "Animal");
    show("[Case] branches return Dog / Cat", expr(Case{var("pet"), {
        {"d", "Dog", object("Dog")}, {"c", "Cat", object("Cat")}
    }}), "Animal");
    const std::vector<Branch> branches = {
        {"a", "Animal", string("animal")}, {"d", "Dog", integer(1)}
    };
    show("[Case] branches return String / Int", expr(Case{var("pet"), branches}), "Object");
    tests.run("case runtime closest branch", [&] {
        const auto index = selectCaseBranch(table, Type{"Dog"}, branches);
        tests.expect(branches[index].type, "Dog");
        std::cout << "[Case runtime] dynamic Dog -> Dog branch (Animal was listed first)\n";
    });
    show("[Dispatch] pet.speak(1)", call(var("pet"), "speak", {integer(1)}), "String");
    show("[Static Dispatch] (new Dog)@Animal.speak(1)",
         call(object("Dog"), "speak", {integer(1)}, "Animal"), "String");
    show("[SELF_TYPE] self", var("self"), "SELF_TYPE");
    show("[Dispatch/SELF_TYPE] (new Stock).inc()", call(object("Stock"), "inc"), "Stock");
}

void regressions(Tests& tests, const ClassTable& table, TypeChecker& checker) {
    const auto good = [&](const std::string& label, E e, const Type& expected) {
        tests.type(checker, label, std::move(e), expected);
    };
    const auto bad = [&](const std::string& label, E e, const std::string& message) {
        tests.rejects(label, message, [&] { checker.check(e); });
    };
    for (auto op : {BinaryOp::Add, BinaryOp::Subtract, BinaryOp::Multiply, BinaryOp::Divide}) {
        good("arithmetic", binary(op, integer(), integer(2)), "Int");
        bad("bad arithmetic", binary(op, integer(), string()), "operands must be Int");
    }
    good("less equal", binary(BinaryOp::LessEqual, integer(), integer()), "Bool");
    good("negate", expr(Unary{UnaryOp::Negate, integer()}), "Int");
    good("isvoid", expr(Unary{UnaryOp::IsVoid, object("Dog")}), "Bool");
    good("new SELF_TYPE", object("SELF_TYPE"), "SELF_TYPE");
    good("object equality unrelated classes", binary(BinaryOp::Equal, object("Dog"), object("Stock")), "Bool");
    good("string equality", binary(BinaryOp::Equal, string(), string()), "Bool");
    good("bool equality", binary(BinaryOp::Equal, boolean(), boolean(false)), "Bool");
    good("assignment does not narrow variable", var("pet"), "Animal");
    good("let restored x", var("x"), "Int");
    good("block last expression", expr(Block{{integer(), string()}}), "String");
    good("self join self", expr(If{boolean(), var("self"), object("SELF_TYPE")}), "SELF_TYPE");
    good("self join current class", expr(If{boolean(), var("self"), object("Main")}), "Main");
    good("dynamic self return", call(var("self"), "copy"), "SELF_TYPE");
    good("static self return", call(var("self"), "copy", {}, "Object"), "SELF_TYPE");
    good("static concrete return", call(object("Dog"), "copy", {}, "Object"), "Dog");
    good("builtin String method", call(string(), "substr", {integer(0), integer(1)}), "String");
    good("builtin IO method", call(var("self"), "out_int", {integer()}), "SELF_TYPE");
    good("SELF_TYPE let", expr(Let{"s", "SELF_TYPE", var("self"), var("s")}), "SELF_TYPE");
    good("multiple let as nesting", expr(Let{"a", "Int", integer(),
         expr(Let{"b", "Int", var("a"), var("b")})}), "Int");
    good("case joins bodies not declarations", expr(Case{var("pet"), {
        {"d", "Dog", integer()}, {"c", "Cat", integer(1)}
    }}), "Int");
    good("case does not demand static coverage", expr(Case{var("pet"), {
        {"s", "Stock", string()}
    }}), "String");
    good("case self join", expr(Case{var("pet"), {
        {"d", "Dog", var("self")}, {"c", "Cat", var("self")}
    }}), "SELF_TYPE");

    bad("undefined variable", var("missing"), "undefined variable");
    bad("unknown new type", object("Unknown"), "undefined class");
    bad("not Int", expr(Unary{UnaryOp::Not, integer()}), "must be Bool");
    bad("negate Bool", expr(Unary{UnaryOp::Negate, boolean()}), "must be Int");
    bad("less String", binary(BinaryOp::Less, string(), string()), "must be Int");
    bad("Int = Bool", binary(BinaryOp::Equal, integer(), boolean()), "same type");
    bad("Object = Int", binary(BinaryOp::Equal, object("Object"), integer()), "same type");
    bad("Int = Object", binary(BinaryOp::Equal, integer(), object("Object")), "same type");
    bad("if condition", expr(If{integer(), integer(), integer()}), "condition must be Bool");
    bad("if checks unselected branch", expr(If{boolean(), integer(), var("missing")}), "undefined variable");
    bad("loop condition", expr(Loop{integer(), integer()}), "condition must be Bool");
    bad("loop checks unreachable body", expr(Loop{boolean(false), var("missing")}), "undefined variable");
    bad("let wrong init", expr(Let{"a", "Dog", object("Animal"), var("a")}), "does not conform");
    bad("let cannot see its own new binding", expr(Let{"fresh", "Int", var("fresh"), var("fresh")}), "undefined variable");
    bad("let cannot shadow self", expr(Let{"self", "Int", integer(), integer()}), "self");
    bad("unknown let type", expr(Let{"a", "Missing", nullptr, integer()}), "undefined class");
    bad("concrete not SELF_TYPE", expr(Let{"s", "SELF_TYPE", object("Main"), var("s")}), "does not conform");
    bad("failed let body", expr(Let{"x", "Bool", boolean(), var("missing")}), "undefined variable");
    good("scope restored after error", var("x"), "Int");
    bad("let does not leak into block", expr(Block{{
        expr(Let{"fresh", "Int", integer(), var("fresh")}), var("fresh")
    }}), "undefined variable");
    bad("assign self", expr(Assign{"self", var("self")}), "self");
    bad("assign wrong type", expr(Assign{"pet", integer()}), "does not conform");
    bad("assign undefined", expr(Assign{"missing", integer()}), "undefined variable");
    bad("empty block", expr(Block{{}}), "empty block");
    bad("empty case", expr(Case{var("pet"), {}}), "no branches");
    bad("duplicate case type", expr(Case{var("pet"), {
        {"a", "Dog", integer()}, {"b", "Dog", integer()}
    }}), "duplicate branch type");
    bad("case SELF_TYPE", expr(Case{var("pet"), {{"a", "SELF_TYPE", integer()}}}), "undefined class");
    bad("case binds self", expr(Case{var("pet"), {{"self", "Dog", integer()}}}), "self");
    bad("case branch scopes isolated", expr(Case{var("pet"), {
        {"d", "Dog", var("d")}, {"c", "Cat", var("d")}
    }}), "undefined variable");
    bad("case checks impossible branch", expr(Case{object("Dog"), {
        {"d", "Dog", integer()}, {"c", "Cat", var("missing")}
    }}), "undefined variable");
    bad("undefined method", call(var("pet"), "missing"), "undefined method");
    bad("wrong arity", call(var("pet"), "speak"), "argument count");
    bad("wrong argument type", call(var("pet"), "speak", {string()}), "argument 1");
    bad("bad static receiver", call(object("Cat"), "speak", {integer()}, "Dog"), "receiver");
    bad("static SELF_TYPE", call(var("self"), "copy", {}, "SELF_TYPE"), "undefined class");

    tests.run("conformance and join", [&] {
        if (!table.conforms("SELF_TYPE", "Object", "Main") ||
            table.conforms("Main", "SELF_TYPE", "Main") ||
            table.conforms("Animal", "Dog", "Main")) {
            throw std::runtime_error("incorrect conformance");
        }
        tests.expect(table.join("Dog", "Cat", "Main"), "Animal");
        tests.expect(table.join("Int", "String", "Main"), "Object");
    });
    tests.run("runtime case fallback and errors", [&] {
        const std::vector<Branch> branches = {{"a", "Animal", integer()}, {"d", "Dog", integer()}};
        tests.expect(std::to_string(selectCaseBranch(table, Type{"Cat"}, branches)), "0");
        for (const auto& type : {std::optional<Type>{}, std::optional<Type>{"Stock"}}) {
            bool rejected = false;
            try { selectCaseBranch(table, type, branches); }
            catch (const std::runtime_error& e) {
                const std::string expected = type ? "no matching branch" : "case on void";
                rejected = std::string(e.what()).find(expected) != std::string::npos;
            }
            if (!rejected) throw std::runtime_error("missing runtime case error");
        }
    });
}

void declarationTests(Tests& tests) {
    const auto rejects = [&](const std::string& label, std::vector<Class> program,
                             const std::string& message) {
        tests.rejects(label, message, [&] { checkProgram(ClassTable(program)); });
    };
    rejects("inheritance cycle", {{"A", "B", {}, {}}, {"B", "A", {}, {}}}, "cycle");
    rejects("unknown parent", {{"A", "Missing", {}, {}}}, "undefined class");
    rejects("sealed parent", {{"A", "Int", {}, {}}}, "illegal parent");
    rejects("SELF_TYPE parent", {{"A", "SELF_TYPE", {}, {}}}, "illegal parent");
    rejects("builtin redefinition", {{"Object", "Object", {}, {}}}, "duplicate class");
    rejects("attribute redefinition", {
        {"A", "Object", {{"x", "Int", nullptr}}, {}},
        {"B", "A", {{"x", "Int", nullptr}}, {}}
    }, "inherited attribute");
    rejects("duplicate attributes", {{"A", "Object", {
        {"x", "Int", nullptr}, {"x", "Bool", nullptr}
    }, {}}}, "duplicate");
    rejects("attribute self", {{"A", "Object", {{"self", "Int", nullptr}}, {}}}, "self");
    rejects("attribute bad initialization", {{"A", "Object", {{"x", "Int", string()}}, {}}}, "[Attr-Init]");
    rejects("uninitialized attribute unknown type", {{"A", "Object", {{"x", "Missing", nullptr}}, {}}}, "undefined class");
    rejects("method bad return", {{"A", "Object", {}, {{"f", {}, "Int", string()}}}}, "[Method]");
    rejects("method concrete return not self", {{"A", "Object", {}, {
        {"f", {}, "SELF_TYPE", object("A")}
    }}}, "[Method]");
    rejects("formal SELF_TYPE", {{"A", "Object", {}, {
        {"f", {{"x", "SELF_TYPE"}}, "Object", var("x")}
    }}}, "undefined class");
    rejects("formal self", {{"A", "Object", {}, {
        {"f", {{"self", "Int"}}, "Object", integer()}
    }}}, "self");
    rejects("duplicate formal", {{"A", "Object", {}, {
        {"f", {{"x", "Int"}, {"x", "Int"}}, "Int", var("x")}
    }}}, "duplicate formal");
    rejects("bad override", {
        {"A", "Object", {}, {{"f", {}, "Object", var("self")}}},
        {"B", "A", {}, {{"f", {}, "B", var("self")}}}
    }, "override signature");
    tests.run("attributes formals and forward methods", [&] {
        ClassTable table({{"A", "Object", {{"x", "Int", nullptr}}, {
            {"f", {{"x", "String"}}, "String", var("x")},
            {"forward", {}, "Int", call(var("self"), "later")},
            {"later", {}, "Int", var("x")},
            {"recurse", {}, "Int", call(var("self"), "recurse")},
            {"take", {{"a", "Object"}}, "Object", var("a")},
            {"passSelf", {}, "Object", call(var("self"), "take", {var("self")})}
        }}});
        checkProgram(table);
    });
    tests.run("inherited SELF_TYPE rebinds to subclass", [&] {
        ClassTable table({
            {"A", "Object", {{"me", "SELF_TYPE", var("self")}}, {}},
            {"B", "A", {}, {{"get", {}, "SELF_TYPE", var("me")}}}
        });
        checkProgram(table);
        TypeChecker checker(table, "B");
        tests.expect(checker.check(call(var("self"), "get")), "SELF_TYPE");
    });
}

} // namespace

int main() {
    try {
        const ClassTable table(sampleProgram());
        TypeChecker checker(table, "Main");
        Tests tests;
        examples(tests, table, checker);
        regressions(tests, table, checker);
        declarationTests(tests);
        std::cout << "\nTests: " << tests.passed << " passed, " << tests.failed << " failed\n";
        return tests.failed == 0 ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "Setup failed: " << e.what() << '\n';
        return 1;
    }
}
