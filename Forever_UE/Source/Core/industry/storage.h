#pragma once

#include "industry/storage_mod.h"
#include "industry/storage_factory.h"

#include <string>
#include <unordered_map>
#include <vector>


// Storage：一个仓库(或者工坊自带的输入/输出cache，见manufacture.h)的实体——只暴露
// "能装什么"+"当前装了多少"，不知道自己的上下游/位置。老工程的仓库还按产品类型固定
// 一对upstreams/downstreams指针，这次不要——运输改成Industry统一做的"全城统一调配"
// (见industry.md)，Storage只是被动地被Input()/Output()读写，见storage.md。
class Storage {
public:
	// 走mod系统创建的普通仓库——id需要在config.json的"storage_mods"里启用。
	Storage(StorageFactory* factory, const std::string& id, const std::string& name);

	// 直接构造，不经过mod系统——专供Manufacture创建自己的内部输入/输出cache用
	// (见manufacture.md)。这两个cache是"生产逻辑的暂存中转"，不是玩家能配置的内容，
	// 老工程用一个内置的"empty"mod id来创建，但那要求这个id同时被注册且在config.json
	// 里启用，属于给纯内部实现细节强加一层没必要的mod配置；这次直接给categories/
	// capacity构造，跳过mod系统。
	Storage(const std::string& name, const std::vector<std::string>& categories, float capacity);

	~Storage(); // 只有mod驱动的构造才需要factory->DestroyStorage(mod)

	// mod驱动的构造：mod为空(id没有被注册/没有在config.json"storage_mods"里启用)
	// 说明创建失败，调用方应当整个丢弃这个Storage。直接构造的Storage总是有效。
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

private:
	StorageFactory* factory = nullptr;
	StorageMod* mod = nullptr;
	bool valid = true;
	std::string name;

	std::vector<std::string> categories;
	float capacity = 0.f;
	std::unordered_map<std::string, float> stock;
};
