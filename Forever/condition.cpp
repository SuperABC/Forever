#include "condition.h"

#include <iostream>
#include <vector>
#include <sstream>
#include <memory>
#include <cmath>
#include <stack>
#include <algorithm>
#include <cctype>
#include <locale>


using namespace std;

bool IsOperatorChar(char c) {
    return c == '=' || c == '!' || c == '>' || c == '<' ||
        c == '+' || c == '-' || c == '*' || c == '/' ||
        c == '%' || c == '^' || c == '&' || c == '|';
}

bool IsSpaceChar(char c) {
    return std::isspace(static_cast<unsigned char>(c)) ||
        c == '　'; // 中文全角空格
}

bool IsIdentifierChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) ||
        c == '_' ||
        (c >= 0x80 && c <= 0xFF); // 支持中文字符和其他扩展字符
}

class VariableExpression : public Expression {
private:
    std::string name;
public:
    VariableExpression(const std::string& n) : name(n) {}

    ValueType Evaluate(std::function<ValueType(const std::string&)> getValue) const override {
        return getValue(name);
    }
};

class ConstantExpression : public Expression {
private:
    ValueType value;
public:
    ConstantExpression(ValueType v) : value(v) {}

    ValueType Evaluate(std::function<ValueType(const std::string&)> getValue) const override {
        return value;
    }
};

class ArrayExpression : public Expression {
private:
    std::vector<std::shared_ptr<Expression>> elements;

public:
    ArrayExpression(std::vector<std::shared_ptr<Expression>> es)
        : elements(std::move(es)) {
    }

    ValueType Evaluate(std::function<ValueType(const std::string&)> getValue) const override {
        std::string result = "[";
        for (size_t i = 0; i < elements.size(); ++i) {
            auto value = elements[i]->Evaluate(getValue);
            result += ToString(value);
            if (i < elements.size() - 1) {
                result += ", ";
            }
        }
        result += "]";
        return result;
    }

    // 获取数组元素的值（用于in操作符）
    std::vector<ValueType> GetElementValues(std::function<ValueType(const std::string&)> getValue) const {
        std::vector<ValueType> values;
        for (const auto& element : elements) {
            values.push_back(element->Evaluate(getValue));
        }
        return values;
    }
};

class UnaryExpression : public Expression {
private:
    UnaryOperator operand;
    std::shared_ptr<Expression> expression;

public:
    UnaryExpression(UnaryOperator op, std::shared_ptr<Expression> operand)
        : operand(op), expression(std::move(operand)) {
    }

    ValueType Evaluate(std::function<ValueType(const std::string&)> getValue) const override {
        auto value = expression->Evaluate(getValue);

        switch (operand) {
        case UnaryOperator::NEGATE:
            return ApplyNegate(value);
        case UnaryOperator::LOGICAL_NOT:
            return ApplyLogicalNot(value);
        default:
            THROW_EXCEPTION(InvalidConfigException, "Unknown unary operator.\n");
        }
    }

private:
    ValueType ApplyNegate(const ValueType& value) const {
        return std::visit([](const auto& v) -> ValueType {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_arithmetic_v<T> && !std::is_same_v<T, bool>) {
                return -v;
            }
            else {
                THROW_EXCEPTION(InvalidConfigException, "Cannot apply negate to boolean or non-numeric type.\n");
            }
            }, value);
    }

    ValueType ApplyLogicalNot(const ValueType& value) const {
        bool bool_val = ConvertToBool(value);
        return !bool_val;
    }

    bool ConvertToBool(const ValueType& value) const {
        return std::visit([](const auto& v) -> bool {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, bool>) {
                return v;
            }
            else if constexpr (std::is_same_v<T, int>) {
                return v != 0;
            }
            else if constexpr (std::is_same_v<T, double>) {
                return std::abs(v) > 1e-10; // 考虑浮点误差
            }
            else if constexpr (std::is_same_v<T, std::string>) {
                return !v.empty();
            }
            else {
                return false;
            }
            }, value);
    }
};

class BinaryExpression : public Expression {
private:
    std::shared_ptr<Expression> left;
    std::shared_ptr<Expression> right;
    Operator operand;

public:
    BinaryExpression(std::shared_ptr<Expression> l,
        std::shared_ptr<Expression> r,
        Operator op)
        : left(std::move(l)), right(std::move(r)), operand(op) {
    }

