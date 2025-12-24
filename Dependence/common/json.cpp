#include "json.h"
#include "error.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <utility>
#include <cerrno>
#include <cctype>


using namespace std;

static inline string codePointToUTF8(unsigned int cp) {
    string result;

    if (cp <= 0x7f) {
        result.resize(1);
        result[0] = static_cast<char>(cp);
    }
    else if (cp <= 0x7FF) {
        result.resize(2);
        result[1] = static_cast<char>(0x80 | (0x3f & cp));
        result[0] = static_cast<char>(0xC0 | (0x1f & (cp >> 6)));
    }
    else if (cp <= 0xFFFF) {
        result.resize(3);
        result[2] = static_cast<char>(0x80 | (0x3f & cp));
        result[1] = 0x80 | static_cast<char>((0x3f & (cp >> 6)));
        result[0] = 0xE0 | static_cast<char>((0xf & (cp >> 12)));
    }
    else if (cp <= 0x10FFFF) {
        result.resize(4);
        result[3] = static_cast<char>(0x80 | (0x3f & cp));
        result[2] = static_cast<char>(0x80 | (0x3f & (cp >> 6)));
        result[1] = static_cast<char>(0x80 | (0x3f & (cp >> 12)));
        result[0] = static_cast<char>(0xF0 | (0x7 & (cp >> 18)));
    }

    return result;
}

static inline bool isControlCharacter(char ch) {
    return ch > 0 && ch <= 0x1F;
}

static inline void uintToString(unsigned long long value, char*& current) {
    *--current = 0;
    do {
        *--current = char(value % 10) + '0';
        value /= 10;
    } while (value != 0);
}

static inline void fixNumericLocale(char* begin, char* end) {
    while (begin < end) {
        if (*begin == ',') {
            *begin = '.';
        }
        ++begin;
    }
}

static inline bool in(char c, char c1, char c2, char c3, char c4) {
    return c == c1 || c == c2 || c == c3 || c == c4;
}

static inline bool in(char c, char c1, char c2, char c3, char c4, char c5) {
    return c == c1 || c == c2 || c == c3 || c == c4 || c == c5;
}

static bool containsNewLine(const char* begin, const char* end) {
    for (; begin < end; ++begin)
        if (*begin == '\n' || *begin == '\r')
            return true;
    return false;
}

JsonReader::JsonReader()
    : errors(), document(), begin(0), end(0), current(0), lastValueEnd(0),
    lastValue(0), commentsBefore(), collectComments(true) {
}

bool JsonReader::Parse(const string& document, JsonValue& root, bool collectComments) {
    this->document = document;
    const char* begin = document.c_str();
    const char* end = begin + document.length();
    return Parse(begin, end, root, collectComments);
}

bool JsonReader::Parse(istream& sin, JsonValue& root, bool collectComments) {
    string doc;
    getline(sin, doc, (char)EOF);
    return Parse(doc, root, collectComments);
}

bool JsonReader::Parse(const char* beginDoc, const char* endDoc, JsonValue& root, bool collectComments) {
    begin = beginDoc;
    end = endDoc;
    this->collectComments = collectComments;
    current = begin;
    lastValueEnd = 0;
    lastValue = 0;
    commentsBefore = "";
    errors.clear();
    while (!nodes.empty())
        nodes.pop();
    nodes.push(&root);

    bool successful = ReadValue();
    Token token;
    SkipCommentTokens(token);
    if (collectComments && !commentsBefore.empty())
        root.SetComment(commentsBefore, COMMENT_AFTER);

    // strict mode: root must be array or object
    if (!root.IsArray() && !root.IsObject()) {
        token.type = TOKEN_ERROR;
        token.start = beginDoc;
        token.end = endDoc;
        AddError(
            "A valid JSON document must be either an array or an object value.",
            token);
        return false;
    }

    return successful;
}

bool JsonReader::ReadValue() {
    Token token;
    SkipCommentTokens(token);
    bool successful = true;

    if (collectComments && !commentsBefore.empty()) {
        size_t lastNonNewline = commentsBefore.find_last_not_of("\r\n");
        if (lastNonNewline != string::npos) {
            commentsBefore.erase(lastNonNewline + 1);
        }
        else {
            commentsBefore.clear();
        }

        CurrentValue().SetComment(commentsBefore, COMMENT_BEFORE);
        commentsBefore = "";
    }

    switch (token.type) {
    case TOKEN_OBJECT_BEGIN:
        successful = ReadObject(token);
        CurrentValue().SetOffsetLimit(current - begin);
        break;
    case TOKEN_ARRAY_BEGIN:
        successful = ReadArray(token);
        CurrentValue().SetOffsetLimit(current - begin);
        break;
    case TOKEN_NUMBER:
        successful = DecodeNumber(token);
        break;
    case TOKEN_STRING:
        successful = DecodeString(token);
        break;
    case TOKEN_TRUE:
        CurrentValue() = true;
        CurrentValue().SetOffsetStart(token.start - begin);
        CurrentValue().SetOffsetLimit(token.end - begin);
        break;
    case TOKEN_FALSE:
        CurrentValue() = false;
        CurrentValue().SetOffsetStart(token.start - begin);
        CurrentValue().SetOffsetLimit(token.end - begin);
        break;
    case TOKEN_NULL:
        CurrentValue() = JsonValue();
        CurrentValue().SetOffsetStart(token.start - begin);
        CurrentValue().SetOffsetLimit(token.end - begin);
        break;
    case TOKEN_ARRAY_SEPARATOR:
        // strict mode doesn't allow dropped null placeholders
        CurrentValue().SetOffsetStart(token.start - begin);
        CurrentValue().SetOffsetLimit(token.end - begin);
        return AddError("Syntax error: value, object or array expected.", token);
    default:
        CurrentValue().SetOffsetStart(token.start - begin);
        CurrentValue().SetOffsetLimit(token.end - begin);
        return AddError("Syntax error: value, object or array expected.", token);
    }

    if (collectComments) {
        lastValueEnd = current;
        lastValue = &CurrentValue();
    }

    return successful;
}

void JsonReader::SkipCommentTokens(Token& token) {
    do {
        ReadToken(token);
    } while (token.type == TOKEN_COMMENT);
}

bool JsonReader::ExpectToken(TokenType type, Token& token, const char* message) {
    ReadToken(token);
    if (token.type != type)
        return AddError(message, token);
    return true;
}

bool JsonReader::ReadToken(Token& token) {
    SkipSpaces();
    token.start = current;
    char c = GetNextChar();
    bool ok = true;
    switch (c) {
    case '{':
        token.type = TOKEN_OBJECT_BEGIN;
        break;
    case '}':
        token.type = TOKEN_OBJECT_END;
        break;
    case '[':
        token.type = TOKEN_ARRAY_BEGIN;
        break;
    case ']':
        token.type = TOKEN_ARRAY_END;
        break;
    case '"':
        token.type = TOKEN_STRING;
        ok = ReadString();
        break;
    case '/':
        token.type = TOKEN_COMMENT;
        ok = ReadComment();
        break;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case '-':
        token.type = TOKEN_NUMBER;
        ReadNumber();
        break;
    case 't':
        token.type = TOKEN_TRUE;
        ok = Match("rue", 3);
        break;
    case 'f':
        token.type = TOKEN_FALSE;
        ok = Match("alse", 4);
        break;
    case 'n':
        token.type = TOKEN_NULL;
        ok = Match("ull", 3);
        break;
    case ',':
        token.type = TOKEN_ARRAY_SEPARATOR;
        break;
    case ':':
        token.type = TOKEN_MEMBER_SEPARATOR;
        break;
    case 0:
        token.type = TOKEN_END_OF_STREAM;
        break;
    default:
        ok = false;
        break;
    }
    if (!ok)
        token.type = TOKEN_ERROR;
    token.end = current;
    return true;
}

