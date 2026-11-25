#pragma once

#include "common/random.h"
#include "common/utility.h"

#include <string>
#include <vector>


// 宿主和mod dll之间的导出符号约定，设计见mod_concepts.md。
// 每个concept三个导出函数：Get(返回本dll提供的mod id列表)、Register(把creator/deleter注册进宿主的Factory)、Finish(注册收尾)。
struct ModConceptDescriptor {
	const char* conceptKey;        // "Terrains"这种复数名，也是配置界面按钮的名字
	const char* getModSymbol;      // void* GetModTerrains()，返回static std::vector<std::string>*
	const char* registerModSymbol; // void RegisterModTerrains(TerrainFactory*)
	const char* finishModSymbol;   // void FinishModTerrains(TerrainFactory*)
};

// 全部23个concept，顺序即配置界面Mod screen按钮的显示顺序(人为分组，不是字母序)。
const std::vector<ModConceptDescriptor>& GetModConceptDescriptors();
// 按conceptKey("Terrains")找，没有返回nullptr。
const ModConceptDescriptor* FindModConceptDescriptor(const std::string& conceptKey);

// 整个dll只有一份的导出符号，返回这个dll里每个mod id各自的依赖列表(std::vector<std::pair<modId, std::vector<depId>>>*)，
// 静态对象指针、调用方只读不释放；不在GetModConceptDescriptors表里(表的行是3个per-concept符号)。
inline constexpr const char* kModDependenciesSymbol = "GetModDependencies";

// ---- 宿主绑定 ----
// 每个mod dll静态链接各自一份Dependence，随机流的host、debugf的输出函数在dll里都是它自己的那份。
// 宿主加载dll后通过dll导出的BindModHost把自己的交过去：GetRandom才受种子控制，debugf才进宿主的日志。
struct ModHostBinding {
	RandomHost* random = nullptr;  // 见common/random.h
	DebugSink debugSink = nullptr; // 见common/utility.h的debugf；nullptr=dll用默认的(写标准错误)
};
// 调用方所在模块当前的host和输出函数，宿主加载mod dll后把它传给BindModHost。
inline ModHostBinding CurrentModHostBinding() { return { GetRandomHost(), GetDebugSink() }; }

// 每个mod dll的入口cpp写一行 FOREVER_MOD_BIND_HOST 即可(导出BindModHost)。
#define FOREVER_MOD_BIND_HOST \
	extern "C" __declspec(dllexport) void BindModHost(const ModHostBinding* binding) { \
		if (!binding) return; \
		if (binding->random) SetRandomHost(binding->random); \
		SetDebugSink(binding->debugSink); \
	}

inline constexpr const char* kBindModHostSymbol = "BindModHost";

// concept名 -> config.json里的key："Terrains"->"terrain_mods"，"Parties"->"party_mods"(ies结尾改y，其余去掉末尾的s，再转小写)。
std::string ModConceptJsonKey(const std::string& conceptKey);

// 不加载、不执行，只读文件判断一个dll是不是mod dll(解析PE的导出表/导入表)，见mod_concepts.md"探测"。
// 扫描目录找mod时先用它筛：只有isMod且不是isDebug的dll才值得LoadLibrary，无关dll的代码一行都不会被执行。
struct ModDllProbe {
	bool readable = false; // 是能解析的64位PE文件(读不了/不是PE/32位都是false)
	bool isMod = false;    // 导出了至少一个GetMod<Concepts>符号
	bool isDebug = false;  // 导入了调试版CRT(MSVCP140D/VCRUNTIME140D/ucrtbased...)：std::string布局和Release不兼容，不能加载
};
ModDllProbe ProbeModDll(const std::string& path);
