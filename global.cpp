#include "local.hpp"
#include "global.hpp"
#include "diagnostics.hpp"
#include "tree.hpp"


namespace Porkchop {

GlobalScope::GlobalScope() = default; // TODO declare builtin global functions here

void GlobalScope::declare(Compiler& compiler, Token token, const TypeReference& type) {
    std::string key(compiler.of(token));
    if (global.contains(key)) {
        raise("global variables or functions are not allowed with duplicated name", token);
    }
    global.emplace(key, type);
}

LocalContext::LookupResult GlobalScope::lookup(const std::string& name, Token token) {
    if (auto lookup = global.find(name); lookup != global.end()) {
        return {lookup->second, (size_t)-1, LocalContext::LookupResult::Scope::GLOBAL};
    }
    raise("unable to resolve this identifier", token);
}


}