void JsonReader::SkipSpaces() {
    while (current != end) {
        char c = *current;
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
            ++current;
        else
            break;
    }
}

bool JsonReader::Match(const char* pattern, int patternLength) {
    if (end - current < patternLength)
        return false;
    int index = patternLength;
    while (index--)
        if (current[index] != pattern[index])
            return false;
    current += patternLength;
    return true;
}

bool JsonReader::ReadComment() {
    const char* commentBegin = current - 1;
    char c = GetNextChar();
    bool successful = false;
    if (c == '*')
        successful = ReadCStyleComment();
    else if (c == '/')
        successful = ReadCppStyleComment();
    if (!successful)
        return false;

    if (collectComments) {
        CommentPlacement placement = COMMENT_BEFORE;
        if (lastValueEnd && !containsNewLine(lastValueEnd, commentBegin)) {
            if (c != '*' || !containsNewLine(commentBegin, current))
                placement = COMMENT_LINE;
        }

        AddComment(commentBegin, current, placement);
    }
    return true;
}

void JsonReader::AddComment(const char* begin, const char* end, CommentPlacement placement) {
    if (placement == COMMENT_LINE) {
        assert(lastValue != 0);
        lastValue->SetComment(string(begin, end), placement);
    }
    else {
        if (!commentsBefore.empty())
            commentsBefore += "\n";
        commentsBefore += string(begin, end);
    }
}

bool JsonReader::ReadCStyleComment() {
    while (current != end) {
        char c = GetNextChar();
        if (c == '*' && *current == '/')
            break;
    }
    return GetNextChar() == '/';
}

bool JsonReader::ReadCppStyleComment() {
    while (current != end) {
        char c = GetNextChar();
        if (c == '\r' || c == '\n')
            break;
    }
    return true;
}

void JsonReader::ReadNumber() {
    while (current != end) {
        if (!(*current >= '0' && *current <= '9') &&
            !in(*current, '.', 'e', 'E', '+', '-'))
            break;
        ++current;
    }
}

bool JsonReader::ReadString() {
    char c = 0;
    while (current != end) {
        c = GetNextChar();
        if (c == '\\')
            GetNextChar();
        else if (c == '"')
            break;
    }
    return c == '"';
}

bool JsonReader::ReadObject(Token& tokenStart) {
    Token tokenName;
    string name;
    CurrentValue() = JsonValue(DATA_OBJECT);
    CurrentValue().SetOffsetStart(tokenStart.start - begin);
    while (ReadToken(tokenName)) {
        bool initialTokenOk = true;
        while (tokenName.type == TOKEN_COMMENT && initialTokenOk)
            initialTokenOk = ReadToken(tokenName);
        if (!initialTokenOk)
            break;
        if (tokenName.type == TOKEN_OBJECT_END && name.empty()) // empty object
            return true;
        name = "";
        if (tokenName.type == TOKEN_STRING) {
            if (!DecodeString(tokenName, name))
                return RecoverFromError(TOKEN_OBJECT_END);
        }
        else {
            break; // strict mode doesn't allow numeric keys
        }

        Token colon;
        if (!ReadToken(colon) || colon.type != TOKEN_MEMBER_SEPARATOR) {
            return AddErrorAndRecover(
                "Missing ':' after object member name", colon, TOKEN_OBJECT_END);
        }
        JsonValue& value = CurrentValue()[name];
        nodes.push(&value);
        bool ok = ReadValue();
        nodes.pop();
        if (!ok) // error already set
            return RecoverFromError(TOKEN_OBJECT_END);

        Token comma;
        if (!ReadToken(comma) ||
            (comma.type != TOKEN_OBJECT_END && comma.type != TOKEN_ARRAY_SEPARATOR &&
                comma.type != TOKEN_COMMENT)) {
            return AddErrorAndRecover(
                "Missing ',' or '}' in object declaration", comma, TOKEN_OBJECT_END);
        }
        bool finalizeTokenOk = true;
        while (comma.type == TOKEN_COMMENT && finalizeTokenOk)
            finalizeTokenOk = ReadToken(comma);
        if (comma.type == TOKEN_OBJECT_END)
            return true;
    }
    return AddErrorAndRecover(
        "Missing '}' or object member name", tokenName, TOKEN_OBJECT_END);
}

bool JsonReader::ReadArray(Token& tokenStart) {
    CurrentValue() = JsonValue(DATA_ARRAY);
    CurrentValue().SetOffsetStart(tokenStart.start - begin);
    SkipSpaces();
    if (*current == ']') // empty array
    {
        Token endArray;
        ReadToken(endArray);
        return true;
    }
    int index = 0;
    for (;;) {
        JsonValue& value = CurrentValue()[index++];
        nodes.push(&value);
        bool ok = ReadValue();
        nodes.pop();
        if (!ok) // error already set
            return RecoverFromError(TOKEN_ARRAY_END);

        Token token;
        ok = ReadToken(token);
        while (token.type == TOKEN_COMMENT && ok) {
            ok = ReadToken(token);
        }
        bool badTokenType =
            (token.type != TOKEN_ARRAY_SEPARATOR && token.type != TOKEN_ARRAY_END);
        if (!ok || badTokenType) {
            return AddErrorAndRecover(
                "Missing ',' or ']' in array declaration", token, TOKEN_ARRAY_END);
        }
        if (token.type == TOKEN_ARRAY_END)
            break;
    }
    return true;
}

bool JsonReader::DecodeNumber(Token& token) {
    JsonValue decoded;
    if (!DecodeNumber(token, decoded))
        return false;
    CurrentValue() = decoded;
    CurrentValue().SetOffsetStart(token.start - begin);
    CurrentValue().SetOffsetLimit(token.end - begin);
    return true;
}

bool JsonReader::DecodeNumber(Token& token, JsonValue& decoded) {
    bool isDouble = false;
    for (const char* inspect = token.start; inspect != token.end; ++inspect) {
        isDouble = isDouble || in(*inspect, '.', 'e', 'E', '+') ||
            (*inspect == '-' && inspect != token.start);
    }
    if (isDouble)
        return DecodeDouble(token, decoded);

    const char* current = token.start;
    bool isNegative = *current == '-';
    if (isNegative)
        ++current;

    unsigned long long maxIntegerValue =
        isNegative ? (unsigned long long)(-JsonValue::minInt)
        : JsonValue::maxUInt;
    unsigned long long threshold = maxIntegerValue / 10;
    unsigned long long value = 0;

    while (current < token.end) {
        char c = *current++;
        if (c < '0' || c > '9')
            return AddError("'" + string(token.start, token.end) +
                "' is not a number.",
                token);
        unsigned int digit(c - '0');
        if (value >= threshold) {
            if (value > threshold || current != token.end ||
                digit > maxIntegerValue % 10) {
                return DecodeDouble(token, decoded);
            }
        }
        value = value * 10 + digit;
    }

    if (isNegative)
        decoded = -(long long)(value);
    else if (value <= (unsigned long long)(JsonValue::maxInt))
        decoded = (long long)(value);
    else
        decoded = value;
    return true;
}