    ValueType Evaluate(std::function<ValueType(const std::string&)> getValue) const override {
        if (operand == Operator::INCLUDE) {
            auto array_expr = dynamic_cast<ArrayExpression*>(right.get());
            if (!array_expr) {
                THROW_EXCEPTION(InvalidConfigException, "Right operand of 'in' must be an array.\n");
            }

            auto array_values = array_expr->GetElementValues(getValue);
            bool found = false;

            auto left_val = left->Evaluate(getValue);
            for (const auto& array_val : array_values) {
                if (GetComparisonResult(left_val, array_val, Operator::EQUAL)) {
                    found = true;
                    break;
                }
            }

            return found;
        }

        if (operand == Operator::LOGICAL_AND) {
            bool left_val = ConvertToBool(left->Evaluate(getValue));
            if (!left_val) return false;
            return ConvertToBool(right->Evaluate(getValue));
        }

        if (operand == Operator::LOGICAL_OR) {
            bool left_val = ConvertToBool(left->Evaluate(getValue));
            if (left_val) return true;
            return ConvertToBool(right->Evaluate(getValue));
        }

        auto left_val = left->Evaluate(getValue);
        auto right_val = right->Evaluate(getValue);

        switch (operand) {
        case Operator::EQUAL:
        case Operator::NOT_EQUAL:
        case Operator::GREATER:
        case Operator::GREATER_EQUAL:
        case Operator::LESS:
        case Operator::LESS_EQUAL:
            return CompareValues(left_val, right_val, operand);

        case Operator::ADD:
        case Operator::SUBTRACT:
        case Operator::MULTIPLY:
        case Operator::DIVIDE:
        case Operator::MODULO:
        case Operator::EXPONENT:
            return ComputeArithmetic(left_val, right_val, operand);

        default:
            THROW_EXCEPTION(InvalidConfigException, "Unknown operator.\n");
        }
    }

private:
    bool GetComparisonResult(const ValueType& left, const ValueType& right, Operator op) const {
        ValueType result = CompareValues(left, right, op);
        if (auto bool_result = std::get_if<bool>(&result)) {
            return *bool_result;
        }
        THROW_EXCEPTION(InvalidConfigException, "Comparison must return boolean value.\n");
    }

    ValueType CompareValues(const ValueType& left, const ValueType& right, Operator op) const {
        bool result = std::visit([op](const auto& l, const auto& r) -> bool {
            using T1 = std::decay_t<decltype(l)>;
            using T2 = std::decay_t<decltype(r)>;

            if constexpr (std::is_same_v<T1, T2>) {
                return CompareSameType(l, r, op);
            }
            else {
                return CompareDifferentType(l, r, op);
            }
            }, left, right);

        return result;
    }

    template<typename T>
    static bool CompareSameType(const T& left, const T& right, Operator op) {
        switch (op) {
        case Operator::EQUAL: return left == right;
        case Operator::NOT_EQUAL: return left != right;
        case Operator::GREATER: return left > right;
        case Operator::GREATER_EQUAL: return left >= right;
        case Operator::LESS: return left < right;
        case Operator::LESS_EQUAL: return left <= right;
        default: return false;
        }
    }

