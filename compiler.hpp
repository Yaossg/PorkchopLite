#pragma once

#include <memory>

#include "source.hpp"

namespace Porkchop {

struct Token;
struct Assembler;
struct FunctionDefinition;
struct GlobalScope;
struct LetExpr;
struct FunctionDeclarator;

struct Compiler: Descriptor {
    GlobalScope* global;
    Source source;


    explicit Compiler(GlobalScope* global, Source source);

    [[nodiscard]] std::string_view of(Token token) const noexcept;

    void parse();

    void compile(Assembler* assembler) const;
    void compileLet(LetExpr* let, Assembler* assembler) const;
    void compileFn(FunctionDeclarator* fn, Assembler* assembler) const;

    [[nodiscard]] std::string_view descriptor() const noexcept override { return "ROOT"; }
    [[nodiscard]] std::vector<const Descriptor *> children() const override;

};

}