bool JsonReader::DecodeDouble(Token& token) {
    JsonValue decoded;
    if (!DecodeDouble(token, decoded))
        return false;
    CurrentValue() = decoded;
    CurrentValue().SetOffsetStart(token.start - begin);
    CurrentValue().SetOffsetLimit(token.end - begin);
    return true;
}

bool JsonReader::DecodeDouble(Token& token, JsonValue& decoded) {
    int length = int(token.end - token.start);

    // Sanity check to avoid buffer overflow exploits.
    if (length < 0) {
        return AddError("Unable to parse token length", token);
    }

    // 使用 strtod 替代 sscanf，提供更好的错误检测
    string buffer(token.start, token.end);
    const char* str = buffer.c_str();

    // 跳过前导空格（虽然我们的token不应该有空格）
    while (*str && isspace(*str)) str++;

    char* endptr;
    double value = strtod(str, &endptr);

    // 检查转换是否成功
    if (endptr == str) {
        // 没有成功转换任何字符
        return AddError("'" + buffer + "' is not a number.", token);
    }

    // 检查是否整个字符串都被成功解析（允许尾随空格）
    while (*endptr && isspace(*endptr)) endptr++;
    if (*endptr != '\0') {
        // 有未解析的字符
        return AddError("'" + buffer + "' contains invalid characters for a number.", token);
    }

    // 检查是否溢出
    if (value == HUGE_VAL || value == -HUGE_VAL) {
        // 检查是否为合法的无穷大表示
        if (errno == ERANGE) {
            return AddError("'" + buffer + "' is out of range for a double.", token);
        }
    }

    decoded = value;
    return true;
}

bool JsonReader::DecodeString(Token& token) {
    string decoded;
    if (!DecodeString(token, decoded))
        return false;
    CurrentValue() = decoded;
    CurrentValue().SetOffsetStart(token.start - begin);
    CurrentValue().SetOffsetLimit(token.end - begin);
    return true;
}

bool JsonReader::DecodeString(Token& token, string& decoded) {
    decoded.reserve(token.end - token.start - 2);
    const char* current = token.start + 1; // skip '"'
    const char* end = token.end - 1;       // do not include '"'
    while (current != end) {
        char c = *current++;
        if (c == '"')
            break;
        else if (c == '\\') {
            if (current == end)
                return AddError("Empty escape sequence in string", token, current);
            char escape = *current++;
            switch (escape) {
            case '"':
                decoded += '"';
                break;
            case '/':
                decoded += '/';
                break;
            case '\\':
                decoded += '\\';
                break;
            case 'b':
                decoded += '\b';
                break;
            case 'f':
                decoded += '\f';
                break;
            case 'n':
                decoded += '\n';
                break;
            case 'r':
                decoded += '\r';
                break;
            case 't':
                decoded += '\t';
                break;
            case 'u': {
                unsigned int unicode;
                if (!DecodeUnicodeCodePoint(token, current, end, unicode))
                    return false;
                decoded += codePointToUTF8(unicode);
            } break;
            default:
                return AddError("Bad escape sequence in string", token, current);
            }
        }
        else {
            decoded += c;
        }
    }
    return true;
}

bool JsonReader::DecodeUnicodeCodePoint(Token& token, const char*& current, const char* end, unsigned int& unicode) {

    if (!DecodeUnicodeEscapeSequence(token, current, end, unicode))
        return false;
    if (unicode >= 0xD800 && unicode <= 0xDBFF) {
        // surrogate pairs
        if (end - current < 6)
            return AddError(
                "additional six characters expected to parse unicode surrogate pair.",
                token,
                current);
        unsigned int surrogatePair;
        if (*(current++) == '\\' && *(current++) == 'u') {
            if (DecodeUnicodeEscapeSequence(token, current, end, surrogatePair)) {
                unicode = 0x10000 + ((unicode & 0x3FF) << 10) + (surrogatePair & 0x3FF);
            }
            else
                return false;
        }
        else
            return AddError("expecting another \\u token to begin the second half of "
                "a unicode surrogate pair",
                token,
                current);
    }
    return true;
}

bool JsonReader::DecodeUnicodeEscapeSequence(Token& token, const char*& current, const char* end, unsigned int& unicode) {
    if (end - current < 4)
        return AddError(
            "Bad unicode escape sequence in string: four digits expected.",
            token,
            current);
    unicode = 0;
    for (int index = 0; index < 4; ++index) {
        char c = *current++;
        unicode *= 16;
        if (c >= '0' && c <= '9')
            unicode += c - '0';
        else if (c >= 'a' && c <= 'f')
            unicode += c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
            unicode += c - 'A' + 10;
        else
            return AddError(
                "Bad unicode escape sequence in string: hexadecimal digit expected.",
                token,
                current);
    }
    return true;
}

bool JsonReader::AddError(const string& message, Token& token, const char* extra) {
    ErrorInfo info;
    info.token = token;
    info.message = message;
    info.extra = extra;
    errors.push_back(info);
    return false;
}

bool JsonReader::RecoverFromError(TokenType skipUntilToken) {
    int errorCount = int(errors.size());
    Token skip;
    for (;;) {
        if (!ReadToken(skip))
            errors.resize(errorCount);
        if (skip.type == skipUntilToken || skip.type == TOKEN_END_OF_STREAM)
            break;
    }
    errors.resize(errorCount);
    return false;
}

bool JsonReader::AddErrorAndRecover(const string& message, Token& token, TokenType skipUntilToken) {
    AddError(message, token);
    return RecoverFromError(skipUntilToken);
}

JsonValue& JsonReader::CurrentValue() { return *(nodes.top()); }

char JsonReader::GetNextChar() {
    if (current == end)
        return 0;
    return *current++;
}

void JsonReader::GetLocationLineAndColumn(const char* location, int& line, int& column) const {
    const char* current = begin;
    const char* lastLineStart = current;
    line = 0;
    while (current < location && current != end) {
        char c = *current++;
        if (c == '\r') {
            if (*current == '\n')
                ++current;
            lastLineStart = current;
            ++line;
        }
        else if (c == '\n') {
            lastLineStart = current;
            ++line;
        }
    }
    column = int(location - lastLineStart) + 1;
    ++line;
}

string JsonReader::GetLocationLineAndColumn(const char* location) const {
    int line, column;
    GetLocationLineAndColumn(location, line, column);
    char buffer[18 + 16 + 16 + 1];
    snprintf(buffer, sizeof(buffer), "Line %d, Column %d", line, column);
    return buffer;
}

string JsonReader::GetErrorMessages() const {
    string formattedMessage;
    for (deque<ErrorInfo>::const_iterator itError = errors.begin();
        itError != errors.end();
        ++itError) {
        const ErrorInfo& error = *itError;
        formattedMessage +=
            "* " + GetLocationLineAndColumn(error.token.start) + "\n";
        formattedMessage += "  " + error.message + "\n";
        if (error.extra)
            formattedMessage +=
            "See " + GetLocationLineAndColumn(error.extra) + " for detail.\n";
    }
    return formattedMessage;
}

