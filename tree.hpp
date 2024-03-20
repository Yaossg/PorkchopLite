#pragma once

#include <optional>
#include <utility>

#include "local.hpp"

namespace Porkchop {

struct Expr;
struct IdExpr;
using ExprHandle = std::unique_ptr<Expr>;
using IdExprHandle = std::unique_ptr<IdExpr>;
struct SimpleDeclarator;
using DeclaratorHandle = std::unique_ptr<SimpleDeclarator>;

struct Expr : Descriptor {
    enum class Level {
        ASSIGNMENT,
        LOR,
        LAND,
        OR,
        XOR,
        AND,
        EQUALITY,
        COMPARISON,
        SHIFT,
        ADDITION,
        MULTIPLICATION,
        PREFIX,
        POSTFIX,
        PRIMARY
    };
    [[nodiscard]] static Level upper(Level level) noexcept {
        return (Level)((int)level + 1);
    }

    Compiler& compiler;

    explicit Expr(Compiler& compiler): compiler(compiler) {}

    [[nodiscard]] virtual Segment segment() const = 0;

    virtual void walkBytecode(Assembler* assembler) const = 0;

    void expect(TypeReference const& expected) const;

    void expect(bool pred(TypeReference const&), const char* expected) const;

    [[noreturn]] void expect(const char* expected) const;

    void neverGonnaGiveYouUp(const char* msg) const;

    TypeReference getType(TypeReference const& infer = nullptr) const {
        if (typeCache == nullptr) {
            typeCache = evalType(infer);
        }
        return typeCache;
    }

    std::optional<$union> getConst() const {
        if (constState == ConstState::INDETERMINATE) {
            if (auto value = evalConst()) {
                constValue = value.value();
                constState = ConstState::CONSTANT;
            } else {
                constState = ConstState::RUNTIME;
            }
        }
        if (constState == ConstState::CONSTANT) {
            return constValue;
        }
        return std::nullopt;
    }
    bool isConst() const {
        return getConst().has_value();
    }
    $union requireConst() const;

    mutable std::string reg = "%error";
protected:
    [[nodiscard]] virtual TypeReference evalType(TypeReference const& infer) const = 0;
    [[nodiscard]] virtual std::optional<$union> evalConst() const {
        return std::nullopt;
    }
private:
    enum class ConstState {
        INDETERMINATE, CONSTANT, RUNTIME
    };
    mutable TypeReference typeCache;
    mutable ConstState constState = ConstState::INDETERMINATE;
    mutable $union constValue;
};

struct ConstExpr : Expr {
    Token token;

    ConstExpr(Compiler& compiler, Token token): Expr(compiler), token(token) {}

    [[nodiscard]] std::string_view descriptor() const noexcept override { return compiler.of(token); }

    [[nodiscard]] Segment segment() const override {
        return token;
    }
};

struct BoolConstExpr : ConstExpr {
    bool parsed;

    BoolConstExpr(Compiler& compiler, Token token);

    [[nodiscard]] TypeReference evalType(TypeReference const& infer) const override;

