#include "parser.hpp"
#include "global.hpp"

namespace Porkchop {

bool isInLevel(TokenType type, Expr::Level level) {
    switch (type) {
        case TokenType::OP_LOR: return level == Expr::Level::LOR;
        case TokenType::OP_LAND: return level == Expr::Level::LAND;
        case TokenType::OP_OR: return level == Expr::Level::OR;
        case TokenType::OP_XOR: return level == Expr::Level::XOR;
        case TokenType::OP_AND: return level == Expr::Level::AND;
        case TokenType::OP_EQ:
        case TokenType::OP_NE: return level == Expr::Level::EQUALITY;
        case TokenType::OP_LT:
        case TokenType::OP_GT:
        case TokenType::OP_LE:
        case TokenType::OP_GE: return level == Expr::Level::COMPARISON;
        case TokenType::OP_SHL:
        case TokenType::OP_SHR:
        case TokenType::OP_USHR: return level == Expr::Level::SHIFT;
        case TokenType::OP_ADD:
        case TokenType::OP_SUB: return level == Expr::Level::ADDITION;
        case TokenType::IDENTIFIER:
        case TokenType::OP_MUL:
        case TokenType::OP_DIV:
        case TokenType::OP_REM: return level == Expr::Level::MULTIPLICATION;
        default: return false;
    }
}

ExprHandle Parser::parseExpression(Expr::Level level) {
    switch (level) {
        case Expr::Level::ASSIGNMENT: {
            switch (Token token = peek(); token.type) {
                case TokenType::KW_BREAK: {
                    auto expr = make<BreakExpr>(next());
                    if (hooks.empty()) {
                        Error().with(
                                ErrorMessage().error(token)
                                .text("wild").quote("break")
                                ).raise();
                    }
                    hooks.back()->breaks.push_back(expr.get());
                    return expr;
                }
                case TokenType::KW_RETURN: {
                    next();
                    auto expr = make<ReturnExpr>(token, parseExpression(level));
                    returns.push_back(expr.get());
                    return expr;
                }
                default: {
                    auto lhs = parseExpression(Expr::upper(level));
                    switch (Token token = peek(); token.type) {
                        case TokenType::OP_ASSIGN:
                        case TokenType::OP_ASSIGN_AND:
                        case TokenType::OP_ASSIGN_XOR:
                        case TokenType::OP_ASSIGN_OR:
                        case TokenType::OP_ASSIGN_SHL:
                        case TokenType::OP_ASSIGN_SHR:
                        case TokenType::OP_ASSIGN_USHR:
                        case TokenType::OP_ASSIGN_ADD:
                        case TokenType::OP_ASSIGN_SUB:
                        case TokenType::OP_ASSIGN_MUL:
                        case TokenType::OP_ASSIGN_DIV:
                        case TokenType::OP_ASSIGN_REM: {
                            next();
                            auto rhs = parseExpression(level);
                            auto segment = rhs->segment();
                            if (auto load = dynamic_pointer_cast<AssignableExpr>(std::move(lhs))) {
                                return make<AssignExpr>(token, std::move(load), std::move(rhs));
                            } else {
                                raise("assignable expression is expected", segment);
                            }
                        }
                    }
                    return lhs;
                }
            }
        }
        [[likely]]
        default: {
            auto lhs = parseExpression(Expr::upper(level));
            while (isInLevel(peek().type, level)) {
                auto token = next();
                auto rhs = parseExpression(Expr::upper(level));
                switch (level) {
                    case Expr::Level::LAND:
                    case Expr::Level::LOR:
                        lhs = make<LogicalExpr>(token, std::move(lhs), std::move(rhs));
                        break;
                    case Expr::Level::COMPARISON:
                    case Expr::Level::EQUALITY:
                        lhs = make<CompareExpr>(token, std::move(lhs), std::move(rhs));
                        break;
                    default:
                        if (token.type == TokenType::IDENTIFIER) {
                            auto id = std::make_unique<IdExpr>(compiler, token);
                            id->initLookup(context);
                            lhs = make<InfixInvokeExpr>(std::move(id), std::move(lhs), std::move(rhs));
                        } else {
                            lhs = make<InfixExpr>(token, std::move(lhs), std::move(rhs));
                        }
                        break;
                }
            }
            return lhs;
        }
        case Expr::Level::PREFIX: {
            switch (Token token = peek(); token.type) {
                case TokenType::OP_ADD:
                case TokenType::OP_SUB: {
                    next();
                    auto rhs = parseExpression(level);
                    if (auto number = dynamic_cast<IntConstExpr*>(rhs.get())) {
                        auto token2 = number->token;
                        if (!number->merged && token.line == token2.line && token.column + token.width == token2.column) {
                            return make<IntConstExpr>(Token{token.line, token.column, token.width + token2.width, token2.type}, true);
                        }
                    }
                    return make<PrefixExpr>(token, std::move(rhs));
                }
                case TokenType::OP_NOT:
                case TokenType::OP_INV: {
                    next();
                    auto rhs = parseExpression(level);
                    return make<PrefixExpr>(token, std::move(rhs));
                }
                case TokenType::OP_MUL: {
                    next();
                    auto rhs = parseExpression(level);
                    return make<DereferenceExpr>(token, std::move(rhs));
                }
                case TokenType::OP_AND: {
                    next();
                    auto rhs = parseExpression(level);
                    auto segment = rhs->segment();
                    if (auto load = dynamic_pointer_cast<AssignableExpr>(std::move(rhs))) {
                        return make<AddressOfExpr>(token, std::move(load));
                    } else {
                        raise("assignable expression is expected", segment);
                    }
                }
                case TokenType::OP_INC:
                case TokenType::OP_DEC: {
                    next();
                    auto rhs = parseExpression(level);
                    auto segment = rhs->segment();
                    if (auto load = dynamic_pointer_cast<AssignableExpr>(std::move(rhs))) {
                        return make<StatefulPrefixExpr>(token, std::move(load));
                    } else {
                        raise("assignable expression is expected", segment);
                    }
                }
                default: {
                    return parseExpression(Expr::upper(level));
                }
            }
        }
        case Expr::Level::POSTFIX: {
            auto lhs = parseExpression(Expr::upper(level));
            bool flag = true;
            while (flag) {
                switch (peek().type) {
                    case TokenType::LPAREN: {
                        auto token1 = next();
                        auto expr = parseExpressions(TokenType::RPAREN);
                        auto token2 = next();
                        lhs = make<InvokeExpr>(token1, token2, std::move(lhs), std::move(expr));
                        break;
                    }
                    case TokenType::LBRACKET: {
                        auto token1 = next();
                        auto rhs = parseExpression();
                        auto token2 = expect(TokenType::RBRACKET, "]");
                        lhs = make<AccessExpr>(token1, token2, std::move(lhs), std::move(rhs));
                        break;
                    }
                    case TokenType::KW_AS: {
                        auto token = next();
                        auto type = parseType();
                        lhs = make<AsExpr>(token, rewind(), std::move(lhs), std::move(type));
                        break;
                    }
                    case TokenType::OP_INC:
                    case TokenType::OP_DEC: {
                        auto token = next();
                        auto segment = lhs->segment();
                        if (auto load = dynamic_pointer_cast<AssignableExpr>(std::move(lhs))) {
                            lhs = make<StatefulPostfixExpr>(token, std::move(load));
                        } else {
                            raise("assignable expression is expected", segment);
                        }
                        break;
                    }
                    default: flag = false;
                }
            }
            return lhs;
        }
        case Expr::Level::PRIMARY: {
            switch (auto token = peek(); token.type) {
                case TokenType::LPAREN: {
                    next();
                    auto expr = parseExpressions(TokenType::RPAREN);
                    auto token2 = next();
                    switch (expr.size()) {
                        case 0:
                            return make<ClauseExpr>(token, token2);
                        case 1:
                            return std::move(expr.front());
                        default:
                            raise("there is no tuple support in PorkchopLite", range(token, token2));
                    }
                }
                case TokenType::LBRACE:
                    return parseClause();

                case TokenType::IDENTIFIER:
                    return parseId(true);

                case TokenType::KW_FALSE:
                case TokenType::KW_TRUE:
                    return make<BoolConstExpr>(next());
                case TokenType::CHARACTER_LITERAL:
                    return make<CharConstExpr>(next());
                case TokenType::BINARY_INTEGER:
                case TokenType::OCTAL_INTEGER:
                case TokenType::DECIMAL_INTEGER:
                case TokenType::HEXADECIMAL_INTEGER:
                case TokenType::KW_LINE:
                    return make<IntConstExpr>(next());
                case TokenType::FLOATING_POINT:
                case TokenType::KW_NAN:
                case TokenType::KW_INF:
                    return make<FloatConstExpr>(next());

                case TokenType::KW_WHILE:
                    return parseWhile();
                case TokenType::KW_IF:
                    return parseIf();
                case TokenType::KW_LET:
                    return parseLet(false);

                case TokenType::KW_ELSE:
                    Error().with(
                            ErrorMessage().error(next())
                            .text("stray").quote("else")
                            ).raise();

                case TokenType::KW_BREAK:
                case TokenType::KW_RETURN:
                    return parseExpression();

                case TokenType::LINEBREAK:
                    raise("unexpected linebreak", next());
                default:
                    raise("unexpected token", next());
            }
        }
    }
}

std::unique_ptr<ClauseExpr> Parser::parseClause() {
    auto token = expect(TokenType::LBRACE, "{");
    LocalContext::Guard guard(context);
    std::vector<ExprHandle> rhs;
    bool flag = true;
    while (flag) {
        switch (peek().type) {
            default: rhs.emplace_back(parseExpression());
            case TokenType::RBRACE:
            case TokenType::LINEBREAK:
                switch (peek().type) {
                    default: raise("a linebreak is expected between expressions", peek());
                    case TokenType::RBRACE: flag = false;
                    case TokenType::LINEBREAK: next(); continue;
                }
        }
    }
    return make<ClauseExpr>(token, rewind(), std::move(rhs));
}

std::vector<ExprHandle> Parser::parseExpressions(TokenType stop) {
    std::vector<ExprHandle> expr;
    while (true) {
        if (peek().type == stop) break;
        expr.emplace_back(parseExpression());
        if (peek().type == stop) break;
        expectComma();
    }
    optionalComma(expr.size());
    return expr;
}

ExprHandle Parser::parseIf() {
    auto token = next();
    LocalContext::Guard guard(context);
    auto cond = parseExpression();
    auto clause = parseClause();
    if (peek().type == TokenType::KW_ELSE) {
        next();
        auto rhs = peek().type == TokenType::KW_IF ? parseIf() : parseClause();
        return make<IfElseExpr>(token, std::move(cond), std::move(clause), std::move(rhs));
    }
    return make<IfElseExpr>(token, std::move(cond), std::move(clause), make<ClauseExpr>(rewind(), rewind()));
}

ExprHandle Parser::parseWhile() {
    auto token = next();
    pushLoop();
    LocalContext::Guard guard(context);
    auto cond = parseExpression();
    auto clause = parseClause();
    return make<WhileExpr>(token, std::move(cond), std::move(clause), popLoop());
}

std::unique_ptr<ParameterList> Parser::parseParameters() {
    expect(TokenType::LPAREN, "(");
    std::vector<IdExprHandle> identifiers;
    std::vector<TypeReference> P;
    while (true) {
        if (peek().type == TokenType::RPAREN) break;
        auto declarator = parseDeclarator();
        if (declarator->designated == nullptr) {
            raise("missing type for the parameter", declarator->segment);
        }
        identifiers.push_back(std::move(declarator->name));
        P.push_back(std::move(declarator->designated));
        if (peek().type == TokenType::RPAREN) break;
        expectComma();
    }
    optionalComma(identifiers.size());
    next();
    return std::make_unique<ParameterList>(std::move(identifiers), std::make_shared<FuncType>(std::move(P),nullptr));
}

ExprHandle Parser::parseFnBody(std::shared_ptr<FuncType> const& F, Segment decl) {
    ExprHandle clause;
    TypeReference type0;
    {
        clause = parseExpression();
        if (returns.empty()) {
            type0 = clause->getType();
        } else {
            type0 = isNever(clause->getType()) ? returns.front()->rhs->getType() : clause->getType();
            for (auto&& return_ : returns) {
                if (!type0->equals(return_->rhs->getType())) {
                    raiseReturns(clause.get(), ErrorMessage().error(decl).text("multiple returns conflict in type"));
                }
            }
        }
    }
    if (F->R == nullptr) {
        F->R = type0;
    } else if (!F->R->assignableFrom(type0)){
        Error error;
        error.with(ErrorMessage().error(clause->segment()).text("actual return type of the function is not assignable to specified one"));
        error.with(ErrorMessage().note().type(type0).text("is not assignable to").type(F->R));
        error.with(ErrorMessage().note(decl).text("declared here"));
        error.raise();
    }
    return clause;
}

void Parser::parseFile() {
    while (remains()) {
        while (remains() && peek().type == TokenType::LINEBREAK) next();
        if (!remains()) return;
        auto token = peek();
        switch (token.type) {
            case TokenType::KW_FN:
                context.global->fns.push_back(parseFn());
                break;
            case TokenType::KW_LET: {
                context.global->lets.push_back(parseLet(true));
                break;
            }
            default:
                raise("fn or let is expected at top level of a file", token);
        }
    }
}

std::unique_ptr<FunctionDeclarator> Parser::parseFn() {
    auto token = next();
    IdExprHandle name = parseId(false);
    auto parameters = parseParameters();
    parameters->prototype->R = optionalType();
    if (auto type = peek().type; type == TokenType::OP_ASSIGN) {
        auto token2 = next();
        context.global->declare(compiler, name->token, parameters->prototype);
        LocalContext::Guard guard(context);
        parameters->declare(compiler, context);
        auto clause = parseFnBody(parameters->prototype, range(token, token2));
        auto definition = std::make_unique<FunctionDefinition>(std::move(clause), std::move(context.localTypes));
        return std::make_unique<FunctionDeclarator>(std::move(name), std::move(parameters), std::move(definition));
    } else {
        if (parameters->prototype->R == nullptr) {
            raise("return type of declared function is missing", rewind());
        }
        context.global->declare(compiler, name->token, parameters->prototype);
        return std::make_unique<FunctionDeclarator>(std::move(name), std::move(parameters), nullptr);
    }
}

std::unique_ptr<LetExpr> Parser::parseLet(bool global) {
    auto token = next();
    auto declarator = parseDeclarator();
    expect(TokenType::OP_ASSIGN, "=");
    auto initializer = parseExpression();
    declarator->infer(initializer->getType(declarator->typeCache));
    if (global) {
        initializer->requireConst();
        context.global->declare(compiler, declarator->name->token, initializer->getType());
    } else {
        declarator->declare(context);
    }
    return make<LetExpr>(token, std::move(declarator), std::move(initializer));
}

TypeReference Parser::parseType() {
    switch (Token token = next(); token.type) {
        case TokenType::IDENTIFIER: {
            auto id = compiler.of(token);
            if (auto it = SCALAR_TYPES.find(id); it != SCALAR_TYPES.end()) {
                return std::make_shared<ScalarType>(it->second);
            }
            if (id == "typeof") {
                expect(TokenType::LPAREN, "(");
                auto expr = parseExpression();
                expect(TokenType::RPAREN, ")");
                return expr->getType();
            }
        }
        default:
            raise("a type is expected", token);
        case TokenType::OP_MUL: {
            auto E = parseType();
            neverGonnaGiveYouUp(E, "as a list element", rewind());
            return std::make_shared<PointerType>(E);
        }
        case TokenType::LPAREN: {
            std::vector<TypeReference> P;
            while (true) {
                if (peek().type == TokenType::RPAREN) break;
                P.emplace_back(parseType());
                neverGonnaGiveYouUp(P.back(), "as a tuple element or a parameter", rewind());
                if (peek().type == TokenType::RPAREN) break;
                expectComma();
            }
            optionalComma(P.size());
            next();
            if (auto R = optionalType()) {
                return std::make_shared<FuncType>(std::move(P), std::move(R));
            } else {
                switch (P.size()) {
                    case 0:
                        return ScalarTypes::NONE;
                    default:
                        expect(TokenType::OP_COLON, "there is no tuple support in PorkchopLite");
                        unreachable();
                }
            }
        }
    }
}

IdExprHandle Parser::parseId(bool initialize) {
    auto token = next();
    if (token.type != TokenType::IDENTIFIER) raise("id-expression is expected", token);
    auto id = std::make_unique<IdExpr>(compiler, token);
    if (initialize) {
        id->initLookup(context);
    }
    return id;
}

std::unique_ptr<SimpleDeclarator> Parser::parseDeclarator() {
    auto id = parseId(false);
    auto type = optionalType();
    bool underscore = compiler.of(id->token) == "_";
    auto segment = range(id->segment(), rewind());
    if (type == nullptr) {
        if (underscore) type = ScalarTypes::NONE;
    } else {
        neverGonnaGiveYouUp(type, "as a declarator", segment);
        if (underscore && !isNone(type)) {
            Error().with(
                    ErrorMessage().error(segment)
                    .text("the type of").quote("_").text("must be none")
                    ).raise();
        }
    }
    return std::make_unique<SimpleDeclarator>(compiler, segment, std::move(id), std::move(type));
}

}