vector<JsonReader::StructuredError> JsonReader::GetErrorStructures() const {
    vector<JsonReader::StructuredError> allErrors;
    for (deque<ErrorInfo>::const_iterator itError = errors.begin();
        itError != errors.end();
        ++itError) {
        const ErrorInfo& error = *itError;
        JsonReader::StructuredError structured;
        structured.offset_start = error.token.start - begin;
        structured.offset_limit = error.token.end - begin;
        structured.message = error.message;
        allErrors.push_back(structured);
    }
    return allErrors;
}

istream& operator>>(istream& sin, JsonValue& root) {
    JsonReader reader;
    bool ok = reader.Parse(sin, root, true);
    if (!ok) {
        THROW_EXCEPTION(JsonFormatException, reader.GetErrorMessages());
    }
    return sin;
}

static const unsigned char kNull[sizeof(JsonValue)] = { 0 };
const JsonValue& JsonValue::null = reinterpret_cast<const JsonValue&>(kNull[0]);
const int JsonValue::minInt = int(~(unsigned int(-1) / 2));
const int JsonValue::maxInt = int(unsigned int(-1) / 2);
const unsigned int JsonValue::maxUInt = unsigned int(-1);

template <typename T, typename U>
static inline bool InRange(double d, T min, U max) {
    return d >= min && d <= max;
}

JsonValue::CommentInfo::CommentInfo() : comment() {}

JsonValue::CommentInfo::~CommentInfo() {
}

void JsonValue::CommentInfo::setComment(const string& text) {
    comment = text;
    assert(comment.empty() || comment[0] == '/' || comment[0] == '\0');
}

JsonValue::JsonValue(DataType type)
    : type(type), valueObject(nullptr), comments(nullptr), start(0), limit(0) {
    switch (type) {
    case DATA_NULL:
        break;
    case DATA_INT:
    case DATA_UINT:
        value.valueInt = 0;
        break;
    case DATA_DOUBLE:
        value.valueDouble = 0.0;
        break;
    case DATA_STRING:
        valueString.clear();
        break;
    case DATA_ARRAY:
    case DATA_OBJECT:
        // 使用自定义比较函数创建 map
        valueObject = new map<string, JsonValue, StringCompare>();
        break;
    case DATA_BOOL:
        value.valueBool = false;
        break;
    default:
        assert(false);
    }
}

JsonValue::JsonValue(int value)
    : type(DATA_INT), valueObject(nullptr), comments(nullptr), start(0), limit(0) {
    this->value.valueInt = value;
}

JsonValue::JsonValue(unsigned int value)
    : type(DATA_UINT), valueObject(nullptr), comments(nullptr), start(0), limit(0) {
    this->value.valueUint = value;
}

JsonValue::JsonValue(long long value)
    : type(DATA_INT), valueObject(nullptr), comments(nullptr), start(0), limit(0) {
    this->value.valueInt = value;
}

JsonValue::JsonValue(unsigned long long value)
    : type(DATA_UINT), valueObject(nullptr), comments(nullptr), start(0), limit(0) {
    this->value.valueUint = value;
}

JsonValue::JsonValue(double value)
    : type(DATA_DOUBLE), valueObject(nullptr), comments(nullptr), start(0), limit(0) {
    this->value.valueDouble = value;
}

JsonValue::JsonValue(const char* value)
    : type(DATA_STRING), valueObject(nullptr), comments(nullptr), start(0), limit(0) {
    valueString = value ? value : "";
}

JsonValue::JsonValue(const char* beginValue, const char* endValue)
    : type(DATA_STRING), valueObject(nullptr), comments(nullptr), start(0), limit(0) {
    valueString.assign(beginValue, endValue);
}

JsonValue::JsonValue(const string& value)
    : type(DATA_STRING), valueObject(nullptr), comments(nullptr), start(0), limit(0) {
    valueString = value;
}

JsonValue::JsonValue(bool value)
    : type(DATA_BOOL), valueObject(nullptr), comments(nullptr), start(0), limit(0) {
    this->value.valueBool = value;
}

JsonValue::JsonValue(const JsonValue& other)
    : type(other.type), valueObject(nullptr), comments(nullptr),
    start(other.start), limit(other.limit) {
    // 复制基本类型值
    value = other.value;

    // 复制字符串
    if (type == DATA_STRING) {
        valueString = other.valueString;
    }

    // 复制对象/数组
    if (type == DATA_ARRAY || type == DATA_OBJECT) {
        // 使用拷贝构造函数，会自动使用相同的比较函数
        valueObject = new map<string, JsonValue, StringCompare>(*other.valueObject);
    }

    // 复制注释
    if (other.comments) {
        comments = new CommentInfo[COMMENT_MULTIPLE];
        for (int comment = 0; comment < COMMENT_MULTIPLE; ++comment) {
            const CommentInfo& otherComment = other.comments[comment];
            comments[comment].setComment(otherComment.comment);
        }
    }
}

JsonValue::~JsonValue() {
    // 删除对象/数组
    if (valueObject) {
        delete valueObject;
    }

    // 删除注释
    if (comments) {
        delete[] comments;
    }
}

JsonValue& JsonValue::operator=(const JsonValue& other) {
    JsonValue temp(other);
    swap(temp);
    return *this;
}

void JsonValue::swap(JsonValue& other) {
    // 交换基本类型值
    std::swap(value, other.value);

    // 交换字符串
    std::swap(valueString, other.valueString);

    // 交换对象指针
    std::swap(valueObject, other.valueObject);

    // 交换其他成员
    DataType temp = type;
    type = other.type;
    other.type = temp;
    std::swap(comments, other.comments);
    std::swap(start, other.start);
    std::swap(limit, other.limit);
}

DataType JsonValue::GetType() const { return type; }

int JsonValue::compare(const JsonValue& other) const {
    if (*this < other)
        return -1;
    if (*this > other)
        return 1;
    return 0;
}

bool JsonValue::operator<(const JsonValue& other) const {
    int typeDelta = type - other.type;
    if (typeDelta)
        return typeDelta < 0 ? true : false;
    switch (type) {
    case DATA_NULL:
        return false;
    case DATA_INT:
        return value.valueInt < other.value.valueInt;
    case DATA_UINT:
        return value.valueUint < other.value.valueUint;
    case DATA_DOUBLE:
        return value.valueDouble < other.value.valueDouble;
    case DATA_BOOL:
        return value.valueBool < other.value.valueBool;
    case DATA_STRING:
        return valueString < other.valueString;
    case DATA_ARRAY:
    case DATA_OBJECT:
        // 现在 map 使用自定义比较函数，直接比较 map 即可
        return (*valueObject) < (*other.valueObject);
    default:
        assert(false);
    }
    return false;
}

bool JsonValue::operator<=(const JsonValue& other) const {
    return !(other < *this);
}

bool JsonValue::operator>=(const JsonValue& other) const {
    return !(*this < other);
}

bool JsonValue::operator>(const JsonValue& other) const {
    return other < *this;
}

bool JsonValue::operator==(const JsonValue& other) const {
    int temp = other.type;
    if (type != temp)
        return false;
    switch (type) {
    case DATA_NULL:
        return true;
    case DATA_INT:
        return value.valueInt == other.value.valueInt;
    case DATA_UINT:
        return value.valueUint == other.value.valueUint;
    case DATA_DOUBLE:
        return value.valueDouble == other.value.valueDouble;
    case DATA_BOOL:
        return value.valueBool == other.value.valueBool;
    case DATA_STRING:
        return valueString == other.valueString;
    case DATA_ARRAY:
    case DATA_OBJECT:
        return (*valueObject) == (*other.valueObject);
    default:
        assert(false);
    }
    return false;
}

