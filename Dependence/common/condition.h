#pragma once

#include "utility.h"
#include "error.h"

#include <string>
#include <memory>
#include <functional>
#include <variant>
#include <unordered_map>


enum class BinaryOperator {
    EQUAL,
    NOT_EQUAL,
    GREATER,
    GREATER_EQUAL,
    LESS,
    LESS_EQUAL,
    INCLUDE,

    ADD,
    SUBTRACT,
    MULTIPLY,
    DIVIDE,
    MODULO,
    EXPONENT,

    LOGICAL_AND,
    LOGICAL_OR
};

enum class UnaryOperator {
    NEGATE,
    LOGICAL_NOT
};

using ValueType = std::variant<int, double, bool, std::string>;

class Expression {
public:
    virtual ~Expression() = default;

    virtual ValueType Evaluate(std::function<std::pair<bool, ValueType>(const std::string&)> getValue) const = 0;
    virtual ValueType Evaluate(std::vector<std::function<std::pair<bool, ValueType>(const std::string&)>> getValues) const = 0;
};

class VariableExpression : public Expression {
private:
    std::string name;
public:
    VariableExpression(const std::string& n);

    ValueType Evaluate(std::function<std::pair<bool, ValueType>(const std::string&)> getValue) const override;
    ValueType Evaluate(std::vector<std::function<std::pair<bool, ValueType>(const std::string&)>> getValues) const override;
};

class ConstantExpression : public Expression {
private:
    ValueType value;
public:
    ConstantExpression(ValueType v);

    ValueType Evaluate(std::function<std::pair<bool, ValueType>(const std::string&)> getValue) const override;
    ValueType Evaluate(std::vector<std::function<std::pair<bool, ValueType>(const std::string&)>> getValues) const override;
};

class ArrayExpression : public Expression {
private:
    std::vector<std::shared_ptr<Expression>> elements;

public:
    ArrayExpression(std::vector<std::shared_ptr<Expression>> es);

    std::vector<ValueType> GetElementValues(std::function<std::pair<bool, ValueType>(const std::string&)> getValue) const;
    std::vector<ValueType> GetElementValues(std::vector < std::function<std::pair<bool, ValueType>(const std::string&)>> getValues) const;

    ValueType Evaluate(std::function<std::pair<bool, ValueType>(const std::string&)> getValue) const override;
    ValueType Evaluate(std::vector<std::function<std::pair<bool, ValueType>(const std::string&)>> getValues) const override;
};

class UnaryExpression : public Expression {
private:
    UnaryOperator operand;
    std::shared_ptr<Expression> expression;

public:
    UnaryExpression(UnaryOperator op, std::shared_ptr<Expression> operand);

    ValueType Evaluate(std::function<std::pair<bool, ValueType>(const std::string&)> getValue) const override;
    ValueType Evaluate(std::vector<std::function<std::pair<bool, ValueType>(const std::string&)>> getValues) const override;

private:
    ValueType ApplyNegate(const ValueType& value) const;

    ValueType ApplyLogicalNot(const ValueType& value) const;

    bool ConvertToBool(const ValueType& value) const;
};

class BinaryExpression : public Expression {
private:
    std::shared_ptr<Expression> left;
    std::shared_ptr<Expression> right;
    BinaryOperator operand;

public:
    BinaryExpression(std::shared_ptr<Expression> l, std::shared_ptr<Expression> r, BinaryOperator op);

    ValueType Evaluate(std::function<std::pair<bool, ValueType>(const std::string&)> getValue) const override;
    ValueType Evaluate(std::vector<std::function<std::pair<bool, ValueType>(const std::string&)>> getValues) const override;

private:
    bool GetComparisonResult(const ValueType& left, const ValueType& right, BinaryOperator op) const;

    ValueType CompareValues(const ValueType& left, const ValueType& right, BinaryOperator op) const;

    template<typename T>
    static bool CompareSameType(const T& left, const T& right, BinaryOperator op) {
        switch (op) {
        case BinaryOperator::EQUAL: return left == right;
        case BinaryOperator::NOT_EQUAL: return left != right;
        case BinaryOperator::GREATER: return left > right;
        case BinaryOperator::GREATER_EQUAL: return left >= right;
        case BinaryOperator::LESS: return left < right;
        case BinaryOperator::LESS_EQUAL: return left <= right;
        default: return false;
        }
    }

    template<typename T1, typename T2>
    static bool CompareDifferentType(const T1& left, const T2& right, BinaryOperator op) {
        if constexpr (std::is_arithmetic_v<T1> && std::is_arithmetic_v<T2>) {
            double l = static_cast<double>(left);
            double r = static_cast<double>(right);
            return CompareSameType(l, r, op);
        }
        else if constexpr (std::is_same_v<T1, std::string> || std::is_same_v<T2, std::string>) {
            std::string l_str = toString(left);
            std::string r_str = toString(right);
            return CompareSameType(l_str, r_str, op);
        }
        else {
            return false;
        }
    }

    ValueType ComputeArithmetic(const ValueType& left, const ValueType& right, BinaryOperator op) const;

    bool ConvertToBool(const ValueType& value) const;

    template<typename T>
    static std::string toString(const T& value) {
        if constexpr (std::is_same_v<T, std::string>) {
            return value;
        }
        else if constexpr (std::is_same_v<T, bool>) {
            return value ? "true" : "false";
        }
        else {
            return std::to_string(value);
        }
    }
};

class Condition {
public:
    bool ParseCondition(const std::string& conditionStr);

    bool EvaluateBool(std::function<std::pair<bool, ValueType>(const std::string&)> getValue) const;
    bool EvaluateBool(std::vector < std::function<std::pair<bool, ValueType>(const std::string&)>> getValues) const;
    ValueType EvaluateValue(std::function<std::pair<bool, ValueType>(const std::string&)> getValue) const;
    ValueType EvaluateValue(std::vector < std::function<std::pair<bool, ValueType>(const std::string&)>> getValues) const;

private:
    std::shared_ptr<Expression> root;

    std::vector<std::string> Tokenize(const std::string& expr);
    std::shared_ptr<Expression> ParseExpression(const std::vector<std::string>& tokens);
    std::vector<std::string> InfixToPostfix(const std::vector<std::string>& infix);
    std::shared_ptr<Expression> ParsePostfix(const std::vector<std::string>& postfix);
    bool IsOperator(const std::string& token);
    bool HigherPrecedence(const std::string& op1, const std::string& op2);
    int GetPrecedence(const std::string& op);
    bool RightAssociative(const std::string& op);
    BinaryOperator GetOperator(const std::string& token);
    std::shared_ptr<Expression> ParseOperand(const std::string& token);
    std::shared_ptr<Expression> ParseConstant(const std::string& token);
    bool OperatorChar(char c);
};

bool IsOperatorChar(char c);
bool IsSpaceChar(char c);
bool IsIdentifierChar(char c);

std::string ToString(const ValueType& value);
ValueType FromString(const std::string& s);
