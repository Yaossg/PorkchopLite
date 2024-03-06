#pragma once

#include "type.hpp"
#include "compiler.hpp"

namespace Porkchop {

struct LocalContext {
    std::vector<std::unordered_map<std::string, size_t>> localIndices{{}};
    std::vector<TypeReference> localTypes;

    GlobalScope* global;

    explicit LocalContext(GlobalScope* global);

    size_t offset = 0;

    void push();
    void pop();
    void local(std::string_view name, TypeReference const& type);

    struct LookupResult {
        TypeReference type;
        size_t index;
        enum class Scope {
            NONE, LOCAL, GLOBAL
        } scope;
    };

    [[nodiscard]] LookupResult lookup(Compiler& compiler, Token token) const;

    struct Guard {
        LocalContext& context;
        explicit Guard(LocalContext& context): context(context) {
            context.push();
        }
        ~Guard() noexcept(false) {
            context.pop();
        }
    };
};

}
