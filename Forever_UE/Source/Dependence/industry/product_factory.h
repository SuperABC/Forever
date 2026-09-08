#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "industry/product_mod.h"

// 阶段3骨架:最小注册表(创建/销毁/枚举),真正的Temp暂存/合并逻辑(旧工程的
// MergeTemp/CleanTemp两段式注册)留到阶段4按需恢复,详见 Source/Core/README.md。
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
// "product_mods"数组解析出的(id, 参数字符串)表交给Factory,一直保留在
// configuredArgs里(不并入registries,避免同一份参数存两份);CreateProduct创建实例后
// 直接按id查configuredArgs、调用instance->ApplyArgs(),不需要mod自己关心参数从哪来。
class ProductFactory {
public:
	using CreateFunc = ProductMod*(*)();
	using DestroyFunc = void(*)(ProductMod*);

	ProductFactory() = default;
	virtual ~ProductFactory() = default;

	virtual void RegisterProduct(const std::string& id, CreateFunc creator, DestroyFunc deleter);

	// 阶段3占位:Mod导出的FinishModProducts(factory)按老约定会调用它,先留空实现。
	virtual void CleanTemp();

	virtual ProductMod* CreateProduct(const std::string& id);

	// 必须走注册时mod提供的deleter释放,不能Factory直接delete——跨DLL new/delete安全,
	// 通过liveInstances反查实例对应的注册id、再取出对应deleter调用。
	virtual void DestroyProduct(ProductMod* instance);

	virtual bool CheckRegistered(const std::string& id) const;
	virtual std::vector<std::string> GetRegisteredIds() const;

	// 设置这个concept从config.json"product_mods"数组解析出的(id, 参数字符串)表,
	// 必须在调用方触发mod的RegisterModProducts导出函数之前调用,否则CreateProduct
	// 拿实例创建时查不到对应参数。
	virtual void SetModArgs(const std::unordered_map<std::string, std::string>& argsById);

private:
	struct Entry {
		CreateFunc creator;
		DestroyFunc deleter;
	};

	std::unordered_map<std::string, Entry> registries;
	std::unordered_map<ProductMod*, std::string> liveInstances;
	std::unordered_map<std::string, std::string> configuredArgs;
};
