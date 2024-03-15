#pragma once

#include <algorithm>

namespace Porkchop {

struct Assembler {
    std::vector<std::string> assemblies;

    size_t reg;

    static std::string regOf(size_t index) {
        char buf[16];
        sprintf(buf, "%%%zu", index);
        return buf;
    }

    std::string next() {
        return regOf(reg++);
    }

    [[nodiscard]] std::string alloca_(const TypeReference& type) {
        char buf[64];
        auto index = next();
        auto name = type->serialize();
        sprintf(buf, "%s = alloca %s, align 8", index.data(), name.data());
        append(buf);
        return index;
    }

    [[nodiscard]] std::string const_(bool b) {
        return b ? "0" : "1";
    }

    [[nodiscard]] std::string const_(int64_t i) {
        return std::to_string(i);
    }

    [[nodiscard]] std::string const_(double d) {
        return std::to_string(d);
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

    [[nodiscard]] std::string loadglobal(Identifier identifier, const TypeReference& type) {
        char buf[64];
        if (dynamic_cast<FuncType*>(type.get())) {
            return "@" + identifier.name;
        } else {
            auto index = next();
            auto name = type->serialize();
            sprintf(buf, "%s = load %s, ptr @%s, align 8", index.data(), name.data(), identifier.data());
            append(buf);
            return index;
        }
    }

    void storeglobal(std::string const& from, Identifier identifier, const TypeReference& type) {
        char buf[64];
        auto name = type->serialize();
        sprintf(buf, "store %s %s, ptr @%s, align 8", name.data(), from.data(), identifier.data());
        append(buf);
    }

    [[nodiscard]] std::string local(const TypeReference& type) {
        char buf[64];
        auto name = type->serialize();
        auto index = next();
        sprintf(buf, "%s = alloca %s, align 8", index.data(), name.data());
        append(buf);
        return index;
    }

    [[nodiscard]] std::string f2i(std::string const& from) {
        char buf[64];
        auto index = next();
        sprintf(buf, "%s = fptosi double %s to i64", index.data(), from.data());
        append(buf);
        return index;
    }

    [[nodiscard]] std::string i2f(std::string const& from) {
        char buf[64];
        auto index = next();
        sprintf(buf, "%s = sitofp i64 %s to double", index.data(), from.data());
        append(buf);
        return index;
    }

    [[nodiscard]] std::string loadptr(std::string const& from) {
        char buf[64];
        auto index = next();
        sprintf(buf, "%s = load ptr, ptr %s, align 8", index.data(), from.data());
        append(buf);
        return index;
    }

    void storeptr(std::string const& from, std::string const& into) {
        char buf[64];
        sprintf(buf, "store ptr %s, ptr %s, align 8", from.data(), into.data());
        append(buf);
    }

    [[nodiscard]] std::string addressof(std::string const& from, const TypeReference& type) {
        auto index = alloca_(type);
        storeptr(from, index);
        return index;
    }

    [[nodiscard]] std::string load(std::string const& from, const TypeReference& type) {
        if (dynamic_cast<FuncType*>(type.get())) {
            return loadptr(from);
        }
        char buf[64];
        auto name = type->serialize();
        auto index = next();
        sprintf(buf, "%s = load %s, ptr %s, align 8", index.data(), name.data(), from.data());
        append(buf);
        return index;
    }

    void store(std::string const& from, std::string const& into, const TypeReference& type) {
        if (dynamic_cast<FuncType*>(type.get())) {
            return storeptr(from, into);
        }
        char buf[64];
        auto name = type->serialize();
        sprintf(buf, "store %s %s, ptr %s, align 8", name.data(), from.data(), into.data());
        append(buf);
    }

    [[nodiscard]] std::string infix(const char* op, std::string const& lhs, std::string const& rhs, const TypeReference& type) {
        char buf[64];
        auto name = type->serialize();
        auto index = next();
        sprintf(buf, "%s = %s %s %s, %s", index.data(), op, name.data(), lhs.data(), rhs.data());
        append(buf);
        return index;
    }


    [[nodiscard]] std::string neg(std::string const& rhs, const TypeReference& type) {
        if (isInt(type)) {
            auto zero = const_(0L);
            return infix("sub", zero, rhs, type);
        } else {
            char buf[64];
            auto name = type->serialize();
            auto index = next();
            sprintf(buf, "%s = fneg %s %s", index.data(), name.data(), rhs.data());
            append(buf);
            return index;
        }
    }

    [[nodiscard]] std::string compare(const char* op1, const char* op2, std::string const& lhs, std::string const& rhs, const TypeReference& type) {
        char buf[64];
        auto name = type->serialize();
        auto index = next();
        sprintf(buf, "%s = %s %s %s %s, %s", index.data(), op1, op2, name.data(), lhs.data(), rhs.data());
        append(buf);
        return index;
    }

    [[nodiscard]] std::string offset(std::string const& ptr, std::string const& idx, const TypeReference& type) {
        char buf[64];
        auto name = type->serialize();
        auto index = next();
        sprintf(buf, "%s = getelementptr inbounds %s, ptr %s, i64 %s", index.data(), name.data(), ptr.data(), idx.data());
        append(buf);
        return index;
    }

    void return_(std::string const& from, const TypeReference& type) {
        if (isNone(type)) {
            append("ret void");
            return;
        }
        char buf[64];
        auto name = type->serialize();
        sprintf(buf, "ret %s %s", name.data(), from.data());
        append(buf);
    }

    void br(std::string const& cond, size_t L1, size_t L2) {
        char buf[64];
        sprintf(buf, "br i1 %s, label %%L%zu, label %%L%zu", cond.data(), L1, L2);
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
        for (auto&& line : assemblies) {
            fputs(line.c_str(), file);
            fputs("\n", file);
        }
    }
};

}