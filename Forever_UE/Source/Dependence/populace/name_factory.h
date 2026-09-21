#pragma once

#include "populace/name_mod.h"

#include <string>
#include <unordered_map>
#include <vector>


// 最小注册表(创建/销毁/枚举)+单选(SetConfig/GetName)——和roadnet_factory.h同一个模式：
// 一次只应该有一个取名算法生效(不是Terrain那种按GetPriority()多mod叠加)，不恢复旧工程
// 的Temp暂存/合并两段式注册，详见Source/Dependence/README.md。
//
// creator/deleter用裸函数指针,不用std::function——mod侧注册的都是无捕获
// (capture-less)lambda,天然能隐式转换成函数指针;裸指针是POD类型,跨DLL传递没有
// "std::function内部堆缓冲由一侧分配、由另一侧释放"的风险(实测std::function版本会在
// Factory析构时崩溃,详见Source/Dependence/README.md)。
//
// 所有公开方法(含析构函数)都标记virtual,即使目前没有任何派生类——原因见
// Source/Dependence/README.md"关键设计"一节:Mod DLL里调用这些方法时,只有virtual
// 才能保证实际执行的是宿主(构造这个Factory实例的那一侧)编译的那份代码,从而让
// registries内部节点的分配和后续析构使用同一侧的堆分配器,避免跨DLL堆损坏。
//
// 参数传递:调用方在RegisterConcept之前调用SetModArgs,把从config.json
// "name_mods"数组解析出的(id, 参数字符串)表交给Factory,一直保留在
// configuredArgs里(不并入registries,避免同一份参数存两份);CreateName创建实例时
// 直接按id查configuredArgs、把参数字符串传给creator,mod自己的creator决定怎么用
// (传给构造函数/自己存着都行)——Factory不再替mod调用任何"创建后初始化"钩子。
class NameFactory {
public:
	using CreateFunc = NameMod*(*)(const std::string&);
	using DestroyFunc = void(*)(NameMod*);

	NameFactory() = default;
	virtual ~NameFactory() = default;

	virtual void RegisterName(const std::string& id, CreateFunc creator, DestroyFunc deleter);

	virtual NameMod* CreateName(const std::string& id);

	// 必须走注册时mod提供的deleter释放,不能Factory直接delete——跨DLL new/delete安全,
	// 通过liveInstances反查实例对应的注册id、再取出对应deleter调用。
	virtual void DestroyName(NameMod* instance);

	virtual bool CheckRegistered(const std::string& id) const;
	virtual std::vector<std::string> GetRegisteredIds() const;

	// 设置这个concept从config.json"name_mods"数组解析出的(id, 参数字符串)表,
	// 必须在调用方触发mod的RegisterModNames导出函数之前调用,否则CreateName
	// 拿实例创建时查不到对应参数。
	virtual void SetModArgs(const std::unordered_map<std::string, std::string>& argsById);

	// 一次只应该有一个取名算法生效——SetConfig标记某个已注册id是否启用(config.json里
	// "name_mods"数组的条目)，GetName返回第一个被标记启用的id(找不到返回空字符串)。
	// 调用方(Populace::InitNames)按这个id唯一决定用哪个mod的取名算法，和
	// Map::InitRoadnet()/RoadnetFactory::GetRoadnet同一个模式。
	virtual void SetConfig(const std::string& id, bool enabled);
	virtual std::string GetName() const;

private:
	struct Entry {
		CreateFunc creator;
		DestroyFunc deleter;
	};

	std::unordered_map<std::string, Entry> registries;
	std::unordered_map<NameMod*, std::string> liveInstances;
	std::unordered_map<std::string, std::string> configuredArgs;
	std::unordered_map<std::string, bool> enabledConfig;
};