bool JsonValue::operator!=(const JsonValue& other) const { return !(*this == other); }

string JsonValue::AsString() const {
    switch (type) {
    case DATA_NULL:
        return "";
    case DATA_STRING:
        return valueString;
    case DATA_BOOL:
        return value.valueBool ? "true" : "false";
    case DATA_INT:
        return to_string(value.valueInt);
    case DATA_UINT:
        return to_string(value.valueUint);
    case DATA_DOUBLE:
        return to_string(value.valueDouble);
    default:
        THROW_EXCEPTION(JsonFormatException, "Type is not convertible to string");
    }
}

int JsonValue::AsInt() const {
    switch (type) {
    case DATA_INT:
        return int(value.valueInt);
    case DATA_UINT:
        return int(value.valueUint);
    case DATA_DOUBLE:
        return int(value.valueDouble);
    case DATA_NULL:
        return 0;
    case DATA_BOOL:
        return value.valueBool ? 1 : 0;
    default:
        break;
    }
    THROW_EXCEPTION(JsonFormatException, "Value is not convertible to Int.");
}

unsigned int JsonValue::AsUInt() const {
    switch (type) {
    case DATA_INT:
        return unsigned int(value.valueInt);
    case DATA_UINT:
        return unsigned int(value.valueUint);
    case DATA_DOUBLE:
        return unsigned int(value.valueDouble);
    case DATA_NULL:
        return 0;
    case DATA_BOOL:
        return value.valueBool ? 1 : 0;
    default:
        break;
    }
    THROW_EXCEPTION(JsonFormatException, "Value is not convertible to uInt.");
}

double JsonValue::AsDouble() const {
    switch (type) {
    case DATA_INT:
        return double(value.valueInt);
    case DATA_UINT:
        return double(value.valueUint);
    case DATA_DOUBLE:
        return value.valueDouble;
    case DATA_NULL:
        return 0.0;
    case DATA_BOOL:
        return value.valueBool ? 1.0 : 0.0;
    default:
        break;
    }
    THROW_EXCEPTION(JsonFormatException, "Value is not convertible to double.");
}

float JsonValue::AsFloat() const { return float(AsDouble()); }

bool JsonValue::AsBool() const {
    switch (type) {
    case DATA_BOOL:
        return value.valueBool;
    case DATA_NULL:
        return false;
    case DATA_INT:
        return value.valueInt ? true : false;
    case DATA_UINT:
        return value.valueUint ? true : false;
    case DATA_DOUBLE:
        return value.valueDouble ? true : false;
    default:
        break;
    }
    THROW_EXCEPTION(JsonFormatException, "Value is not convertible to bool.");
}

bool JsonValue::ConvertibleTo(DataType other) const {
    switch (other) {
    case DATA_NULL:
        return (IsNumeric() && AsDouble() == 0.0) ||
            (type == DATA_BOOL && value.valueBool == false) ||
            (type == DATA_STRING && AsString() == "") ||
            (type == DATA_ARRAY && valueObject && valueObject->size() == 0) ||
            (type == DATA_OBJECT && valueObject && valueObject->size() == 0) ||
            type == DATA_NULL;
    case DATA_INT:
        return IsInt() ||
            (type == DATA_DOUBLE && InRange(value.valueDouble, minInt, maxInt)) ||
            type == DATA_BOOL || type == DATA_NULL;
    case DATA_UINT:
        return IsUInt() ||
            (type == DATA_DOUBLE && InRange(value.valueDouble, 0, maxUInt)) ||
            type == DATA_BOOL || type == DATA_NULL;
    case DATA_DOUBLE:
        return IsNumeric() || type == DATA_BOOL || type == DATA_NULL;
    case DATA_BOOL:
        return IsNumeric() || type == DATA_BOOL || type == DATA_NULL;
    case DATA_STRING:
        return IsNumeric() || type == DATA_BOOL || type == DATA_STRING ||
            type == DATA_NULL;
    case DATA_ARRAY:
        return type == DATA_ARRAY || type == DATA_NULL;
    case DATA_OBJECT:
        return type == DATA_OBJECT || type == DATA_NULL;
    }
    assert(false);
    return false;
}

unsigned int JsonValue::size() const {
    switch (type) {
    case DATA_NULL:
    case DATA_INT:
    case DATA_UINT:
    case DATA_DOUBLE:
    case DATA_BOOL:
    case DATA_STRING:
        return 0;
    case DATA_ARRAY:
    case DATA_OBJECT:
        return valueObject ? (unsigned int)valueObject->size() : 0;
    }
    assert(false);
    return 0;
}

bool JsonValue::empty() const {
    if (IsNull() || IsArray() || IsObject())
        return size() == 0u;
    else
        return false;
}

bool JsonValue::operator!() const { return IsNull(); }

void JsonValue::clear() {
    assert(type == DATA_NULL || type == DATA_ARRAY || type == DATA_OBJECT);
    start = 0;
    limit = 0;
    switch (type) {
    case DATA_ARRAY:
    case DATA_OBJECT:
        if (valueObject) {
            valueObject->clear();
        }
        break;
    default:
        break;
    }
}

void JsonValue::resize(unsigned int newSize) {
    assert(type == DATA_NULL || type == DATA_ARRAY);
    if (type == DATA_NULL) {
        *this = JsonValue(DATA_ARRAY);
    }

    unsigned int oldSize = size();
    if (newSize == 0) {
        clear();
    }
    else if (newSize > oldSize) {
        (*this)[newSize - 1];
    }
    else {
        for (unsigned int index = newSize; index < oldSize; ++index) {
            if (valueObject) {
                valueObject->erase(to_string(index));
            }
        }
        assert(size() == newSize);
    }
}

JsonValue& JsonValue::operator[](unsigned int index) {
    assert(type == DATA_NULL || type == DATA_ARRAY);
    if (type == DATA_NULL) {
        *this = JsonValue(DATA_ARRAY);
    }

    string key = to_string(index);
    auto it = valueObject->find(key);
    if (it != valueObject->end()) {
        return it->second;
    }

    valueObject->insert({ key, JsonValue() });
    return valueObject->find(key)->second;
}

JsonValue& JsonValue::operator[](int index) {
    assert(index >= 0);
    return (*this)[unsigned int(index)];
}

const JsonValue& JsonValue::operator[](unsigned int index) const {
    assert(type == DATA_NULL || type == DATA_ARRAY);
    if (type == DATA_NULL) {
        return null;
    }

    string key = to_string(index);
    if (!valueObject) {
        return null;
    }

    auto it = valueObject->find(key);
    if (it == valueObject->end()) {
        return null;
    }
    return it->second;
}

const JsonValue& JsonValue::operator[](int index) const {
    assert(index >= 0);
    return (*this)[unsigned int(index)];
}

JsonValue& JsonValue::operator[](const char* key) {
    assert(type == DATA_NULL || type == DATA_OBJECT);
    if (type == DATA_NULL) {
        *this = JsonValue(DATA_OBJECT);
    }

    string actualKey(key);
    auto it = valueObject->find(actualKey);
    if (it != valueObject->end()) {
        return it->second;
    }

    valueObject->insert({ actualKey, JsonValue() });
    return valueObject->find(actualKey)->second;
}

JsonValue JsonValue::get(unsigned int index, const JsonValue& defaultValue) const {
    const JsonValue* value = &((*this)[index]);
    return value == &null ? defaultValue : *value;
}

