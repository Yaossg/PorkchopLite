#pragma once

#include <algorithm>

namespace Porkchop {

struct Assembler {
    std::vector<std::string> table; // string constant
    std::vector<std::string> assemblies;

    size_t reg;

    [[nodiscard]] size_t alloca_(const TypeReference& type) {
        char buf[64];
        size_t index = reg++;
        auto name = type->serialize();
        sprintf(buf, "%%%zu = alloca %s, align 8", index, name.data());
        append(buf);
        return index;
    }

    [[nodiscard]] size_t const_(bool b) {
        char buf[64];
        size_t index = reg++;
        sprintf(buf, "%%%zu = alloca i1, align 8", index);
        append(buf);
        sprintf(buf, "store i1 %d, i1* %%%zu, align 8", b, index);
        append(buf);
        return load(index, ScalarTypes::BOOL);
    }

    [[nodiscard]] size_t const_(int64_t i) {
        char buf[64];
        size_t index = reg++;
        sprintf(buf, "%%%zu = alloca i64, align 8", index);
        append(buf);
        sprintf(buf, "store i64 %ld, i64* %%%zu, align 8", i, index);
        append(buf);
        return load(index, ScalarTypes::INT);
    }

    [[nodiscard]] size_t const_(double d) {
        char buf[64];
        size_t index = reg++;
        sprintf(buf, "%%%zu = alloca double, align 8", index);
        append(buf);
        sprintf(buf, "store double %e, double* %%%zu, align 8", d, index);
        append(buf);
        return load(index, ScalarTypes::FLOAT);
    }

    struct Identifier {
        std::string name;

        Identifier(std::string_view source) {
            bool simple = true;
            for (char ch : source) {
                if (ch != '_' && !isalnum(ch)) {
                    simple = false;
                    break;
                }
            }
            if (simple) {
                name.assign(source);
            } else {
                name += '"';
                for (char ch : source) {
                    name += '\\';
                    char buf[3];
                    sprintf(buf, "%hhX", ch);
                    name += buf;
                }
                name += '"';
            }
        }

        const char* data() const {
            return name.c_str();
        }
    };

    void global(Identifier identifier, int64_t i) {
        char buf[64];
        sprintf(buf, "@%s = global i64 %ld, align 8", identifier.data(), i);
        append(buf);
    }

    void global(Identifier identifier, double d) {
        char buf[64];
        sprintf(buf, "@%s = global double %e, align 8", identifier.data(), d);
        append(buf);
    }

    [[nodiscard]] size_t loadglobal(Identifier identifier, const TypeReference& type) {
        char buf[64];
        auto tn = type->serialize();
        if (dynamic_cast<FuncType*>(type.get())) {
            auto index = alloca_(type);
            sprintf(buf, "store ptr @%s, ptr %%%zu, align 8", identifier.data(), index);
            append(buf);
            return loadptr(index);
        } else {
            size_t index = reg++;
            sprintf(buf, "%%%zu = load %s, ptr @%s, align 8", index, tn.data(), identifier.data());
            append(buf);
            return index;
        }
    }

    void storeglobal(size_t from, Identifier identifier, const TypeReference& type) {
        char buf[64];
        auto tn = type->serialize();
        sprintf(buf, "store %s %%%zu, %s* @%s, align 8", tn.data(), from, tn.data(), identifier.data());
        append(buf);
    }

    [[nodiscard]] size_t local(const TypeReference& type) {
        char buf[64];
        auto name = type->serialize();
        size_t index = reg++;
        sprintf(buf, "%%%zu = alloca %s, align 8", index, name.data());
        append(buf);
        return index;
    }

    [[nodiscard]] size_t f2i(size_t from) {
        char buf[64];
        size_t index = reg++;
        sprintf(buf, "%%%zu = fptosi double %%%zu to i64", index, from);
        append(buf);
        return index;
    }

    [[nodiscard]] size_t i2f(size_t from) {
        char buf[64];
        size_t index = reg++;
        sprintf(buf, "%%%zu = sitofp i64 %%%zu to double", index, from);
        append(buf);
        return index;
    }

    [[nodiscard]] size_t loadptr(size_t from) {
        char buf[64];
        auto name = "ptr";
        size_t index = reg++;
        sprintf(buf, "%%%zu = load %s, %s %%%zu, align 8", index, name, name, from);
        append(buf);
        return index;
    }