    template<typename T1, typename T2>
    static bool CompareDifferentType(const T1& left, const T2& right, Operator op) {
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

    ValueType ComputeArithmetic(const ValueType& left, const ValueType& right, Operator op) const {
        return std::visit([op](const auto& l, const auto& r) -> ValueType {
            using T1 = std::decay_t<decltype(l)>;
            using T2 = std::decay_t<decltype(r)>;

            if constexpr (std::is_arithmetic_v<T1> && std::is_arithmetic_v<T2>) {
                double l_val = static_cast<double>(l);
                double r_val = static_cast<double>(r);

                switch (op) {
                case Operator::ADD: return l_val + r_val;
                case Operator::SUBTRACT: return l_val - r_val;
                case Operator::MULTIPLY: return l_val * r_val;
                case Operator::DIVIDE:
                    if (std::abs(r_val) < 1e-10) {
                        THROW_EXCEPTION(ArithmaticException, "Division by zero.\n");
                    }
                    return l_val / r_val;
                case Operator::MODULO:
                    if (std::abs(r_val) < 1e-10) {
                        THROW_EXCEPTION(ArithmaticException, "Modulo by zero.\n");
                    }
                    return std::fmod(l_val, r_val);
                case Operator::EXPONENT:
                    return std::pow(l_val, r_val);
                default:
                    THROW_EXCEPTION(ArithmaticException, "Unsupported arithmetic operator.\n");
                }
            }
            else {
                THROW_EXCEPTION(ArithmaticException, "Arithmetic operations only supported for numeric types.\n");
            }
            }, left, right);
    }

    bool ConvertToBool(const ValueType& value) const {
        return std::visit([](const auto& v) -> bool {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, bool>) {
                return v;
            }
            else if constexpr (std::is_same_v<T, int>) {
                return v != 0;
            }
            else if constexpr (std::is_same_v<T, double>) {
                return std::abs(v) > 1e-10;
            }
            else if constexpr (std::is_same_v<T, std::string>) {
                return !v.empty();
            }
            else {
                return false;
            }
            }, value);
    }

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

bool Condition::ParseCondition(const std::string& conditionStr) {
    try {
        std::vector<std::string> tokens = Tokenize(conditionStr);
        if(tokens.size() > 0)root = ParseExpression(tokens);
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Parse error: " << e.what() << std::endl;
        return false;
    }
}

bool Condition::EvaluateResult(std::function<ValueType(const std::string&)> getValue) const {
    if (!root) {
        return true;
    }

    auto result = root->Evaluate(getValue);
    if (auto bool_val = std::get_if<bool>(&result)) {
        return *bool_val;
    }
    THROW_EXCEPTION(InvalidConfigException, "Condition must Evaluate to boolean.\n");
}

std::vector<std::string> Condition::Tokenize(const std::string& expr) {
    std::vector<std::string> tokens;
    std::string current;

    for (size_t i = 0; i < expr.length(); ++i) {
        char c = expr[i];

        if (IsSpaceChar(c)) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        }
        else if (IsOperatorChar(c) || c == '(' || c == ')' || c == '[' || c == ']' || c == ',') {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }

            // 处理双字符运算符
            if ((c == '&' && i + 1 < expr.length() && expr[i + 1] == '&') ||
                (c == '|' && i + 1 < expr.length() && expr[i + 1] == '|') ||
                (c == '=' && i + 1 < expr.length() && expr[i + 1] == '=') ||
                (c == '!' && i + 1 < expr.length() && expr[i + 1] == '=') ||
                (c == '<' && i + 1 < expr.length() && expr[i + 1] == '=') ||
                (c == '>' && i + 1 < expr.length() && expr[i + 1] == '=')) {
                tokens.push_back(std::string(1, c) + std::string(1, expr[i + 1]));
                i++;
            }
            else {
                tokens.push_back(std::string(1, c));
            }
        }
        else {
            current += c;
        }
    }

    if (!current.empty()) {
        tokens.push_back(current);
    }

    return tokens;
}

bool Condition::OperatorChar(char c) {
    return IsOperatorChar(c);
}

std::shared_ptr<Expression> Condition::ParseExpression(const std::vector<std::string>& tokens) {
    std::vector<std::string> postfix = InfixToPostfix(tokens);
    return ParsePostfix(postfix);
}

std::vector<std::string> Condition::InfixToPostfix(const std::vector<std::string>& infix) {
    std::vector<std::string> postfix;
    std::stack<std::string> opStack;

    for (const auto& token : infix) {
        if (IsOperator(token)) {
            while (!opStack.empty() && opStack.top() != "(" &&
                HigherPrecedence(opStack.top(), token)) {
                postfix.push_back(opStack.top());
                opStack.pop();
            }
            opStack.push(token);
        }
        else if (token == "(") {
            opStack.push(token);
        }
        else if (token == ")") {
            while (!opStack.empty() && opStack.top() != "(") {
                postfix.push_back(opStack.top());
                opStack.pop();
            }
            if (opStack.empty()) {
                THROW_EXCEPTION(InvalidConfigException, "Mismatched parentheses.\n");
            }
            opStack.pop();
        }
        else if (token == "[") {
            postfix.push_back(token);
        }
        else if (token == "]") {
            postfix.push_back(token);
        }
        else {
            postfix.push_back(token);
        }
    }

    while (!opStack.empty()) {
        if (opStack.top() == "(") {
            THROW_EXCEPTION(InvalidConfigException, "Mismatched parentheses.\n");
        }
        postfix.push_back(opStack.top());
        opStack.pop();
    }

    return postfix;
}

