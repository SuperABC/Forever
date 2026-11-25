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
	// 每个DLL静态链接各自一份Dependence：把Core的随机host交给它，GetRandom才会用到Core的流。没导出就退回不可控随机。
	if (auto bind = reinterpret_cast<void(*)(RandomHost*)>(GetProcAddress(handle, kBindRandomHostSymbol))) {
		bind(GetRandomHost());
	}
	return handle;
}

void* ModLoader::GetSymbol(void* handle, const char* name) {
	return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle), name));
}

void ModLoader::UnloadAll() {
	for (auto& [path, handle] : modHandles) {
		FreeLibrary(static_cast<HMODULE>(handle));
	}
	modHandles.clear();
}
