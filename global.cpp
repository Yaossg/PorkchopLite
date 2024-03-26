#include "local.hpp"
#include "global.hpp"
#include "diagnostics.hpp"
#include "common.hpp"
#include <unordered_set>

namespace Porkchop {

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

GlobalScope::GlobalScope(fs::path path) : path(std::move(path)) {}


FILE* open(const char *filename, const char *mode, Compiler& compiler, Token token) {
    FILE* file = fopen(filename, mode);
    if (file == nullptr) {
        Error error;
        error.with(ErrorMessage().error(token).text("failed to open input file: ").text(filename));
        error.report(&compiler.source);
        std::exit(20);
    }
    return file;
}

std::unordered_set<fs::path> pending;
std::unordered_map<fs::path, std::unordered_map<std::string, std::shared_ptr<FuncType>>> cache;

void GlobalScope::import_(std::string const& filename, bool exported, Compiler& parent, Token token) {
    auto file = path.parent_path() / filename;
    if (!cache.contains(file)) {
        if (pending.contains(file)) {
            raise("recursive import detected", token);
        }
        pending.insert(file);
        auto original = readText(open(file.c_str(), "r", parent, token));
        Source source;
        tokenize(source, original);
        GlobalScope child(path);
        Compiler compiler(&child, std::move(source));
        parse(compiler);
        cache.emplace(file, std::move(child.exports));
        pending.erase(file);
    }
    for (auto&& [key, prototype] : cache[file]) {
        global.emplace(key, prototype);
        imports.emplace(key, prototype);
        if (exported) {
            exports.emplace(key, prototype);
        }
    }
}


}