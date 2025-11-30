#pragma once

#include "utility.h"
#include "error.h"

#include <string>
#include <memory>
#include <functional>
#include <variant>
#include <unordered_map>


enum class Operator {
    // 比较运算符
    EQUAL,          // ==
    NOT_EQUAL,      // !=
    GREATER,        // >
    GREATER_EQUAL,  // >=
    LESS,           // <
    LESS_EQUAL,     // <=
    INCLUDE,             // in

    // 算术运算符
    ADD,            // +
    SUBTRACT,       // -
    MULTIPLY,       // *
    DIVIDE,         // /
    MODULO,         // %
    EXPONENT,       // ^

    // 逻辑运算符
    LOGICAL_AND,    // &&
    LOGICAL_OR      // ||
};

enum class UnaryOperator {
    NEGATE,         // 负号 -
    LOGICAL_NOT     // 逻辑非 !
};

using ValueType = std::variant<int, double, bool, std::string>;

class Expression {
public:
    virtual ~Expression() = default;

    virtual ValueType Evaluate(std::function<ValueType(const std::string&)> getValue) const = 0;
};

class Condition {
public:
    bool ParseCondition(const std::string& conditionStr);

    bool EvaluateBool(std::function<ValueType(const std::string&)> getValue) const;
	ValueType EvaluateValue(std::function<ValueType(const std::string&)> getValue) const;

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
    Operator GetOperator(const std::string& token);
    std::shared_ptr<Expression> ParseOperand(const std::string& token);
    std::shared_ptr<Expression> ParseConstant(const std::string& token);
    bool OperatorChar(char c);
};

bool IsOperatorChar(char c);
bool IsSpaceChar(char c);
bool IsIdentifierChar(char c);

std::string ToString(const ValueType& value);
ValueType FromString(const std::string& s);
