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

    static inline bool debug_flag = false;
    std::vector<std::string> debug_info;
    size_t dbg = 10;
    std::vector<std::string> gves;

    void init_debug(std::string const& filename, std::string const& directory) {
        if (!debug_flag) return;
        std::initializer_list<std::string> header = {
                "!llvm.dbg.cu = !{!0}",
                "!llvm.module.flags = !{!3, !4, !5}",
                "!llvm.ident = !{!2}",
                "!0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, producer: \"PorkchopLite\", "
                                            "isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, "
                                            "globals: !9, splitDebugInlining: false, nameTableKind: None)",
                "!1 = !DIFile(filename: \"" + filename + "\", directory: \"" + directory + "\")",
                "!2 = !{!\"PorkchopLite\"}",
                "!3 = !{i32 7, !\"Dwarf Version\", i32 5}",
                "!4 = !{i32 2, !\"Debug Info Version\", i32 3}",
                "!5 = !{i32 1, !\"PIC Level\", i32 2}",
                "!6 = !DIBasicType(name: \"bool\", size: 8, encoding: DW_ATE_boolean)",
                "!7 = !DIBasicType(name: \"int\", size: 64, encoding: DW_ATE_signed)",
                "!8 = !DIBasicType(name: \"float\", size: 64, encoding: DW_ATE_float)",
        };
        debug_info.assign(header);
    }

    struct DEBUG {
        static constexpr auto UNIT = "!0";
        static constexpr auto FILE = "!1";

        static constexpr auto NONE = "null";
        static constexpr auto BOOL = "!6";
        static constexpr auto INT = "!7";
        static constexpr auto FLOAT = "!8";
        static constexpr auto GLOBALS = "!9";

        static std::string listOf(std::vector<std::string> const& elements) {
            std::string ret = "!{";
            bool first = true;
            for (auto&& element : elements) {
                if (first) first = false; else ret += ", ";
                ret += element;
            }
            ret += "}";
            return ret;
        }
    };

    std::string typeOf(TypeReference const& type) {
        if (auto ptr = dynamic_cast<PointerType*>(type.get())) {
            char buf[64];
            sprintf(buf, "!DIDerivedType(tag: DW_TAG_pointer_type, baseType: %s, size: 64)", typeOf(ptr->E).data());
            return debug(buf);
        }
        if (auto func = dynamic_pointer_cast<FuncType>(type)) {
            char buf[64];
            sprintf(buf, "!DIDerivedType(tag: DW_TAG_pointer_type, baseType: %s, size: 64)", prototypeOf(func).data());
            return debug(buf);
        }
        if (auto scalar = dynamic_cast<ScalarType*>(type.get())) {
            switch (scalar->S) {
                case ScalarTypeKind::BOOL:
                    return DEBUG::BOOL;
                case ScalarTypeKind::INT:
                    return DEBUG::INT;
                case ScalarTypeKind::FLOAT:
                    return DEBUG::FLOAT;
                default:
                    return DEBUG::NONE;
            }
        }
        return DEBUG::NONE;
    }


    std::string prototypeOf(std::shared_ptr<FuncType> const& type) {
        std::vector<std::string> types{typeOf(type->R)};
        for (auto&& P : type->P) {
            types.emplace_back(typeOf(P));
        }
        return debug("!DISubroutineType(types: " + Assembler::DEBUG::listOf(types) + ")");
    }


    static std::string debugOf(size_t index) {
        char buf[16];
        sprintf(buf, "!%zu", index);
        return buf;
    }

    std::string debug(std::string const& content) {
        auto index = debugOf(dbg++);
        debug_info.emplace_back(index + " = " + content);
        return index;
    }

    void appendDBG(std::string const& index) {
        assemblies.back() += ", !dbg ";
        assemblies.back() += index;
    }

    std::string scope;

    void appendLocation(Token token) {
        char buf[64];
        sprintf(buf, "!DILocation(line: %zu, column: %zu, scope: %s)", token.line + 1, token.column, scope.data());
        auto loc = debug(buf);
        appendDBG(loc);
    }

    [[nodiscard]] std::string alloca_(const TypeReference& type) {
        char buf[64];
        auto index = next();
        auto name = type->serialize();
        sprintf(buf, "%s = alloca %s", index.data(), name.data());
        append(buf);
        return index;
    }

    [[nodiscard]] static std::string const_(bool b) {
        return b ? "0" : "1";
    }

    [[nodiscard]] static std::string const_(int64_t i) {
        return std::to_string(i);
    }

    [[nodiscard]] static std::string const_(double d) {
        return std::to_string(d);
    }

    static std::string escape(std::string_view source) {
        std::string name;
        bool simple = true;
        for (char ch : source) {
            if (ch != '_' && !isalnum(ch)) {
                simple = false;
                break;
            }
        }
        name = "@";
        if (simple) {
            name += source;
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
        return name;
    }

    static std::string quote(std::string_view source) {
        std::string id(source.substr(1)); // removing leading @
        if (!id.starts_with('"')) {
            id = '"' + id + '"';
        }
        return id;
    }

    void local(const Compiler& compiler, Token token, std::string const& address,TypeReference const& type, size_t arg = 0) {
        if (!debug_flag) return;
        char buf[256];
        sprintf(buf, "!DILocalVariable(name: %s, scope: %s, file: %s, line: %zu, type: %s, arg: %zu)",
                quote(escape(compiler.of(token))).data(), scope.data(),
                DEBUG::FILE, token.line, typeOf(type).data(), arg);
        auto lv = debug(buf);
        sprintf(buf, "call void @llvm.dbg.declare(metadata ptr %s, metadata %s, metadata !DIExpression())",
                address.data(), lv.data());
        append(buf, token);
    }

    void global(std::string const& identifier, const TypeReference& type, std::string const& initial, size_t line) {
        char buf[256];
        auto name = type->serialize();
        sprintf(buf, "%s = global %s %s", identifier.data(), name.data(), initial.data());
        append(buf);
        if (!debug_flag) return;
        sprintf(buf, "distinct !DIGlobalVariable(name: %s, scope: %s, file: %s, line: %zu, type: %s, isLocal: false, isDefinition: true)",
                quote(identifier).data(), DEBUG::UNIT, DEBUG::FILE, line + 1, typeOf(type).data());
        auto gv = debug(buf);
        sprintf(buf, "!DIGlobalVariableExpression(var: %s, expr: !DIExpression())", gv.data());
        auto gve = debug(buf);
        appendDBG(gve);
        gves.emplace_back(std::move(gve));
    }

    [[nodiscard]] std::string cast(const char* op, std::string const& from, const TypeReference& type1, const TypeReference& type2, Token token) {
        char buf[64];
        auto index = next();
        sprintf(buf, "%s = %s %s %s to %s", index.data(), op, type1->serialize().data(), from.data(), type2->serialize().data());
        append(buf, token);
        return index;
    }

    [[nodiscard]] std::string load(std::string const& from, const TypeReference& type, Token token) {
        char buf[64];
        auto name = type->serialize();
        auto index = next();
        sprintf(buf, "%s = load %s, ptr %s", index.data(), name.data(), from.data());
        append(buf, token);
        return index;
    }

    void store(std::string const& from, std::string const& into, const TypeReference& type, Token token) {
        char buf[64];
        auto name = type->serialize();
        sprintf(buf, "store %s %s, ptr %s", name.data(), from.data(), into.data());
        append(buf, token);
    }

    [[nodiscard]] std::string infix(const char* op, std::string const& lhs, std::string const& rhs, const TypeReference& type, Token token) {
        char buf[64];
        auto name = type->serialize();
        auto index = next();
        sprintf(buf, "%s = %s %s %s, %s", index.data(), op, name.data(), lhs.data(), rhs.data());
        append(buf, token);
        return index;
    }

    [[nodiscard]] std::string neg(std::string const& rhs, const TypeReference& type, Token token) {
        if (isInt(type)) {
            return infix("sub", "0", rhs, type, token);
        } else {
            char buf[64];
            auto name = type->serialize();
            auto index = next();
            sprintf(buf, "%s = fneg %s %s", index.data(), name.data(), rhs.data());
            append(buf, token);
            return index;
        }
    }

    [[nodiscard]] std::string compare(const char* op1, const char* op2, std::string const& lhs, std::string const& rhs, const TypeReference& type, Token token) {
        char buf[64];
        auto name = type->serialize();
        auto index = next();
        sprintf(buf, "%s = %s %s %s %s, %s", index.data(), op1, op2, name.data(), lhs.data(), rhs.data());
        append(buf, token);
        return index;
    }

    [[nodiscard]] std::string offset(std::string const& ptr, std::string const& idx, const TypeReference& type, Token token) {
        char buf[64];
        auto name = type->serialize();
        auto index = next();
        sprintf(buf, "%s = getelementptr inbounds %s, ptr %s, i64 %s", index.data(), name.data(), ptr.data(), idx.data());
        append(buf, token);
        return index;
    }

    void return_(std::string const& from, const TypeReference& type, Token token) {
        if (isNone(type)) {
            append("ret void", token);
            return;
        }
        char buf[64];
        auto name = type->serialize();
        sprintf(buf, "ret %s %s", name.data(), from.data());
        append(buf, token);
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

    void append(std::string line, Token token) {
        line.insert(0, std::string(indent, ' '));
        assemblies.emplace_back(std::move(line));
        if (debug_flag) appendLocation(token);
    }

    void write(FILE* file) {
        if (debug_flag)
            assemblies.emplace_back("declare void @llvm.dbg.declare(metadata, metadata, metadata)");
        for (auto&& line : assemblies) {
            fputs(line.c_str(), file);
            fputs("\n", file);
        }
        if (debug_flag) {
            debug_info.emplace_back(DEBUG::GLOBALS + (" = " + DEBUG::listOf(gves)));
            for (auto&& line : debug_info) {
                fputs(line.c_str(), file);
                fputs("\n", file);
            }
        }
    }
};

}