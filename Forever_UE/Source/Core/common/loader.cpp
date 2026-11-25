#include "loader.h"

#include "common/random.h"

#include <windows.h>


using namespace std;

ModLoader::ModLoader() {
}

ModLoader::~ModLoader() {
	UnloadAll();
}

void* ModLoader::LoadHandle(const string& dllPath) {
	auto it = modHandles.find(dllPath);
	if (it != modHandles.end())
		return it->second;

	HMODULE handle = LoadLibraryA(dllPath.data());
	if (!handle)
		return nullptr;

	modHandles[dllPath] = handle;
	// 每个DLL静态链接各自一份Dependence：把宿主的随机host和debugf输出函数交给它，GetRandom才会用到Core的流、
	// 它的debugf才进宿主的日志。没导出就退回不可控随机+写标准错误。
	if (auto bind = reinterpret_cast<void(*)(const ModHostBinding*)>(GetProcAddress(handle, kBindModHostSymbol))) {
		const ModHostBinding binding = CurrentModHostBinding();
		bind(&binding);
	}
	return handle;
}

void* ModLoader::GetSymbol(void* handle, const char* name) {
	return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle), name));
}

void ModLoader::Unload(const string& dllPath) {
	auto it = modHandles.find(dllPath);
	if (it == modHandles.end()) return;
	FreeLibrary(static_cast<HMODULE>(it->second));
	modHandles.erase(it);
}

void ModLoader::UnloadAll() {
	for (auto& [path, handle] : modHandles) {
		FreeLibrary(static_cast<HMODULE>(handle));
	}
	modHandles.clear();
}
