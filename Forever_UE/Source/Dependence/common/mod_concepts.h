#pragma once

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

// 整个dll只有一份的导出符号，返回这个dll里每个mod id各自的依赖列表(std::vector<std::pair<modId, std::vector<depId>>>*)，
// 静态对象指针、调用方只读不释放；不在GetModConceptDescriptors表里(表的行是3个per-concept符号)。
inline constexpr const char* kModDependenciesSymbol = "GetModDependencies";

// concept名 -> config.json里的key："Terrains"->"terrain_mods"，"Parties"->"party_mods"(ies结尾改y，其余去掉末尾的s，再转小写)。
std::string ModConceptJsonKey(const std::string& conceptKey);
