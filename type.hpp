#pragma once

#include <unordered_map>
#include <string>
#include <memory>

#include "descriptor.hpp"

namespace Porkchop {

struct Type;
using TypeReference = std::shared_ptr<Type>;

enum class ScalarTypeKind {
    NONE,
    NEVER,
    BOOL,
    INT,
    FLOAT,
};

constexpr std::string_view SCALAR_TYPE_NAME[] = {
    "none",
    "never",
    "bool",
    "int",
    "float",
};

constexpr std::string_view SCALAR_TYPE_DESC[] = {
    "void",
    "void",
    "i1",
    "i64",
    "double",
};

const std::unordered_map<std::string_view, ScalarTypeKind> SCALAR_TYPES {
    {"none",   ScalarTypeKind::NONE},
    {"never",  ScalarTypeKind::NEVER},
    {"bool",   ScalarTypeKind::BOOL},
    {"int",    ScalarTypeKind::INT},
    {"float",  ScalarTypeKind::FLOAT},
};

struct Type : Descriptor {
    [[nodiscard]] virtual std::string toString() const = 0;
    [[nodiscard]] virtual bool equals(const TypeReference& type) const noexcept = 0;
    [[nodiscard]] virtual bool assignableFrom(const TypeReference& type) const noexcept {
        return equals(type);
    }
    [[nodiscard]] virtual std::string serialize() const = 0;
};

[[nodiscard]] inline bool isNever(TypeReference const& type) noexcept;

struct ScalarType : Type {
    ScalarTypeKind S;

    explicit ScalarType(ScalarTypeKind S): S(S) {}

    [[nodiscard]] std::string toString() const override {
        return std::string{SCALAR_TYPE_NAME[(size_t) S]};
    }

    [[nodiscard]] bool equals(const TypeReference& type) const noexcept override {
        if (this == type.get()) return true;
        if (auto scalar = dynamic_cast<const ScalarType*>(type.get())) {
            return scalar->S == S;
        }
        return false;
    }

    [[nodiscard]] bool assignableFrom(const TypeReference& type) const noexcept override {
        if (this == type.get()) return true;
        switch (S) {
            case ScalarTypeKind::NEVER:
                return false;
            case ScalarTypeKind::NONE:
                return !isNever(type);
            default:
                return equals(type);
        }
    }

    [[nodiscard]] std::string_view descriptor() const noexcept override {
        return SCALAR_TYPE_NAME[(size_t) S];
    }

    [[nodiscard]] std::string serialize() const override {
        return std::string{SCALAR_TYPE_DESC[(size_t) S]};
    }
};

namespace ScalarTypes {
const TypeReference NONE = std::make_shared<ScalarType>(ScalarTypeKind::NONE);
const TypeReference NEVER = std::make_shared<ScalarType>(ScalarTypeKind::NEVER);
const TypeReference BOOL = std::make_shared<ScalarType>(ScalarTypeKind::BOOL);
const TypeReference INT = std::make_shared<ScalarType>(ScalarTypeKind::INT);
const TypeReference FLOAT = std::make_shared<ScalarType>(ScalarTypeKind::FLOAT);
}

[[nodiscard]] inline bool isScalar(TypeReference const& type, ScalarTypeKind kind) noexcept {
    if (auto scalar = dynamic_cast<ScalarType*>(type.get())) {
        return scalar->S == kind;
    }
    return false;
}

[[nodiscard]] inline bool isScalar(TypeReference const& type, bool pred(ScalarTypeKind) noexcept) noexcept {
    if (auto scalar = dynamic_cast<ScalarType*>(type.get()))
        return pred(scalar->S);
    return false;
}

[[nodiscard]] inline bool isNone(TypeReference const& type) noexcept {
    return isScalar(type, ScalarTypeKind::NONE);
}

[[nodiscard]] inline bool isNever(TypeReference const& type) noexcept {
    return isScalar(type, ScalarTypeKind::NEVER);
}

[[nodiscard]] inline bool isBool(TypeReference const& type) noexcept {
    return isScalar(type, ScalarTypeKind::BOOL);
}

[[nodiscard]] inline bool isInt(TypeReference const& type) noexcept {
    return isScalar(type, ScalarTypeKind::INT);
}

[[nodiscard]] inline bool isFloat(TypeReference const& type) noexcept {
    return isScalar(type, ScalarTypeKind::FLOAT);
}

[[nodiscard]] inline bool isSimilar(bool pred(TypeReference const&), TypeReference const& type1, TypeReference const& type2) noexcept {
    return pred(type1) && pred(type2);
}

[[nodiscard]] inline bool isArithmetic(TypeReference const& type) noexcept {
    return isScalar(type, [](ScalarTypeKind kind) noexcept { return kind == ScalarTypeKind::INT || kind == ScalarTypeKind::FLOAT; });
}

struct PointerType : Type {
    TypeReference E;

