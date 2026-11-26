#include "error.h"

#include <cstring>
#include <new>


using namespace std;

const char* const kErrorScopeMod = "mod";
const char* const kErrorScopeFile = "file";
const char* const kErrorScopeStep = "step";
const char* const kErrorScopeScript = "script";

const char* ErrorKindName(ErrorKind kind) noexcept {
	switch (kind) {
	case ErrorKind::Data: return "data";
	case ErrorKind::Mod: return "mod";
	case ErrorKind::Internal: return "internal";
	}
	return "internal";
}

ExceptionBase::ExceptionBase(ErrorKind kind, const string& message,
	const string& file, int line, const string& function) noexcept :
	kind(kind),
	message(message),
	file(file),
	line(line),
	function(function) {
	whatCache = message + " [at " + file + ":" + to_string(line) + "]";
	if (!function.empty()) {
		whatCache += " in " + function;
	}
}

const char* ExceptionBase::what() const noexcept {
	return whatCache.data();
}

ErrorKind ExceptionBase::GetKind() const noexcept {
	return kind;
}

namespace {
	struct TraceFrame {
		string label;
		string detail;
	};

	// 每个模块各一份(Dependence是静态库，链接进谁就是谁的)。
	vector<TraceFrame>& Trace() noexcept {
		thread_local vector<TraceFrame> trace;
		return trace;
	}

	string TrimTrailingNewlines(string text) {
		while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' ')) text.pop_back();
		return text;
	}

	// 路径只留最后两段，日志里够定位又不会太长。
	string ShortPath(const string& path) {
		size_t cut = string::npos;
		int seen = 0;
		for (size_t i = path.size(); i > 0; i--) {
			if (path[i - 1] == '/' || path[i - 1] == '\\') {
				if (++seen == 2) { cut = i; break; }
			}
		}
		return cut == string::npos ? path : path.substr(cut);
	}
}

ErrorScope::ErrorScope(const char* label, const char* detail) noexcept :
	label(label), detail(detail), uncaughtAtEntry(uncaught_exceptions()) {
	// 正常路径上又进了一段新工作，而轨迹里还有东西：那是上一次异常留下的(接住它的地方没取走)，已经过期。
	if (uncaughtAtEntry == 0) {
		vector<TraceFrame>& trace = Trace();
		if (!trace.empty()) trace.clear();
	}
}

ErrorScope::ErrorScope(const char* label, const string& detail) noexcept :
	ErrorScope(label, detail.c_str()) {
}

ErrorScope::~ErrorScope() {
	if (uncaught_exceptions() <= uncaughtAtEntry) return;
	// 有异常正穿过这个scope。记不下来(内存不够)就算了，不能在展开途中再抛。
	try {
		Trace().push_back(TraceFrame{ label ? label : "", detail ? detail : "" });
	}
	catch (...) {}
}

void ClearErrorTrace() noexcept {
	Trace().clear();
}

string ErrorReport::ToLine() const {
	string line = string("[") + ErrorKindName(kind) + "] " + message;
	if (!context.empty()) {
		line += " (";
		for (size_t i = 0; i < context.size(); i++) {
			if (i > 0) line += " < ";
			line += context[i];
		}
		line += ")";
	}
	return line;
}

string ErrorReport::ToText() const {
	string text;
	switch (kind) {
	case ErrorKind::Data: text = "数据错误"; break;
	case ErrorKind::Mod: text = "mod错误"; break;
	case ErrorKind::Internal: text = "内部错误"; break;
	}
	if (!mod.empty()) text += "（mod: " + mod + "）";
	text += "：" + message;
	if (!location.empty()) text += "\n  抛出位置：" + location;
	if (!context.empty()) {
		text += "\n  当时正在：";
		// 从外到里读起来更顺：先是哪个阶段，再是哪个mod、哪个文件。
		for (size_t i = context.size(); i > 0; i--) {
			text += context[i - 1];
			if (i > 1) text += " > ";
		}
	}
	return text;
}

ErrorReport DescribeCurrentException() noexcept {
	ErrorReport report;
	try {
		vector<TraceFrame> frames;
		frames.swap(Trace());
		bool inMod = false;
		for (const TraceFrame& frame : frames) {
			if (frame.label == kErrorScopeMod && !inMod) {
				inMod = true;
				report.mod = frame.detail;
			}
			report.context.push_back(frame.detail.empty() ? frame.label : frame.label + " " + frame.detail);
		}

		// 把当前异常原样再抛一次，按类型分别取信息。只读what()/GetKind()/几个const引用，不拷贝异常对象：
		// 它可能是别的模块(mod dll)抛的，拷贝和析构都得留给那个模块自己的代码。
		try {
			throw;
		}
		catch (const ExceptionBase& e) {
			report.kind = e.GetKind();
			report.message = TrimTrailingNewlines(e.GetMessage().c_str());
			report.location = ShortPath(e.GetFile().c_str()) + ":" + to_string(e.GetLine());
			if (!e.GetFunction().empty()) report.location += string(" in ") + e.GetFunction().c_str();
		}
		catch (const bad_alloc&) {
			report.kind = ErrorKind::Internal;
			report.message = "out of memory";
		}
		catch (const exception& e) {
			report.kind = inMod ? ErrorKind::Mod : ErrorKind::Internal;
			const char* what = e.what();
			report.message = TrimTrailingNewlines(what ? what : "");
			if (report.message.empty()) report.message = "exception with no message";
		}
		catch (...) {
			report.kind = inMod ? ErrorKind::Mod : ErrorKind::Internal;
			report.message = "unknown exception (not derived from std::exception)";
		}
	}
	catch (...) {
		// 描述异常时自己又出错(几乎只可能是内存不够)：报告留空也要正常返回。
	}
	return report;
}