    [[nodiscard]] std::optional<$union> evalConst() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct CharConstExpr : ConstExpr {
    char32_t parsed;

    CharConstExpr(Compiler& compiler, Token token);

    [[nodiscard]] TypeReference evalType(TypeReference const& infer) const override;

    [[nodiscard]] std::optional<$union> evalConst() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct IntConstExpr : ConstExpr {
    int64_t parsed;
    bool merged;

    IntConstExpr(Compiler& compiler, Token token, bool merged = false);

    [[nodiscard]] TypeReference evalType(TypeReference const& infer) const override;

    [[nodiscard]] std::optional<$union> evalConst() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct FloatConstExpr : ConstExpr {
    double parsed;

    FloatConstExpr(Compiler& compiler, Token token);

    [[nodiscard]] TypeReference evalType(TypeReference const& infer) const override;

    [[nodiscard]] std::optional<$union> evalConst() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct SizeofExpr : Expr {
    TypeReference type;
    Segment seg;

    SizeofExpr(Compiler& compiler, TypeReference type, Segment seg): Expr(compiler), type(std::move(type)), seg(seg) {}

    TypeReference evalType(const Porkchop::TypeReference &infer) const override { return ScalarTypes::INT; }

    std::optional<$union> evalConst() const override { return type->size(); }

    void walkBytecode(Porkchop::Assembler *assembler) const override;

    [[nodiscard]] std::string_view descriptor() const noexcept override { return "sizeof"; }
    [[nodiscard]] std::vector<const Descriptor *> children() const override { return {type.get()}; }

    [[nodiscard]] Segment segment() const override {
        return seg;
    }
};

struct AssignableExpr : Expr {
    explicit AssignableExpr(Compiler& compiler): Expr(compiler) {}

    void walkBytecode(Porkchop::Assembler *assembler) const override;
    void walkStoreBytecode(std::string const& from, Assembler* assembler) const;

    virtual void ensureAssignable() const = 0;

    virtual std::string addressOf(Assembler* assembler) const = 0;
};

struct IdExpr : AssignableExpr {
    Token token;
    LocalContext::LookupResult lookup;

    IdExpr(Compiler& compiler, Token token): AssignableExpr(compiler), token(token) {}

    [[nodiscard]] std::string_view descriptor() const noexcept override { return compiler.of(token); }

    [[nodiscard]] Segment segment() const override {
        return token;
    }

    void initLookup(LocalContext& context) {
        lookup = context.lookup(compiler, token);
    }

    [[nodiscard]] TypeReference evalType(TypeReference const& infer) const override;

    [[nodiscard]] std::optional<$union> evalConst() const override;

    void walkBytecode(Porkchop::Assembler *assembler) const override;

    void ensureAssignable() const override;

    std::string addressOf(Assembler* assembler) const override;
};

struct PrefixExpr : Expr {
    Token token;
    ExprHandle rhs;

    PrefixExpr(Compiler& compiler, Token token, ExprHandle rhs): Expr(compiler), token(token), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {rhs.get()}; }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return compiler.of(token); }

    [[nodiscard]] Segment segment() const override {
        return range(token, rhs->segment());
    }

    [[nodiscard]] TypeReference evalType(TypeReference const& infer) const override;

    [[nodiscard]] std::optional<$union> evalConst() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct StatefulPrefixExpr : Expr {
    Token token;
    std::unique_ptr<AssignableExpr> rhs;

    StatefulPrefixExpr(Compiler& compiler, Token token, std::unique_ptr<AssignableExpr> rhs): Expr(compiler), token(token), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {rhs.get()}; }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return compiler.of(token); }

    [[nodiscard]] Segment segment() const override {
        return range(token, rhs->segment());
    }

    [[nodiscard]] TypeReference evalType(TypeReference const& infer) const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct StatefulPostfixExpr : Expr {
    Token token;
    std::unique_ptr<AssignableExpr> lhs;

    StatefulPostfixExpr(Compiler& compiler, Token token, std::unique_ptr<AssignableExpr> lhs): Expr(compiler), token(token), lhs(std::move(lhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {lhs.get()}; }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return compiler.of(token); }

    [[nodiscard]] Segment segment() const override {
        return range(lhs->segment(), token);
    }

    [[nodiscard]] TypeReference evalType(TypeReference const& infer) const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct AddressOfExpr : Expr {
    Token token;
    std::unique_ptr<AssignableExpr> rhs;

    AddressOfExpr(Compiler& compiler, Token token, std::unique_ptr<AssignableExpr> rhs): Expr(compiler), token(token), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {rhs.get()}; }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return compiler.of(token); }

    [[nodiscard]] Segment segment() const override {
        return range(token, rhs->segment());
    }

    [[nodiscard]] TypeReference evalType(TypeReference const& infer) const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct DereferenceExpr : AssignableExpr {
    Token token;
    std::unique_ptr<Expr> rhs;

    DereferenceExpr(Compiler& compiler, Token token, std::unique_ptr<Expr> rhs): AssignableExpr(compiler), token(token), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {rhs.get()}; }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return compiler.of(token); }

    [[nodiscard]] Segment segment() const override {
        return range(token, rhs->segment());
    }

