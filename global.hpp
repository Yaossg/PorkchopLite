#pragma once

#include <vector>
#include <memory>

#include "local.hpp"

namespace Porkchop {

struct Assembler;
struct FunctionDeclarator;
struct LetExpr;


struct GlobalScope {
    std::vector<std::unique_ptr<FunctionDeclarator>> fns;
    std::vector<std::unique_ptr<LetExpr>> lets;
    std::unordered_map<std::string, TypeReference> global;

    size_t labelUntil = 0;

    GlobalScope();

    void declare(Compiler& compiler, Token token, const TypeReference& type);
    LocalContext::LookupResult lookup(const std::string& name, Token token);
};


}
