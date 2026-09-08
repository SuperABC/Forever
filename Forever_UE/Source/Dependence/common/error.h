#pragma once

#include <string>
#include <sstream>
#include <exception>


class ExceptionBase : public std::exception {
public:

	/*
	* 构造异常，记录消息、文件、行号、函数名
	* @message: 异常消息
	* @file: 发生异常的文件名
	* @line: 发生异常的行号
	* @function: 发生异常的函数名
	*/
	ExceptionBase(const std::string& message,
		const std::string& file,
		int line,
		const std::string& function = "") noexcept;

	virtual ~ExceptionBase() noexcept = default;

	/*
	* 返回格式化后的异常描述（含文件/行号/函数名）
	* @return: C字符串形式的异常描述
	*/
	const char* what() const noexcept override;

	/*
	* 获取详细异常信息（多行文本）
	* @return: 包含消息、文件、行号、函数名的详细描述字符串
	*/
	virtual std::string GetDetailedInfo() const;

	/*
	* 获取异常消息文本
	* @return: 异常消息字符串
	*/
	const std::string& GetMessage() const noexcept { return message; }

	/*
	* 获取发生异常的文件名
	* @return: 文件名字符串
	*/
	const std::string& GetFile() const noexcept { return file; }

	/*
	* 获取发生异常的行号
	* @return: 行号整数
	*/
	int GetLine() const noexcept { return line; }

	/*
	* 获取发生异常的函数名
	* @return: 函数名字符串
	*/
	const std::string& GetFunction() const noexcept { return function; }

protected:

	/*
	* 格式化 what 缓存字符串
	*/
	virtual void FormatWhat();

	// 异常消息文本
	std::string message;

	// 发生异常的文件名
	std::string file;

	// 发生异常的行号
	int line;

	// 发生异常的函数名
	std::string function;

	// what() 返回的缓存字符串
	std::string whatCache;
};

// 访问空指针
class NullPointerException : public ExceptionBase {
public:
	using ExceptionBase::ExceptionBase;
};

// 命令行输入有误
class CommandException : public ExceptionBase {
public:
	using ExceptionBase::ExceptionBase;
};

// 函数参数有误
class InvalidArgumentException : public ExceptionBase {
public:
	using ExceptionBase::ExceptionBase;
};

// 输入输出异常
class IOException : public ExceptionBase {
public:
	using ExceptionBase::ExceptionBase;
};

// 数组访问越界
class OutOfRangeException : public ExceptionBase {
	using ExceptionBase::ExceptionBase;
};

// 死循环
class DeadLoopException : public ExceptionBase {
	using ExceptionBase::ExceptionBase;
};

// 网络异常
class NetworkException : public ExceptionBase {
	using ExceptionBase::ExceptionBase;
};

// 内存异常
class MemoryException : public ExceptionBase {
	using ExceptionBase::ExceptionBase;
};

// Json格式有误
class JsonFormatException : public ExceptionBase {
	using ExceptionBase::ExceptionBase;
};

// 其他
class RuntimeException : public ExceptionBase {
	using ExceptionBase::ExceptionBase;
};

template<typename ExceptionClass>
[[noreturn]] void ThrowException(const std::string& message,
	const char* file,
	int line,
	const char* function = "") {
	throw ExceptionClass(message, file, line, function);
}

#define THROW_EXCEPTION(ExceptionClass, message) \
	ThrowException<ExceptionClass>(message, __FILE__, __LINE__, __FUNCTION__)