    [[nodiscard]] TypeReference evalType(TypeReference const& infer) const override;

    void ensureAssignable() const override;

    std::string addressOf(Assembler* assembler) const override;
};

struct InfixExprBase : Expr {
    Token token;
    ExprHandle lhs;
    ExprHandle rhs;

    InfixExprBase(Compiler& compiler, Token token, ExprHandle lhs, ExprHandle rhs): Expr(compiler),
        token(token), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {lhs.get(), rhs.get()}; }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return compiler.of(token); }

    [[nodiscard]] Segment segment() const override {
        return range(lhs->segment(), rhs->segment());
    }
};

struct InfixExpr : InfixExprBase {
    InfixExpr(Compiler& compiler, Token token, ExprHandle lhs, ExprHandle rhs):
        InfixExprBase(compiler, token, std::move(lhs), std::move(rhs)) {}

    [[nodiscard]] TypeReference evalType(TypeReference const& infer) const override;

    [[nodiscard]] std::optional<$union> evalConst() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct CompareExpr : InfixExprBase {
    CompareExpr(Compiler& compiler, Token token, ExprHandle lhs, ExprHandle rhs):
        InfixExprBase(compiler, token, std::move(lhs), std::move(rhs)) {}

    [[nodiscard]] TypeReference evalType(TypeReference const& infer) const override;

    [[nodiscard]] std::optional<$union> evalConst() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct LogicalExpr : InfixExprBase {
    LogicalExpr(Compiler& compiler, Token token, ExprHandle lhs, ExprHandle rhs):
        InfixExprBase(compiler, token, std::move(lhs), std::move(rhs)) {}

    [[nodiscard]] TypeReference evalType(TypeReference const& infer) const override;

    [[nodiscard]] std::optional<$union> evalConst() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct InfixInvokeExpr : InfixExprBase {
    IdExprHandle infix;

    InfixInvokeExpr(Compiler& compiler, IdExprHandle infix, ExprHandle lhs, ExprHandle rhs):
        InfixExprBase(compiler, infix->token, std::move(lhs), std::move(rhs)), infix(std::move(infix)) {}

    [[nodiscard]] TypeReference evalType(TypeReference const& infer) const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct AssignExpr : Expr {
    Token token;
    std::unique_ptr<AssignableExpr> lhs;
    ExprHandle rhs;

    AssignExpr(Compiler& compiler, Token token, std::unique_ptr<AssignableExpr> lhs, ExprHandle rhs): Expr(compiler),
        token(token), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {lhs.get(), rhs.get()}; }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return compiler.of(token); }

    [[nodiscard]] Segment segment() const override {
        return range(lhs->segment(), rhs->segment());
    }

    [[nodiscard]] TypeReference evalType(TypeReference const& infer) const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct AccessExpr : AssignableExpr {
    Token token1, token2;
    ExprHandle lhs;
    ExprHandle rhs;

    AccessExpr(Compiler& compiler, Token token1, Token token2, ExprHandle lhs, ExprHandle rhs): AssignableExpr(compiler),
        token1(token1), token2(token2), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {lhs.get(), rhs.get()}; }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return "[]"; }

    [[nodiscard]] Segment segment() const override {
        return range(lhs->segment(), token2);
    }

    [[nodiscard]] TypeReference evalType(TypeReference const& infer) const override;

    void ensureAssignable() const override;

    std::string addressOf(Assembler* assembler) const override;
};

struct InvokeExpr : Expr {
    Token token1, token2;
    ExprHandle lhs;
    std::vector<ExprHandle> rhs;

    InvokeExpr(Compiler& compiler, Token token1, Token token2, ExprHandle lhs, std::vector<ExprHandle> rhs): Expr(compiler),
        token1(token1), token2(token2), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override {
        std::vector<const Descriptor*> ret{lhs.get()};
        for (auto&& e : rhs) ret.push_back(e.get());
        return ret;
    }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return "()"; }

    [[nodiscard]] Segment segment() const override {
        return range(lhs->segment(), token2);
    }

    [[nodiscard]] TypeReference evalType(TypeReference const& infer) const override;