    explicit PointerType(TypeReference E): E(std::move(E)) {}

    [[nodiscard]] std::string toString() const override {
        return '*' + E->toString();
    }

    [[nodiscard]] bool equals(const TypeReference& type) const noexcept override {
        if (this == type.get()) return true;
        if (auto list = dynamic_cast<const PointerType*>(type.get())) {
            return list->E->equals(E);
        }
        return false;
    }

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {E.get()}; }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return "*"; }

    [[nodiscard]] std::string serialize() const override {
        return "ptr";
    }
};

struct FuncType : Type {
    std::vector<TypeReference> P;
    TypeReference R;

    explicit FuncType(std::vector<TypeReference> P, TypeReference R): P(std::move(P)), R(std::move(R)) {}

    [[nodiscard]] std::string toString() const override {
        std::string buf = "(";
        bool first = true;
        for (auto&& p : P) {
            if (first) { first = false; } else { buf += ", "; }
            buf += p->toString();
        }
        buf += "): ";
        buf += R->toString();
        return buf;
    }

    [[nodiscard]] bool equals(const TypeReference& type) const noexcept override {
        if (this == type.get()) return true;
        if (auto func = dynamic_cast<const FuncType*>(type.get())) {
            return func->R->equals(R) &&
            std::equal(P.begin(), P.end(), func->P.begin(), func->P.end(),
                       [](const TypeReference& type1, const TypeReference& type2) { return type1->equals(type2); });
        }
        return false;
    }

    [[nodiscard]] bool assignableFrom(const TypeReference& type) const noexcept override {
        if (this == type.get()) return true;
        if (auto func = dynamic_cast<const FuncType*>(type.get())) {
            return (func->R->assignableFrom(R) || isNever(R) && isNever(func->R)) &&
            std::equal(P.begin(), P.end(), func->P.begin(), func->P.end(),
                       [](const TypeReference& type1, const TypeReference& type2) { return type1->assignableFrom(type2); });
        }
        return false;
    }

    [[nodiscard]] std::vector<const Descriptor*> children() const override {
        std::vector<const Descriptor*> ret;
        for (auto&& e : P) ret.push_back(e.get());
        ret.push_back(R.get());
        return ret;
    }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return "():"; }

    [[nodiscard]] std::string serialize() const override {
        return "ptr";
    }
};


[[nodiscard]] inline TypeReference eithertype(TypeReference const& type1, TypeReference const& type2) noexcept {
    if (type1->equals(type2)) return type1;
    if (isNever(type1)) return type2;
    if (isNever(type2)) return type1;
    if (isNone(type1) || isNone(type2)) return ScalarTypes::NONE;
    return nullptr;
}

union $union {
    size_t $size;
    std::nullptr_t $none;
    bool $bool;
    uint8_t $byte;
    int64_t $int;
    double $float;
    $union(): $union(nullptr) {}
    $union(size_t $size): $size($size) {}
    $union(std::nullptr_t $none): $none($none) {}
    $union(bool $bool): $bool($bool) {}
    $union(uint8_t $byte): $byte($byte) {}
    $union(int64_t $int): $int($int) {}
    $union(double $float): $float($float) {}
};

}
