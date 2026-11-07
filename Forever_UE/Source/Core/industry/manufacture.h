#pragma once

#include "industry/manufacture_mod.h"
#include "industry/manufacture_factory.h"
#include "industry/storage.h"

#include <string>
#include <unordered_map>

class Industry;

// Manufacture(工坊)：实际生产东西的单位，自带inputCache/outputCache两个内部Storage。
// 配方展开算法与每日结算流程见manufacture.md。
class Manufacture {
	friend class IndustrySaveCodec;
public:
	Manufacture(ManufactureFactory* factory, const std::string& id, const std::string& name,
		Industry* industry);
	~Manufacture(); // factory->DestroyManufacture(mod) + delete inputCache/outputCache

	// mod为空(id没有注册/没有启用)说明创建失败，调用方应当整个丢弃这个Manufacture。
	bool IsValid() const;

	const std::string& GetName() const;

	Storage* GetInputCache() const;
	Storage* GetOutputCache() const;

	// 展开+抵消后的"每天净外部原料需求"(绝对量，假设全部活跃批次都能达成)。
	const std::unordered_map<std::string, float>& GetIngredientNeeds() const;

	// 新开局时把产出cache放进半天的产量(每个目标product：批数×批量÷2，取整)，读档不调用。
	void SeedOutputCache();

	// 阶段A：把上一轮StartProduce()算出的pendingProduction结算进outputCache。
	void WorkAccount();

	// 最近一次WorkAccount结算进产出cache的产量（含副产品，product type->数量），报表用。
	const std::unordered_map<std::string, float>& GetLastProduced() const { return lastProduced; }
	// 满负荷每天的净目标批数（product type->批数），乘以Product::GetBatchSize()是设计日产能。
	const std::unordered_map<std::string, int>& GetTargets() const { return targets; }

	// 阶段C：算这一轮实际能产几批存进pendingProduction，并预扣inputCache里的原料。
	// capacityRatio是产能比例(1=设计产能)，可以大于1；原料和产出空间不够时仍按比例压低。
	void StartProduce(float capacityRatio = 1.f);

private:
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
	std::unordered_map<std::string, float> lastProduced;
	std::unordered_map<std::string, int> pendingProduction; // 上一轮StartProduce()算出的"这批能产几批"，下一轮WorkAccount()结算
};
