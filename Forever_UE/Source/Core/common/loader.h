#pragma once

#include <string>
#include <vector>
#include <unordered_map>


// 21个concept对应的DLL导出符号名,和7个domain的完整对照表见同目录loader.md。
// Config::AddDllPath用getModSymbol探测某个dll是否是合法mod;ModLoader::RegisterConcept
// 用registerSymbol/finishSymbol把探测通过的dll真正接入某个具体Factory。
struct ModConceptDescriptor {
	const char* conceptKey;
	const char* getModSymbol;
	const char* registerModSymbol;
	const char* finishModSymbol;
};

const std::vector<ModConceptDescriptor>& GetModConceptDescriptors();

// 不是per-concept的，整个DLL只有一份导出符号——由Config::AddDllPath在探测21个concept符号
// 的同一次LoadLibrary/FreeLibrary里顺带探测，和上面GetModConceptDescriptors()的20行表无关
// (表的行形状是3个per-concept符号，容不下这第4种符号)。但返回内容不是"整个DLL共用一份
// 依赖列表"，而是"这个DLL里每个mod id各自的依赖列表"配对表(vector<pair<modId,
// vector<depId>>>，静态对象指针，调用方只读不释放，和GetMod<Concept>()同一套约定)——一个
// DLL哪怕注册了好几个互不相关的mod，也能让各自声明不同的依赖，不需要共用一份。modId本身
// 全局唯一(CONVENTIONS.md §5)，不需要再带concept前缀区分。给游戏启动配置界面的依赖校验
// 用，见config.md。
inline constexpr const char* kModDependenciesSymbol = "GetModDependencies";

// 不在头文件里include windows.h,避免和UE头文件的宏产生冲突——句柄一律存成void*,
// LoadLibrary/GetProcAddress/FreeLibrary只在loader.cpp里出现。
//
// 参数(config.json里"<concept>_mods"数组每项"id 参数..."中的参数部分)不经过ModLoader——
// 那是按mod id配置的,而ModLoader只按dll路径工作、不知道一个dll会注册哪些id。参数改由
// 调用方在RegisterConcept之前调用Factory::SetModArgs(id->参数表)预先设置好,mod调用
// Factory::Register<Concept>(id, ...)时Factory自己按id查表、存进注册项,Create<Concept>(id)
// 创建实例时把参数字符串直接传给注册的creator函数。详见 Source/Dependence/README.md。
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

	// 这个dll的句柄当前是不是已经加载并缓存(LoadLibrary失败的路径返回false)。
	bool IsLoaded(const std::string& dllPath) const { return modHandles.count(dllPath) > 0; }

private:
	void* LoadHandle(const std::string& dllPath);
	static void* GetSymbol(void* handle, const char* name);

	std::unordered_map<std::string, void*> modHandles;
};