bool JsonValue::ValidIndex(unsigned int index) const { return index < size(); }

const JsonValue& JsonValue::operator[](const char* key) const {
    assert(type == DATA_NULL || type == DATA_OBJECT);
    if (type == DATA_NULL) {
        return null;
    }

    if (!valueObject) {
        return null;
    }

    string actualKey(key);
    auto it = valueObject->find(actualKey);
    if (it == valueObject->end()) {
        return null;
    }
    return it->second;
}

JsonValue& JsonValue::operator[](const string& key) {
    return (*this)[key.c_str()];
}

const JsonValue& JsonValue::operator[](const string& key) const {
    return (*this)[key.c_str()];
}

JsonValue& JsonValue::append(const JsonValue& value) {
    return (*this)[size()] = value;
}

JsonValue JsonValue::get(const char* key, const JsonValue& defaultValue) const {
    const JsonValue* value = &((*this)[key]);
    return value == &null ? defaultValue : *value;
}

JsonValue JsonValue::get(const string& key, const JsonValue& defaultValue) const {
    return get(key.c_str(), defaultValue);
}

JsonValue JsonValue::RemoveMember(const string& key) {
    assert(type == DATA_NULL || type == DATA_OBJECT);
    if (type == DATA_NULL) {
        return null;
    }

    if (!valueObject) {
        return null;
    }

    auto it = valueObject->find(key);
    if (it == valueObject->end()) {
        return null;
    }

    JsonValue old(it->second);
    valueObject->erase(it);
    return old;
}

bool JsonValue::ValidMember(const string& key) const {
    const JsonValue* value = &((*this)[key]);
    return value != &null;
}

vector<string> JsonValue::GetMembers() const {
    assert(type == DATA_NULL || type == DATA_OBJECT);
    if (type == DATA_NULL || !valueObject) {
        return vector<string>();
    }

    vector<string> members;
    members.reserve(valueObject->size());
    for (auto it = valueObject->begin(); it != valueObject->end(); ++it) {
        members.push_back(it->first);
    }
    return members;
}

static bool IsIntegral(double d) {
    double integral_part;
    return modf(d, &integral_part) == 0.0;
}

bool JsonValue::IsNull() const {
    return type == DATA_NULL;
}

bool JsonValue::IsBool() const {
    return type == DATA_BOOL;
}

bool JsonValue::IsInt() const {
    switch (type) {
    case DATA_INT:
        return value.valueInt >= minInt && value.valueInt <= maxInt;
    case DATA_UINT:
        return value.valueUint <= maxInt;
    case DATA_DOUBLE:
        return value.valueDouble >= minInt && value.valueDouble <= maxInt &&
            ::IsIntegral(value.valueDouble);
    default:
        break;
    }
    return false;
}

bool JsonValue::IsUInt() const {
    switch (type) {
    case DATA_INT:
        return value.valueInt >= 0 && value.valueInt <= maxInt;
    case DATA_UINT:
        return value.valueUint <= maxUInt;
    case DATA_DOUBLE:
        return value.valueDouble >= 0 && value.valueDouble <= maxUInt &&
            ::IsIntegral(value.valueDouble);
    default:
        break;
    }
    return false;
}

bool JsonValue::IsIntegral() const {
    return IsInt() || IsUInt();
}
bool JsonValue::IsDouble() const {
    return type == DATA_DOUBLE || IsIntegral();
}
bool JsonValue::IsNumeric() const {
    return IsIntegral() || IsDouble();
}

bool JsonValue::IsString() const {
    return type == DATA_STRING;
}

bool JsonValue::IsArray() const {
    return type == DATA_ARRAY;
}

bool JsonValue::IsObject() const {
    return type == DATA_OBJECT;
}

void JsonValue::SetComment(const string& comment, CommentPlacement placement) {
    if (!comments) {
        comments = new CommentInfo[COMMENT_MULTIPLE];
    }
    comments[placement].setComment(comment);
}

bool JsonValue::HasComment(CommentPlacement placement) const {
    return comments != 0 && !comments[placement].comment.empty();
}

string JsonValue::GetComment(CommentPlacement placement) const {
    if (HasComment(placement)) {
        return comments[placement].comment;
    }
    return "";
}

void JsonValue::SetOffsetStart(size_t start) {
    this->start = start;
}

void JsonValue::SetOffsetLimit(size_t limit) {
    this->limit = limit;
}

size_t JsonValue::GetOffsetStart() const {
    return start;
}

size_t JsonValue::GetOffsetLimit() const {
    return limit;
}

string JsonValue::WriteCompact() const {
    FastWriter writer;
    return writer.Write(*this);
}

string JsonValue::WriteStyled() const {
    StyledWriter writer;
    return writer.Write(*this);
}

ValueConstIterator JsonValue::begin() const {
    switch (type) {
    case DATA_ARRAY:
    case DATA_OBJECT:
        if (valueObject)
            return ValueConstIterator(valueObject->begin());
        break;
    default:
        break;
    }
    return ValueConstIterator();
}

ValueConstIterator JsonValue::end() const {
    switch (type) {
    case DATA_ARRAY:
    case DATA_OBJECT:
        if (valueObject)
            return ValueConstIterator(valueObject->end());
        break;
    default:
        break;
    }
    return ValueConstIterator();
}

ValueIterator JsonValue::begin() {
    switch (type) {
    case DATA_ARRAY:
    case DATA_OBJECT:
        if (valueObject)
            return ValueIterator(valueObject->begin());
        break;
    default:
        break;
    }
    return ValueIterator();
}

ValueIterator JsonValue::end() {
    switch (type) {
    case DATA_ARRAY:
    case DATA_OBJECT:
        if (valueObject)
            return ValueIterator(valueObject->end());
        break;
    default:
        break;
    }
    return ValueIterator();
}

ValueIteratorBase::ValueIteratorBase()
    : current(), isNull(true) {
}

ValueIteratorBase::ValueIteratorBase(const map<string, JsonValue, StringCompare>::iterator& current)
    : current(current), isNull(false) {
}

JsonValue& ValueIteratorBase::deref() const {
    return current->second;
}

void ValueIteratorBase::increment() {
    ++current;
}

void ValueIteratorBase::decrement() {
    --current;
}

int ValueIteratorBase::distance(const ValueIteratorBase& other) const {
    if (isNull && other.isNull)
        return 0;

    int myDistance = 0;
    for (auto it = current; it != other.current; ++it)
        ++myDistance;
    return myDistance;
}

bool ValueIteratorBase::equal(const ValueIteratorBase& other) const {
    if (isNull)
        return other.isNull;
    return current == other.current;
}

void ValueIteratorBase::copy(const ValueIteratorBase& other) {
    current = other.current;
    isNull = other.isNull;
}

JsonValue ValueIteratorBase::key() const {
    return JsonValue(current->first);
}

unsigned int ValueIteratorBase::index() const {
    return -1;
}

const char* ValueIteratorBase::name() const {
    return current->first.c_str();
}

ValueConstIterator::ValueConstIterator() {
}

ValueConstIterator::ValueConstIterator(const map<string, JsonValue, StringCompare>::iterator& current)
    : ValueIteratorBase(current) {
}

ValueConstIterator& ValueConstIterator::operator=(const ValueIteratorBase& other) {
    copy(other);
    return *this;
}

