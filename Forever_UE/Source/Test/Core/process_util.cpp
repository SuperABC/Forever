#include "process_util.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")

#include <cstdio>
#include <fstream>
#include <sstream>


using namespace std;

string SelfPath() {
	char buffer[MAX_PATH * 2];
	DWORD length = GetModuleFileNameA(nullptr, buffer, static_cast<DWORD>(sizeof(buffer)));
	return string(buffer, length);
}

namespace {
	string Quote(const string& text) {
		if (text.find_first_of(" \t\"") == string::npos && !text.empty()) return text;
		string out = "\"";
		for (char c : text) {
			if (c == '"') out += "\\\"";
			else out += c;
		}
		return out + "\"";
	}
}

int RunSelf(const vector<string>& args, const string& logPath, int timeoutSeconds) {
	SECURITY_ATTRIBUTES security = {};
	security.nLength = sizeof(security);
	security.bInheritHandle = TRUE;
	HANDLE log = CreateFileA(logPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &security, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (log == INVALID_HANDLE_VALUE) return -1;

	string commandLine = Quote(SelfPath());
	for (const string& arg : args) commandLine += " " + Quote(arg);

	STARTUPINFOA startup = {};
	startup.cb = sizeof(startup);
	startup.dwFlags = STARTF_USESTDHANDLES;
	startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	startup.hStdOutput = log;
	startup.hStdError = log;
	PROCESS_INFORMATION info = {};
	vector<char> mutableCommand(commandLine.begin(), commandLine.end());
	mutableCommand.push_back('\0');
	if (!CreateProcessA(nullptr, mutableCommand.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &startup, &info)) {
		CloseHandle(log);
		return -1;
	}
	CloseHandle(log);

	DWORD waited = WaitForSingleObject(info.hProcess, timeoutSeconds > 0 ? static_cast<DWORD>(timeoutSeconds) * 1000 : INFINITE);
	int result;
	if (waited == WAIT_TIMEOUT) {
		TerminateProcess(info.hProcess, 1);
		WaitForSingleObject(info.hProcess, 5000);
		result = -2;
	}
	else {
		DWORD code = 0;
		GetExitCodeProcess(info.hProcess, &code);
		result = static_cast<int>(code);
	}
	CloseHandle(info.hProcess);
	CloseHandle(info.hThread);
	return result;
}

string ReadWholeFile(const string& path) {
	ifstream in(path, ios::binary);
	if (!in) return "";
	ostringstream buffer;
	buffer << in.rdbuf();
	return buffer.str();
}

namespace {
	LONG WINAPI CrashFilter(EXCEPTION_POINTERS* info) {
		fprintf(stderr, "[CRASH] exception code 0x%08lx at %p\n", info->ExceptionRecord->ExceptionCode, info->ExceptionRecord->ExceptionAddress);
		CONTEXT context = *info->ContextRecord;
		STACKFRAME64 frame = {};
		frame.AddrPC.Offset = context.Rip; frame.AddrPC.Mode = AddrModeFlat;
		frame.AddrFrame.Offset = context.Rbp; frame.AddrFrame.Mode = AddrModeFlat;
		frame.AddrStack.Offset = context.Rsp; frame.AddrStack.Mode = AddrModeFlat;
		HANDLE process = GetCurrentProcess();
		HANDLE thread = GetCurrentThread();
		for (int i = 0; i < 40; i++) {
			if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread, &frame, &context, nullptr,
				SymFunctionTableAccess64, SymGetModuleBase64, nullptr)) break;
			if (frame.AddrPC.Offset == 0) break;
			HMODULE module = nullptr;
			GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				reinterpret_cast<LPCSTR>(frame.AddrPC.Offset), &module);
			char name[MAX_PATH] = "?";
			if (module) GetModuleFileNameA(module, name, MAX_PATH);
			fprintf(stderr, "[CRASH]   #%d %s +0x%llx\n", i, name, static_cast<unsigned long long>(frame.AddrPC.Offset - reinterpret_cast<unsigned long long>(module)));
		}
		fflush(stderr);
		return EXCEPTION_EXECUTE_HANDLER;
	}
}

void InstallCrashHandler() {
	SetUnhandledExceptionFilter(CrashFilter);
}
