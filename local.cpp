#include "local.hpp"
#include "diagnostics.hpp"
#include "tree.hpp"
#include "global.hpp"

namespace Porkchop {

LocalContext::LocalContext(GlobalScope* global): global(global) {}

void LocalContext::push() {
    localIndices.emplace_back();
}

void LocalContext::pop() {
    localIndices.pop_back();
}

void LocalContext::local(std::string_view name, const TypeReference& type) {
    if (name == "_") return;
    localIndices.back().insert_or_assign(std::string(name), localTypes.size() + offset);
    localTypes.push_back(type);
}

LocalContext::LookupResult LocalContext::lookup(Compiler& compiler, Token token) const {
    std::string name(compiler.of(token));
    if (name == "_") return {ScalarTypes::NONE, 0, LookupResult::Scope::NONE};
    for (auto it = localIndices.rbegin(); it != localIndices.rend(); ++it) {
        if (auto lookup = it->find(name); lookup != it->end()) {
            size_t index = lookup->second;
            return {localTypes[index - offset], index, LookupResult::Scope::LOCAL};
        }
    }
    return global->lookup(name, token);
}


}