#pragma once

#include "../common/utility.h"
#include "../common/error.h"

#include <string>
#include <memory>
#include <functional>
#include <unordered_map>


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

// 表达式基类
class Expression {
public:
	/*
	* 构造表达式基类
	*/
	Expression();

	/*
	* 析构表达式基类
	*/
	virtual ~Expression();

	/*
	* 求值
	* @getValues: 变量查询函数列表
	* @return: 表达式求值结果
	*/
	virtual ValueType Evaluate(std::vector<std::function<std::pair<bool, ValueType>(const std::string&)>> getValues) const = 0;
};

// 变量表达式
class VariableExpression : public Expression {
private:
	// 变量名
	std::string name;

public:
	/*
	* 按变量名构造变量表达式
	* @expression: 变量名字符串
	*/
	VariableExpression(const std::string& expression);

	/*
	* 析构变量表达式
	*/
	virtual ~VariableExpression();

	/*
	* 求值：查找变量名对应的值
	* @getValues: 变量查询函数列表
	* @return: 变量值
	*/
	ValueType Evaluate(std::vector<std::function<std::pair<bool, ValueType>(const std::string&)>> getValues) const override;
};

// 间接寻址表达式（$$(expr) 语法，将内部表达式求值结果作为变量名查找）
class IndirectExpression : public Expression {
private:
	// 内部表达式
	std::shared_ptr<Expression> inner;

public:
	/*
	* 按内部表达式构造间接寻址表达式
	* @inner: 内部表达式
	*/
	IndirectExpression(std::shared_ptr<Expression> inner);

	/*
	* 析构间接寻址表达式
	*/
	virtual ~IndirectExpression();

	/*
	* 求值：将内部表达式结果作为变量名再次查询
	* @getValues: 变量查询函数列表
	* @return: 间接寻址结果值
	*/
	ValueType Evaluate(std::vector<std::function<std::pair<bool, ValueType>(const std::string&)>> getValues) const override;
};

// 常量表达式
class ConstantExpression : public Expression {
private:
	// 常量值
	ValueType value;

public:
	/*
	* 按常量值构造常量表达式
	* @expression: 常量值
	*/
	ConstantExpression(ValueType expression);

	/*
	* 析构常量表达式
	*/
	~ConstantExpression();

	/*
	* 求值：直接返回常量值
	* @getValues: 变量查询函数列表
	* @return: 常量值
	*/
	ValueType Evaluate(std::vector<std::function<std::pair<bool, ValueType>(const std::string&)>> getValues) const override;
};

// 数组表达式
class ArrayExpression : public Expression {
private:
	// 元素表达式列表
	std::vector<std::shared_ptr<Expression>> elements;

public:
	/*
	* 按元素表达式列表构造数组表达式
	* @expression: 元素表达式列表
	*/
	ArrayExpression(std::vector<std::shared_ptr<Expression>> expression);

	/*
	* 析构数组表达式
	*/
	~ArrayExpression();

	/*
	* 获取各元素求值结果列表
	* @getValues: 变量查询函数列表
	* @return: 各元素值的列表
	*/
	std::vector<ValueType> GetElementValues(std::vector < std::function<std::pair<bool, ValueType>(const std::string&)>> getValues) const;

	/*
	* 求值：将所有元素打包为数组值
	* @getValues: 变量查询函数列表
	* @return: 数组值
	*/
	ValueType Evaluate(std::vector<std::function<std::pair<bool, ValueType>(const std::string&)>> getValues) const override;
};

// 单目表达式
class UnaryExpression : public Expression {
private:
	// 单目运算符
	UnaryOperator operand;

	// 操作数表达式
	std::shared_ptr<Expression> expression;

public:
	/*
	* 按运算符和操作数构造单目表达式
	* @op: 单目运算符
	* @operand: 操作数表达式
	*/
	UnaryExpression(UnaryOperator op, std::shared_ptr<Expression> operand);

	/*
	* 析构单目表达式
	*/
	~UnaryExpression();

	/*
	* 求值：对操作数应用单目运算符
	* @getValues: 变量查询函数列表
	* @return: 运算结果
	*/
	ValueType Evaluate(std::vector<std::function<std::pair<bool, ValueType>(const std::string&)>> getValues) const override;

private:
	/*
	* 对值应用取反运算
	* @value: 待取反的值
	* @return: 取反后的值
	*/
	ValueType ApplyNegate(const ValueType& value) const;

	/*
	* 对值应用逻辑非运算
	* @value: 待取非的值
	* @return: 逻辑非结果
	*/
	ValueType ApplyLogicalNot(const ValueType& value) const;

	/*
	* 将值转换为布尔类型
	* @value: 待转换的值
	* @return: 布尔值
	*/
	bool ConvertToBool(const ValueType& value) const;
};

// 双目表达式
class BinaryExpression : public Expression {
private:
	// 左操作数表达式
	std::shared_ptr<Expression> left;

	// 右操作数表达式
	std::shared_ptr<Expression> right;

	// 双目运算符
	BinaryOperator operand;

public:
	/*
	* 按左右操作数和运算符构造双目表达式
	* @left, right: 左右操作数表达式
	* @op: 双目运算符
	*/
	BinaryExpression(std::shared_ptr<Expression> left, std::shared_ptr<Expression> right, BinaryOperator op);

	/*
	* 析构双目表达式
	*/
	~BinaryExpression();