ValueConstIterator ValueConstIterator::operator++(int) {
    ValueConstIterator temp(*this);
    ++*this;
    return temp;
}

ValueConstIterator ValueConstIterator::operator--(int) {
    ValueConstIterator temp(*this);
    --*this;
    return temp;
}

ValueConstIterator& ValueConstIterator::operator--() {
    decrement();
    return *this;
}

ValueConstIterator& ValueConstIterator::operator++() {
    increment();
    return *this;
}

const JsonValue& ValueConstIterator::operator*() const {
    return deref();
}

ValueIterator::ValueIterator() {
}

ValueIterator::ValueIterator(const ValueConstIterator& other)
    : ValueIteratorBase(other) {
}

ValueIterator::ValueIterator(const ValueIterator& other)
    : ValueIteratorBase(other) {
}

ValueIterator::ValueIterator(const map<string, JsonValue, StringCompare>::iterator& current)
    : ValueIteratorBase(current) {
}

ValueIterator& ValueIterator::operator=(const ValueIterator& other) {
    copy(other);
    return *this;
}

ValueIterator ValueIterator::operator++(int) {
    ValueIterator temp(*this);
    ++*this;
    return temp;
}

ValueIterator ValueIterator::operator--(int) {
    ValueIterator temp(*this);
    --*this;
    return temp;
}

ValueIterator& ValueIterator::operator--() {
    decrement();
    return *this;
}

ValueIterator& ValueIterator::operator++() {
    increment();
    return *this;
}

JsonValue& ValueIterator::operator*() const {
    return deref();
}

Writer::~Writer() {}

FastWriter::FastWriter() {
}

string FastWriter::Write(const JsonValue& root) {
    document = "";
    WriteValue(root);
    document += "\n";
    return document;
}

void FastWriter::WriteValue(const JsonValue& value) {
    switch (value.GetType()) {
    case DATA_NULL:
        document += "null";
        break;
    case DATA_INT:
        document += to_string(value.AsInt());
        break;
    case DATA_UINT:
        document += to_string(value.AsUInt());
        break;
    case DATA_DOUBLE:
        document += to_string(value.AsDouble());
        break;
    case DATA_STRING:
        document += "\"" + value.AsString() + "\"";
        break;
    case DATA_BOOL:
        document += value.AsBool() ? "true" : "false";
        break;
    case DATA_ARRAY: {
        document += "[";
        int size = value.size();
        for (int index = 0; index < size; ++index) {
            if (index > 0)
                document += ",";
            WriteValue(value[index]);
        }
        document += "]";
    } break;
    case DATA_OBJECT: {
        vector<string> members(value.GetMembers());
        document += "{";
        for (auto it = members.begin(); it != members.end(); ++it) {
            const string& name = *it;
            if (it != members.begin())
                document += ",";
            document += "\"" + name + "\"";
            document += ":";
            WriteValue(value[name]);
        }
        document += "}";
    } break;
    }
}

StyledWriter::StyledWriter()
    : rightMargin(74), indentSize(3), addChildValues() {
}

string StyledWriter::Write(const JsonValue& root) {
    document = "";
    addChildValues = false;
    indentString = "";
    WriteCommentBeforeValue(root);
    WriteValue(root);
    WriteCommentAfterValueOnSameLine(root);
    document += "\n";
    return document;
}

void StyledWriter::WriteValue(const JsonValue& value) {
    switch (value.GetType()) {
    case DATA_NULL:
        PushValue("null");
        break;
    case DATA_INT:
        PushValue(to_string(value.AsInt()));
        break;
    case DATA_UINT:
        PushValue(to_string(value.AsUInt()));
        break;
    case DATA_DOUBLE:
        PushValue(to_string(value.AsDouble()));
        break;
    case DATA_STRING:
        PushValue("\"" + value.AsString() + "\"");
        break;
    case DATA_BOOL:
        PushValue(value.AsBool() ? "true" : "false");
        break;
    case DATA_ARRAY:
        WriteArrayValue(value);
        break;
    case DATA_OBJECT: {
        vector<string> members(value.GetMembers());
        if (members.empty())
            PushValue("{}");
        else {
            WriteWithIndent("{");
            Indent();
            auto it = members.begin();
            for (;;) {
                const string& name = *it;
                const JsonValue& childValue = value[name];
                WriteCommentBeforeValue(childValue);
                WriteWithIndent("\"" + name + "\"");
                document += " : ";
                WriteValue(childValue);
                if (++it == members.end()) {
                    WriteCommentAfterValueOnSameLine(childValue);
                    break;
                }
                document += ",";
                WriteCommentAfterValueOnSameLine(childValue);
            }
            Unindent();
            WriteWithIndent("}");
        }
    } break;
    }
}

void StyledWriter::WriteArrayValue(const JsonValue& value) {
    unsigned size = value.size();
    if (size == 0)
        PushValue("[]");
    else {
        bool isArrayMultiLine = IsMultineArray(value);
        if (isArrayMultiLine) {
            WriteWithIndent("[");
            Indent();
            bool hasChildValue = !childValues.empty();
            unsigned index = 0;
            for (;;) {
                const JsonValue& childValue = value[index];
                WriteCommentBeforeValue(childValue);
                if (hasChildValue)
                    WriteWithIndent(childValues[index]);
                else {
                    WriteIndent();
                    WriteValue(childValue);
                }
                if (++index == size) {
                    WriteCommentAfterValueOnSameLine(childValue);
                    break;
                }
                document += ",";
                WriteCommentAfterValueOnSameLine(childValue);
            }
            Unindent();
            WriteWithIndent("]");
        }
        else {
            assert(childValues.size() == size);
            document += "[ ";
            for (unsigned index = 0; index < size; ++index) {
                if (index > 0)
                    document += ", ";
                document += childValues[index];
            }
            document += " ]";
        }
    }
}

bool StyledWriter::IsMultineArray(const JsonValue& value) {
    int size = value.size();
    bool isMultiLine = size * 3 >= rightMargin;
    childValues.clear();
    for (int index = 0; index < size && !isMultiLine; ++index) {
        const JsonValue& childValue = value[index];
        isMultiLine =
            isMultiLine || ((childValue.IsArray() || childValue.IsObject()) &&
                childValue.size() > 0);
    }

    if (!isMultiLine) {
        childValues.reserve(size);
        addChildValues = true;
        int lineLength = 4 + (size - 1) * 2;
        for (int index = 0; index < size; ++index) {
            WriteValue(value[index]);
            lineLength += int(childValues[index].length());
        }
        addChildValues = false;
        isMultiLine = isMultiLine || lineLength >= rightMargin;
    }
    return isMultiLine;
}

void StyledWriter::PushValue(const string& value) {
    if (addChildValues)
        childValues.push_back(value);
    else
        document += value;
}

void StyledWriter::WriteIndent() {
    if (!document.empty()) {
        char last = document[document.length() - 1];
        if (last == ' ')
            return;
        if (last != '\n')
            document += '\n';
    }
    document += indentString;
}

void StyledWriter::WriteWithIndent(const string& value) {
    WriteIndent();
    document += value;
}

void StyledWriter::Indent() {
    indentString += string(indentSize, ' ');
}

void StyledWriter::Unindent() {
    assert(int(indentString.size()) >= indentSize);
    indentString.resize(indentString.size() - indentSize);
}

