#include "error.h"


using namespace std;

ExceptionBase::ExceptionBase(const string& message,
	const string& file, int line, const string& function) noexcept :
	message(message),
	file(file),
	line(line),
	function(function) {
	FormatWhat();
}

const char* ExceptionBase::what() const noexcept {
	return whatCache.data();
}

string ExceptionBase::GetDetailedInfo() const {
	ostringstream oss;
	oss << "Exception: " << message << "\n"
		<< "File: " << file << "\n"
		<< "Line: " << line << "\n"
		<< "Function: " << (function.empty() ? "Unknown" : function);
	return oss.str();
}

void ExceptionBase::FormatWhat() {
	whatCache = message + " [at " + file + ":" + to_string(line) + "]";
	if (!function.empty()) {
		whatCache += " in " + function;
	}
}
