#pragma once

#include "lexer.hpp"
#include "parser.hpp"

namespace Porkchop {

inline void tokenize(Source& source, std::string const& original) try {
    source.append(original);
} catch (Porkchop::Error& e) {
    e.report(&source, true);
    std::exit(-3);
}

inline void parse(Compiler& compiler) try {
    compiler.parse();
} catch (Porkchop::Error& e) {
    e.report(&compiler.source, true);
    std::exit(-1);
}

}