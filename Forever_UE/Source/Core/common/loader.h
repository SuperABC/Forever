#pragma once

#include "common/mod_concepts.h"

#include <string>
#include <vector>
#include <unordered_map>


// 23个concept的导出符号表(GetModConceptDescriptors/kModDependenciesSymbol)在Dependence的common/mod_concepts.h。
// 不include windows.h避免和UE宏冲突，句柄一律存成void*。mod参数不经过ModLoader，见loader.md
class ModLoader {
public:
	ModLoader();
	~ModLoader();

	// 对每个dll解析并调用registerSymbol/finishSymbol；句柄按路径缓存，多个concept复用同一dll不会重复LoadLibrary
	template <typename FactoryT>
	void RegisterConcept(const std::vector<std::string>& dllPaths,
		const char* registerSymbol, const char* finishSymbol, FactoryT* factory) {
		for (const std::string& dllPath : dllPaths) {
			void* handle = LoadHandle(dllPath);
			if (!handle) continue;

			auto registerFunc = reinterpret_cast<void(*)(FactoryT*)>(GetSymbol(handle, registerSymbol));
			if (registerFunc) registerFunc(factory);

			auto finishFunc = reinterpret_cast<void(*)(FactoryT*)>(GetSymbol(handle, finishSymbol));
			if (finishFunc) finishFunc(factory);
		}
	}

	void UnloadAll();
	// 卸载一个dll(没加载过就什么都不做)。调用前必须已经把它的注册项从各Factory注销，见Registry::UnloadMods。
	void Unload(const std::string& dllPath);

	bool IsLoaded(const std::string& dllPath) const { return modHandles.count(dllPath) > 0; }

private:
	void* LoadHandle(const std::string& dllPath);
	static void* GetSymbol(void* handle, const char* name);

	std::unordered_map<std::string, void*> modHandles;
};