	/*
	* 求值：对左右操作数应用双目运算符
	* @getValues: 变量查询函数列表
	* @return: 运算结果
	*/
	ValueType Evaluate(std::vector<std::function<std::pair<bool, ValueType>(const std::string&)>> getValues) const override;

private:
	/*
	* 对两个值执行比较运算并返回布尔结果
	* @left, right: 左右操作数值
	* @op: 比较运算符
	* @return: 比较结果
	*/
	bool GetComparisonResult(const ValueType& left, const ValueType& right, BinaryOperator op) const;

	/*
	* 根据运算符对两个值执行比较，返回布尔类型的ValueType
	* @left, right: 左右操作数值
	* @op: 比较运算符
	* @return: 比较结果（ValueType包装的bool）
	*/
	ValueType CompareValues(const ValueType& left, const ValueType& right, BinaryOperator op) const;

	/*
	* 对同类型值执行比较运算
	* @left, right: 同类型的左右操作数
	* @op: 比较运算符
	* @return: 比较结果
	*/
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

	/*
	* 对不同类型值执行比较运算（尝试数值提升或转字符串比较）
	* @left, right: 不同类型的左右操作数
	* @op: 比较运算符
	* @return: 比较结果
	*/
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

	/*
	* 对两个值执行算术运算
	* @left, right: 左右操作数值
	* @op: 算术运算符
	* @return: 算术运算结果
	*/
	ValueType ComputeArithmetic(const ValueType& left, const ValueType& right, BinaryOperator op) const;

	/*
	* 将值转换为布尔类型
	* @value: 待转换的值
	* @return: 布尔值
	*/
	bool ConvertToBool(const ValueType& value) const;

	/*
	* 将值转换为字符串
	* @value: 待转换的值
	* @return: 字符串表示
	*/
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

// 表达式接口
class Condition {
public:
	/*
	* 解析条件字符串，构建表达式树
	* @conditionStr: 条件字符串
	* @return: 解析是否成功
	*/
	bool ParseCondition(const std::string& conditionStr);

	/*
	* 对条件求值并返回布尔结果
	* @getValues: 变量查询函数列表
	* @return: 条件布尔求值结果
	*/
	bool EvaluateBool(std::vector < std::function<std::pair<bool, ValueType>(const std::string&)>> getValues) const;

	/*
	* 对条件求值并返回通用类型结果
	* @getValues: 变量查询函数列表
	* @return: 条件求值结果
	*/
	ValueType EvaluateValue(std::vector < std::function<std::pair<bool, ValueType>(const std::string&)>> getValues) const;

private:
	// 表达式树根节点
	std::shared_ptr<Expression> root;

	/*
	* 将表达式字符串分词
	* @expr: 待分词的表达式字符串
	* @return: 词元列表
	*/
	std::vector<std::string> Tokenize(const std::string& expr);

	/*
	* 将词元列表解析为表达式树
	* @tokens: 词元列表
	* @return: 表达式树根节点
	*/
	std::shared_ptr<Expression> ParseExpression(const std::vector<std::string>& tokens);

	/*
	* 将中缀词元列表转换为后缀（逆波兰）形式
	* @infix: 中缀词元列表
	* @return: 后缀词元列表
	*/
	std::vector<std::string> InfixToPostfix(const std::vector<std::string>& infix);

	/*
	* 将后缀词元列表构建为表达式树
	* @postfix: 后缀词元列表
	* @return: 表达式树根节点
	*/
	std::shared_ptr<Expression> ParsePostfix(const std::vector<std::string>& postfix);

	/*
	* 判断词元是否为运算符
	* @token: 待判断的词元
	* @return: 是否为运算符
	*/
	bool IsOperator(const std::string& token);

	/*
	* 比较两个运算符的优先级
	* @op1, op2: 两个运算符
	* @return: op1优先级是否高于op2
	*/
	bool HigherPrecedence(const std::string& op1, const std::string& op2);

	/*
	* 获取运算符优先级数值
	* @op: 运算符
	* @return: 优先级数值
	*/
	int GetPrecedence(const std::string& op) const;

	/*
	* 判断运算符是否右结合
	* @op: 运算符
	* @return: 是否右结合
	*/
	bool RightAssociative(const std::string& op);

	/*
	* 将字符串词元转换为对应的双目运算符枚举值
	* @token: 运算符词元
	* @return: 双目运算符枚举值
	*/
	BinaryOperator GetOperator(const std::string& token) const;

	/*
	* 将词元解析为操作数表达式
	* @token: 操作数词元
	* @return: 操作数表达式
	*/
	std::shared_ptr<Expression> ParseOperand(const std::string& token);

	/*
	* 将词元解析为常量表达式
	* @token: 常量词元
	* @return: 常量表达式
	*/
	std::shared_ptr<Expression> ParseConstant(const std::string& token);

	/*
	* 判断字符是否为运算符字符
	* @c: 待判断的字符
	* @return: 是否为运算符字符
	*/
	bool OperatorChar(char c);
};

/*
* 判断字符是否为运算符字符（全局工具函数）
* @c: 待判断的字符
* @return: 是否为运算符字符
*/
bool IsOperatorChar(char c);

/*
* 判断字符是否为空白字符（全局工具函数）
* @c: 待判断的字符
* @return: 是否为空白字符
*/
bool IsSpaceChar(char c);

/*
* 判断字符是否为标识符字符（全局工具函数）
* @c: 待判断的字符
* @return: 是否为标识符字符
*/
bool IsIdentifierChar(char c);
