#include "industry/manufacture.h"

#include "industry/industry.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <unordered_set>

using namespace std;

Manufacture::Manufacture(ManufactureFactory* factory, const string& id, const string& name,
	Industry* industry) :
	industry(industry), factory(factory), name(name) {
	mod = factory->CreateManufacture(id);
	inputCache = nullptr;
	outputCache = nullptr;
	if (!mod) return;

	// 内部cache直接构造，不走mod系统(见storage.md)，categories留空——
	// Industry::GlobalAllocate()对工坊cache是直接查Manufacture自己的
	// ingredients/targets/byproducts表来判断"缺哪种类型"，不靠Storage::AcceptsCategory()。
	inputCache = new Storage(name + "#input", {}, 0.f);
	outputCache = new Storage(name + "#output", {}, 0.f);

	mod->SetTargets();
	ComputeRecipe();
}

Manufacture::~Manufacture() {
	delete inputCache;
	delete outputCache;
	if (mod) factory->DestroyManufacture(mod);
}

bool Manufacture::IsValid() const { return mod != nullptr; }

const string& Manufacture::GetName() const { return name; }

Storage* Manufacture::GetInputCache() const { return inputCache; }
Storage* Manufacture::GetOutputCache() const { return outputCache; }

const unordered_map<string, float>& Manufacture::GetIngredientNeeds() const { return ingredients; }

void Manufacture::ComputeRecipe() {
	targets.clear();
	ingredients.clear();
	byproducts.clear();
	byproductsByTarget.clear();

	if (mod->targets.empty()) {
		inputCache->SetCapacity(0.f);
		outputCache->SetCapacity(0.f);
		return;
	}

	unordered_set<string> targetTypes;
	for (const auto& [target, N] : mod->targets) targetTypes.insert(target);

	// Step1(估算用): 假设全部配置的targets原样满批达成,算出全部副产品的绝对产出量，
	// 只用来给Step2算"副产物能覆盖几整批"，不直接作为最终的byproducts结果——真正的
	// byproducts要等Step2把targets收敛成"净活跃批次数"之后，按活跃批次重新算一遍
	// (见下面的Step1b)，否则会把"本来就没有真的去生产"的那部分批次的副产品也算进去。
	unordered_map<string, float> estimatedByproducts;
	for (const auto& [target, N] : mod->targets) {
		Product* p = industry->FindProduct(target);
		if (!p) continue;
		for (const auto& [bp, ratePerBatch] : p->GetByproducts()) {
			estimatedByproducts[bp] += ratePerBatch * N;
		}
	}

	// Step2: 副产物覆盖抵消，把"净目标批次数"降成整数。
	for (const auto& [target, N] : mod->targets) {
		Product* p = industry->FindProduct(target);
		if (!p || p->GetBatchSize() <= 0.f) continue;
		float coverage = estimatedByproducts.count(target) ? estimatedByproducts.at(target) : 0.f;
		int coveredBatches = static_cast<int>(floor(coverage / p->GetBatchSize()));
		int activeN = max(0, N - coveredBatches);
		if (activeN > 0) targets[target] = activeN;
	}

	// Step1b: 用Step2收敛出的"净活跃批次数"重新算一遍byproducts，这次是真正要用的结果，
	// 同时记下每个target各自贡献了多少(byproductsByTarget)，供WorkAccount()按"这一轮
	// 实际产出批次/净活跃批次"的比例结算副产品用。
	unordered_map<string, float> allByproducts;
	for (const auto& [target, activeN] : targets) {
		Product* p = industry->FindProduct(target);
		if (!p) continue;
		unordered_map<string, float> contribution;
		for (const auto& [bp, ratePerBatch] : p->GetByproducts()) {
			float amount = ratePerBatch * activeN;
			allByproducts[bp] += amount;
			contribution[bp] = amount;
		}
		byproductsByTarget[target] = contribution;
	}

	// Step3: 递归展开原料需求——原料本身也是本工坊某个target时继续展开，否则算作
	// 外部原料，按绝对量累加。
	unordered_map<string, float> rawIngredients;
	function<void(const string&, float)> expand = [&](const string& productType, float absoluteQuantity) {
		Product* p = industry->FindProduct(productType);
		if (!p || p->GetBatchSize() <= 0.f) return;
		float batches = absoluteQuantity / p->GetBatchSize();
		for (const auto& [ingredient, ratePerBatch] : p->GetIngredients()) {
			float need = ratePerBatch * batches;
			if (targetTypes.count(ingredient)) {
				expand(ingredient, need);
			} else {
				rawIngredients[ingredient] += need;
			}
		}
	};
	for (const auto& [target, activeN] : targets) {
		Product* p = industry->FindProduct(target);
		if (!p) continue;
		expand(target, activeN * p->GetBatchSize());
	}

	// Step4: 副产物再次抵消外部原料需求。
	for (const auto& [ingredient, needed] : rawIngredients) {
		float covered = allByproducts.count(ingredient) ? allByproducts.at(ingredient) : 0.f;
		float actual = max(0.f, needed - covered);
		if (actual > 0.f) ingredients[ingredient] = actual;
	}

	byproducts = allByproducts;

	float inputSize = 0.f;
	for (const auto& [type, amount] : ingredients) inputSize += amount;

	float outputSize = 0.f;
	for (const auto& [target, activeN] : targets) {
		Product* p = industry->FindProduct(target);
		if (p) outputSize += activeN * p->GetBatchSize();
	}
	for (const auto& [type, amount] : byproducts) outputSize += amount;

	inputCache->SetCapacity(inputSize);
	outputCache->SetCapacity(outputSize);
}

