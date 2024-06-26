#include <cmath>
#include "tree.hpp"
#include "assembler.hpp"
#include "diagnostics.hpp"
#include "lexer.hpp"
#include "global.hpp"

namespace Porkchop {

void Expr::expect(const TypeReference& expected) const {
    if (!getType(expected)->equals(expected)) {
        Error().with(
                ErrorMessage().error(segment())
                .text("expected ").type(expected).text("but got").type(getType())
                ).raise();
    }
}

void Expr::expect(bool pred(const TypeReference&), const char* expected) const {
    if (!pred(getType())) {
        expect(expected);
    }
}

void Expr::expect(const char *expected) const {
    Error().with(
            ErrorMessage().error(segment())
            .text("expected ").text(expected).text(" but got").type(getType())
            ).raise();
}

void matchOperands(Expr* lhs, Expr* rhs) {
    if (!lhs->getType()->equals(rhs->getType())) {
        Error error;
        error.with(ErrorMessage().error(range(lhs->segment(), rhs->segment())).text("type mismatch on both operands"));
        error.with(ErrorMessage().note(lhs->segment()).text("type of left operand is").type(lhs->getType()));
        error.with(ErrorMessage().note(rhs->segment()).text("type of right operand is").type(rhs->getType()));
        error.raise();
    }
}

void assignable(TypeReference const& type, TypeReference const& expected, Segment segment) {
    if (!expected->assignableFrom(type)) {
        Error().with(
                ErrorMessage().error(segment)
                .type(type).text("is not assignable to").type(expected)
                ).raise();
    }
}

void Expr::neverGonnaGiveYouUp(const char* msg) const {
    Porkchop::neverGonnaGiveYouUp(getType(), msg, segment());
}

$union Expr::requireConst() const {
    if (!isConst()) raise("cannot evaluate at compile-time", segment());
    return constValue;
}

TypeReference ensureElements(std::vector<ExprHandle> const& elements, Segment segment, const char* msg) {
    auto type0 = elements.front()->getType();
    elements.front()->neverGonnaGiveYouUp(msg);
    for (size_t i = 1; i < elements.size(); ++i) {
        if (!elements[i]->getType(elements.front()->getType())->equals(type0)) {
            Error error;
            error.with(ErrorMessage().error(segment).text("type must be identical ").text(msg));
            for (auto&& element : elements) {
                error.with(ErrorMessage().note(element->segment()).text("type of this is").type(element->getType()));
            }
            error.raise();
        }
    }
    return type0;
}


BoolConstExpr::BoolConstExpr(Compiler &compiler, Token token) : ConstExpr(compiler, token) {
    parsed = token.type == TokenType::KW_TRUE;
}

TypeReference BoolConstExpr::evalType(TypeReference const& infer) const {
    return ScalarTypes::BOOL;
}

std::optional<$union> BoolConstExpr::evalConst() const {
    return parsed;
}

void BoolConstExpr::walkBytecode(Assembler* assembler) const {
    reg = assembler->const_(parsed);
}

CharConstExpr::CharConstExpr(Compiler &compiler, Token token) : ConstExpr(compiler, token) {
    parsed = parseChar(compiler.source, token);
}

TypeReference CharConstExpr::evalType(TypeReference const& infer) const {
    return ScalarTypes::INT;
}

void CharConstExpr::walkBytecode(Assembler* assembler) const {
    reg = assembler->const_((int64_t)parsed);
}

std::optional<$union> CharConstExpr::evalConst() const {
    return (int64_t)parsed;
}

IntConstExpr::IntConstExpr(Compiler &compiler, Token token, bool merged) : ConstExpr(compiler, token), merged(merged) {
    switch (token.type) {
        case TokenType::KW_LINE:
            parsed = int64_t(token.line);
            break;
        case TokenType::BINARY_INTEGER:
        case TokenType::OCTAL_INTEGER:
        case TokenType::DECIMAL_INTEGER:
        case TokenType::HEXADECIMAL_INTEGER:
            parsed = parseInt(compiler.source, token);
            break;
        default:
            unreachable();
    }
}

TypeReference IntConstExpr::evalType(TypeReference const& infer) const {
    return ScalarTypes::INT;
}

std::optional<$union> IntConstExpr::evalConst() const {
    return parsed;
}

void IntConstExpr::walkBytecode(Assembler* assembler) const {
    reg = assembler->const_(parsed);
}

FloatConstExpr::FloatConstExpr(Compiler &compiler, Token token) : ConstExpr(compiler, token) {
    parsed = parseFloat(compiler.source, token);
}

TypeReference FloatConstExpr::evalType(TypeReference const& infer) const {
    return ScalarTypes::FLOAT;
}

void FloatConstExpr::walkBytecode(Assembler* assembler) const {
     reg = assembler->const_(parsed);
}

std::optional<$union> FloatConstExpr::evalConst() const {
    return parsed;
}

TypeReference IdExpr::evalType(TypeReference const& infer) const {
    return lookup.type;
}

void SizeofExpr::walkBytecode(Porkchop::Assembler *assembler) const {
    reg = assembler->const_(type->size());
}

void AssignableExpr::walkBytecode(Porkchop::Assembler *assembler) const {
    auto seg = segment();
    Token token{.line = seg.line1, .column = seg.column1};
    reg = assembler->load(addressOf(assembler), getType(), token);
}

void AssignableExpr::walkStoreBytecode(const std::string &from, Assembler *assembler) const {
    auto seg = segment();
    Token token{.line = seg.line1, .column = seg.column1};
    assembler->store(from, addressOf(assembler), getType(), token);
}

std::optional<$union> IdExpr::evalConst() const {
    if (compiler.of(token) == "_")
        return nullptr;
    return Expr::evalConst();
}

void IdExpr::ensureAssignable() const {
    if (lookup.scope == LocalContext::LookupResult::Scope::GLOBAL && isFunction(getType())) {
        raise("function is not assignable", segment());
    }
}

void IdExpr::walkBytecode(Porkchop::Assembler *assembler) const {
    if (lookup.scope == LocalContext::LookupResult::Scope::GLOBAL && isFunction(getType())) {
        reg = Assembler::escape(compiler.of(token));
        return;
    }
    AssignableExpr::walkBytecode(assembler);
}

std::string IdExpr::addressOf(Assembler *assembler) const {
    switch (lookup.scope) {
        case LocalContext::LookupResult::Scope::NONE:
            return "%error";
        case LocalContext::LookupResult::Scope::LOCAL:
            return Assembler::regOf(lookup.index);
        case LocalContext::LookupResult::Scope::GLOBAL:
            return Assembler::escape(compiler.of(token));
        default:
            unreachable();
    }
}

TypeReference PrefixExpr::evalType(TypeReference const& infer) const {
    auto type = rhs->getType();
    switch (token.type) {
        case TokenType::OP_ADD:
        case TokenType::OP_SUB:
            rhs->expect(isArithmetic, "arithmetic type");
            break;
        case TokenType::OP_NOT:
            rhs->expect(ScalarTypes::BOOL);
            break;
        case TokenType::OP_INV:
            rhs->expect(ScalarTypes::INT);
            break;
        default:
            unreachable();
    }
    return rhs->getType();
}

std::optional<$union> PrefixExpr::evalConst() const {
    auto type = rhs->getType();
    if (!rhs->isConst()) return std::nullopt;
    auto value = rhs->requireConst();
    switch (token.type) {
        case TokenType::OP_ADD:
            return value;
        case TokenType::OP_SUB:
            if (isInt(type)) {
                return -value.$int;
            } else {
                return -value.$float;
            }
        case TokenType::OP_NOT:
            return !value.$bool;
        case TokenType::OP_INV:
            return ~value.$int;
        default:
            return Expr::evalConst();
    }
}

void PrefixExpr::walkBytecode(Assembler* assembler) const {
    auto type = rhs->getType();
    rhs->walkBytecode(assembler);
    switch (token.type) {
        case TokenType::OP_ADD:
            reg = rhs->reg;
            break;
        case TokenType::OP_SUB: {
            reg = assembler->neg(rhs->reg, getType(), token);
            break;
        }
        case TokenType::OP_NOT: {
            reg = assembler->infix("xor", "1", rhs->reg, getType(), token);
            break;
        }
        case TokenType::OP_INV: {
            reg = assembler->infix("xor", "-1", rhs->reg, getType(), token);
            break;
        }
        default:
            unreachable();
    }
}

TypeReference AddressOfExpr::evalType(const TypeReference &infer) const {
    rhs->neverGonnaGiveYouUp("to take address");
    return std::make_shared<PointerType>(rhs->getType());
}

void AddressOfExpr::walkBytecode(Assembler *assembler) const {
    reg = rhs->addressOf(assembler);
}

TypeReference DereferenceExpr::evalType(const TypeReference &infer) const {
    if (auto ptr = dynamic_cast<PointerType*>(rhs->getType().get())) {
        if (isNone(ptr->E)) {
            raise("none* is not allowed to dereference", segment());
        }
        return ptr->E;
    }
    rhs->expect("pointer type");
}

void DereferenceExpr::ensureAssignable() const {

}

std::string DereferenceExpr::addressOf(Assembler *assembler) const {
    rhs->walkBytecode(assembler);
    return rhs->reg;
}

TypeReference StatefulPrefixExpr::evalType(TypeReference const& infer) const {
    rhs->ensureAssignable();
    auto type = rhs->getType();
    if (!isInt(type) && !isPointer(type)) {
        rhs->expect("int or pointer type");
    }
    return type;
}

void StatefulPrefixExpr::walkBytecode(Assembler* assembler) const {
    auto one = token.type == TokenType::OP_INC ? "1" : "-1";
    rhs->walkBytecode(assembler);
    auto type = rhs->getType();
    if (isInt(type)) {
        reg = assembler->infix("add", rhs->reg, one, type, token);
    } else {
        reg = assembler->offset(rhs->reg, one, type, token);
    }
    rhs->walkStoreBytecode(reg, assembler);
}

TypeReference StatefulPostfixExpr::evalType(TypeReference const& infer) const {
    lhs->ensureAssignable();
    auto type = lhs->getType();
    if (!isInt(type) && !isPointer(type)) {
        lhs->expect("int or pointer type");
    }
    return type;
}

void StatefulPostfixExpr::walkBytecode(Assembler* assembler) const {
    auto one = token.type == TokenType::OP_INC ? "1" : "-1";
    auto type = lhs->getType();
    lhs->walkBytecode(assembler);
    lhs->walkStoreBytecode(
            isInt(type)
            ? assembler->infix("add", lhs->reg, one, type, token)
            : assembler->offset(lhs->reg, one, type, token),
            assembler);
    reg = lhs->reg;
}

TypeReference InfixExpr::evalType(TypeReference const& infer) const {
    auto type1 = lhs->getType(), type2 = rhs->getType();
    if (auto ptr = dynamic_cast<PointerType*>(type1.get()); ptr && token.type == TokenType::OP_SUB && type1->equals(type2)) {
        if (isNone(ptr->E)) {
            raise("none* cannot get involved in pointer arithmetics", segment());
        }
        return ScalarTypes::INT;
    }
    switch (token.type) {
        case TokenType::OP_OR:
        case TokenType::OP_XOR:
        case TokenType::OP_AND:
            lhs->expect(ScalarTypes::INT);
            matchOperands(lhs.get(), rhs.get());
            return type1;
        case TokenType::OP_SHL:
        case TokenType::OP_SHR:
        case TokenType::OP_USHR:
            lhs->expect(ScalarTypes::INT);
            rhs->expect(ScalarTypes::INT);
            return type1;
        case TokenType::OP_ADD:
            if (auto ptr = dynamic_cast<PointerType*>(type2.get()); ptr && isInt(type1)) {
                if (isNone(ptr->E)) {
                    raise("none* cannot get involved in pointer arithmetics", segment());
                }
                return type2;
            }
        case TokenType::OP_SUB:
            if (auto ptr = dynamic_cast<PointerType*>(type1.get()); ptr && isInt(type2)) {
                if (isNone(ptr->E)) {
                    raise("none* cannot get involved in pointer arithmetics", segment());
                }
                return type1;
            }
        case TokenType::OP_MUL:
        case TokenType::OP_DIV:
        case TokenType::OP_REM:
            matchOperands(lhs.get(), rhs.get());
            lhs->expect(isArithmetic, "arithmetic");
            return type1;
        default:
            unreachable();
    }
}

std::optional<$union> InfixExpr::evalConst() const {
    if (!lhs->isConst() || !rhs->isConst()) return std::nullopt;
    auto value1 = lhs->requireConst(), value2 = rhs->requireConst();
    switch (token.type) {
        case TokenType::OP_OR:
            return value1.$size | value2.$size;
        case TokenType::OP_XOR:
            return value1.$size ^ value2.$size;
        case TokenType::OP_AND:
            return value1.$size & value2.$size;
        case TokenType::OP_SHL:
            return value1.$int << value2.$int;
        case TokenType::OP_SHR:
            return value1.$int >> value2.$int;
        case TokenType::OP_USHR:
            return value1.$size >> value2.$int;
        case TokenType::OP_ADD:
            if (isInt(lhs->getType())) {
                return value1.$int + value2.$int;
            } else {
                return value1.$float + value2.$float;
            }
        case TokenType::OP_SUB:
            if (isInt(lhs->getType())) {
                return value1.$int - value2.$int;
            } else {
                return value1.$float - value2.$float;
            }
        case TokenType::OP_MUL:
            if (isInt(lhs->getType())) {
                return value1.$int * value2.$int;
            } else {
                return value1.$float * value2.$float;
            }
        case TokenType::OP_DIV:
            if (isInt(lhs->getType())) {
                int64_t divisor = value2.$int;
                if (divisor == 0) raise("divided by zero", segment());
                return value1.$int / divisor;
            } else {
                return value1.$float / value2.$float;
            }
        case TokenType::OP_REM:
            if (isInt(lhs->getType())) {
                int64_t divisor = value2.$int;
                if (divisor == 0) raise("divided by zero", segment());
                return value1.$int % divisor;
            } else {
                return std::fmod(value1.$float, value2.$float);
            }
        default:
            unreachable();
    }
}

void InfixExpr::walkBytecode(Assembler* assembler) const {
    lhs->walkBytecode(assembler);
    rhs->walkBytecode(assembler);
    bool i = isInt(lhs->getType());
    auto type1 = lhs->getType(), type2 = rhs->getType();
    if (auto ptr = dynamic_cast<PointerType*>(type1.get()); ptr && token.type == TokenType::OP_SUB && type1->equals(type2)) {
        auto ptr1 = assembler->cast("ptrtoint", lhs->reg, type1, ScalarTypes::INT, token);
        auto ptr2 = assembler->cast("ptrtoint", rhs->reg, type2, ScalarTypes::INT, token);
        auto sub = assembler->infix("sub", ptr1, ptr2, ScalarTypes::INT, token);
        auto sdiv = assembler->infix("sdiv", sub, assembler->const_(ptr->E->size()), ScalarTypes::INT, token);
        reg = std::move(sdiv);
        return;
    }
    switch (token.type) {
        case TokenType::OP_OR:
            reg = assembler->infix("or", lhs->reg, rhs->reg, getType(), token);
            break;
        case TokenType::OP_XOR:
            reg = assembler->infix("xor", lhs->reg, rhs->reg, getType(), token);
            break;
        case TokenType::OP_AND:
            reg = assembler->infix("and", lhs->reg, rhs->reg, getType(), token);
            break;
        case TokenType::OP_SHL:
            reg = assembler->infix("shl", lhs->reg, rhs->reg, getType(), token);
            break;
        case TokenType::OP_SHR:
            reg = assembler->infix("ashr", lhs->reg, rhs->reg, getType(), token);
            break;
        case TokenType::OP_USHR:
            reg = assembler->infix("lshr", lhs->reg, rhs->reg, getType(), token);
            break;
        case TokenType::OP_ADD:
            if (isInt(type1) && isPointer(type2)) {
                reg = assembler->offset(rhs->reg, lhs->reg, getType(), token);
            } else if (isInt(type2) && isPointer(type1)) {
                reg = assembler->offset(lhs->reg, rhs->reg, getType(), token);
            } else {
                reg = assembler->infix(i ? "add" : "fadd", lhs->reg, rhs->reg, getType(), token);
            }
            break;
        case TokenType::OP_SUB:
            if (isInt(type2) && isPointer(type1)) {
                reg = assembler->offset(lhs->reg, assembler->neg(rhs->reg, rhs->getType(), token), getType(), token);
            } else {
                reg = assembler->infix(i ? "sub" : "fsub", lhs->reg, rhs->reg, getType(), token);
            }
            break;
        case TokenType::OP_MUL:
            reg = assembler->infix(i ? "mul" : "fmul", lhs->reg, rhs->reg, getType(), token);
            break;
        case TokenType::OP_DIV:
            reg = assembler->infix(i ? "sdiv" : "fdiv", lhs->reg, rhs->reg, getType(), token);
            break;
        case TokenType::OP_REM:
            reg = assembler->infix(i ? "srem" : "frem", lhs->reg, rhs->reg, getType(), token);
            break;
        default:
            unreachable();
    }
}

TypeReference CompareExpr::evalType(TypeReference const& infer) const {
    matchOperands(lhs.get(), rhs.get());
    lhs->neverGonnaGiveYouUp("in relational operations");
    auto type = lhs->getType();
    bool equality = token.type == TokenType::OP_EQ || token.type == TokenType::OP_NE;
    if (!equality) {
        if (isNone(type)) {
            raise("none only support equality operators", segment());
        } else if (isFunction(type)) {
            raise("function type only support equality operators", segment());
        }
    }
    return ScalarTypes::BOOL;
}

std::optional<$union> CompareExpr::evalConst() const {
    auto type = lhs->getType();
    if (isNone(type)) return token.type == TokenType::OP_EQ;
    if (!lhs->isConst() || !rhs->isConst()) return std::nullopt;
    auto value1 = lhs->requireConst(), value2 = rhs->requireConst();
    std::partial_ordering cmp = value1.$size <=> value2.$size;
    if (isInt(type)) {
        cmp = value1.$int <=> value2.$int;
    } else if (isFloat(type)) {
        cmp = value1.$float <=> value2.$float;
    }
    switch (token.type) {
        case TokenType::OP_EQ:
            return cmp == 0;
        case TokenType::OP_NE:
            return cmp != 0;
        case TokenType::OP_LT:
            return cmp < 0;
        case TokenType::OP_GT:
            return cmp > 0;
        case TokenType::OP_LE:
            return cmp <= 0;
        case TokenType::OP_GE:
            return cmp >= 0;
        default:
            unreachable();
    }
}

void CompareExpr::walkBytecode(Assembler *assembler) const {
    auto type = lhs->getType();
    if (isNone(type)) {
        reg = assembler->const_(token.type == TokenType::OP_EQ);
        return;
    }
    lhs->walkBytecode(assembler);
    rhs->walkBytecode(assembler);
    const char* op1 = "", *op2 = "";
    if (!isFloat(lhs->getType())) {
        bool p = isPointer(type);
        op1 = "icmp";
        switch (token.type) {
            case TokenType::OP_EQ:
                op2 = "eq";
                break;
            case TokenType::OP_NE:
                op2 = "ne";
                break;
            case TokenType::OP_LT:
                op2 = p ? "ult" : "slt";
                break;
            case TokenType::OP_LE:
                op2 = p ? "ule" : "sle";
                break;
            case TokenType::OP_GT:
                op2 = p ? "ugt" : "sgt";
                break;
            case TokenType::OP_GE:
                op2 = p ? "uge" : "sge";
                break;
            default:
                unreachable();
        }
    } else {
        op1 = "fcmp";
        switch (token.type) {
            case TokenType::OP_EQ:
                op2 = "oeq";
                break;
            case TokenType::OP_NE:
                op2 = "une";
                break;
            case TokenType::OP_LT:
                op2 = "olt";
                break;
            case TokenType::OP_LE:
                op2 = "ole";
                break;
            case TokenType::OP_GT:
                op2 = "ogt";
                break;
            case TokenType::OP_GE:
                op2 = "oge";
                break;
            default:
                unreachable();
        }
    }
    reg = assembler->compare(op1, op2, lhs->reg, rhs->reg, lhs->getType(), token);
}

TypeReference LogicalExpr::evalType(TypeReference const& infer) const {
    lhs->expect(ScalarTypes::BOOL);
    rhs->expect(ScalarTypes::BOOL);
    return ScalarTypes::BOOL;
}

std::optional<$union> LogicalExpr::evalConst() const {
    auto conjunction = token.type == TokenType::OP_LAND;
    if (!lhs->isConst()) return std::nullopt;
    auto value1 = lhs->requireConst();
    if (conjunction == value1.$bool) {
        if (!rhs->isConst()) return std::nullopt;
        return rhs->requireConst();
    }
    return value1.$bool;
}

void LogicalExpr::walkBytecode(Assembler* assembler) const {
    if (token.type == TokenType::OP_LAND) {
        static const BoolConstExpr zero{compiler, {0, 0, 0, TokenType::KW_FALSE}};
        reg = IfElseExpr::walkBytecode(lhs.get(), rhs.get(), &zero, compiler, assembler, ScalarTypes::BOOL, token);
    } else {
        static const BoolConstExpr one{compiler, {0, 0, 0, TokenType::KW_TRUE}};
        reg = IfElseExpr::walkBytecode(lhs.get(), &one, rhs.get(), compiler, assembler, ScalarTypes::BOOL, token);
    }
}

TypeReference InfixInvokeExpr::evalType(TypeReference const& infer) const {
    if (auto func = dynamic_cast<FuncType*>(infix->getType().get())) {
        if (func->P.size() != 2) {
            Error error;
            error.with(ErrorMessage().error(segment()).text("infix invocation expected a function with exact two parameters but got").num(func->P.size()));
            error.with(ErrorMessage().note(infix->segment()).text("type of this function is").type(infix->getType()));
            error.raise();
        }
        if (!func->P[0]->assignableFrom(lhs->getType(func->P[0]))) {
            Error error;
            error.with(ErrorMessage().error(lhs->segment()).type(lhs->getType()).text("is not assignable to").type(func->P[0]));
            error.with(ErrorMessage().note(infix->segment()).text("type of this function is").type(infix->getType()));
            error.raise();
        }
        if (!func->P[1]->assignableFrom(rhs->getType(func->P[1]))) {
            Error error;
            error.with(ErrorMessage().error(rhs->segment()).type(rhs->getType()).text("is not assignable to").type(func->P[1]));
            error.with(ErrorMessage().note(infix->segment()).text("type of this function is").type(infix->getType()));
            error.raise();
        }
        return func->R;
    }
    infix->expect("invocable type");
}

void InfixInvokeExpr::walkBytecode(Assembler *assembler) const {
    reg = InvokeExpr::walkBytecode(infix.get(), {lhs.get(), rhs.get()}, assembler, getType(), token, token);
}

TypeReference AssignExpr::evalType(TypeReference const& infer) const {
    lhs->ensureAssignable();
    auto type1 = lhs->getType();
    switch (token.type) {
        case TokenType::OP_ASSIGN:
            assignable(rhs->getType(lhs->getType()), lhs->getType(), segment());
            return type1;
        case TokenType::OP_ASSIGN_AND:
        case TokenType::OP_ASSIGN_XOR:
        case TokenType::OP_ASSIGN_OR:
            lhs->expect(ScalarTypes::INT);
            rhs->expect(type1);
            return type1;
        case TokenType::OP_ASSIGN_SHL:
        case TokenType::OP_ASSIGN_SHR:
        case TokenType::OP_ASSIGN_USHR:
            lhs->expect(ScalarTypes::INT);
            rhs->expect(ScalarTypes::INT);
            return type1;
        case TokenType::OP_ASSIGN_ADD:
        case TokenType::OP_ASSIGN_SUB:
            if (isPointer(type1)) {
                rhs->expect(ScalarTypes::INT);
                return type1;
            }
        case TokenType::OP_ASSIGN_MUL:
        case TokenType::OP_ASSIGN_DIV:
        case TokenType::OP_ASSIGN_REM:
            lhs->expect(isArithmetic, "arithmetic type");
            rhs->expect(type1);
            return type1;
        default:
            unreachable();
    }
}

void AssignExpr::walkBytecode(Assembler* assembler) const {
    if (token.type == TokenType::OP_ASSIGN) {
        rhs->walkBytecode(assembler);
        lhs->walkStoreBytecode(reg = rhs->reg, assembler);
    } else {
        lhs->walkBytecode(assembler);
        rhs->walkBytecode(assembler);
        bool i = isInt(lhs->getType());
        bool p = isPointer(lhs->getType());
        switch (token.type) {
            case TokenType::OP_ASSIGN_OR:
                reg = assembler->infix("or", lhs->reg, rhs->reg, getType(), token);
                break;
            case TokenType::OP_ASSIGN_XOR:
                reg = assembler->infix("xor", lhs->reg, rhs->reg, getType(), token);
                break;
            case TokenType::OP_ASSIGN_AND:
                reg = assembler->infix("and", lhs->reg, rhs->reg, getType(), token);
                break;
            case TokenType::OP_ASSIGN_SHL:
                reg = assembler->infix("shl", lhs->reg, rhs->reg, getType(), token);
                break;
            case TokenType::OP_ASSIGN_SHR:
                reg = assembler->infix("ashr", lhs->reg, rhs->reg, getType(), token);
                break;
            case TokenType::OP_ASSIGN_USHR:
                reg = assembler->infix("lshr", lhs->reg, rhs->reg, getType(), token);
                break;
            case TokenType::OP_ASSIGN_ADD:
                if (p) {
                    reg = assembler->offset(lhs->reg, rhs->reg, getType(), token);
                } else {
                    reg = assembler->infix(i ? "add" : "fadd", lhs->reg, rhs->reg, getType(), token);
                }
                break;
            case TokenType::OP_ASSIGN_SUB:
                if (p) {
                    reg = assembler->offset(lhs->reg, assembler->neg(rhs->reg, rhs->getType(), token), getType(), token);
                } else {
                    reg = assembler->infix(i ? "sub" : "fsub", lhs->reg, rhs->reg, getType(), token);
                }
                break;
            case TokenType::OP_ASSIGN_MUL:
                reg = assembler->infix(i ? "mul" : "fmul", lhs->reg, rhs->reg, getType(), token);
                break;
            case TokenType::OP_ASSIGN_DIV:
                reg = assembler->infix(i ? "sdiv" : "fdiv", lhs->reg, rhs->reg, getType(), token);
                break;
            case TokenType::OP_ASSIGN_REM:
                reg = assembler->infix(i ? "srem" : "frem", lhs->reg, rhs->reg, getType(), token);
                break;
            default:
                unreachable();
        }
        lhs->walkStoreBytecode(reg, assembler);
    }
}

TypeReference AccessExpr::evalType(TypeReference const& infer) const {
    rhs->expect(ScalarTypes::INT);
    auto type = lhs->getType();
    if (auto ptr = dynamic_cast<PointerType*>(type.get())) {
        return ptr->E;
    }
    lhs->expect("pointer type");
}

void AccessExpr::ensureAssignable() const {

}

std::string AccessExpr::addressOf(Assembler *assembler) const {
    lhs->walkBytecode(assembler);
    rhs->walkBytecode(assembler);
    return assembler->offset(lhs->reg, rhs->reg, getType(), token1);
}

TypeReference InvokeExpr::evalType(TypeReference const& infer) const {
    if (auto func = dynamic_cast<FuncType*>(lhs->getType().get())) {
        if (rhs.size() != func->P.size()) {
            Error error;
            error.with(ErrorMessage().error(range(token1, token2)).text("expected").num(func->P.size()).text("parameters but got").num(rhs.size()));
            error.with(ErrorMessage().note(lhs->segment()).text("type of this function is").type(lhs->getType()));
            error.raise();
        }
        for (size_t i = 0; i < rhs.size(); ++i) {
            if (!func->P[i]->assignableFrom(rhs[i]->getType(func->P[i]))) {
                Error error;
                error.with(ErrorMessage().error(rhs[i]->segment()).type(rhs[i]->getType()).text("is not assignable to").type(func->P[i]));
                error.with(ErrorMessage().note(lhs->segment()).text("type of this function is").type(lhs->getType()));
                error.raise();
            }
        }
        return func->R;
    }
    lhs->expect("invocable type");
}

std::string InvokeExpr::walkBytecode(const Expr *lhs, const std::vector<const Expr *> &rhs, Assembler *assembler, const TypeReference& type, Token token1, Token token2) {
    std::string reg = "%error";
    lhs->walkBytecode(assembler);
    for (auto& e : rhs) {
        e->walkBytecode(assembler);
    }
    std::string call;
    if (!isNone(type) && !isNever(type)) {
        reg = assembler->next();
        call += reg;
        call += " = ";
    }
    call += "call ";
    call += type->serialize();
    call += " ";
    call += lhs->reg;
    call += "(";
    bool first = true;
    for (auto& e : rhs) {
        if (first) first = false; else call += ", ";
        call += e->getType()->serialize();
        call += " ";
        call += e->reg;
    }
    call += ")";
    assembler->append(call, token1);
    if (isNever(type)) {
        assembler->append("unreachable", token2);
    }
    return reg;
}

void InvokeExpr::walkBytecode(Assembler* assembler) const {
    std::vector<Expr const*> params;
    for (auto&& e : rhs) {
        params.push_back(e.get());
    }
    reg = walkBytecode(lhs.get(), params, assembler, getType(), token1, token2);
}

TypeReference AsExpr::evalType(TypeReference const& infer) const {
    auto type = lhs->getType(T);
    if (T->assignableFrom(type)
        || isSimilar(isArithmetic, type, T) || isSimilar(isPointerLike, type, T)) return T;
    Error().with(
            ErrorMessage().error(segment())
            .text("cannot cast this expression from").type(type).text("to").type(T)
            ).raise();
}

std::optional<$union> AsExpr::evalConst() const {
    if (!lhs->isConst()) return std::nullopt;
    auto value = lhs->requireConst();
    if (isInt(lhs->getType())) {
        if (isFloat(T)) {
            return (double)value.$int;
        }
    } else if (isInt(T)) {
        if (isFloat(lhs->getType())) {
            return (int64_t)value.$float;
        }
    }
    return value;
}

void AsExpr::walkBytecode(Assembler* assembler) const {
    lhs->walkBytecode(assembler);
    auto type = lhs->getType();
    if (isNone(T)) return;
    if (type->equals(T)) {
        reg = lhs->reg;
        return;
    }
    if (isInt(type)) {
        if (isFloat(T)) {
            reg = assembler->cast("sitofp", lhs->reg, type, T, token);
        } else if (isPointer(T)) {
            reg = assembler->cast("inttoptr", lhs->reg, type, T, token);
        }
    } else if (isPointer(type)) {
        if (isInt(T)) {
            reg = assembler->cast("ptrtoint", lhs->reg, type, T, token);
        } else if (isPointer(T)) {
            reg = lhs->reg;
        }
    } else if (isFloat(type)) {
        if (isInt(T)) {
            reg = assembler->cast("fptosi", lhs->reg, type, T, token);
        }
    }
}

TypeReference ClauseExpr::evalType(TypeReference const& infer) const {
    if (lines.empty()) return ScalarTypes::NONE;
    for (size_t i = 0; i < lines.size() - 1; ++i) {
        if (isNever(lines[i]->getType())) {
            Error error;
            error.with(ErrorMessage().error(lines[i + 1]->segment()).text("this line is unreachable"));
            error.with(ErrorMessage().note(lines[i]->segment()).text("since the previous line never returns"));
            error.raise();
        }
    }
    return lines.back()->getType();
}

std::optional<$union> ClauseExpr::evalConst() const {
    if (lines.empty()) return nullptr;
    $union value = nullptr;
    for (auto&& expr : lines) {
        if (!expr->isConst()) return std::nullopt;
        value = expr->requireConst();
    }
    return value;
}

void ClauseExpr::walkBytecode(Assembler* assembler) const {
    char buf[256];
    auto old = std::move(assembler->scope);
    if (Assembler::debug_flag) {
        sprintf(buf, "distinct !DILexicalBlock(scope: %s, file: %s, line: %zu, column: %zu)",
                old.data(), Assembler::DEBUG::FILE, token1.line, token1.column);
        assembler->scope = assembler->debug(buf);
    }
    for (auto&& line : lines) {
        line->walkBytecode(assembler);
        reg = line->reg;
    }
    assembler->scope = std::move(old);
}

TypeReference IfElseExpr::evalType(TypeReference const& infer) const {
    cond->expect(ScalarTypes::BOOL);
    if (auto either = eithertype(lhs->getType(), rhs->getType())) {
        return either;
    } else {
        matchOperands(lhs.get(), rhs.get());
        unreachable();
    }
}

std::optional<$union> IfElseExpr::evalConst() const {
    if (!cond->isConst()) return std::nullopt;
    Expr* expr = (cond->requireConst().$bool ? lhs.get() : rhs.get());
    if (!expr->isConst()) return std::nullopt;
    return expr->requireConst();
}

void IfElseExpr::walkBytecode(Assembler* assembler) const {
    reg = walkBytecode(cond.get(), lhs.get(), rhs.get(), compiler, assembler, getType(), token);
}

std::string IfElseExpr::walkBytecode(Expr const* cond, Expr const* lhs, Expr const* rhs, Compiler& compiler, Assembler* assembler, const TypeReference& type, Token token) {
    size_t A = compiler.global->labelUntil++;
    size_t B = compiler.global->labelUntil++;
    size_t C = compiler.global->labelUntil++;
    bool store = !isNone(type);
    auto reg = store ? assembler->alloca_(type) : "%error";
    cond->walkBytecode(assembler);
    assembler->br(cond->reg, A, B);
    assembler->label(A);
    lhs->walkBytecode(assembler);
    if (store && !isNever(lhs->getType())) assembler->store(lhs->reg, reg, type, token);
    if (!isNever(lhs->getType()))
        assembler->br(C);
    assembler->label(B);
    rhs->walkBytecode(assembler);
    if (store && !isNever(rhs->getType())) assembler->store(rhs->reg, reg, type, token);
    if (!isNever(rhs->getType()))
        assembler->br(C);
    if (isNever(type)) return reg;
    assembler->label(C);
    return store ? assembler->load(reg, type, token) : reg;
}

TypeReference BreakExpr::evalType(TypeReference const& infer) const {
    return ScalarTypes::NEVER;
}

void BreakExpr::walkBytecode(Assembler* assembler) const {
    size_t A = hook->loop->breakpoint;
    assembler->br(A);
}

TypeReference WhileExpr::evalType(TypeReference const& infer) const {
    if (isNever(cond->getType())) return ScalarTypes::NEVER;
    cond->expect(ScalarTypes::BOOL);
    if (cond->isConst() && cond->requireConst().$bool && hook->breaks.empty())
        return ScalarTypes::NEVER;
    return ScalarTypes::NONE;
}

void WhileExpr::walkBytecode(Assembler* assembler) const {
    size_t A = compiler.global->labelUntil++;
    size_t B = compiler.global->labelUntil++;
    size_t C = compiler.global->labelUntil++;
    breakpoint = C;
    assembler->br(A);
    assembler->label(A);
    cond->walkBytecode(assembler);
    assembler->br(cond->reg, B, C);
    assembler->label(B);
    clause->walkBytecode(assembler);
    if (!isNever(clause->getType())) {
        assembler->br(A);
    }
    assembler->label(C);
    if (isNever(getType())) {
        assembler->append("unreachable", token);
    }
}

TypeReference ReturnExpr::evalType(TypeReference const& infer) const {
    rhs->neverGonnaGiveYouUp("to return");
    return ScalarTypes::NEVER;
}

void ReturnExpr::walkBytecode(Assembler* assembler) const {
    rhs->walkBytecode(assembler);
    assembler->return_(rhs->reg, rhs->getType(), token);
}

void SimpleDeclarator::infer(TypeReference type) {
    if (designated == nullptr) {
        designated = type;
    } else {
        assignable(type, designated, segment);
    }
    typeCache = designated;
}

void SimpleDeclarator::declare(LocalContext &context) const {
    context.local(compiler.of(name->token), designated);
    name->initLookup(context);
}

void SimpleDeclarator::walkBytecode(std::string const& from, Assembler *assembler) const {
    name->walkStoreBytecode(from, assembler);
}

TypeReference LetExpr::evalType(TypeReference const& infer) const {
    if (isNone(declarator->typeCache))
        raise("PorkchopLite does not support let of none type", segment());
    return declarator->typeCache;
}

void LetExpr::walkBytecode(Assembler *assembler) const {
    initializer->walkBytecode(assembler);
    declarator->walkBytecode(reg = initializer->reg, assembler);
    assembler->local(compiler, declarator->name->token, declarator->name->addressOf(assembler), getType());
}

}
