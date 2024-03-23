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
    std::string value;
    if (isBool(type)) {
        value = assembler->const_(initial.$bool);
    } else if (isInt(type)) {
        value = assembler->const_(initial.$int);
    } else if (isFloat(type)) {
        value = assembler->const_(initial.$float);
    } else {
        raise("PorkchopLite does not support let of none type", let->segment());
    }
    assembler->global(Assembler::escape(name), type, value, let->token.line);
}

std::string fnHeader(bool definition, std::string_view name, std::shared_ptr<FuncType> const& prototype) {
    std::string declare(definition ? "define" : "declare");
    declare += " ";
    declare += prototype->R->serialize();
    declare += " ";
    std::string identifier = Assembler::escape(name);
    declare += identifier;
    declare += "(";
    size_t index = 0;
    for (auto&& param : prototype->P) {
        if (index > 0) {
            declare += ", ";
        }
        declare += param->serialize();
        declare += " ";
        declare += Assembler::regOf(index++);
    }
    declare += ")";
    return declare;
}

void Compiler::compileFn(FunctionDeclarator* fn, Assembler* assembler) const {
    auto definition = fn->definition.get();
    std::string declare = Porkchop::fnHeader(definition, of(fn->name->token), fn->parameters->prototype);
    size_t index = fn->parameters->prototype->P.size();
    if (definition) {
        if (Assembler::debug_flag) {
            std::string identifier = Assembler::escape(of(fn->name->token));
            char buf[256];
            auto line = fn->definition->clause->segment().line1 + 1;
            sprintf(buf, "distinct !DISubprogram(name: %s, scope: %s, file: %s, line: %zu, type: %s, scopeLine: %zu, "
                         "flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: %s, retainedNodes: !{})",
                    Assembler::quote(identifier).data(), Assembler::DEBUG::FILE, Assembler::DEBUG::FILE, line,
                    assembler->prototypeOf(fn->parameters->prototype).data(), line, Assembler::DEBUG::UNIT);
            auto sp = assembler->debug(buf);
            assembler->scope = sp;
            declare += " !dbg ";
            declare += sp;
        }
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
            auto reg = assembler->alloca_(local);
            if (param < index) {
                auto type = fn->parameters->prototype->P[param];
                auto token = fn->parameters->identifiers[param]->token;
                assembler->store(Assembler::regOf(param), reg, type, token);
                assembler->local(*this, token, reg, type, param + 1);
                ++param;
            }
        }
        definition->clause->walkBytecode(assembler);
        if (!isNever(definition->clause->getType())) {
            auto segment =  fn->definition->clause->segment();
            Token token2{.line = segment.line2, .column = segment.column2 - 1};
            assembler->return_(definition->clause->reg, fn->parameters->prototype->R, token2);
        }
        assembler->indent -= 4;
        assembler->append("}");
    }
}

void Compiler::compile(Assembler* assembler) const {
    for (auto&& let : global->lets) {
        compileLet(let.get(), assembler);
    }
    for (auto&& [key, prototype] : global->imports) {
        assembler->append(Porkchop::fnHeader(false, key, prototype));
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