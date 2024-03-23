#pragma once

#include <vector>
#include <memory>
#include <filesystem>

namespace fs = std::filesystem;

#include "local.hpp"

namespace Porkchop {

struct Assembler;
struct FunctionDeclarator;
struct LetExpr;


struct GlobalScope {
    std::vector<std::unique_ptr<FunctionDeclarator>> fns;
    std::vector<std::unique_ptr<LetExpr>> lets;

    std::unordered_map<std::string, TypeReference> global;
    std::unordered_map<std::string, std::shared_ptr<FuncType>> imports;
    std::unordered_map<std::string, std::shared_ptr<FuncType>> exports;

    size_t labelUntil = 0;
    fs::path path;

    explicit GlobalScope(fs::path path);

    void declare(Compiler& compiler, Token token, const TypeReference& type);
    LocalContext::LookupResult lookup(const std::string& name, Token token);

    void import_(std::string const& filename, bool exported, Compiler& compiler, Token token);
};


}
