#pragma once

#include "industry/manufacture_mod.h"
#include "industry/manufacture_factory.h"
#include "industry/storage.h"

#include <string>
#include <unordered_map>

class Industry;

// Manufacture(工坊)：实际生产东西的单位——"工厂"这个中文叫法在本项目里专指
// mod创建/销毁管理器(XxxFactory)，容易混淆，这个概念中文统一叫"工坊"，类名不变。
// 自带inputCache/outputCache两个内部Storage(直接构造，不走mod系统，见storage.md)，
// 负责当天原料/产出的暂存中转。核心是构造函数里ComputeRecipe()那套"目标产量→副产物
// 覆盖抵消→配方递归展开→再次副产物抵消"四步算法(批次化改造版，见manufacture.md)。
class Manufacture {
public:
	Manufacture(ManufactureFactory* factory, const std::string& id, const std::string& name,
		Industry* industry);
	~Manufacture(); // factory->DestroyManufacture(mod) + delete inputCache/outputCache

	// mod为空(id没有被注册/没有在config.json"manufacture_mods"里启用)说明创建失败，
	// 调用方应当整个丢弃这个Manufacture。
	bool IsValid() const;

	const std::string& GetName() const;

	Storage* GetInputCache() const;
	Storage* GetOutputCache() const;

	// 展开+抵消后的"每天净外部原料需求"(绝对量，假设今天全部活跃批次都能达成)——
	// Industry::GlobalAllocate()用这个减去inputCache当前存量算出"这个工坊的输入cache
	// 还缺多少某种原料"。
	const std::unordered_map<std::string, float>& GetIngredientNeeds() const;

	// 每日三阶段中的两步，第三步(全城统一调配)在Industry::GlobalAllocate()里对全部
	// Storage/Manufacture统一做，不是Manufacture自己的方法，见industry.md。

	// 阶段A：把上一轮StartProduce()算出的pendingProduction结算进outputCache——
	// 结算的产量/副产品都按"这一轮实际产出批次数 / 这一轮活跃目标批次数"的比例来，
	// 不是按理论满批量结算，见manufacture.md。
	void WorkAccount();

	// 阶段C：用inputCache当前存量(应该已经被本轮GlobalAllocate补过货)和outputCache
	// 剩余空间，算这一轮实际能产几批，把结果存进pendingProduction供下一轮WorkAccount
	// 结算，并按实际产量预扣inputCache里的原料。
	void StartProduce();

private:
	// 配方展开+副产物抵消算法，构造函数内部调，见manufacture.md。
	void ComputeRecipe();

	Industry* industry;
	ManufactureFactory* factory;
	ManufactureMod* mod;
	std::string name;

	Storage* inputCache;
	Storage* outputCache;

	std::unordered_map<std::string, int> targets;         // 副产物覆盖抵消后的"净目标批次数"(整数，>0才会出现在表里)
	std::unordered_map<std::string, float> ingredients;    // 展开+抵消后的"每天净外部原料需求"(绝对量)
	std::unordered_map<std::string, float> byproducts;     // 按targets(净活跃批次)算出的全部副产品绝对产出量
	std::unordered_map<std::string, std::unordered_map<std::string, float>> byproductsByTarget; // 每个target各自贡献了多少副产品，供WorkAccount按比例结算
	std::unordered_map<std::string, int> pendingProduction; // 上一轮StartProduce()算出的"这批能产几批"，下一轮WorkAccount()结算
};
