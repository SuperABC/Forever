#pragma once

#include "industry/product.h"
#include "industry/storage.h"
#include "industry/manufacture.h"

#include "common/class.h"
#include "common/handle.h"

#include <string>
#include <unordered_map>

struct ScriptContext;

// Industry：工业系统域类，持有Product类型目录+Storage/Manufacture两张表，驱动每天的
// "结算→全城统一调配→算今天产量"三阶段。照抄Traffic持有Vehicle的模式(见
// traffic/traffic.h/.md)，不知道UE Actor的存在。
//
// 这次迁移自老工程(E:/Projects/Forever_UE)的工业系统，但用户明确要求几处关键改动：
// "工厂"改叫"工坊"(避免和XxxFactory这个mod管理器术语混淆)；老工程按距离就近静态
// 建图的仓库运输改成这次的"全城统一调配"(GlobalAllocate，每天重新算一遍谁收谁发，
// 完全不考虑物理距离)；老工程的"单位配方+连续效率"改成"批次配方+整数批次"。测试
// 阶段不绑定房间，见industry.md。
class Industry {
public:
	Industry();
	~Industry(); // delete全部products/storages/manufactures

	// 目录性质：同一个产品type只会真正创建一次，重复调用直接返回已有的那份，不会
	// 重复new。创建失败(id未注册/未在config.json"product_mods"里启用)返回nullptr。
	Product* CreateProduct(const std::string& modId);
	Product* FindProduct(const std::string& type) const;

	// 按name索引的普通仓库，创建失败(id未注册/未启用)返回nullptr，不会把半成品塞进
	// storages表。同名已存在会先delete旧的，这一阶段不要求全局唯一性校验之外的东西，
	// 纯测试用途，照抄Traffic::CreateVehicle的模式。
	Storage* CreateStorage(const std::string& modId, const std::string& name);
	void DestroyStorage(const std::string& name);
	Storage* FindStorageByName(const std::string& name) const;

	Manufacture* CreateManufacture(const std::string& modId, const std::string& name);
	void DestroyManufacture(const std::string& name);
	Manufacture* FindManufactureByName(const std::string& name) const;

	// 跨天(crossedDay)时按"全体WorkAccount→全城统一调配→全体StartProduce"的顺序
	// 跑一遍，见industry.md"每天三阶段"一节。非跨天的普通帧不做任何事——这次没有
	// "每帧都要处理"的逻辑，不像Populace那样还有个按秒触发的timer集合。
	void Tick(const Time& currentTime, bool crossedDay, PostHandle* post);

	// 阶段占位：目前没有任何Change子类是Industry域自己认识、需要处理的，空实现——
	// 和其余五个域一样不打"未实现"警告，唯一的兜底警告在Story::ApplyChange。
	void ApplyChange(const Change* change, const ScriptContext& context);

private:
	// 全城统一调配核心算法：对系统里出现过的每一种产品type分别做一轮分配，见
	// industry.md"全城统一调配"一节。
	void GlobalAllocate();

	ProductFactory& productFactory;
	StorageFactory& storageFactory;
	ManufactureFactory& manufactureFactory;

	std::unordered_map<std::string, Product*> products;        // 目录，key=产品type
	std::unordered_map<std::string, Storage*> storages;         // key=name，普通仓库
	std::unordered_map<std::string, Manufacture*> manufactures; // key=name
};