std::shared_ptr<Expression> Condition::ParsePostfix(const std::vector<std::string>& postfix) {
    std::stack<std::shared_ptr<Expression>> exprStack;

    for (size_t i = 0; i < postfix.size(); ++i) {
        const auto& token = postfix[i];

        if (token == "[") {
            std::vector<std::shared_ptr<Expression>> elements;
            size_t j = i + 1;

            while (j < postfix.size() && postfix[j] != "]") {
                if (postfix[j] == ",") {
                    j++;
                    continue;
                }

                if (IsOperator(postfix[j])) {
                    THROW_EXCEPTION(InvalidConfigException, "Unexpected operator in array: " + postfix[j] + ".\n");
                }

                elements.push_back(ParseOperand(postfix[j]));
                j++;
            }

            if (j >= postfix.size() || postfix[j] != "]") {
                THROW_EXCEPTION(InvalidConfigException, "Unclosed array.\n");
            }

            exprStack.push(std::make_unique<ArrayExpression>(std::move(elements)));
            i = j;
        }
        else if (IsOperator(token)) {
            if (exprStack.size() < 2) {
                THROW_EXCEPTION(InvalidConfigException, "Insufficient operands for operator: " + token + ".\n");
            }

            auto right = std::move(exprStack.top());
            exprStack.pop();
            auto left = std::move(exprStack.top());
            exprStack.pop();

            Operator op = GetOperator(token);
            exprStack.push(std::make_unique<BinaryExpression>(std::move(left), std::move(right), op));
        }
        else if (token == "-" && exprStack.size() == 1) {
            auto operand = std::move(exprStack.top());
            exprStack.pop();
            exprStack.push(std::make_unique<UnaryExpression>(UnaryOperator::NEGATE, std::move(operand)));
        }
        else if (token == "!" && exprStack.size() >= 1) {
            auto operand = std::move(exprStack.top());
            exprStack.pop();
            exprStack.push(std::make_unique<UnaryExpression>(UnaryOperator::LOGICAL_NOT, std::move(operand)));
        }
        else {
            exprStack.push(ParseOperand(token));
        }
    }

    if (exprStack.size() != 1) {
        THROW_EXCEPTION(InvalidConfigException, "Invalid expression.\n");
    }

    return std::move(exprStack.top());
}

bool Condition::IsOperator(const std::string& token) {
    return token == "+" || token == "-" || token == "*" || token == "/" ||
        token == "%" || token == "^" || token == "==" || token == "!=" ||
        token == ">" || token == ">=" || token == "<" || token == "<=" ||
        token == "in" || token == "&&" || token == "||";
}

bool Condition::HigherPrecedence(const std::string& op1, const std::string& op2) {
    int prec1 = GetPrecedence(op1);
    int prec2 = GetPrecedence(op2);

    if (prec1 == prec2) {
        return !RightAssociative(op1);
    }
    return prec1 > prec2;
}

int Condition::GetPrecedence(const std::string& op) {
    if (op == "!" || op == "negate") return 8;
    if (op == "^") return 7;
    if (op == "*" || op == "/" || op == "%") return 6;
    if (op == "+" || op == "-") return 5;
    if (op == "in") return 4;
    if (op == "==" || op == "!=" || op == ">" || op == ">=" || op == "<" || op == "<=") return 4;
    if (op == "&&") return 3;
    if (op == "||") return 2;
    return 0;
}

bool Condition::RightAssociative(const std::string& op) {
    return op == "^" || op == "!";
}

Operator Condition::GetOperator(const std::string& token) {
    if (token == "==") return Operator::EQUAL;
    else if (token == "!=") return Operator::NOT_EQUAL;
    else if (token == ">") return Operator::GREATER;
    else if (token == ">=") return Operator::GREATER_EQUAL;
    else if (token == "<") return Operator::LESS;
    else if (token == "<=") return Operator::LESS_EQUAL;
    else if (token == "in") return Operator::INCLUDE;
    else if (token == "+") return Operator::ADD;
    else if (token == "-") return Operator::SUBTRACT;
    else if (token == "*") return Operator::MULTIPLY;
    else if (token == "/") return Operator::DIVIDE;
    else if (token == "%") return Operator::MODULO;
    else if (token == "^") return Operator::EXPONENT;
    else if (token == "&&") return Operator::LOGICAL_AND;
    else if (token == "||") return Operator::LOGICAL_OR;
    else throw std::runtime_error("Unknown operator: " + token);
}

std::shared_ptr<Expression> Condition::ParseOperand(const std::string& token) {
    // 变量（支持中文变量名）
    if (token.length() >= 2 && token.substr(0, 2) == "$$") {
        std::string varName = token.substr(2);
        // 检查变量名是否合法（支持中文字符）
        if (!varName.empty() && (std::isalpha(static_cast<unsigned char>(varName[0])) ||
            varName[0] == '_' ||
            (varName[0] & 0x80))) {
            return std::make_unique<VariableExpression>(varName);
        }
    }

    // 常量
    return ParseConstant(token);
}

