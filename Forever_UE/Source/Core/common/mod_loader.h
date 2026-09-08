#pragma once

#include <string>
#include <vector>
#include <unordered_map>

// 21个concept对应的DLL导出符号名,和8个domain/概念的完整对照表见同目录mod_loader.md。
// Config::AddDllPath用getModSymbol探测某个dll是否是合法mod;ModLoader::RegisterConcept
// 用registerSymbol/finishSymbol把探测通过的dll真正接入某个具体Factory。
struct ModConceptDescriptor {
	const char* conceptKey;
	const char* getModSymbol;
	const char* registerModSymbol;
	const char* finishModSymbol;
};

const std::vector<ModConceptDescriptor>& GetModConceptDescriptors();

// 不在头文件里include windows.h,避免和UE头文件的宏产生冲突——句柄一律存成void*,
// LoadLibrary/GetProcAddress/FreeLibrary只在mod_loader.cpp里出现。
//
// 参数(config.json里"<concept>_mods"数组每项"id 参数..."中的参数部分)不经过ModLoader——
// 那是按mod id配置的,而ModLoader只按dll路径工作、不知道一个dll会注册哪些id。参数改由
// 调用方在RegisterConcept之前调用Factory::SetModArgs(id->参数表)预先设置好,mod调用
// Factory::Register<Concept>(id, ...)时Factory自己按id查表、存进注册项,创建实例时再调用
// instance->ApplyArgs(...)。详见 Source/Dependence/README.md。
class ModLoader {
public:
	ModLoader();
	~ModLoader();

	// 对dllPaths里的每个dll,尝试解析registerSymbol/finishSymbol并调用,把该dll实现的mod
	// 注册进factory。dll句柄按路径缓存在modHandles里,同一个dll被多个concept复用时不会
	// 重复LoadLibrary。
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

	// 释放所有缓存句柄,调用方在用完这一批mod后应调用一次(见ForeverModSubsystem.cpp)。
	void UnloadAll();

private:
	void* LoadHandle(const std::string& dllPath);
	static void* GetSymbol(void* handle, const char* name);

	std::unordered_map<std::string, void*> modHandles;
};
