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

VariableExpression::VariableExpression(const string& n) : name(n) {

}

ValueType VariableExpression::Evaluate(function<pair<bool, ValueType>(const string&)> getValue) const {
	std::vector<std::function<std::pair<bool, ValueType>(const std::string&)>> getValues = { getValue };
    return Evaluate(getValues);
}

ValueType VariableExpression::Evaluate(std::vector<std::function<std::pair<bool, ValueType>(const std::string&)>> getValues) const {
    for(auto getValue : getValues) {
        auto value = getValue(name);
        if (value.first) {
            return value.second;
        }
	}
    return 0;
}

ConstantExpression::ConstantExpression(ValueType v) : value(v) {

}

ValueType ConstantExpression::Evaluate(function<pair<bool, ValueType>(const string&)> getValue) const {
    return value;
}

ValueType ConstantExpression::Evaluate(std::vector<std::function<std::pair<bool, ValueType>(const std::string&)>> getValues) const {
    return value;
}

ArrayExpression::ArrayExpression(vector<shared_ptr<Expression>> es)
    : elements(move(es)) {
}

ValueType ArrayExpression::Evaluate(function<pair<bool, ValueType>(const string&)> getValue) const {
    std::vector<std::function<std::pair<bool, ValueType>(const std::string&)>> getValues = { getValue };
    return Evaluate(getValues);
}

ValueType ArrayExpression::Evaluate(std::vector<std::function<std::pair<bool, ValueType>(const std::string&)>> getValues) const {
    string result = "[";
    for (size_t i = 0; i < elements.size(); ++i) {
        auto value = elements[i]->Evaluate(getValues);
        result += ToString(value);
        if (i < elements.size() - 1) {
            result += ", ";
        }
    }
    result += "]";
    return result;
}

vector<ValueType> ArrayExpression::GetElementValues(function<pair<bool, ValueType>(const string&)> getValue) const {
    std::vector<std::function<std::pair<bool, ValueType>(const std::string&)>> getValues = { getValue };
    return GetElementValues(getValues);
}

std::vector<ValueType> ArrayExpression::GetElementValues(std::vector<std::function<std::pair<bool, ValueType>(const std::string&)>> getValues) const {
    vector<ValueType> values;
    for (const auto& element : elements) {
        values.push_back(element->Evaluate(getValues));
    }
    return values;
}

UnaryExpression::UnaryExpression(UnaryOperator op, shared_ptr<Expression> operand)
    : operand(op), expression(move(operand)) {

}

ValueType UnaryExpression::Evaluate(function<pair<bool, ValueType>(const string&)> getValue) const {
    std::vector<std::function<std::pair<bool, ValueType>(const std::string&)>> getValues = { getValue };
    return Evaluate(getValues);
}

ValueType UnaryExpression::Evaluate(std::vector<std::function<std::pair<bool, ValueType>(const std::string&)>> getValues) const {
    auto value = expression->Evaluate(getValues);

    switch (operand) {
    case UnaryOperator::NEGATE:
        return ApplyNegate(value);
    case UnaryOperator::LOGICAL_NOT:
        return ApplyLogicalNot(value);
    default:
        THROW_EXCEPTION(InvalidConfigException, "Unknown unary operator.\n");
    }
}

ValueType UnaryExpression::ApplyNegate(const ValueType& value) const {
    return visit([](const auto& v) -> ValueType {
        using T = decay_t<decltype(v)>;
        if constexpr (is_arithmetic_v<T> && !is_same_v<T, bool>) {
            return -v;
        }
        else {
            THROW_EXCEPTION(InvalidConfigException, "Cannot apply negate to boolean or non-numeric type.\n");
        }
        }, value);
}

ValueType UnaryExpression::ApplyLogicalNot(const ValueType& value) const {
    bool bool_val = ConvertToBool(value);
    return !bool_val;
}

bool UnaryExpression::ConvertToBool(const ValueType& value) const {
    return visit([](const auto& v) -> bool {
        using T = decay_t<decltype(v)>;
        if constexpr (is_same_v<T, bool>) {
            return v;
        }
        else if constexpr (is_same_v<T, int>) {
            return v != 0;
        }
        else if constexpr (is_same_v<T, double>) {
            return abs(v) > 1e-10;
        }
        else if constexpr (is_same_v<T, string>) {
            return !v.empty();
        }
        else {
            return false;
        }
        }, value);
}

BinaryExpression::BinaryExpression(shared_ptr<Expression> l,
    shared_ptr<Expression> r,
    BinaryOperator op)
    : left(move(l)), right(move(r)), operand(op) {
}

