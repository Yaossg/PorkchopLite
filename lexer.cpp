#include "lexer.hpp"
#include "diagnostics.hpp"
#include "unicode/unicode.hpp"

namespace Porkchop {

[[nodiscard]] constexpr bool isBinary(char ch) noexcept {
    return ch == '0' || ch == '1';
}

[[nodiscard]] constexpr bool isOctal(char ch) noexcept {
    return ch >= '0' && ch <= '7';
}

[[nodiscard]] constexpr bool isDecimal(char ch) noexcept {
    return ch >= '0' && ch <= '9';
}

[[nodiscard]] constexpr bool isHexadecimal(char ch) noexcept {
    return ch >= '0' && ch <= '9' || ch >= 'a' && ch <= 'f' || ch >= 'A' && ch <= 'F';
}

[[nodiscard]] constexpr bool isNumberStart(char ch) noexcept {
    return isDecimal(ch);
}

// not a punctuation: _ #
// unused: ` ? @ $
[[nodiscard]] constexpr bool isPunctuation(char ch) {
    return ch == '!' || ch == '"' || '$' < ch && ch <= '/' || ':' <= ch && ch <= '>' || '[' <= ch && ch <= '^' || '{' <= ch && ch <= '~';
}

[[nodiscard]] bool isIdentifierStart(char32_t ch) {
    return ch == '_' || isUnicodeIdentifierStart(ch);
}

[[nodiscard]] bool isIdentifierPart(char32_t ch) {
    return isUnicodeIdentifierPart(ch); // contains '_' already
}

void LineTokenizer::addLBrace() {
    add(TokenType::LBRACE);
}

void LineTokenizer::addRBrace() {
    add(TokenType::RBRACE);
}

void LineTokenizer::raise(const char *msg) const {
    Porkchop::raise(msg, make(TokenType::INVALID));
}

void LineTokenizer::tokenize() {
    while (remains()) {
        switch (char ch = getc()) {
            case '\\':
                if (backslash) raise("multiple backslash in one line");
                backslash = true;
                break;
            case '#':
                q = r;
                break;
            case '\n':
            case '\r':
            case '\t':
            case ' ':
                break;
            case *"'":
                addChar();
                break;
            case '"':
                addString();
                break;
            case '{':
                addLBrace();
                break;
            case '}':
                addRBrace();
                break;
            case ';':
                addLinebreak(true);
                break;
            default: {
                ungetc(ch);
                if (isNumberStart(ch)) {
                    addNumber();
                } else if (isPunctuation(ch)) {
                    addPunct();
                } else {
                    addId();
                }
            }
        }
        step();
    }
    if (!backslash)
        addLinebreak(false);
}

void LineTokenizer::addLinebreak(bool semicolon) {
    if (context.tokens.empty() || context.tokens.back().type == TokenType::LINEBREAK) {
        return;
    }
    if (!context.greedy.empty() && context.greedy.back().type != TokenType::LBRACE) {
        if (semicolon) raise("semicolon is not allowed here");
        return;
    }
    add(TokenType::LINEBREAK);
}

void LineTokenizer::addId() {
    std::string_view remains{p, r};
    UnicodeParser up(remains, line, column());
    if (!isIdentifierStart(up.decodeUnicode())) {
        raise("unexpected character");
    }
    do q = up.q;
    while (up.remains() && isIdentifierPart(up.decodeUnicode()));
    std::string_view token{p, q};
    if (auto it = KEYWORDS.find(token); it != KEYWORDS.end()) {
        add(it->second);
    } else {
        add(TokenType::IDENTIFIER);
    }
}

void LineTokenizer::addPunct() {
    std::string_view remains{p, r};
    auto punct = PUNCTUATIONS.end();
    for (auto it = PUNCTUATIONS.begin(); it != PUNCTUATIONS.end(); ++it) {
        if (remains.starts_with(it->first)
            && (punct == PUNCTUATIONS.end() || it->first.length() > punct->first.length())) {
            punct = it;
        }
    }
    if (punct != PUNCTUATIONS.end()) {
        q += punct->first.length();
        add(punct->second);
    } else {
        raise("invalid punctuation");
    }
}

void LineTokenizer::scanDigits(bool (*pred)(char) noexcept) {
    char ch = getc();
    if (!pred(ch)) raise("invalid number literal");
    do ch = getc();
    while (ch == '_' || pred(ch));
    ungetc(ch);
    if (q[-1] == '_') raise("invalid number literal");
}

void LineTokenizer::addNumber() {
    // scan number prefix
    TokenType base = TokenType::DECIMAL_INTEGER;
    bool (*pred)(char) noexcept = isDecimal;
    if (peekc() == '0') {
        getc();
        switch (char ch = peekc()) {
            case 'x': case 'X':
                base = TokenType::HEXADECIMAL_INTEGER; pred = isHexadecimal; getc(); break;
            case 'o': case 'O':
                base = TokenType::OCTAL_INTEGER; pred = isOctal; getc(); break;
            case 'b': case 'B':
                base = TokenType::BINARY_INTEGER; pred = isBinary; getc(); break;
            default:
                if (isNumberStart(ch)) {
                    raise("redundant 0 ahead is forbidden to avoid ambiguity, use 0o if octal");
                } else {
                    ungetc('0');
                }
        }
    }
    // scan digits
    scanDigits(pred);
    bool flt = false;
    if (peekc() == '.') {
        getc();
        if (pred(peekc())) {
            scanDigits(pred);
            flt = true;
        } else {
            ungetc('.');
        }
    }
    if (base == TokenType::DECIMAL_INTEGER && (peekc() == 'e' || peekc() == 'E')
        || base == TokenType::HEXADECIMAL_INTEGER && (peekc() == 'p' || peekc() == 'P')) {
        flt = true;
        getc();
        if (peekc() == '+' || peekc() == '-') getc();
        scanDigits(isDecimal);
    }
    // classification
    TokenType type = base;
    if (flt) {
        switch (base) {
            case TokenType::BINARY_INTEGER:
            case TokenType::OCTAL_INTEGER:
                raise("binary or octal float literal is invalid");
            case TokenType::DECIMAL_INTEGER:
            case TokenType::HEXADECIMAL_INTEGER:
                type = TokenType::FLOATING_POINT;
                break;
        }
    }
    add(type);
}

void LineTokenizer::addChar() {
    while (char ch = getc()) {
        switch (ch) {
            case *"'":
                add(TokenType::CHARACTER_LITERAL);
                return;
            case '\\':
                getc();
                break;
        }
    }
    raise("unterminated character literal");
}

void LineTokenizer::addString() {
    while (char ch = getc()) {
        switch (ch) {
            case '"':
                add(TokenType::STRING_LITERAL);
                return;
            case '\\':
                getc();
                break;
        }
    }
    raise("unterminated string literal");
}

void LineTokenizer::add(TokenType type) {
    if (backslash) raise("no token is allowed after backslash in one line");
    context.tokens.push_back(make(type));
    step();
    switch (type) {
        case TokenType::LPAREN:
        case TokenType::LBRACKET:
        case TokenType::LBRACE:
            context.greedy.push_back(context.tokens.back());
            break;
        case TokenType::RPAREN:
            checkGreedy("(", ")", TokenType::LPAREN);
            break;
        case TokenType::RBRACKET:
            checkGreedy("[", "]", TokenType::LBRACKET);
            break;
        case TokenType::RBRACE:
            checkGreedy("{", "}", TokenType::LBRACE);
            break;
    }
}

void LineTokenizer::checkGreedy(const char* left, const char* right, TokenType match) {
    if (context.greedy.empty()) {
        Error().with(
                ErrorMessage().error(context.tokens.back())
                .text("stray").quote(right).text("without").quote(left).text("to match")
                ).raise();
    }
    if (context.greedy.back().type != match) {
        Error error;
        error.with(ErrorMessage().error(context.tokens.back()).quote(right).text("mismatch"));
        error.with(ErrorMessage().note(context.greedy.back()).quote(left).text("expected here"));
        for (auto it = context.greedy.rbegin(); it != context.greedy.rend(); ++it) {
            if (it->type == match) {
                error.with(ErrorMessage().note(*it).text("nearest matching").quote(left).text("is here"));
                break;
            }
        }
        if (error.messages.size() < 3) {
            error.with(ErrorMessage().note().text("stray").quote(right).text("without").quote(left).text("to match"));
        }
        error.raise();
    }
    context.greedy.pop_back();
}

int64_t parseInt(Source& source, Token token) try {
    int base;
    switch (token.type) {
        case TokenType::BINARY_INTEGER: base = 2; break;
        case TokenType::OCTAL_INTEGER: base = 8; break;
        case TokenType::DECIMAL_INTEGER: base = 10; break;
        case TokenType::HEXADECIMAL_INTEGER: base = 16; break;
        default: unreachable();
    }
    std::string literal(source.of(token));
    std::erase(literal, '_');
    if (base != 10) {
        literal.erase(literal.front() == '+' || literal.front() == '-', 2);
    }
    return std::stoll(literal, nullptr, base);
} catch (std::out_of_range& e) {
    raise("int literal out of range", token);
}

double parseFloat(Source& source, Token token) try {
    std::string literal(source.of(token));
    std::erase(literal, '_');
    return std::stod(literal);
} catch (std::out_of_range& e) {
    raise("float literal out of range", token);
}

char32_t parseChar(Source& source, Token token) {
    return UnicodeParser(source.of(token), token).unquoteChar(token);
}

std::string parseString(Source& source, Token token) {
    size_t prefix = 1, suffix = 1; bool escape = true;
    auto view = source.of(token);
    view.remove_prefix(prefix);
    view.remove_suffix(suffix);
    auto parsed = UnicodeParser(view, token).unquoteString(escape);
    return parsed;
}

}