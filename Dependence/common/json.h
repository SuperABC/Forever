#pragma once

#include <string>
#include <vector>
#include <map>
#include <deque>
#include <stack>
#include <iostream>
#include <cassert>
#include <stdexcept>
#include <cstdlib>
#include <stdlib.h>


class FastWriter;
class StyledWriter;
class JsonReader;
class JsonValue;
class ValueIteratorBase;
class ValueIterator;
class ValueConstIterator;

enum DataType {
    DATA_NULL = 0,
    DATA_INT,
    DATA_UINT,
    DATA_DOUBLE,
    DATA_STRING,
    DATA_BOOL,
    DATA_ARRAY,
    DATA_OBJECT
};

enum CommentPlacement {
    COMMENT_BEFORE = 0,
    COMMENT_LINE,
    COMMENT_AFTER,
    COMMENT_MULTIPLE
};

// 字符串比较函数，特别处理数字索引
struct StringCompare {
    bool operator()(const std::string& s1, const std::string& s2) const {
        // 如果两个字符串都可以解析为数字，按数字比较
        char* end1, * end2;
        long long n1 = strtoll(s1.c_str(), &end1, 10);
        long long n2 = strtoll(s2.c_str(), &end2, 10);

        // 如果两个字符串都可以完全转换为数字，按数字比较
        if (end1 != s1.c_str() && *end1 == '\0' &&
            end2 != s2.c_str() && *end2 == '\0') {
            return n1 < n2;
        }

        // 否则按字符串字典序比较
        return s1 < s2;
    }
};

// 表示一个JSON值
class JsonValue {
    friend class ValueIteratorBase;

public:
    // 使用自定义比较函数的map类型
    std::map<std::string, JsonValue, StringCompare> ObjectValues;

    static const JsonValue& null;
    static const int minInt;
    static const int maxInt;
    static const unsigned int maxUInt;

    JsonValue(DataType type = DATA_NULL);
    JsonValue(int value);
    JsonValue(unsigned int value);
    JsonValue(long long value);
    JsonValue(unsigned long long value);
    JsonValue(double value);
    JsonValue(const char* value);
    JsonValue(const char* beginValue, const char* endValue);
    JsonValue(const std::string& value);
    JsonValue(bool value);
    JsonValue(const JsonValue& other);
    ~JsonValue();

    bool operator<(const JsonValue& other) const;
    bool operator<=(const JsonValue& other) const;
    bool operator>=(const JsonValue& other) const;
    bool operator>(const JsonValue& other) const;
    bool operator==(const JsonValue& other) const;
    bool operator!=(const JsonValue& other) const;
    JsonValue& operator=(const JsonValue& other);
    bool operator!() const;
    JsonValue& operator[](unsigned int index);
    JsonValue& operator[](int index);
    const JsonValue& operator[](unsigned int index) const;
    const JsonValue& operator[](int index) const;
    JsonValue& operator[](const char* key);
    const JsonValue& operator[](const char* key) const;
    JsonValue& operator[](const std::string& key);
    const JsonValue& operator[](const std::string& key) const;

    void swap(JsonValue& other);
    int compare(const JsonValue& other) const;
    unsigned int size() const;
    bool empty() const;
    void clear();
    void resize(unsigned int size);
    JsonValue get(unsigned int index, const JsonValue& defaultValue) const;
    JsonValue get(const char* key, const JsonValue& defaultValue) const;
    JsonValue get(const std::string& key, const JsonValue& defaultValue) const;
    JsonValue& append(const JsonValue& value);
    ValueConstIterator begin() const;
    ValueConstIterator end() const;
    ValueIterator begin();
    ValueIterator end();

    DataType GetType() const;

    std::string AsString() const;
    int AsInt() const;
    unsigned int AsUInt() const;
    float AsFloat() const;
    double AsDouble() const;
    bool AsBool() const;

    bool IsNull() const;
    bool IsBool() const;
    bool IsInt() const;
    bool IsUInt() const;
    bool IsIntegral() const;
    bool IsDouble() const;
    bool IsNumeric() const;
    bool IsString() const;
    bool IsArray() const;
    bool IsObject() const;

    bool ConvertibleTo(DataType other) const;

