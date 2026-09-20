#include "expression.h"

#include "event.h"

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

ExpressionNode::ExpressionNode() {

}

ExpressionNode::~ExpressionNode() {

}

VariableExpression::VariableExpression(const string& expression) :
	name(expression) {

}

VariableExpression::~VariableExpression() {

}

ValueType VariableExpression::Evaluate(const ScriptContext& context) const {
	// 点号本身不参与分词、也不做任何字符替换，这里只找第一个'.'或'_'切出前缀关键字，
	// 两种分隔符等价；subkey剩余部分原样保留（如"job.title"不会变成"job_title"）。
	size_t split = name.find_first_of("._");
	if (split == string::npos) {
		return 0;
	}
	string prefix = name.substr(0, split);
	string subkey = name.substr(split + 1);

	if (prefix == "self") {
		if (context.self) {
			auto value = context.self->GetValue(subkey);
			if (value.first) return value.second;
		}
	}
	else if (prefix == "system") {
		if (context.system) {
			auto value = context.system->GetValue(subkey);
			if (value.first) return value.second;
		}
	}
	else if (prefix == "local") {
		if (context.local) {
			auto value = context.local->GetLocalValue(subkey);
			if (value.first) return value.second;
		}
	}

	return 0;
}

ConstantExpression::ConstantExpression(ValueType expression) : value(expression) {

}

ConstantExpression::~ConstantExpression() {

}

ValueType ConstantExpression::Evaluate(const ScriptContext& context) const {
	return value;
}

ArrayExpression::ArrayExpression(vector<shared_ptr<ExpressionNode>> expression)
	: elements(move(expression)) {

}

ArrayExpression::~ArrayExpression() {

}

ValueType ArrayExpression::Evaluate(const ScriptContext& context) const {
	string result = "[";
	for (size_t i = 0; i < elements.size(); i++) {
		auto value = elements[i]->Evaluate(context);
		result += ToString(value);
		if (i < elements.size() - 1) {
			result += ", ";
		}
	}
	result += "]";
	return result;
}

vector<ValueType> ArrayExpression::GetElementValues(const ScriptContext& context) const {
	vector<ValueType> values;
	for (const auto& element : elements) {
		values.push_back(element->Evaluate(context));
	}
	return values;
}

UnaryExpression::UnaryExpression(UnaryOperator op, shared_ptr<ExpressionNode> operand)
	: operand(op), expression(move(operand)) {

}

UnaryExpression::~UnaryExpression() {

}

ValueType UnaryExpression::Evaluate(const ScriptContext& context) const {
	auto value = expression->Evaluate(context);

	switch (operand) {
	case UnaryOperator::NEGATE:
		return ApplyNegate(value);
	case UnaryOperator::LOGICAL_NOT:
		return ApplyLogicalNot(value);
	default:
		THROW_EXCEPTION(InvalidArgumentException, "Unknown unary operator.\n");
	}
}

ValueType UnaryExpression::ApplyNegate(const ValueType& value) const {
	return visit([](const auto& v) -> ValueType {
		using T = decay_t<decltype(v)>;
		if constexpr (is_arithmetic_v<T> && !is_same_v<T, bool>) {
			return -v;
		}
		else {
			THROW_EXCEPTION(RuntimeException, "Cannot apply negate to boolean or non-numeric type.\n");
		}
		}, value);
}

ValueType UnaryExpression::ApplyLogicalNot(const ValueType& value) const {
	bool v = ConvertToBool(value);
	return !v;
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

BinaryExpression::BinaryExpression(shared_ptr<ExpressionNode> left,
	shared_ptr<ExpressionNode> right,
	BinaryOperator op)
	: left(move(left)), right(move(right)), operand(op) {
}

BinaryExpression::~BinaryExpression() {

}

ValueType BinaryExpression::Evaluate(const ScriptContext& context) const {
	if (operand == BinaryOperator::INCLUDE) {
		auto array_expr = dynamic_cast<ArrayExpression*>(right.get());
		if (!array_expr) {
			THROW_EXCEPTION(RuntimeException, "Right operand of 'in' must be an array.\n");
		}

		auto array_values = array_expr->GetElementValues(context);
		bool found = false;

		auto left_val = left->Evaluate(context);
		for (const auto& array_val : array_values) {
			if (GetComparisonResult(left_val, array_val, BinaryOperator::EQUAL)) {
				found = true;
				break;
			}
		}

		return found;
	}

	if (operand == BinaryOperator::LOGICAL_AND) {
		bool left_val = ConvertToBool(left->Evaluate(context));
		if (!left_val) return false;
		bool right_val = ConvertToBool(right->Evaluate(context));
		return right_val;
	}

	if (operand == BinaryOperator::LOGICAL_OR) {
		bool left_val = ConvertToBool(left->Evaluate(context));
		if (left_val) return true;
		bool right_val = ConvertToBool(right->Evaluate(context));
		return right_val;
	}

	auto left_val = left->Evaluate(context);
	auto right_val = right->Evaluate(context);

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
		THROW_EXCEPTION(RuntimeException, "Unknown binary operator.\n");
	}
}