void StyledWriter::WriteCommentBeforeValue(const JsonValue& root) {
    if (!root.HasComment(COMMENT_BEFORE))
        return;

    document += "\n";
    WriteIndent();
    string normalizedComment = NormalizeEOL(root.GetComment(COMMENT_BEFORE));
    for (size_t i = 0; i < normalizedComment.length(); ++i) {
        document += normalizedComment[i];
        if (normalizedComment[i] == '\n' && i + 1 < normalizedComment.length() &&
            normalizedComment[i + 1] == '/')
            WriteIndent();
    }
    document += "\n";
}

void StyledWriter::WriteCommentAfterValueOnSameLine(const JsonValue& root) {
    if (root.HasComment(COMMENT_LINE))
        document += " " + NormalizeEOL(root.GetComment(COMMENT_LINE));

    if (root.HasComment(COMMENT_AFTER)) {
        document += "\n";
        document += NormalizeEOL(root.GetComment(COMMENT_AFTER));
        document += "\n";
    }
}

bool StyledWriter::HasCommentForValue(const JsonValue& value) {
    return value.HasComment(COMMENT_BEFORE) ||
        value.HasComment(COMMENT_LINE) ||
        value.HasComment(COMMENT_AFTER);
}

string StyledWriter::NormalizeEOL(const string& text) {
    string normalized;
    normalized.reserve(text.length());
    const char* current = text.c_str();
    const char* end = current + text.length();

    while (current != end) {
        char c = *current++;
        if (c == '\r') {
            if (*current == '\n')
                ++current;
            normalized += '\n';
        }
        else
            normalized += c;
    }
    return normalized;
}

StyledStreamWriter::StyledStreamWriter(string indentation)
    : document(0), rightMargin(74), indentation(indentation), addChildValues() {
}

void StyledStreamWriter::Write(ostream& out, const JsonValue& root) {
    document = &out;
    addChildValues = false;
    indentString = "";
    WriteCommentBeforeValue(root);
    WriteValue(root);
    WriteCommentAfterValueOnSameLine(root);
    *document << "\n";
    document = 0;
}

void StyledStreamWriter::WriteValue(const JsonValue& value) {
    switch (value.GetType()) {
    case DATA_NULL:
        PushValue("null");
        break;
    case DATA_INT:
        PushValue(to_string(value.AsInt()));
        break;
    case DATA_UINT:
        PushValue(to_string(value.AsUInt()));
        break;
    case DATA_DOUBLE:
        PushValue(to_string(value.AsDouble()));
        break;
    case DATA_STRING:
        PushValue("\"" + value.AsString() + "\"");
        break;
    case DATA_BOOL:
        PushValue(value.AsBool() ? "true" : "false");
        break;
    case DATA_ARRAY:
        WriteArrayValue(value);
        break;
    case DATA_OBJECT: {
        vector<string> members(value.GetMembers());
        if (members.empty())
            PushValue("{}");
        else {
            WriteWithIndent("{");
            Indent();
            auto it = members.begin();
            for (;;) {
                const string& name = *it;
                const JsonValue& childValue = value[name];
                WriteCommentBeforeValue(childValue);
                WriteWithIndent("\"" + name + "\"");
                *document << " : ";
                WriteValue(childValue);
                if (++it == members.end()) {
                    WriteCommentAfterValueOnSameLine(childValue);
                    break;
                }
                *document << ",";
                WriteCommentAfterValueOnSameLine(childValue);
            }
            Unindent();
            WriteWithIndent("}");
        }
    } break;
    }
}

void StyledStreamWriter::WriteArrayValue(const JsonValue& value) {
    unsigned size = value.size();
    if (size == 0)
        PushValue("[]");
    else {
        bool isArrayMultiLine = IsMultineArray(value);
        if (isArrayMultiLine) {
            WriteWithIndent("[");
            Indent();
            bool hasChildValue = !childValues.empty();
            unsigned index = 0;
            for (;;) {
                const JsonValue& childValue = value[index];
                WriteCommentBeforeValue(childValue);
                if (hasChildValue)
                    WriteWithIndent(childValues[index]);
                else {
                    WriteIndent();
                    WriteValue(childValue);
                }
                if (++index == size) {
                    WriteCommentAfterValueOnSameLine(childValue);
                    break;
                }
                *document << ",";
                WriteCommentAfterValueOnSameLine(childValue);
            }
            Unindent();
            WriteWithIndent("]");
        }
        else {
            assert(childValues.size() == size);
            *document << "[ ";
            for (unsigned index = 0; index < size; ++index) {
                if (index > 0)
                    *document << ", ";
                *document << childValues[index];
            }
            *document << " ]";
        }
    }
}

bool StyledStreamWriter::IsMultineArray(const JsonValue& value) {
    int size = value.size();
    bool isMultiLine = size * 3 >= rightMargin;
    childValues.clear();
    for (int index = 0; index < size && !isMultiLine; ++index) {
        const JsonValue& childValue = value[index];
        isMultiLine =
            isMultiLine || ((childValue.IsArray() || childValue.IsObject()) &&
                childValue.size() > 0);
    }

    if (!isMultiLine) {
        childValues.reserve(size);
        addChildValues = true;
        int lineLength = 4 + (size - 1) * 2;
        for (int index = 0; index < size; ++index) {
            WriteValue(value[index]);
            lineLength += int(childValues[index].length());
        }
        addChildValues = false;
        isMultiLine = isMultiLine || lineLength >= rightMargin;
    }
    return isMultiLine;
}

void StyledStreamWriter::PushValue(const string& value) {
    if (addChildValues)
        childValues.push_back(value);
    else
        *document << value;
}

void StyledStreamWriter::WriteIndent() {
    *document << '\n' << indentString;
}

void StyledStreamWriter::WriteWithIndent(const string& value) {
    WriteIndent();
    *document << value;
}

void StyledStreamWriter::Indent() {
    indentString += indentation;
}

void StyledStreamWriter::Unindent() {
    assert(indentString.size() >= indentation.size());
    indentString.resize(indentString.size() - indentation.size());
}

void StyledStreamWriter::WriteCommentBeforeValue(const JsonValue& root) {
    if (!root.HasComment(COMMENT_BEFORE))
        return;
    *document << NormalizeEOL(root.GetComment(COMMENT_BEFORE));
    *document << "\n";
}

void StyledStreamWriter::WriteCommentAfterValueOnSameLine(const JsonValue& root) {
    if (root.HasComment(COMMENT_LINE))
        *document << " " + NormalizeEOL(root.GetComment(COMMENT_LINE));

    if (root.HasComment(COMMENT_AFTER)) {
        *document << "\n";
        *document << NormalizeEOL(root.GetComment(COMMENT_AFTER));
        *document << "\n";
    }
}

bool StyledStreamWriter::HasCommentForValue(const JsonValue& value) {
    return value.HasComment(COMMENT_BEFORE) ||
        value.HasComment(COMMENT_LINE) ||
        value.HasComment(COMMENT_AFTER);
}

string StyledStreamWriter::NormalizeEOL(const string& text) {
    string normalized;
    normalized.reserve(text.length());
    const char* current = text.c_str();
    const char* end = current + text.length();

    while (current != end) {
        char c = *current++;
        if (c == '\r') {
            if (*current == '\n')
                ++current;
            normalized += '\n';
        }
        else
            normalized += c;
    }
    return normalized;
}

ostream& operator<<(ostream& sout, const JsonValue& root) {
    StyledStreamWriter writer;
    writer.Write(sout, root);
    return sout;
}