    bool ValidIndex(unsigned int index) const;
    bool ValidMember(const std::string& key) const;
    JsonValue RemoveMember(const std::string& key);
    std::vector<std::string> GetMembers() const;

    void SetComment(const std::string& comment, CommentPlacement placement);
    bool HasComment(CommentPlacement placement) const;
    std::string GetComment(CommentPlacement placement) const;

    std::string WriteCompact() const;
    std::string WriteStyled() const;

    void SetOffsetStart(size_t start);
    void SetOffsetLimit(size_t limit);
    size_t GetOffsetStart() const;
    size_t GetOffsetLimit() const;

private:
    struct CommentInfo {
        CommentInfo();
        ~CommentInfo();
        void setComment(const std::string& text);
        std::string comment;
    };

    // 基本类型值的联合体
    union {
        long long valueInt;
        unsigned long long valueUint;
        double valueDouble;
        bool valueBool;
    } value;
    std::string valueString;
    std::map<std::string, JsonValue, StringCompare>* valueObject;

    DataType type;
    CommentInfo* comments;
    size_t start;
    size_t limit;
};

class ValueIteratorBase {
public:
    ValueIteratorBase();
    explicit ValueIteratorBase(const std::map<std::string, JsonValue, StringCompare>::iterator& current);

    bool operator==(const ValueIteratorBase& other) const { return equal(other); }
    bool operator!=(const ValueIteratorBase& other) const { return !equal(other); }
    int operator-(const ValueIteratorBase& other) const { return distance(other); }

    JsonValue key() const;
    unsigned int index() const;
    const char* name() const;

protected:
    JsonValue& deref() const;
    void increment();
    void decrement();
    int distance(const ValueIteratorBase& other) const;
    bool equal(const ValueIteratorBase& other) const;
    void copy(const ValueIteratorBase& other);

private:
    std::map<std::string, JsonValue, StringCompare>::iterator current;
    bool isNull;
};

class ValueConstIterator : public ValueIteratorBase {
public:
    ValueConstIterator();
    explicit ValueConstIterator(const std::map<std::string, JsonValue, StringCompare>::iterator& current);

    ValueConstIterator& operator=(const ValueIteratorBase& other);
    ValueConstIterator operator++(int);
    ValueConstIterator operator--(int);
    ValueConstIterator& operator--();
    ValueConstIterator& operator++();
    const JsonValue& operator*() const;
};

class ValueIterator : public ValueIteratorBase {
public:
    ValueIterator();
    ValueIterator(const ValueConstIterator& other);
    ValueIterator(const ValueIterator& other);
    explicit ValueIterator(const std::map<std::string, JsonValue, StringCompare>::iterator& current);

    ValueIterator& operator=(const ValueIterator& other);
    ValueIterator operator++(int);
    ValueIterator operator--(int);
    ValueIterator& operator--();
    ValueIterator& operator++();
    JsonValue& operator*() const;
};

class JsonReader {
public:
    struct StructuredError {
        size_t offset_start;
        size_t offset_limit;
        std::string message;
    };

    JsonReader();

    bool Parse(const std::string& document, JsonValue& root, bool collectComments = true);
    bool Parse(const char* beginDoc, const char* endDoc, JsonValue& root, bool collectComments = true);
    bool Parse(std::istream& is, JsonValue& root, bool collectComments = true);

    std::string GetErrorMessages() const;
    std::vector<StructuredError> GetErrorStructures() const;

private:
    enum TokenType {
        TOKEN_END_OF_STREAM = 0,
        TOKEN_OBJECT_BEGIN,
        TOKEN_OBJECT_END,
        TOKEN_ARRAY_BEGIN,
        TOKEN_ARRAY_END,
        TOKEN_STRING,
        TOKEN_NUMBER,
        TOKEN_TRUE,
        TOKEN_FALSE,
        TOKEN_NULL,
        TOKEN_ARRAY_SEPARATOR,
        TOKEN_MEMBER_SEPARATOR,
        TOKEN_COMMENT,
        TOKEN_ERROR
    };

    class Token {
    public:
        TokenType type;
        const char* start;
        const char* end;
    };

    class ErrorInfo {
    public:
        Token token;
        std::string message;
        const char* extra;
    };