std::shared_ptr<Expression> Condition::ParseConstant(const std::string& token) {
    if (token == "true") {
        return std::make_unique<ConstantExpression>(true);
    }
    else if (token == "false") {
        return std::make_unique<ConstantExpression>(false);
    }

    // 尝试解析为整数
    try {
        size_t pos;
        int int_val = std::stoi(token, &pos);
        if (pos == token.length()) {
            return std::make_unique<ConstantExpression>(int_val);
        }
    }
    catch (...) {}

    // 尝试解析为浮点数
    try {
        size_t pos;
        double double_val = std::stod(token, &pos);
        if (pos == token.length()) {
            return std::make_unique<ConstantExpression>(double_val);
        }
    }
    catch (...) {}

    // 处理字符串（支持中文字符串）
    std::string str_val = token;
    if (str_val.length() >= 2 &&
        ((str_val.front() == '"' && str_val.back() == '"') ||
            (str_val.front() == '\'' && str_val.back() == '\''))) {
        str_val = str_val.substr(1, str_val.length() - 2);
    }

    // 检查是否是纯数字字符串（如果是，保持为字符串类型）
    bool is_all_digit = !str_val.empty();
    for (char c : str_val) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            is_all_digit = false;
            break;
        }
    }

    if (is_all_digit && str_val.length() > 1 && str_val[0] == '0') {
        // 以0开头的多位数数字，保持为字符串
        return std::make_unique<ConstantExpression>(str_val);
    }

    // 其他情况都作为字符串处理（支持中文）
    return std::make_unique<ConstantExpression>(str_val);
}

string ToString(const ValueType& value) {
    return visit([](const auto& v) -> string {
        using T = decay_t<decltype(v)>;

        if constexpr (is_same_v<T, int>) {
            return to_string(v);
        }
        else if constexpr (is_same_v<T, double>) {
            string str = to_string(v);
            size_t dot_pos = str.find('.');
            if (dot_pos != string::npos) {
                size_t last_non_zero = str.find_last_not_of('0');
                if (last_non_zero != string::npos && last_non_zero > dot_pos) {
                    if (str[last_non_zero] == '.') {
                        str = str.substr(0, last_non_zero);
                    }
                    else {
                        str = str.substr(0, last_non_zero + 1);
                    }
                }
            }
            return str;
        }
        else if constexpr (is_same_v<T, bool>) {
            return v ? "true" : "false";
        }
        else if constexpr (is_same_v<T, string>) {
            return v;
        }
        else {
            return "unknown_type";
        }
        }, value);
}

ValueType FromString(const std::string& s) {
    if (s.empty()) {
        return std::string("");
    }

    if (s == "true") {
        return true;
    }

    if (s == "false") {
        return false;
    }

    // 处理带引号的字符串（支持中文）
    if (s.length() >= 2 &&
        ((s.front() == '"' && s.back() == '"') ||
            (s.front() == '\'' && s.back() == '\''))) {
        return s.substr(1, s.length() - 2);
    }

    // 检查是否包含非ASCII字符，如果有则直接返回字符串
    for (char c : s) {
        if (static_cast<unsigned char>(c) > 127) {
            return s;
        }
    }

    try {
        size_t pos;
        int int_val = std::stoi(s, &pos);
        if (pos == s.length()) {
            return int_val;
        }
    }
    catch (const std::exception&) {}

    // 尝试解析为浮点数
    try {
        size_t pos;
        double double_val = std::stod(s, &pos);
        if (pos == s.length()) {
            return double_val;
        }
    }
    catch (const std::exception&) {}

    bool is_all_digit = !s.empty();
    for (char c : s) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            is_all_digit = false;
            break;
        }
    }

    if (is_all_digit) {
        if ((s.length() > 1 && s[0] == '0') || s.length() > 10) {
            return s;
        }
        try {
            return std::stoi(s);
        }
        catch (...) {
            return s;
        }
    }

    if (!s.empty() && std::isdigit(static_cast<unsigned char>(s[0]))) {
        bool has_non_digit = false;
        bool has_dot = false;
        bool has_exponent = false;

        for (size_t i = 0; i < s.length(); ++i) {
            char c = s[i];
            if (!std::isdigit(static_cast<unsigned char>(c))) {
                if (c == '.' && !has_dot) {
                    has_dot = true;
                }
                else if ((c == 'e' || c == 'E') && !has_exponent) {
                    has_exponent = true;
                    if (i + 1 < s.length() && (s[i + 1] == '+' || s[i + 1] == '-')) {
                        i++;
                    }
                }
                else {
                    has_non_digit = true;
                    break;
                }
            }
        }

        if (!has_non_digit && (has_dot || has_exponent)) {
            try {
                return std::stod(s);
            }
            catch (...) {}
        }
    }

    return s;
}


