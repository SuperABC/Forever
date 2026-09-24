#pragma once

#include "player/weapon_mod.h"

#include <string>
#include <unordered_map>
#include <vector>


// 最小注册表(创建/销毁/枚举)——和AssetFactory/PuzzleFactory同一个模板，一字不差照抄
// （player域几个concept的Factory本来就长一个样，武器不需要Building那套Acreage/Power/
// Assign额外查询函数，照抄最简版即可）。
//
// creator/deleter用裸函数指针,不用std::function——mod侧注册的都是无捕获
// (capture-less)lambda,天然能隐式转换成函数指针;裸指针是POD类型,跨DLL传递没有
// "std::function内部堆缓冲由一侧分配、由另一侧释放"的风险(实测std::function版本会在
// Factory析构时崩溃,详见Source/Dependence/README.md)。
//
// 所有公开方法(含析构函数)都标记virtual——原因见Source/Dependence/README.md"关键设计"
// 一节:Mod DLL里调用这些方法时,只有virtual才能保证实际执行的是宿主(构造这个Factory实例
// 的那一侧)编译的那份代码,从而让registries内部节点的分配和后续析构使用同一侧的堆分配器,
// 避免跨DLL堆损坏。
//
// 参数传递:调用方在RegisterConcept之前调用SetModArgs,把从config.json
// "weapon_mods"数组解析出的(id, 参数字符串)表交给Factory,一直保留在
// configuredArgs里(不并入registries,避免同一份参数存两份);CreateWeapon创建实例时
// 直接按id查configuredArgs、把参数字符串传给creator,mod自己的creator决定怎么用。
class WeaponFactory {
public:
	using CreateFunc = WeaponMod*(*)(const std::string&);
	using DestroyFunc = void(*)(WeaponMod*);

	WeaponFactory() = default;
	virtual ~WeaponFactory() = default;

	virtual void RegisterWeapon(const std::string& id, CreateFunc creator, DestroyFunc deleter);

	virtual WeaponMod* CreateWeapon(const std::string& id);

	// 必须走注册时mod提供的deleter释放,不能Factory直接delete——跨DLL new/delete安全,
	// 通过liveInstances反查实例对应的注册id、再取出对应deleter调用。
	virtual void DestroyWeapon(WeaponMod* instance);

	virtual bool CheckRegistered(const std::string& id) const;
	virtual std::vector<std::string> GetRegisteredIds() const;

	// 设置这个concept从config.json"weapon_mods"数组解析出的(id, 参数字符串)表,
	// 必须在调用方触发mod的RegisterModWeapons导出函数之前调用,否则CreateWeapon
	// 拿实例创建时查不到对应参数。
	virtual void SetModArgs(const std::unordered_map<std::string, std::string>& argsById);

private:
	// 已注册且已在config.json对应"weapon_mods"数组里列出（即configuredArgs里
	// 有这个id）才算启用——CreateXxx/CheckRegistered/GetRegisteredIds都据此判断，
	// 见Source/Core/story/script.md"mod_dependences"一节的设计决策。
	bool IsEnabled(const std::string& id) const;

	struct Entry {
		CreateFunc creator;
		DestroyFunc deleter;
	};

	std::unordered_map<std::string, Entry> registries;
	std::unordered_map<WeaponMod*, std::string> liveInstances;
	std::unordered_map<std::string, std::string> configuredArgs;
};