ValueType BinaryExpression::Evaluate(function<pair<bool, ValueType>(const string&)> getValue) const {
    std::vector<std::function<std::pair<bool, ValueType>(const std::string&)>> getValues = { getValue };
    return Evaluate(getValues);
}

ValueType BinaryExpression::Evaluate(std::vector<std::function<std::pair<bool, ValueType>(const std::string&)>> getValues) const {
    if (operand == BinaryOperator::INCLUDE) {
        auto array_expr = dynamic_cast<ArrayExpression*>(right.get());
        if (!array_expr) {
            THROW_EXCEPTION(InvalidConfigException, "Right operand of 'in' must be an array.\n");
        }

        auto array_values = array_expr->GetElementValues(getValues);
        bool found = false;

        auto left_val = left->Evaluate(getValues);
        for (const auto& array_val : array_values) {
            if (GetComparisonResult(left_val, array_val, BinaryOperator::EQUAL)) {
                found = true;
                break;
            }
        }

        return found;
    }

    if (operand == BinaryOperator::LOGICAL_AND) {
        bool left_val = ConvertToBool(left->Evaluate(getValues));
        if (!left_val) return false;
        bool right_val = ConvertToBool(right->Evaluate(getValues));
        return right_val;
    }

    if (operand == BinaryOperator::LOGICAL_OR) {
        bool left_val = ConvertToBool(left->Evaluate(getValues));
        if (left_val) return true;
        bool right_val = ConvertToBool(right->Evaluate(getValues));
        return right_val;
    }

    auto left_val = left->Evaluate(getValues);
    auto right_val = right->Evaluate(getValues);

    switch (operand) {
    case BinaryOperator::EQUAL:
    case BinaryOperator::NOT_EQUAL:
    case BinaryOperator::GREATER:
    case BinaryOperator::GREATER_EQUAL:
    case BinaryOperator::LESS:
    case BinaryOperator::LESS_EQUAL:
        return CompareValues(left_val, right_val, operand);

    case BinaryOperator::ADD:
    case BinaryOperator::SUBTRACT:
    case BinaryOperator::MULTIPLY:
    case BinaryOperator::DIVIDE:
    case BinaryOperator::MODULO:
    case BinaryOperator::EXPONENT:
        return ComputeArithmetic(left_val, right_val, operand);

    default:
        THROW_EXCEPTION(InvalidConfigException, "Unknown operator.\n");
    }
}

bool BinaryExpression::GetComparisonResult(const ValueType& left, const ValueType& right, BinaryOperator op) const {
    ValueType result = CompareValues(left, right, op);
    if (auto bool_result = get_if<bool>(&result)) {
        return *bool_result;
    }
    THROW_EXCEPTION(InvalidConfigException, "Comparison must return boolean value.\n");
}

ValueType BinaryExpression::CompareValues(const ValueType& left, const ValueType& right, BinaryOperator op) const {
    bool result = visit([op](const auto& l, const auto& r) -> bool {
        using T1 = decay_t<decltype(l)>;
        using T2 = decay_t<decltype(r)>;

        if constexpr (is_same_v<T1, T2>) {
            return CompareSameType(l, r, op);
        }
        else {
            return CompareDifferentType(l, r, op);
        }
        }, left, right);

    return result;
}