bool BinaryExpression::GetComparisonResult(const ValueType& left, const ValueType& right, BinaryOperator op) const {
	ValueType result = CompareValues(left, right, op);
	if (auto bool_result = get_if<bool>(&result)) {
		return *bool_result;
	}
	THROW_EXCEPTION(RuntimeException, "Comparison must return boolean value.\n");
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
					THROW_EXCEPTION(RuntimeException, "Unsupported type for string concatenation.\n");
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
					THROW_EXCEPTION(RuntimeException, "Unsupported type for string concatenation.\n");
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
					THROW_EXCEPTION(RuntimeException, "Division by zero.\n");
				}
				return l / r;
			case BinaryOperator::MODULO:
				if (r == 0) {
					THROW_EXCEPTION(RuntimeException, "Modulo by zero.\n");
				}
				return l % r;
			case BinaryOperator::EXPONENT: {
				if (r < 0) {
					double result = pow(static_cast<double>(l), static_cast<double>(r));
					return result;
				}
				int result = 1;
				for (int i = 0; i < r; i++) {
					result *= l;
				}
				return result;
			}
			default:
				THROW_EXCEPTION(RuntimeException, "Unsupported arithmetic operator.\n");
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
					THROW_EXCEPTION(RuntimeException, "Division by zero.\n");
				}
				return l_val / r_val;
			case BinaryOperator::MODULO:
				if (abs(r_val) < 1e-10) {
					THROW_EXCEPTION(RuntimeException, "Modulo by zero.\n");
				}
				return fmod(l_val, r_val);
			case BinaryOperator::EXPONENT:
				return pow(l_val, r_val);
			default:
				THROW_EXCEPTION(RuntimeException, "Unsupported arithmetic operator.\n");
			}
		}
		else {
			THROW_EXCEPTION(RuntimeException, "Arithmetic operations only supported for numeric types.\n");
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

bool Expression::Parse(const string& expr) {
	try {
		vector<string> tokens = Tokenize(expr);
		if (tokens.size() > 0) root = ParseExpression(tokens);
		else root = nullptr;
		return true;
	}
	catch (const exception& e) {
		cerr << "Parse error: " << e.what() << endl;
		return false;
	}
}

bool Expression::EvaluateBool(const ScriptContext& context) const {
	if (!root) {
		return true;
	}

	auto result = root->Evaluate(context);
	if (auto bool_val = get_if<bool>(&result)) {
		return *bool_val;
	}
	THROW_EXCEPTION(RuntimeException, "Expression must Evaluate to boolean.\n");
}

ValueType Expression::EvaluateValue(const ScriptContext& context) const {
	if (!root) {
		return string("");
	}
	return root->Evaluate(context);
}

ValueType EvaluateExpression(const string& source, const ScriptContext& context) {
	Expression expression;
	expression.Parse(source);
	return expression.EvaluateValue(context);
}

bool EvaluateExpressionBool(const string& source, const ScriptContext& context) {
	Expression expression;
	expression.Parse(source);
	return expression.EvaluateBool(context);
}

static size_t FindMatchingQuote(const string& expr, size_t start) {
	char quote = expr[start];
	if (quote != '"' && quote != '\'')
		return string::npos;
	for (size_t i = start + 1; i < expr.length(); i++) {
		if (expr[i] == '\\') {
			i++;
			continue;
		}
		if (expr[i] == quote)
			return i;
	}
	return string::npos;
}

vector<string> Expression::Tokenize(const string& expr) {
	vector<string> tokens;
	string current;
	size_t i = 0;
	while (i < expr.length()) {
		char c = expr[i];

		if (IsSpaceChar(c)) {
			if (!current.empty()) {
				tokens.push_back(current);
				current.clear();
			}
			++i;
		}
		else if (c == '"' || c == '\'') {
			if (!current.empty()) {
				tokens.push_back(current);
				current.clear();
			}
			size_t end = FindMatchingQuote(expr, i);
			if (end == string::npos) {
				THROW_EXCEPTION(RuntimeException, "Unmatched quote in expression");
			}
			string quoted = expr.substr(i, end - i + 1);
			tokens.push_back(quoted);
			i = end + 1;
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
				i += 2;
			}
			else {
				tokens.push_back(string(1, c));
				++i;
			}
		}
		else {
			if (c == '$' && !current.empty() && current.back() != '$') {
				tokens.push_back(current);
				current.clear();
			}
			current += c;
			++i;
		}
	}

	if (!current.empty()) {
		tokens.push_back(current);
	}
	return tokens;
}

