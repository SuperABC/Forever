#pragma once

#include <string>
#include <sstream>
#include <exception>


class ExceptionBase : public std::exception {
public:
    ExceptionBase(const std::string& message,
        const std::string& file,
        int line,
        const std::string& function = "") noexcept;

    virtual ~ExceptionBase() noexcept = default;

    // 获取报错
    const char* what() const noexcept override;
    virtual std::string GetDetailedInfo() const;
    const std::string& getMessage() const noexcept { return m_message; }
    const std::string& getFile() const noexcept { return m_file; }
    int getLine() const noexcept { return m_line; }
    const std::string& getFunction() const noexcept { return m_function; }

protected:
    virtual void formatWhat();

    std::string m_message;
    std::string m_file;
    int m_line;
    std::string m_function;
    std::string m_what;
};

class NullPointerException : public ExceptionBase {
public:
    using ExceptionBase::ExceptionBase;
};

class CommandException : public ExceptionBase {
public:
    using ExceptionBase::ExceptionBase;
};

class InvalidArgumentException : public ExceptionBase {
public:
    using ExceptionBase::ExceptionBase;
};

class InvalidConfigException : public ExceptionBase {
public:
    using ExceptionBase::ExceptionBase;
};

class ArithmaticException : public ExceptionBase {
    using ExceptionBase::ExceptionBase;
};

class IOException : public ExceptionBase {
public:
    using ExceptionBase::ExceptionBase;
};

class OutOfRangeException : public ExceptionBase {
    using ExceptionBase::ExceptionBase;
};

class NetworkException : public ExceptionBase {
    using ExceptionBase::ExceptionBase;
};

class MemoryException : public ExceptionBase {
    using ExceptionBase::ExceptionBase;
};

class JsonFormatException : public ExceptionBase {
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