ValueType BinaryExpression::ComputeArithmetic(const ValueType& left, const ValueType& right, BinaryOperator op) const {
    if (op == BinaryOperator::ADD) {
        // 如果至少有一个操作数是字符串，进行字符串连接
        if (holds_alternative<string>(left) || holds_alternative<string>(right)) {
            string left_str = visit([](const auto& v) -> string {
                using T = decay_t<decltype(v)>;
                if constexpr (is_same_v<T, string>) {
                    return v;
                }
                else if constexpr (is_same_v<T, bool>) {
                    return v ? "true" : "false";
                }
                else if constexpr (is_arithmetic_v<T>) {
                    return to_string(v);
                }
                else {
                    THROW_EXCEPTION(ArithmeticException, "Unsupported type for string concatenation.\n");
                }
                }, left);

            string right_str = visit([](const auto& v) -> string {
                using T = decay_t<decltype(v)>;
                if constexpr (is_same_v<T, string>) {
                    return v;
                }
                else if constexpr (is_same_v<T, bool>) {
                    return v ? "true" : "false";
                }
                else if constexpr (is_arithmetic_v<T>) {
                    return to_string(v);
                }
                else {
                    THROW_EXCEPTION(ArithmeticException, "Unsupported type for string concatenation.\n");
                }
                }, right);

            return left_str + right_str;
        }
    }

    return visit([op](const auto& l, const auto& r) -> ValueType {
        using T1 = decay_t<decltype(l)>;
        using T2 = decay_t<decltype(r)>;

        // 如果两个操作数都是整数，进行整数运算
        if constexpr (is_same_v<T1, int> && is_same_v<T2, int>) {
            switch (op) {
            case BinaryOperator::ADD: return l + r;
            case BinaryOperator::SUBTRACT: return l - r;
            case BinaryOperator::MULTIPLY: return l * r;
            case BinaryOperator::DIVIDE:
                if (r == 0) {
                    THROW_EXCEPTION(ArithmeticException, "Division by zero.\n");
                }
                return l / r;
            case BinaryOperator::MODULO:
                if (r == 0) {
                    THROW_EXCEPTION(ArithmeticException, "Modulo by zero.\n");
                }
                return l % r;
            case BinaryOperator::EXPONENT: {
                if (r < 0) {
                    double result = pow(static_cast<double>(l), static_cast<double>(r));
                    return result;
                }
                int result = 1;
                for (int i = 0; i < r; ++i) {
                    result *= l;
                }
                return result;
            }
            default:
                THROW_EXCEPTION(ArithmeticException, "Unsupported arithmetic operator.\n");
            }
        }
        // 如果至少有一个操作数是浮点数，进行浮点数运算
        else if constexpr ((is_arithmetic_v<T1> && is_arithmetic_v<T2>) &&
            !(is_same_v<T1, bool> || is_same_v<T2, bool>)) {
            double l_val = static_cast<double>(l);
            double r_val = static_cast<double>(r);

            switch (op) {
            case BinaryOperator::ADD: return l_val + r_val;
            case BinaryOperator::SUBTRACT: return l_val - r_val;
            case BinaryOperator::MULTIPLY: return l_val * r_val;
            case BinaryOperator::DIVIDE:
                if (abs(r_val) < 1e-10) {
                    THROW_EXCEPTION(ArithmeticException, "Division by zero.\n");
                }
                return l_val / r_val;
            case BinaryOperator::MODULO:
                if (abs(r_val) < 1e-10) {
                    THROW_EXCEPTION(ArithmeticException, "Modulo by zero.\n");
                }
                return fmod(l_val, r_val);
            case BinaryOperator::EXPONENT:
                return pow(l_val, r_val);
            default:
                THROW_EXCEPTION(ArithmeticException, "Unsupported arithmetic operator.\n");
            }
        }
        else {
            THROW_EXCEPTION(ArithmeticException, "Arithmetic operations only supported for numeric types.\n");
        }
        }, left, right);
}

bool BinaryExpression::ConvertToBool(const ValueType& value) const {
    return visit([](const auto& v) -> bool {
        using T = decay_t<decltype(v)>;
        if constexpr (is_same_v<T, bool>) {
            return v;
        }
        else if constexpr (is_same_v<T, int>) {
            return v != 0;
        }
        else if constexpr (is_same_v<T, double>) {
            return abs(v) > 1e-10;
        }
        else if constexpr (is_same_v<T, string>) {
            return !v.empty();
        }
        else {
            return false;
        }
        }, value);
}

bool Condition::ParseCondition(const string& conditionStr) {
    try {
        vector<string> tokens = Tokenize(conditionStr);
        if (tokens.size() > 0)root = ParseExpression(tokens);
        else root = nullptr;
        return true;
    }
    catch (const exception& e) {
        cerr << "Parse error: " << e.what() << endl;
        return false;
    }
}

bool Condition::EvaluateBool(function<pair<bool, ValueType>(const string&)> getValue) const {
    std::vector<std::function<std::pair<bool, ValueType>(const std::string&)>> getValues = { getValue };
    return EvaluateBool(getValues);
}

bool Condition::EvaluateBool(std::vector<std::function<std::pair<bool, ValueType>(const std::string&)>> getValues) const {
    if (!root) {
        return true;
    }

    auto result = root->Evaluate(getValues);
    if (auto bool_val = get_if<bool>(&result)) {
        return *bool_val;
    }
    THROW_EXCEPTION(InvalidConfigException, "Condition must Evaluate to boolean.\n");
}

ValueType Condition::EvaluateValue(function<pair<bool, ValueType>(const string&)> getValue) const {
    std::vector<std::function<std::pair<bool, ValueType>(const std::string&)>> getValues = { getValue };
    return EvaluateValue(getValues);
}

