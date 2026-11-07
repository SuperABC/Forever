#pragma once

#include "industry/storage_mod.h"
#include "industry/storage_factory.h"

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>


// Storage：一个仓库(或工坊自带的输入/输出cache)，被动地被Industry物流读写，见storage.md。
class Storage {
	friend class IndustrySaveCodec;
public:
	// 走mod系统创建的普通仓库——id需要在config.json的"storage_mods"里启用。
	Storage(StorageFactory* factory, const std::string& id, const std::string& name);

	// 直接构造，不经过mod系统，专供Manufacture创建内部cache，见storage.md。
	Storage(const std::string& name, const std::vector<std::string>& categories, float capacity);

	~Storage(); // 只有mod驱动的构造才需要factory->DestroyStorage(mod)

	// mod驱动的构造失败(id没有注册/没有启用)时为false，调用方应当丢弃；直接构造的总是有效。
	bool IsValid() const;

	const std::string& GetName() const;

	// 任一交集即可——productCategories来自某个Product::GetCategories()。
	bool AcceptsCategory(const std::vector<std::string>& productCategories) const;

	float GetAmount(const std::string& type) const;
	float GetSpace() const; // capacity - 所有类型存量之和(共享容量池)
	void SetCapacity(float newCapacity); // 供Manufacture在配方展开算法算出容量后调整

	// 按剩余空间截断，返回实际吃进的量(可能小于期望值)。
	float Input(const std::string& type, float amount);
	// 按当前存量截断，返回实际吐出的量(可能小于期望值)。
	float Output(const std::string& type, float amount);

	// 物流取放权限与补货需求，语义见storage_mod.h。直接构造的cache由Manufacture自己设置。
	bool CanTake() const { return canTake; }
	bool CanPut() const { return canPut; }
	void SetAccess(bool take, bool put) { canTake = take; canPut = put; }
	float GetCapacity() const { return capacity; }
	const std::unordered_map<std::string, float>& GetStock() const { return stock; }
	const std::vector<std::pair<std::string, float>>& GetServiceTypeDemand() const { return serviceTypeDemand; }
	const std::vector<std::pair<std::string, float>>& GetProductDemand() const { return productDemand; }
	void SetProductDemand(const std::vector<std::pair<std::string, float>>& demand) { productDemand = demand; serviceTypeDemand.clear(); }

private:
	StorageFactory* factory = nullptr;
	StorageMod* mod = nullptr;
	bool valid = true;
	std::string name;

	std::vector<std::string> categories;
	float capacity = 0.f;
	std::unordered_map<std::string, float> stock;

	bool canTake = true;
	bool canPut = true;
	std::vector<std::pair<std::string, float>> serviceTypeDemand;
	std::vector<std::pair<std::string, float>> productDemand;
};