    void storeptr(size_t from, size_t into) {
        char buf[64];
        auto name = "ptr";
        sprintf(buf, "store %s %%%zu, %s %%%zu, align 8", name, from, name, into);
        append(buf);
    }

    [[nodiscard]] size_t addressof(size_t from, const TypeReference& type) {
        size_t index = alloca_(type);
        storeptr(from, index);
        return index;
    }

    [[nodiscard]] size_t load(size_t from, const TypeReference& type) {
        if (dynamic_cast<FuncType*>(type.get())) {
            return loadptr(from);
        }
        char buf[64];
        auto name = type->serialize();
        size_t index = reg++;
        sprintf(buf, "%%%zu = load %s, %s* %%%zu, align 8", index, name.data(), name.data(), from);
        append(buf);
        return index;
    }

    void store(size_t from, size_t into, const TypeReference& type) {
        if (dynamic_cast<FuncType*>(type.get())) {
            return storeptr(from, into);
        }
        char buf[64];
        auto name = type->serialize();
        sprintf(buf, "store %s %%%zu, %s* %%%zu, align 8", name.data(), from, name.data(), into);
        append(buf);
    }

    [[nodiscard]] size_t infix(const char* op, size_t lhs, size_t rhs, const TypeReference& type) {
        char buf[64];
        auto name = type->serialize();
        size_t index = reg++;
        sprintf(buf, "%%%zu = %s %s %%%zu, %%%zu", index, op, name.data(), lhs, rhs);
        append(buf);
        return index;
    }


    [[nodiscard]] size_t neg(size_t rhs, const TypeReference& type) {
        if (isInt(type)) {
            auto zero = const_(0L);
            return infix("sub", zero, rhs, type);
        } else {
            char buf[64];
            auto name = type->serialize();
            size_t index = reg++;
            sprintf(buf, "%%%zu = fneg %s %%%zu", index, name.data(), rhs);
            append(buf);
            return index;
        }
    }

    [[nodiscard]] size_t compare(const char* op1, const char* op2, size_t lhs, size_t rhs, const TypeReference& type) {
        char buf[64];
        auto name = type->serialize();
        size_t index = reg++;
        sprintf(buf, "%%%zu = %s %s %s %%%zu, %%%zu", index, op1, op2, name.data(), lhs, rhs);
        append(buf);
        return index;
    }

    [[nodiscard]] size_t offset(size_t ptr, size_t idx, const TypeReference& type) {
        char buf[64];
        auto name = type->serialize();
        size_t index = reg++;
        sprintf(buf, "%%%zu = getelementptr inbounds %s, ptr %%%zu, i64 %%%zu", index, name.data(), ptr, idx);
        append(buf);
        return index;
    }

    void return_(size_t from, const TypeReference& type) {
        if (isNone(type)) {
            append("ret void");
            return;
        }
        char buf[64];
        auto name = type->serialize();
        sprintf(buf, "ret %s %%%zu", name.data(), from);
        append(buf);
    }

    void br(size_t cond, size_t L1, size_t L2) {
        char buf[64];
        sprintf(buf, "br i1 %%%zu, label %%L%zu, label %%L%zu", cond, L1, L2);
        append(buf);
    }

    void br(size_t L) {
        char buf[64];
        sprintf(buf, "br label %%L%zu", L);
        append(buf);
    }

    void label(size_t index) {
        char buf[24];
        sprintf(buf, "L%zu:", index);
        indent -= 4;
        append(buf);
        indent += 4;
    }
    size_t indent = 0;

    void append(std::string line) {
        line.insert(0, std::string(indent, ' '));
        assemblies.emplace_back(std::move(line));
    }

    void write(FILE* file) {
        for (auto&& s : table) {
            std::string buf = "string " + std::to_string(s.length()) + " ";
            for (char ch : s) {
                char digits[3];
                sprintf(digits, "%02hhX", ch);
                buf += digits;
            }
            fputs(buf.c_str(), file);
            fputs("\n", file);
        }
        for (auto&& line : assemblies) {
            fputs(line.c_str(), file);
            fputs("\n", file);
        }
    }
};

}