bool Expression::OperatorChar(char c) {
	return IsOperatorChar(c);
}

shared_ptr<ExpressionNode> Expression::ParseExpression(const vector<string>& tokens) {
	vector<string> postfix = InfixToPostfix(tokens);
	return ParsePostfix(postfix);
}

vector<string> Expression::InfixToPostfix(const vector<string>& infix) {
	vector<string> postfix;
	stack<string> opStack;
	string prev;

	// 前一个 token 是右值（operand、)、]），即可作为隐式 + 的左侧
	auto isRhs = [&](const string& t) {
		return !t.empty() && !IsOperator(t) && t != "(" && t != "[" && t != ",";
	};

	for (const auto& token : infix) {
		bool isOperand = !IsOperator(token) && token != "(" && token != ")"
			&& token != "[" && token != "]" && token != ",";

		// 相邻 operand 之间插入隐式 + 实现字符串模板拼接
		if (isOperand && isRhs(prev)) {
			while (!opStack.empty() && opStack.top() != "(" &&
				HigherPrecedence(opStack.top(), "+")) {
				postfix.push_back(opStack.top());
				opStack.pop();
			}
			opStack.push("+");
		}

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
				THROW_EXCEPTION(RuntimeException, "Mismatched parentheses.\n");
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

		prev = token;
	}

	while (!opStack.empty()) {
		if (opStack.top() == "(") {
			THROW_EXCEPTION(RuntimeException, "Mismatched parentheses.\n");
		}
		postfix.push_back(opStack.top());
		opStack.pop();
	}

	return postfix;
}

shared_ptr<ExpressionNode> Expression::ParsePostfix(const vector<string>& postfix) {
	stack<shared_ptr<ExpressionNode>> exprStack;

	for (size_t i = 0; i < postfix.size(); i++) {
		const auto& token = postfix[i];

		if (token == "[") {
			vector<shared_ptr<ExpressionNode>> elements;
			size_t j = i + 1;

			while (j < postfix.size() && postfix[j] != "]") {
				if (postfix[j] == ",") {
					j++;
					continue;
				}

				if (IsOperator(postfix[j])) {
					THROW_EXCEPTION(RuntimeException, "Unexpected operator in array: " + postfix[j] + ".\n");
				}

				elements.push_back(ParseOperand(postfix[j]));
				j++;
			}

			if (j >= postfix.size() || postfix[j] != "]") {
				THROW_EXCEPTION(RuntimeException, "Unclosed array.\n");
			}

			exprStack.push(make_unique<ArrayExpression>(move(elements)));
			i = j;
		}
		else if (IsOperator(token)) {
			if (token == "-" && exprStack.size() == 1) {
				auto operand = move(exprStack.top());
				exprStack.pop();
				exprStack.push(make_unique<UnaryExpression>(UnaryOperator::NEGATE, move(operand)));
			}
			else if (token == "!" && exprStack.size() >= 1) {
				auto operand = move(exprStack.top());
				exprStack.pop();
				exprStack.push(make_unique<UnaryExpression>(UnaryOperator::LOGICAL_NOT, move(operand)));
			}
			else if (exprStack.size() < 2) {
				THROW_EXCEPTION(RuntimeException, "Insufficient operands for operator: " + token + ".\n");
			}
			else {
				auto right = move(exprStack.top());
				exprStack.pop();
				auto left = move(exprStack.top());
				exprStack.pop();

				BinaryOperator op = GetOperator(token);
				exprStack.push(make_unique<BinaryExpression>(move(left), move(right), op));
			}
		}
		else {
			exprStack.push(ParseOperand(token));
		}
	}

	if (exprStack.size() != 1) {
		THROW_EXCEPTION(RuntimeException, "Invalid expression.\n");
	}

	return move(exprStack.top());
}

bool Expression::IsOperator(const string& token) {
	return token == "+" || token == "-" || token == "*" || token == "/" ||
		token == "%" || token == "^" || token == "==" || token == "!=" ||
		token == ">" || token == ">=" || token == "<" || token == "<=" ||
		token == "in" || token == "&&" || token == "||" || token == "!";
}

bool Expression::HigherPrecedence(const string& op1, const string& op2) {
	int prec1 = GetPrecedence(op1);
	int prec2 = GetPrecedence(op2);

	if (prec1 == prec2) {
		return !RightAssociative(op1);
	}
	return prec1 > prec2;
}

int Expression::GetPrecedence(const string& op) const {
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

bool Expression::RightAssociative(const string& op) {
	return op == "^" || op == "!";
}

BinaryOperator Expression::GetOperator(const string& token) const {
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
	else THROW_EXCEPTION(RuntimeException, "Unknown operator: " + token);
}

shared_ptr<ExpressionNode> Expression::ParseOperand(const string& token) {
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

shared_ptr<ExpressionNode> Expression::ParseConstant(const string& token) {
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
