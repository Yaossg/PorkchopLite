#pragma once

#include <cstdio>
#include <cstring>
#include <string>
#include <memory>
#include <vector>

namespace Porkchop {

FILE* open(const char* filename, const char* mode);

inline std::string readText(const char* filename) {
    FILE* input_file = open(filename, "r");
    fseek(input_file, 0, SEEK_END);
    size_t size = ftell(input_file);
    fseek(input_file, 0, SEEK_SET);
    std::string fileBuffer(size, '\0');
    fread(fileBuffer.data(), 1, size, input_file);
    fclose(input_file);
    return fileBuffer;
}

inline void forceUTF8() {
#ifdef _WIN32
    system("chcp>nul 65001");
#endif
}

inline std::vector<std::string_view> splitLines(std::string_view view) {
    std::vector<std::string_view> lines;
    const char *p = view.begin(), *q = p;
    while (q != view.end()) {
        if (*q == '\n' || *q == '\r') {
            lines.emplace_back(p, q);
            if (q[0] == '\r' && q[1] == '\n') {
                ++q;
            }
            p = ++q;
        } else {
            ++q;
        }
    }
    lines.emplace_back(p, q);
    return lines;
}

[[noreturn]] inline void unreachable() {
    __builtin_unreachable();
}

template<typename... Args>
inline std::string join(Args&&... args) {
    std::string result;
    ((result += args), ...);
    return result;
}

template<typename Derived, typename Base>
inline std::unique_ptr<Derived> dynamic_pointer_cast(std::unique_ptr<Base>&& base) noexcept {
    if (auto derived = dynamic_cast<Derived*>(base.get())) {
        std::ignore = base.release();
        return std::unique_ptr<Derived>(derived);
    }
    return nullptr;
}

inline void replaceAll(std::string& inout, std::string_view what, std::string_view with) {
    for (size_t pos = 0; std::string::npos != (pos = inout.find(what.data(), pos, what.length())); pos += with.length()) {
        inout.replace(pos, what.length(), with.data(), with.length());
    }
}

}