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

	// 内部cache直接构造，不走mod系统(见storage.md)，categories留空：
	// 物流靠SetAccess/SetProductDemand驱动，不靠Storage::AcceptsCategory()。
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

	// Step1(估算用): 假设全部配置的targets原样满批达成算副产品量，只供Step2判断能覆盖几整批，
	// 不是最终byproducts(最终值见Step1b)。
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

	// Step1b: 按净活跃批次数重算byproducts，并记下每个target各自的贡献(byproductsByTarget)。
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

	// 原料cache只能放(需求方)，精确product需求=各原料占单日总量的比例；产出cache只能取(供给方)。
	inputCache->SetAccess(false, true);
	vector<pair<string, float>> demand;
	if (inputSize > 0.f) {
		vector<string> types;
		for (const auto& [type, amount] : ingredients) types.push_back(type);
		sort(types.begin(), types.end());
		for (const string& type : types) demand.push_back({ type, ingredients.at(type) / inputSize });
	}
	inputCache->SetProductDemand(demand);
	outputCache->SetAccess(true, false);
}

void Manufacture::SeedOutputCache() {
	for (const auto& [target, activeN] : targets) {
		Product* p = industry->FindProduct(target);
		if (!p) continue;
		float half = floor(activeN * p->GetBatchSize() / 2.f);
		if (half > 0.f) outputCache->Input(target, half);
	}
}

void Manufacture::WorkAccount() {
	lastProduced.clear();
	for (const auto& [target, batches] : pendingProduction) {
		if (batches <= 0) continue;
		auto targetIt = targets.find(target);
		if (targetIt == targets.end() || targetIt->second <= 0) continue;

		Product* p = industry->FindProduct(target);
		if (!p) continue;

		float actualRatio = static_cast<float>(batches) / static_cast<float>(targetIt->second);
		outputCache->Input(target, batches * p->GetBatchSize());
		lastProduced[target] += batches * p->GetBatchSize();

		auto contribIt = byproductsByTarget.find(target);
		if (contribIt != byproductsByTarget.end()) {
			for (const auto& [bp, fullAmount] : contribIt->second) {
				outputCache->Input(bp, fullAmount * actualRatio);
				lastProduced[bp] += fullAmount * actualRatio;
			}
		}
	}

	pendingProduction.clear();
}

void Manufacture::StartProduce(float capacityRatio) {
	if (targets.empty()) return;

	// 原料限制：按所有原料里最紧张的那个比例整体限制全部target（不按target分摊，见manufacture.md待办）。
	float materialRatio = 1.f;
	for (const auto& [ingredient, standard] : ingredients) {
		if (standard <= 0.f) continue;
		float have = inputCache->GetAmount(ingredient);
		materialRatio = min(materialRatio, max(0.f, have / (standard * capacityRatio)));
	}

	float outputStandard = 0.f;
	for (const auto& [target, activeN] : targets) {
		Product* p = industry->FindProduct(target);
		if (p) outputStandard += activeN * p->GetBatchSize();
	}
	for (const auto& [type, amount] : byproducts) outputStandard += amount;
	outputStandard *= capacityRatio;

	float spaceRatio = 1.f;
	if (outputStandard > 0.f) {
		spaceRatio = max(0.f, min(1.f, outputCache->GetSpace() / outputStandard));
	}

	float overallRatio = min(materialRatio, spaceRatio);

	pendingProduction.clear();
	for (const auto& [target, activeN] : targets) {
		int actualBatches = static_cast<int>(floor(activeN * capacityRatio * overallRatio));
		actualBatches = max(0, actualBatches);
		if (actualBatches > 0) pendingProduction[target] = actualBatches;
	}

	// 按overallRatio预扣原料，让下一轮StartProduce()看到准确的当前存量。
	for (const auto& [ingredient, standard] : ingredients) {
		float consume = standard * capacityRatio * overallRatio;
		if (consume > 0.f) inputCache->Output(ingredient, consume);
	}
}
