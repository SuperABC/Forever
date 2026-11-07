#pragma once

#include <string>
#include <vector>
#include <unordered_map>


// 21个concept的DLL导出符号名(concept->domain对照表见loader.md)
struct ModConceptDescriptor {
	const char* conceptKey;
	const char* getModSymbol;
	const char* registerModSymbol;
	const char* finishModSymbol;
};

const std::vector<ModConceptDescriptor>& GetModConceptDescriptors();

// 整个DLL只有一份的导出符号，返回这个DLL里每个mod id各自的依赖列表(vector<pair<modId, vector<depId>>>)，
// 静态对象指针、调用方只读不释放；不在GetModConceptDescriptors表里(表的行是3个per-concept符号)
inline constexpr const char* kModDependenciesSymbol = "GetModDependencies";

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

	bool IsLoaded(const std::string& dllPath) const { return modHandles.count(dllPath) > 0; }

private:
	void* LoadHandle(const std::string& dllPath);
	static void* GetSymbol(void* handle, const char* name);

	std::unordered_map<std::string, void*> modHandles;
};
