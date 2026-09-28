#include "industry/industry.h"

#include "common/registry.h"

#include <algorithm>
#include <vector>

using namespace std;

Industry::Industry() :
	productFactory(Registry::Get().GetProductFactory()),
	storageFactory(Registry::Get().GetStorageFactory()),
	manufactureFactory(Registry::Get().GetManufactureFactory()) {
}

Industry::~Industry() {
	for (auto& [name, manufacture] : manufactures) delete manufacture;
	for (auto& [name, storage] : storages) delete storage;
	for (auto& [type, product] : products) delete product;
}

Product* Industry::CreateProduct(const string& modId) {
	auto it = products.find(modId);
	if (it != products.end()) return it->second;

	Product* product = new Product(&productFactory, modId);
	if (!product->IsValid()) {
		delete product;
		return nullptr;
	}

	products.insert_or_assign(modId, product);
	return product;
}

Product* Industry::FindProduct(const string& type) const {
	auto it = products.find(type);
	return it != products.end() ? it->second : nullptr;
}

Storage* Industry::CreateStorage(const string& modId, const string& name) {
	auto it = storages.find(name);
	if (it != storages.end()) {
		delete it->second;
		storages.erase(it);
	}

	Storage* storage = new Storage(&storageFactory, modId, name);
	if (!storage->IsValid()) {
		delete storage;
		return nullptr;
	}

	storages.insert_or_assign(name, storage);
	return storage;
}

void Industry::DestroyStorage(const string& name) {
	auto it = storages.find(name);
	if (it == storages.end()) return;
	delete it->second;
	storages.erase(it);
}

Storage* Industry::FindStorageByName(const string& name) const {
	auto it = storages.find(name);
	return it != storages.end() ? it->second : nullptr;
}

Manufacture* Industry::CreateManufacture(const string& modId, const string& name) {
	auto it = manufactures.find(name);
	if (it != manufactures.end()) {
		delete it->second;
		manufactures.erase(it);
	}

	Manufacture* manufacture = new Manufacture(&manufactureFactory, modId, name, this);
	if (!manufacture->IsValid()) {
		delete manufacture;
		return nullptr;
	}

	manufactures.insert_or_assign(name, manufacture);
	return manufacture;
}

void Industry::DestroyManufacture(const string& name) {
	auto it = manufactures.find(name);
	if (it == manufactures.end()) return;
	delete it->second;
	manufactures.erase(it);
}

Manufacture* Industry::FindManufactureByName(const string& name) const {
	auto it = manufactures.find(name);
	return it != manufactures.end() ? it->second : nullptr;
}

void Industry::Tick(const Time& currentTime, bool crossedDay, PostHandle* post) {
	if (!crossedDay) return;

	// 阶段A：全体工坊先把上一轮算出的产量结算进各自的outputCache。
	for (auto& [name, manufacture] : manufactures) {
		manufacture->WorkAccount();
	}

	// 阶段B：全城统一调配，替代老工程按距离就近建的静态运输图。
	GlobalAllocate();

	// 阶段C：全体工坊用调配完之后的inputCache存量+outputCache剩余空间，算今天能产
	// 几批，为下一次跨天的WorkAccount()做准备。
	for (auto& [name, manufacture] : manufactures) {
		manufacture->StartProduce();
	}
}

void Industry::ApplyChange(const Change* change, const ScriptContext& context) {
	// 占位，等Industry域真的有需要处理的Change子类时再补，见industry.h声明处注释。
}

void Industry::GlobalAllocate() {
	// 收集系统里出现过的全部产品type：普通仓库当前存量里出现过的、工坊输出cache里
	// 出现过的、工坊净外部原料需求里出现过的——遍历一遍unordered_map，顺序就是它们
	// 各自的遍历顺序，不需要额外维护插入顺序的vector(用户已确认这个"固定顺序"不需要
	// 是创建顺序/字典序，只要同一次调配里遍历顺序确定即可)，见industry.md。
	// Storage不暴露"当前有哪些type"的遍历接口(见storage.h，只按需查GetAmount)，这里
	// 改成从Product目录+工坊配方表收集"系统里认识的type"——普通仓库只是被动存储，
	// 不会凭空出现目录之外的type。
	unordered_map<string, bool> seenTypes;
	auto markType = [&](const string& type) { seenTypes[type] = true; };

	for (const auto& [type, product] : products) {
		markType(type);
	}
	for (const auto& [name, manufacture] : manufactures) {
		for (const auto& [type, amount] : manufacture->GetIngredientNeeds()) {
			markType(type);
		}
	}

	for (const auto& [type, _] : seenTypes) {
		// sources：按storages/manufactures各自的遍历顺序，收集当前持有该type存量>0的cache。
		vector<Storage*> sources;
		for (const auto& [name, storage] : storages) {
			if (storage->GetAmount(type) > 0.f) sources.push_back(storage);
		}
		for (const auto& [name, manufacture] : manufactures) {
			Storage* out = manufacture->GetOutputCache();
			if (out->GetAmount(type) > 0.f) sources.push_back(out);
		}

		// destinations：普通仓库只要分类匹配且还有空间就是需求方(缺口=剩余空间，
		// 仓库对已接受的类型没有上限，能装多少要多少)；工坊的inputCache只有type在
		// 它自己的净外部原料需求表里才算需求方，缺口=需求量-当前存量。
		Product* product = FindProduct(type);
		vector<pair<Storage*, float>> destinations;
		if (product) {
			for (const auto& [name, storage] : storages) {
				if (!storage->AcceptsCategory(product->GetCategories())) continue;
				float space = storage->GetSpace();
				if (space > 0.f) destinations.push_back({ storage, space });
			}
		}
		for (const auto& [name, manufacture] : manufactures) {
			const auto& needs = manufacture->GetIngredientNeeds();
			auto needIt = needs.find(type);
			if (needIt == needs.end()) continue;
			Storage* in = manufacture->GetInputCache();
			float deficit = needIt->second - in->GetAmount(type);
			if (deficit > 0.f) destinations.push_back({ in, deficit });
		}

		// 按destinations固定顺序，依次从sources固定顺序里拿货，先到先得。
		for (auto& [dest, deficit] : destinations) {
			float remaining = deficit;
			for (Storage* src : sources) {
				if (remaining <= 0.f) break;
				if (src == dest) continue; // 自己不能当自己的补给来源，否则会白白吃掉deficit额度
				float avail = src->GetAmount(type);
				if (avail <= 0.f) continue;
				float actual = min(avail, remaining);
				src->Output(type, actual);
				dest->Input(type, actual);
				remaining -= actual;
			}
		}
	}
}
