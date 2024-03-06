#pragma once

#include <string>
#include <vector>
#include <deque>

#include "token.hpp"

namespace Porkchop {

struct Token;

struct Source {
    std::vector<std::string> lines;
    std::vector<Token> tokens;
    std::vector<Token> greedy;

    [[nodiscard]] std::string_view of(Token token) const noexcept;
    void append(std::string const& code);
    bool remains();
};

}