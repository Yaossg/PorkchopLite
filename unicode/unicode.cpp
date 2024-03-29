#include "unicode.hpp"
#include "../diagnostics.hpp"

#include <charconv>

namespace Porkchop {

char32_t parseHex(const char *first, const char *last, unsigned int bound, Segment segment) {
    unsigned value = 0;
    auto [ptr, ec] = std::from_chars(first, last, value, 16);
    if (ptr != last) {
        Error().with(
                ErrorMessage().error(segment)
                .text("escape sequence expect exact").num(std::distance(first, last)).text("hexadecimal digits")
                ).raise();
    }
    if (value > bound) {
        ec = std::errc::result_out_of_range;
    }
    if (ec == std::errc{}) {
        if (isSurrogate(value)) {
            raise("the hexadecimal value represents a surrogate", segment);
        } else {
            return value;
        }
    }
    switch (ec) {
        case std::errc::invalid_argument:
            raise("the hexadecimal value is invalid", segment);
        case std::errc::result_out_of_range:
            raise("the hexadecimal value is out of valid range", segment);
        default:
            unreachable();
    }
}

std::string encodeUnicode(char32_t unicode) {
    if (unicode <= ASCII_UPPER_BOUND) { // ASCII
        return {char(unicode)};
    } else if (unicode <= 0x7FF) { // 2 bytes
        return {char((unicode >> 6) | 0xC0),
                char((unicode & 0x3F) | 0x80)};
    } else if (unicode <= 0xFFFF) { // 3 bytes
        return {char((unicode >> 12) | 0xE0),
                char(((unicode >> 6) & 0x3F) | 0x80),
                char((unicode & 0x3F) | 0x80)};
    } else if (unicode <= UNICODE_UPPER_BOUND) { // 4 bytes
        return {char((unicode >> 18) | 0xF0),
                char(((unicode >> 12) & 0x3F) | 0x80),
                char(((unicode >> 6) & 0x3F) | 0x80),
                char((unicode & 0x3F) | 0x80)};
    }
    throw std::out_of_range("invalid unicode beyond 0x10FFFF");
}

int UnicodeParser::successiveUTF8Length(char8_t byte) const {
    switch(int ones = countl_one(byte)) {
        case 0:
            return 1;
        case 2:
        case 3:
        case 4:
            return ones;
        case 1:
        default:
            raise("invalid UTF-8 multibyte sequence");
    }
}

char UnicodeParser::getUTF8Continue() {
    char ch = getc();
    if (notUTF8Continue(ch))
        raise("invalid UTF-8 multibyte sequence");
    return ch;
}

char UnicodeParser::parseHexASCII() {
    const char *first = q;
    getc(); getc();
    const char *last = q;
    return (char) parseHex(first, last, ASCII_UPPER_BOUND, make());
}

char32_t UnicodeParser::parseHexUnicode() {
    const char *first = q;
    getc(); getc(); getc(); getc(); getc(); getc();
    const char *last = q;
    return parseHex(first, last, UNICODE_UPPER_BOUND, make());
}

char32_t UnicodeParser::decodeUnicode() {
    char32_t result;
    char8_t ch1 = getc();
    switch (successiveUTF8Length(ch1)) {
        case 1:
            result = ch1;
            break;
        case 2: {
            char8_t ch2 = getUTF8Continue();
            result = ((ch1 & ~0xC0) << 6) | (ch2 & ~0x80);
        } break;
        case 3: {
            char8_t ch2 = getUTF8Continue();
            char8_t ch3 = getUTF8Continue();
            result = ((ch1 & ~0xE0) << 12) | ((ch2 & ~0x80) << 6) | (ch3 & ~0x80);
        } break;
        case 4: {
            char8_t ch2 = getUTF8Continue();
            char8_t ch3 = getUTF8Continue();
            char8_t ch4 = getUTF8Continue();
            result = ((ch1 & ~0xF0) << 18) | ((ch2 & ~0x80) << 12) | ((ch3 & ~0x80) << 6) | (ch4 & ~0x80);
        } break;
    }
    if (isSurrogate(result))
        raise("the value of UTF-8 series represents a surrogate");
    if (result > UNICODE_UPPER_BOUND)
        raise("the value of UTF-8 series exceeds upper bound of Unicode");
    return result;
}

char32_t UnicodeParser::unquoteChar(Token token) {
    getc(); step();
    char32_t result;
    char8_t ch1 = peekc();
    if (ch1 == '\\') {
        getc();
        switch (getc()) {
            case '\'': result = '\''; break;
            case '\"': result = '\"'; break;
            case '\\': result = '\\'; break;
            case '$': result = '$'; break;
            case '0': result = '\0'; break;
            case 'a': result = '\a'; break;
            case 'b': result = '\b'; break;
            case 'f': result = '\f'; break;
            case 'n': result = '\n'; break;
            case 'r': result = '\r'; break;
            case 't': result = '\t'; break;
            case 'v': result = '\v'; break;
            case 'x': { // ASCII hex
                result = parseHexASCII();
                break;
            }
            case 'u': { // Unicode hex
                result = parseHexUnicode();
                break;
            }
            default:
                raise("unknown escape sequence");
        }
    } else if (ch1 == '\'') {
        Porkchop::raise("empty character literal", token);
    } else {
        result = decodeUnicode();
    }
    if (getc() != '\'') Porkchop::raise("multiple characters in the character literal", token);
    return result;
}

std::string UnicodeParser::unquoteString(bool escape) {
    std::string result;
    while (remains()) {
        switch (char ch1 = getc(); successiveUTF8Length(ch1)) {
            case 1:
                if (ch1 == '\\' && escape) {
                    switch (getc()) {
                        case '\'': result += '\''; break;
                        case '\"': result += '\"'; break;
                        case '\\': result += '\\'; break;
                        case '$': result += '$'; break;
                        case '0': result += '\0'; break;
                        case 'a': result += '\a'; break;
                        case 'b': result += '\b'; break;
                        case 'f': result += '\f'; break;
                        case 'n': result += '\n'; break;
                        case 'r': result += '\r'; break;
                        case 't': result += '\t'; break;
                        case 'v': result += '\v'; break;
                        case 'x': { // ASCII hex
                            result += parseHexASCII();
                            break;
                        }
                        case 'u': { // Unicode hex
                            result += encodeUnicode(parseHexUnicode());
                            break;
                        }
                        default:
                            raise("unknown escape sequence");
                    }
                } else result += ch1;
                break;
            case 2: {
                result += ch1;
                result += getUTF8Continue();
            } break;
            case 3: {
                result += ch1;
                result += getUTF8Continue();
                result += getUTF8Continue();
            } break;
            case 4: {
                result += ch1;
                result += getUTF8Continue();
                result += getUTF8Continue();
                result += getUTF8Continue();
            } break;
        }
        step();
    }
    return result;
}

void UnicodeParser::raise(const char *msg) const {
    Porkchop::raise(msg, make());
}

}