ValueType Condition::EvaluateValue(std::vector<std::function<std::pair<bool, ValueType>(const std::string&)>> getValues) const {
    if (!root) {
        return 0;
    }
    return root->Evaluate(getValues);
}

vector<string> Condition::Tokenize(const string& expr) {
    vector<string> tokens;
    string current;

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

            if ((c == '&' && i + 1 < expr.length() && expr[i + 1] == '&') ||
                (c == '|' && i + 1 < expr.length() && expr[i + 1] == '|') ||
                (c == '=' && i + 1 < expr.length() && expr[i + 1] == '=') ||
                (c == '!' && i + 1 < expr.length() && expr[i + 1] == '=') ||
                (c == '<' && i + 1 < expr.length() && expr[i + 1] == '=') ||
                (c == '>' && i + 1 < expr.length() && expr[i + 1] == '=')) {
                tokens.push_back(string(1, c) + string(1, expr[i + 1]));
                i++;
            }
            else {
                tokens.push_back(string(1, c));
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

shared_ptr<Expression> Condition::ParseExpression(const vector<string>& tokens) {
    vector<string> postfix = InfixToPostfix(tokens);
    return ParsePostfix(postfix);
}

vector<string> Condition::InfixToPostfix(const vector<string>& infix) {
    vector<string> postfix;
    stack<string> opStack;

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

shared_ptr<Expression> Condition::ParsePostfix(const vector<string>& postfix) {
    stack<shared_ptr<Expression>> exprStack;

    for (size_t i = 0; i < postfix.size(); ++i) {
        const auto& token = postfix[i];

        if (token == "[") {
            vector<shared_ptr<Expression>> elements;
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

            exprStack.push(make_unique<ArrayExpression>(move(elements)));
            i = j;
        }
        else if (IsOperator(token)) {
            if (exprStack.size() < 2) {
                THROW_EXCEPTION(InvalidConfigException, "Insufficient operands for operator: " + token + ".\n");
            }

            auto right = move(exprStack.top());
            exprStack.pop();
            auto left = move(exprStack.top());
            exprStack.pop();

            BinaryOperator op = GetOperator(token);
            exprStack.push(make_unique<BinaryExpression>(move(left), move(right), op));
        }
        else if (token == "-" && exprStack.size() == 1) {
            auto operand = move(exprStack.top());
            exprStack.pop();
            exprStack.push(make_unique<UnaryExpression>(UnaryOperator::NEGATE, move(operand)));
        }
        else if (token == "!" && exprStack.size() >= 1) {
            auto operand = move(exprStack.top());
            exprStack.pop();
            exprStack.push(make_unique<UnaryExpression>(UnaryOperator::LOGICAL_NOT, move(operand)));
        }
        else {
            exprStack.push(ParseOperand(token));
        }
    }

    if (exprStack.size() != 1) {
        THROW_EXCEPTION(InvalidConfigException, "Invalid expression.\n");
    }

    return move(exprStack.top());
}

bool Condition::IsOperator(const string& token) {
    return token == "+" || token == "-" || token == "*" || token == "/" ||
        token == "%" || token == "^" || token == "==" || token == "!=" ||
        token == ">" || token == ">=" || token == "<" || token == "<=" ||
        token == "in" || token == "&&" || token == "||";
}

bool Condition::HigherPrecedence(const string& op1, const string& op2) {
    int prec1 = GetPrecedence(op1);
    int prec2 = GetPrecedence(op2);

    if (prec1 == prec2) {
        return !RightAssociative(op1);
    }
    return prec1 > prec2;
}

int Condition::GetPrecedence(const string& op) {
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

bool Condition::RightAssociative(const string& op) {
    return op == "^" || op == "!";
}

BinaryOperator Condition::GetOperator(const string& token) {
    if (token == "==") return BinaryOperator::EQUAL;
    else if (token == "!=") return BinaryOperator::NOT_EQUAL;
    else if (token == ">") return BinaryOperator::GREATER;
    else if (token == ">=") return BinaryOperator::GREATER_EQUAL;
    else if (token == "<") return BinaryOperator::LESS;
    else if (token == "<=") return BinaryOperator::LESS_EQUAL;
    else if (token == "in") return BinaryOperator::INCLUDE;
    else if (token == "+") return BinaryOperator::ADD;
    else if (token == "-") return BinaryOperator::SUBTRACT;
    else if (token == "*") return BinaryOperator::MULTIPLY;
    else if (token == "/") return BinaryOperator::DIVIDE;
    else if (token == "%") return BinaryOperator::MODULO;
    else if (token == "^") return BinaryOperator::EXPONENT;
    else if (token == "&&") return BinaryOperator::LOGICAL_AND;
    else if (token == "||") return BinaryOperator::LOGICAL_OR;
    else throw runtime_error("Unknown operator: " + token);
}

shared_ptr<Expression> Condition::ParseOperand(const string& token) {
    if (token.length() >= 2 && token.substr(0, 2) == "$$") {
        string varName = token.substr(2);
        if (!varName.empty() && (isalpha(static_cast<unsigned char>(varName[0])) ||
            varName[0] == '_' ||
            (varName[0] & 0x80))) {
            return make_unique<VariableExpression>(varName);
        }
    }

    return ParseConstant(token);
}

shared_ptr<Expression> Condition::ParseConstant(const string& token) {
    if (token == "true") {
        return make_unique<ConstantExpression>(true);
    }
    else if (token == "false") {
        return make_unique<ConstantExpression>(false);
    }

    // 尝试解析为整数
    try {
        size_t pos;
        int int_val = stoi(token, &pos);
        if (pos == token.length() && !(token.length() > 1 && token[0] == '0' && isdigit(token[1]))) {
            return make_unique<ConstantExpression>(int_val);
        }
    }
    catch (const exception&) {}

    // 尝试解析为浮点数
    try {
        size_t pos;
        double double_val = stod(token, &pos);
        if (pos == token.length()) {
            if (token.find('.') != string::npos ||
                token.find('e') != string::npos ||
                token.find('E') != string::npos) {
                return make_unique<ConstantExpression>(double_val);
            }
            else {
                return make_unique<ConstantExpression>(static_cast<int>(double_val));
            }
        }
    }
    catch (const exception&) {}

    // 处理字符串
    string str_val = token;
    if (str_val.length() >= 2 &&
        ((str_val.front() == '"' && str_val.back() == '"') ||
            (str_val.front() == '\'' && str_val.back() == '\''))) {
        str_val = str_val.substr(1, str_val.length() - 2);
    }

    return make_unique<ConstantExpression>(str_val);
}

bool IsOperatorChar(char c) {
    return c == '=' || c == '!' || c == '>' || c == '<' ||
        c == '+' || c == '-' || c == '*' || c == '/' ||
        c == '%' || c == '^' || c == '&' || c == '|';
}

bool IsSpaceChar(char c) {
    return isspace(static_cast<unsigned char>(c)) || c == '　';
}

bool IsIdentifierChar(char c) {
    return isalnum(static_cast<unsigned char>(c)) || c == '_' || (c >= 0x80 && c <= 0xFF);
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

ValueType FromString(const string& s) {
    if (s.empty()) {
        return string("");
    }

    if (s == "true") {
        return true;
    }

    if (s == "false") {
        return false;
    }

    // 处理带引号的字符串
    if (s.length() >= 2 &&
        ((s.front() == '"' && s.back() == '"') ||
            (s.front() == '\'' && s.back() == '\''))) {
        return s.substr(1, s.length() - 2);
    }

    // 非ASCII字符直接返回字符串
    for (char c : s) {
        if (static_cast<unsigned char>(c) > 127) {
            return s;
        }
    }

    // 尝试解析为整数
    try {
        size_t pos;
        int int_val = stoi(s, &pos);
        if (pos == s.length()) {
            return int_val;
        }
    }
    catch (const exception&) {}

    // 尝试解析为浮点数
    try {
        size_t pos;
        double double_val = stod(s, &pos);
        if (pos == s.length()) {
            return double_val;
        }
    }
    catch (const exception&) {}

    bool is_all_digit = !s.empty();
    for (char c : s) {
        if (!isdigit(static_cast<unsigned char>(c))) {
            is_all_digit = false;
            break;
        }
    }

    if (is_all_digit) {
        if ((s.length() > 1 && s[0] == '0') || s.length() > 10) {
            return s;
        }
        try {
            return stoi(s);
        }
        catch (const exception&) {
            return s;
        }
    }

    if (!s.empty() && isdigit(static_cast<unsigned char>(s[0]))) {
        bool has_non_digit = false;
        bool has_dot = false;
        bool has_exponent = false;

        for (size_t i = 0; i < s.length(); ++i) {
            char c = s[i];
            if (!isdigit(static_cast<unsigned char>(c))) {
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
                return stod(s);
            }
            catch (const exception&) {}
        }
    }

    return s;
}

