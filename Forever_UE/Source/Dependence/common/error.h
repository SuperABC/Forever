#pragma once

#include <exception>
#include <string>
#include <vector>

// 项目统一的异常体系，设计和使用规则见error.md、CONVENTIONS.md"异常"一节。
//
// 异常只分三类，按"接住之后该怎么办"分，不按"错误长什么样"分：
//   Data     外部数据写错了(config、layout、脚本、存档、路网文件、没启用的mod id……)。
//            接住后：这一次加载/这一条脚本指令失败，把文件和位置报给写数据的人。
//   Mod      mod的代码有问题(mod抛了异常，或者mod交给Core的参数自相矛盾)。
//            接住后：同上，并且指名是哪个mod。
//   Internal 程序自己的不变量被破坏了(Core/Dependence的bug)。
//            接住后：这一局结束，世界不再继续用。
enum class ErrorKind : int { Data = 0, Mod = 1, Internal = 2 };

// "data"/"mod"/"internal"，给日志和测试用。
const char* ErrorKindName(ErrorKind kind) noexcept;

class ExceptionBase : public std::exception {
public:
	ExceptionBase(ErrorKind kind, const std::string& message,
		const std::string& file, int line, const std::string& function = "") noexcept;

	virtual ~ExceptionBase() noexcept = default;

	// 只有这两个虚函数会被"另一个模块"调用(异常可能是mod dll里的Dependence代码抛的)：
	// 都不返回需要对方释放的东西。不要再加按值返回std::string的虚函数，见error.md"跨DLL"。
	const char* what() const noexcept override;
	virtual ErrorKind GetKind() const noexcept;

	// 下面几个只读、不分配。
	const std::string& GetMessage() const noexcept { return message; }
	const std::string& GetFile() const noexcept { return file; }
	int GetLine() const noexcept { return line; }
	const std::string& GetFunction() const noexcept { return function; }

protected:
	ErrorKind kind;
	std::string message;
	std::string file;
	int line;
	std::string function;
	std::string whatCache; // "消息 [at 文件:行] in 函数"，构造时拼好，what()直接返回
};

// 外部数据写错了。
class DataException : public ExceptionBase {
public:
	DataException(const std::string& message, const std::string& file, int line, const std::string& function = "") noexcept :
		ExceptionBase(ErrorKind::Data, message, file, line, function) {}
};

// mod的代码有问题。
class ModException : public ExceptionBase {
public:
	ModException(const std::string& message, const std::string& file, int line, const std::string& function = "") noexcept :
		ExceptionBase(ErrorKind::Mod, message, file, line, function) {}
};

// 程序自己的不变量被破坏了。
class InternalException : public ExceptionBase {
public:
	InternalException(const std::string& message, const std::string& file, int line, const std::string& function = "") noexcept :
		ExceptionBase(ErrorKind::Internal, message, file, line, function) {}
};

template<typename ExceptionClass>
[[noreturn]] void ThrowException(const std::string& message,
	const char* file,
	int line,
	const char* function = "") {
	throw ExceptionClass(message, file, line, function);
}

// THROW_EXCEPTION(DataException, "……")：自动带上文件、行号、函数名。
#define THROW_EXCEPTION(ExceptionClass, message) \
	ThrowException<ExceptionClass>(message, __FILE__, __LINE__, __FUNCTION__)

// ---- 出错时"当时正在做什么" ----
//
// 在进入一段有名字的工作时放一个ErrorScope：
//     ErrorScope scope(kErrorScopeMod, mod->GetType());
// 正常返回时它什么都不做(构造和析构各只有几条指令)；有异常穿过它时，它把自己记进当前线程的"展开轨迹"，
// 接住异常的地方用DescribeCurrentException()一并取出来。这样不用在每个调用点写try/catch，也能知道异常
// 来自哪个mod、哪个文件、哪个阶段。
//
// 轨迹是每个模块(exe/dll)各自一份的线程局部变量：ErrorScope和DescribeCurrentException要在同一个模块里用
// 才对得上。宿主(Core/UE/CoreTest)里放、宿主里取；mod自己不用放。
extern const char* const kErrorScopeMod;    // detail = mod id
extern const char* const kErrorScopeFile;   // detail = 文件路径
extern const char* const kErrorScopeStep;   // detail = 阶段/步骤名
extern const char* const kErrorScopeScript; // detail = 脚本里的位置(milestone名、change类型……)