    static std::string walkBytecode(const Expr *lhs, const std::vector<const Expr *> &rhs, Assembler *assembler, const TypeReference& type);

    void walkBytecode(Assembler* assembler) const override;
};

struct AsExpr : Expr {
    Token token, token2;
    ExprHandle lhs;
    TypeReference T;

    AsExpr(Compiler& compiler, Token token, Token token2, ExprHandle lhs, TypeReference T): Expr(compiler),
        token(token), token2(token2), lhs(std::move(lhs)), T(std::move(T)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {lhs.get(), T.get()}; }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return "as"; }

    [[nodiscard]] Segment segment() const override {
        return range(lhs->segment(), token2);
    }

    [[nodiscard]] TypeReference evalType(TypeReference const& infer) const override;

    [[nodiscard]] std::optional<$union> evalConst() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct ClauseExpr : Expr {
    Token token1, token2;
    std::vector<ExprHandle> lines;

    ClauseExpr(Compiler& compiler, Token token1, Token token2, std::vector<ExprHandle> lines = {}): Expr(compiler),
        token1(token1), token2(token2), lines(std::move(lines)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override {
        std::vector<const Descriptor*> ret;
        for (auto&& e : lines) ret.push_back(e.get());
        return ret;
    }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return "{}"; }

    [[nodiscard]] Segment segment() const override {
        return range(token1, token2);
    }

    [[nodiscard]] TypeReference evalType(TypeReference const& infer) const override;

    [[nodiscard]] std::optional<$union> evalConst() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct IfElseExpr : Expr {
    Token token;
    ExprHandle cond;
    ExprHandle lhs;
    ExprHandle rhs;

    IfElseExpr(Compiler& compiler, Token token, ExprHandle cond, ExprHandle lhs, ExprHandle rhs): Expr(compiler),
        token(token), cond(std::move(cond)), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {cond.get(), lhs.get(), rhs.get()}; }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return "if-else"; }

    [[nodiscard]] Segment segment() const override {
        return range(token, rhs->segment());
    }

    [[nodiscard]] TypeReference evalType(TypeReference const& infer) const override;

    [[nodiscard]] std::optional<$union> evalConst() const override;

    void walkBytecode(Assembler* assembler) const override;

    [[nodiscard]] static std::string walkBytecode(Expr const* cond, Expr const* lhs, Expr const* rhs, Compiler& compiler, Assembler* assembler, const TypeReference& type);
};

struct LoopHook;
struct LoopExpr;

struct BreakExpr : Expr {
    Token token;
    std::shared_ptr<LoopHook> hook;

    BreakExpr(Compiler& compiler, Token token): Expr(compiler), token(token) {}

    [[nodiscard]] std::string_view descriptor() const noexcept override { return "break"; }

    [[nodiscard]] Segment segment() const override {
        return token;
    }

    [[nodiscard]] TypeReference evalType(TypeReference const& infer) const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct LoopHook {
    std::vector<BreakExpr*> breaks;
    const LoopExpr* loop;
};

struct LoopExpr : Expr {
    Token token;
    ExprHandle clause;
    std::shared_ptr<LoopHook> hook;
    mutable size_t breakpoint;

    LoopExpr(Compiler& compiler, Token token, ExprHandle clause, std::shared_ptr<LoopHook> hook):
        Expr(compiler), token(token), clause(std::move(clause)), hook(std::move(hook)) {
        this->hook->loop = this;
        for (auto&& e : this->hook->breaks) {
            e->hook = this->hook;
        }
    }

    [[nodiscard]] Segment segment() const override {
        return range(token, clause->segment());
    }
};

struct WhileExpr : LoopExpr {
    ExprHandle cond;

    WhileExpr(Compiler& compiler, Token token, ExprHandle cond, ExprHandle clause, std::shared_ptr<LoopHook> hook):
        cond(std::move(cond)), LoopExpr(compiler, token, std::move(clause), std::move(hook)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {cond.get(), clause.get()}; }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return "while"; }