void Manufacture::WorkAccount() {
	for (const auto& [target, batches] : pendingProduction) {
		if (batches <= 0) continue;
		auto targetIt = targets.find(target);
		if (targetIt == targets.end() || targetIt->second <= 0) continue;

		Product* p = industry->FindProduct(target);
		if (!p) continue;

		float actualRatio = static_cast<float>(batches) / static_cast<float>(targetIt->second);
		outputCache->Input(target, batches * p->GetBatchSize());

		auto contribIt = byproductsByTarget.find(target);
		if (contribIt != byproductsByTarget.end()) {
			for (const auto& [bp, fullAmount] : contribIt->second) {
				outputCache->Input(bp, fullAmount * actualRatio);
			}
		}
	}

	pendingProduction.clear();
}

void Manufacture::StartProduce() {
	if (targets.empty()) return;

	// 原料限制：这一轮先按"所有原料里最紧张的那个比例"整体限制全部target——这是这次
	// 优先跑通简单案例(单target/无中间产品展开)的简化版本，测试案例(农场/牧场/食品
	// 加工厂)都只有一个target，这个简化和"精确到每个target分摊"在这些案例上等价。
	// 更复杂的多target互相抵消场景要不要精确到按target分摊，留到后续更复杂的测试
	// 案例再验证，见manufacture.md。
	float materialRatio = 1.f;
	for (const auto& [ingredient, standard] : ingredients) {
		if (standard <= 0.f) continue;
		float have = inputCache->GetAmount(ingredient);
		materialRatio = min(materialRatio, max(0.f, have / standard));
	}

	float outputStandard = 0.f;
	for (const auto& [target, activeN] : targets) {
		Product* p = industry->FindProduct(target);
		if (p) outputStandard += activeN * p->GetBatchSize();
	}
	for (const auto& [type, amount] : byproducts) outputStandard += amount;

	float spaceRatio = 1.f;
	if (outputStandard > 0.f) {
		spaceRatio = max(0.f, min(1.f, outputCache->GetSpace() / outputStandard));
	}

	float overallRatio = min(materialRatio, spaceRatio);

	pendingProduction.clear();
	for (const auto& [target, activeN] : targets) {
		int actualBatches = static_cast<int>(floor(activeN * overallRatio));
		actualBatches = max(0, min(actualBatches, activeN));
		if (actualBatches > 0) pendingProduction[target] = actualBatches;
	}

	// 按overallRatio预扣inputCache里对应原料——为下一轮StartProduce()腾出准确的
	// "当前存量"基准，同老工程的"预扣"设计。
	for (const auto& [ingredient, standard] : ingredients) {
		float consume = standard * overallRatio;
		if (consume > 0.f) inputCache->Output(ingredient, consume);
	}
}