class ErrorScope {
public:
	// label用上面的常量(或别的字符串字面量)；detail必须活得比这个scope久——只存指针，不拷贝。
	ErrorScope(const char* label, const char* detail) noexcept;
	ErrorScope(const char* label, const std::string& detail) noexcept;
	ErrorScope(const char* label, std::string&& detail) = delete; // 临时字符串会在这一行结束时销毁
	~ErrorScope();

	ErrorScope(const ErrorScope&) = delete;
	ErrorScope& operator=(const ErrorScope&) = delete;

private:
	const char* label;
	const char* detail;
	int uncaughtAtEntry;
};

// 宿主调mod的代码时用：
//     MOD_CALL(mod, mod->Layout(direction, *this, roads));
//     int count = MOD_CALL(mod, mod->Loop(canvas, ms, post));
// 等价于在这一次调用外面套一个ErrorScope(kErrorScopeMod, mod->GetType())，调用的返回值原样传出来。
// mod可以是任何带GetType()(返回const char*)的指针表达式；mod为空时不能用(和直接调用一样会崩)。
#define MOD_CALL(mod, ...) \
	([&]() -> decltype(auto) { ErrorScope modCallScope(kErrorScopeMod, (mod)->GetType()); return (__VA_ARGS__); })()

// 同MOD_CALL，但手里没有mod实例、只有mod id时用(工厂里调mod登记的函数指针)：id是活得够久的const std::string&。
#define MOD_ID_CALL(id, ...) \
	([&]() -> decltype(auto) { ErrorScope modCallScope(kErrorScopeMod, (id)); return (__VA_ARGS__); })()

// 一次异常的完整描述，全是本模块自己分配的字符串，可以随便拷贝、保存、跨线程传。
struct ErrorReport {
	ErrorKind kind = ErrorKind::Internal;
	std::string message;              // 异常自带的文字(末尾换行已去掉)
	std::string location;             // "文件:行 in 函数"；不是ExceptionBase的异常为空
	std::vector<std::string> context; // "当时正在做什么"，从最里层到最外层，例如"mod building_port"
	std::string mod;                  // 最里层那个mod的id，不在mod里出的错为空

	// 单行："[data] 消息 (mod building_port < step map.buildings)"。给界面、给存档读取的错误文本。
	std::string ToLine() const;
	// 多行，给日志。
	std::string ToText() const;
};

// 只能在catch块里调用：描述当前正在处理的那个异常，并取走(清空)本线程的展开轨迹。
// 分类规则：ExceptionBase用它自己的类别；其它异常(std::exception、未知类型)如果是在mod里抛的算Mod，否则算Internal。
// 接住异常后先调它，再做别的——别的代码如果又放了ErrorScope，旧轨迹会被当成过期的清掉。
ErrorReport DescribeCurrentException() noexcept;

// 丢弃本线程的展开轨迹。接住异常但不需要报告时调(否则下一个ErrorScope构造时也会清)。
void ClearErrorTrace() noexcept;

// 跑一段代码，任何异常都接住：成功返回true；失败返回false，report里是描述。
// 用在"会话边界"：接住之后这段代码动过的东西都不能再用(见error.md)。
template<typename Body>
bool GuardCall(Body&& body, ErrorReport& report) {
	try {
		body();
		return true;
	}
	catch (...) {
		report = DescribeCurrentException();
		return false;
	}
}

// 跑一段"可以整体跳过"的操作(一条脚本指令、一个资源文件)：只接数据错误，返回false；
// Mod错误和内部错误原样往外抛，交给会话边界。
template<typename Body>
bool GuardData(Body&& body, ErrorReport& report) {
	try {
		body();
		return true;
	}
	catch (const ExceptionBase& e) {
		if (e.GetKind() != ErrorKind::Data) throw;
		report = DescribeCurrentException();
		return false;
	}
}