    [[nodiscard]] TypeReference evalType(TypeReference const& infer) const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct ReturnExpr : Expr {
    Token token;
    ExprHandle rhs;

    ReturnExpr(Compiler& compiler, Token token, ExprHandle rhs): Expr(compiler), token(token), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {rhs.get()}; }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return "return"; }

    [[nodiscard]] Segment segment() const override {
        return range(token, rhs->segment());
    }

    [[nodiscard]] TypeReference evalType(TypeReference const& infer) const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct ParameterList : Descriptor {
    std::vector<IdExprHandle> identifiers;
    std::shared_ptr<FuncType> prototype;

    ParameterList(std::vector<IdExprHandle> identifiers, std::shared_ptr<FuncType> prototype)
            : identifiers(std::move(identifiers)), prototype(std::move(prototype)) {}

    [[nodiscard]] std::string_view descriptor() const noexcept override { return "()"; }
    [[nodiscard]] std::vector<const Descriptor*> children() const override {
        std::vector<const Descriptor*> ret;
        for (auto&& e : identifiers) ret.push_back(e.get());
        ret.push_back(prototype.get());
        return ret;
    }

    void declare(Compiler& compiler, LocalContext& context) {
        context.offset = identifiers.size();
        for (size_t i = 0; i < identifiers.size(); ++i) {
            context.local(compiler.of(identifiers[i]->token), prototype->P[i]);
        }
    }
};

struct FunctionDefinition : Descriptor {
    ExprHandle clause;
    std::vector<TypeReference> locals;

    FunctionDefinition(ExprHandle clause, std::vector<TypeReference> locals)
            : clause(std::move(clause)), locals(std::move(locals)) {}

    [[nodiscard]] std::string_view descriptor() const noexcept override { return "=" ; }
    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {clause.get()}; }
};

struct FunctionDeclarator: Descriptor {
    std::unique_ptr<IdExpr> name;
    std::unique_ptr<ParameterList> parameters;
    std::unique_ptr<FunctionDefinition> definition;

    FunctionDeclarator(std::unique_ptr<IdExpr> name,
            std::unique_ptr<ParameterList> parameters,
            std::unique_ptr<FunctionDefinition> definition)
    : name(std::move(name)), parameters(std::move(parameters)), definition(std::move(definition)) {}

    [[nodiscard]] std::string_view descriptor() const noexcept override { return "fn" ; }
    [[nodiscard]] std::vector<const Descriptor*> children() const override {
        std::vector<const Descriptor*> children{name.get(), parameters.get()};
        if (definition) {
            children.push_back(definition.get());
        }
        return children;
    }
};

struct Declarator : Descriptor {
    Compiler& compiler;
    Segment segment;
    TypeReference typeCache;

    explicit Declarator(Compiler& compiler, Segment segment): compiler(compiler), segment(segment) {}

    virtual void infer(TypeReference type) = 0;
    virtual void declare(LocalContext& context) const = 0;
    virtual void walkBytecode(std::string const&, Assembler* assembler) const = 0;
};

struct SimpleDeclarator : Declarator {
    IdExprHandle name;
    TypeReference designated;

    SimpleDeclarator(Compiler& compiler, Segment segment, IdExprHandle name, TypeReference designated)
        : Declarator(compiler, segment), name(std::move(name)), designated(std::move(designated))
        { typeCache = this->designated; }

    [[nodiscard]] std::string_view descriptor() const noexcept override { return ":"; }
    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {name.get(), designated.get()}; }

    void infer(TypeReference type) override;
    void declare(LocalContext &context) const override;
    void walkBytecode(std::string const&, Assembler *assembler) const override;
};

struct LetExpr : Expr {
    Token token;
    DeclaratorHandle declarator;
    ExprHandle initializer;

    LetExpr(Compiler& compiler, Token token, DeclaratorHandle declarator, ExprHandle initializer): Expr(compiler),
           token(token), declarator(std::move(declarator)), initializer(std::move(initializer)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {declarator.get(), initializer.get()}; }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return "let"; }

    [[nodiscard]] Segment segment() const override {
        return range(token, initializer->segment());
    }

    [[nodiscard]] TypeReference evalType(TypeReference const& infer) const override;

    void walkBytecode(Assembler* assembler) const override;
};

}
