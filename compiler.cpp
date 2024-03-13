#include "parser.hpp"
#include "compiler.hpp"
#include "assembler.hpp"
#include "global.hpp"

namespace Porkchop {

Compiler::Compiler(GlobalScope* global, Source source)
        : global(global), source(std::move(source)) {}

std::string_view Compiler::of(Token token) const noexcept {
    return source.of(token);
}

void Compiler::parse() {
    if (source.tokens.empty())
        Error().with(ErrorMessage().fatal().text("no token to compile")).raise();
    if (!source.greedy.empty()) {
        Error error;
        error.with(ErrorMessage().fatal().text("greedy tokens mismatch, source seems incomplete"));
        for (auto&& token : source.greedy) {
            const char* msg;
            switch (token.type) {
                case TokenType::LPAREN: msg = ")"; break;
                case TokenType::LBRACKET: msg = "]"; break;
                case TokenType::LBRACE: msg = "}"; break;
                default: unreachable();
            }
            error.with(ErrorMessage().note(token).quote(msg).text("is expected to match this"));
        }
        error.raise();
    }
    LocalContext context(global);
    Parser parser(*this, source.tokens.begin(), source.tokens.end(), context);
    parser.parseFile();
}

void Compiler::compileLet(LetExpr *let, Assembler *assembler) const {
    auto initial = let->initializer->requireConst();
    auto type = let->initializer->getType();
    std::string_view name(of(let->declarator->name->token));
    if (isInt(type)) {
        assembler->global(name, initial.$int);
    } else {
        assembler->global(name, initial.$float);
    }
}

void Compiler::compileFn(FunctionDeclarator* fn, Assembler* assembler) const {
    auto definition = fn->definition.get();
    std::string declare(definition ? "define" : "declare");
    declare +=" ";
    declare += fn->parameters->prototype->R->serialize();
    declare += " @";
    declare += of(fn->name->token);
    declare += "(";
    size_t index = 0;
    for (auto&& param : fn->parameters->prototype->P) {
        if (index > 0) {
            declare += ", ";
        }
        declare += param->serialize();
        declare += " %";
        declare += std::to_string(index++);
    }
    declare += ")";
    if (definition) {
        declare += " {";
    }
    assembler->append(declare);
    if (definition) {
        assembler->reg = index;
        assembler->indent += 4;
        global->labelUntil = 0;
        assembler->label(global->labelUntil++);
        size_t param = 0;
        for (auto&& local : definition->locals) {
            auto reg = assembler->local(local);
            if (param < index) {
                assembler->store(Assembler::regOf(param), reg, fn->parameters->prototype->P[param]);
                ++param;
            }
        }
        definition->clause->walkBytecode(assembler);
        if (!isNever(definition->clause->getType())) {
            assembler->return_(definition->clause->reg, fn->parameters->prototype->R);
        }
        assembler->indent -= 4;
        assembler->append("}");
    }
}

void Compiler::compile(Assembler* assembler) const {
    for (auto&& let : global->lets) {
        compileLet(let.get(), assembler);
    }
    for (auto&& fn : global->fns) {
        compileFn(fn.get(), assembler);
    }
}

std::vector<const Descriptor *> Compiler::children() const {
    std::vector<const Descriptor *> children;
    for (auto&& let : global->lets) {
        children.push_back(let.get());
    }
    for (auto&& fn : global->fns) {
        children.push_back(fn.get());
    }
    return children;
}

}