    bool ExpectToken(TokenType type, Token& token, const char* message);
    bool ReadToken(Token& token);
    void SkipSpaces();
    bool Match(const char* pattern, int patternLength);
    bool ReadComment();
    bool ReadCStyleComment();
    bool ReadCppStyleComment();
    bool ReadString();
    void ReadNumber();
    bool ReadValue();
    bool ReadObject(Token& token);
    bool ReadArray(Token& token);
    bool DecodeNumber(Token& token);
    bool DecodeNumber(Token& token, JsonValue& decoded);
    bool DecodeString(Token& token);
    bool DecodeString(Token& token, std::string& decoded);
    bool DecodeDouble(Token& token);
    bool DecodeDouble(Token& token, JsonValue& decoded);
    bool DecodeUnicodeCodePoint(Token& token, const char*& current, const char* end, unsigned int& unicode);
    bool DecodeUnicodeEscapeSequence(Token& token, const char*& current, const char* end, unsigned int& unicode);
    bool AddError(const std::string& message, Token& token, const char* extra = 0);
    bool RecoverFromError(TokenType skipUntilToken);
    bool AddErrorAndRecover(const std::string& message, Token& token, TokenType skipUntilToken);
    JsonValue& CurrentValue();
    char GetNextChar();
    void GetLocationLineAndColumn(const char* location, int& line, int& column) const;
    std::string GetLocationLineAndColumn(const char* location) const;
    void AddComment(const char* begin, const char* end, CommentPlacement placement);
    void SkipCommentTokens(Token& token);

    std::stack<JsonValue*> nodes;
    std::deque<ErrorInfo> errors;
    std::string document;
    const char* begin;
    const char* end;
    const char* current;
    const char* lastValueEnd;
    JsonValue* lastValue;
    std::string commentsBefore;
    bool collectComments;
};

std::istream& operator>>(std::istream&, JsonValue&);

class Writer {
public:
    virtual ~Writer();
    virtual std::string Write(const JsonValue& root) = 0;
};

// 输出紧凑格式的JSON
class FastWriter : public Writer {
public:
    FastWriter();
    virtual ~FastWriter() {}
    virtual std::string Write(const JsonValue& root);

private:
    void WriteValue(const JsonValue& value);
    std::string document;
};

// 输出格式化美观的JSON
class StyledWriter : public Writer {
public:
    StyledWriter();
    virtual ~StyledWriter() {}
    virtual std::string Write(const JsonValue& root);

private:
    void WriteValue(const JsonValue& value);
    void WriteArrayValue(const JsonValue& value);
    bool IsMultineArray(const JsonValue& value);
    void PushValue(const std::string& value);
    void WriteIndent();
    void WriteWithIndent(const std::string& value);
    void Indent();
    void Unindent();
    void WriteCommentBeforeValue(const JsonValue& root);
    void WriteCommentAfterValueOnSameLine(const JsonValue& root);
    bool HasCommentForValue(const JsonValue& value);
    static std::string NormalizeEOL(const std::string& text);

    std::vector<std::string> childValues;
    std::string document;
    std::string indentString;
    int rightMargin;
    int indentSize;
    bool addChildValues;
};

class StyledStreamWriter {
public:
    StyledStreamWriter(std::string indentation = "\t");
    ~StyledStreamWriter() {}
    void Write(std::ostream& out, const JsonValue& root);

private:
    void WriteValue(const JsonValue& value);
    void WriteArrayValue(const JsonValue& value);
    bool IsMultineArray(const JsonValue& value);
    void PushValue(const std::string& value);
    void WriteIndent();
    void WriteWithIndent(const std::string& value);
    void Indent();
    void Unindent();
    void WriteCommentBeforeValue(const JsonValue& root);
    void WriteCommentAfterValueOnSameLine(const JsonValue& root);
    bool HasCommentForValue(const JsonValue& value);
    static std::string NormalizeEOL(const std::string& text);

    std::vector<std::string> childValues;
    std::ostream* document;
    std::string indentString;
    int rightMargin;
    std::string indentation;
    bool addChildValues;
};

std::ostream& operator<<(std::ostream&, const JsonValue& root);

