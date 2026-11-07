#pragma once

#include "../common/utility.h"
#include "../common/error.h"

#include <string>
#include <memory>
#include <functional>
#include <unordered_map>


class Event;

// 表达式求值时的变量路由上下文：self./system./local. 前缀和无前缀（全局）分别路由到下面四个槽位，
// 槽位为空或变量不存在时一律按"变量不存在"处理（求值为0，不报错），见expression.md
struct ScriptContext {
	// self. 前缀路由目标：拥有当前表达式的 Script 自己的变量池
	Container* self = nullptr;

	// system. 前缀路由目标：Story 挂载的系统变量池——脚本全局可读、不可写，只由C++写
	Container* system = nullptr;

	// 没有前缀的变量的路由目标：Story 挂载的全局变量池——脚本全局可读可写(set_value)
	Container* global = nullptr;

	// local. 前缀路由目标：触发本次匹配/求值的运行时 Event（可为空，为空时 local. 一律查不到）
	const Event* local = nullptr;
};

enum class BinaryOperator : int {
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

enum class UnaryOperator : int {
	NEGATE,
	LOGICAL_NOT
};

// 表达式树节点基类（名字叫ExpressionNode，Expression这个名字留给对外门面类用）
class ExpressionNode {
public:
	ExpressionNode();

	virtual ~ExpressionNode();

	virtual ValueType Evaluate(const ScriptContext& context) const = 0;
};

// 变量表达式：name是$$之后的原始文本(如"self.name"/"local.message"/"fuck_mona")，
// 求值时按ResolveVariableName路由到ScriptContext对应槽位
class VariableExpression : public ExpressionNode {
private:
	// 变量名（$$之后的原始文本，不做任何字符替换）
	std::string name;

public:
	VariableExpression(const std::string& expression);

	virtual ~VariableExpression();

	ValueType Evaluate(const ScriptContext& context) const override;
};

// 常量表达式
class ConstantExpression : public ExpressionNode {
private:
	// 常量值
	ValueType value;

public:
	ConstantExpression(ValueType expression);

	~ConstantExpression();

	ValueType Evaluate(const ScriptContext& context) const override;
};

// 数组表达式
class ArrayExpression : public ExpressionNode {
private:
	// 元素表达式列表
	std::vector<std::shared_ptr<ExpressionNode>> elements;

public:
	ArrayExpression(std::vector<std::shared_ptr<ExpressionNode>> expression);

	~ArrayExpression();

	std::vector<ValueType> GetElementValues(const ScriptContext& context) const;

	ValueType Evaluate(const ScriptContext& context) const override;
};

// 单目表达式
class UnaryExpression : public ExpressionNode {
private:
	// 单目运算符
	UnaryOperator operand;

	// 操作数表达式
	std::shared_ptr<ExpressionNode> expression;

public:
	UnaryExpression(UnaryOperator op, std::shared_ptr<ExpressionNode> operand);

	~UnaryExpression();

	ValueType Evaluate(const ScriptContext& context) const override;

private:
	ValueType ApplyNegate(const ValueType& value) const;

	ValueType ApplyLogicalNot(const ValueType& value) const;

	bool ConvertToBool(const ValueType& value) const;
};

// 双目表达式
class BinaryExpression : public ExpressionNode {
private:
	// 左操作数表达式
	std::shared_ptr<ExpressionNode> left;

	// 右操作数表达式
	std::shared_ptr<ExpressionNode> right;

	// 双目运算符
	BinaryOperator operand;

public:
	BinaryExpression(std::shared_ptr<ExpressionNode> left, std::shared_ptr<ExpressionNode> right, BinaryOperator op);

	~BinaryExpression();

	ValueType Evaluate(const ScriptContext& context) const override;

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
			double leftVal = static_cast<double>(left);
			double rightVal = static_cast<double>(right);
			return CompareSameType(leftVal, rightVal, op);
		}
		else if constexpr (std::is_same_v<T1, std::string> || std::is_same_v<T2, std::string>) {
			std::string leftStr = toString(left);
			std::string rightStr = toString(right);
			return CompareSameType(leftStr, rightStr, op);
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

// 表达式门面：剧情json里所有的值理论上都是字符串表达式，统一由这个类解析+求值，见expression.md
class Expression {
public:
	bool Parse(const std::string& expr);

	bool EvaluateBool(const ScriptContext& context) const;

	ValueType EvaluateValue(const ScriptContext& context) const;

private:
	// 表达式树根节点
	std::shared_ptr<ExpressionNode> root;

	std::vector<std::string> Tokenize(const std::string& expr);

	std::shared_ptr<ExpressionNode> ParseExpression(const std::vector<std::string>& tokens);

	std::vector<std::string> InfixToPostfix(const std::vector<std::string>& infix);

	std::shared_ptr<ExpressionNode> ParsePostfix(const std::vector<std::string>& postfix);

	bool IsOperator(const std::string& token);

	bool HigherPrecedence(const std::string& op1, const std::string& op2);

	int GetPrecedence(const std::string& op) const;

	bool RightAssociative(const std::string& op);

	BinaryOperator GetOperator(const std::string& token) const;

	std::shared_ptr<ExpressionNode> ParseOperand(const std::string& token);

	std::shared_ptr<ExpressionNode> ParseConstant(const std::string& token);

	bool OperatorChar(char c);
};

ValueType EvaluateExpression(const std::string& source, const ScriptContext& context);

bool EvaluateExpressionBool(const std::string& source, const ScriptContext& context);

// 变量所在的变量池。
enum VARIABLE_SCOPE : int {
	VARIABLE_SCOPE_SELF,   // 当前脚本自己的变量(self.前缀)，脚本可读可写
	VARIABLE_SCOPE_SYSTEM, // 系统变量(system.前缀)，脚本只读，只由C++写
	VARIABLE_SCOPE_LOCAL,  // 当前事件的字段(local.前缀)，只读
	VARIABLE_SCOPE_GLOBAL, // 全局变量(没有前缀)，脚本可读可写
};

VARIABLE_SCOPE ResolveVariableName(const std::string& name, std::string& outKey);

bool IsOperatorChar(char c);

bool IsSpaceChar(char c);
