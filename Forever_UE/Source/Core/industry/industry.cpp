#include "industry/industry.h"

#include "common/registry.h"

#include "map/component.h"
#include "map/room.h"

#include <algorithm>
#include <cmath>
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
	Product* product = new Product(&productFactory, modId);
	if (!product->IsValid()) {
		delete product;
		return nullptr;
	}

	// 目录key是product的type(FindProduct的查询键)，不假定mod id等于type。
	auto it = products.find(product->GetType());
	if (it != products.end()) {
		delete product;
		return it->second;
	}

	products.insert_or_assign(product->GetType(), product);
	return product;
}

void Industry::Init(const vector<Component*>& components) {
	vector<string> ids = productFactory.GetRegisteredIds();
	sort(ids.begin(), ids.end());
	for (const string& id : ids) CreateProduct(id);

	for (Component* component : components) {
		if (!component) continue;
		for (Room* room : component->GetRooms()) {
			if (!room) continue;
			if (room->IsStorage() && !room->StorageType().empty()) {
				Storage* storage = CreateStorage(room->StorageType(), room->GetName());
				if (storage) {
					storageRooms[storage] = room;
					roomStorages[room] = storage;
				}
			}
			if (room->IsManufacture()) {
				for (const string& type : room->ManufactureTypes()) {
					Manufacture* manufacture = CreateManufacture(type, room->GetName() + "#" + type);
					if (!manufacture) continue;
					manufactureRooms[manufacture] = room;
					storageRooms[manufacture->GetInputCache()] = room;
					storageRooms[manufacture->GetOutputCache()] = room;
				}
			}
		}
	}
}

void Industry::SeedInitialStock() {
	for (auto& [name, manufacture] : manufactures) manufacture->SeedOutputCache();
}

vector<Product*> Industry::GetProducts() const {
	vector<Product*> result;
	for (const auto& [type, product] : products) result.push_back(product);
	return result;
}

Room* Industry::GetStorageRoom(const Storage* storage) const {
	auto it = storageRooms.find(storage);
	return it != storageRooms.end() ? it->second : nullptr;
}

Storage* Industry::GetRoomStorage(const Room* room) const {
	auto it = roomStorages.find(room);
	return it != roomStorages.end() ? it->second : nullptr;
}

Room* Industry::GetManufactureRoom(const Manufacture* manufacture) const {
	auto it = manufactureRooms.find(manufacture);
	return it != manufactureRooms.end() ? it->second : nullptr;
}

Product* Industry::FindProduct(const string& type) const {
	auto it = products.find(type);
	return it != products.end() ? it->second : nullptr;
}

Storage* Industry::CreateStorage(const string& modId, const string& name) {
	auto it = storages.find(name);
	if (it != storages.end()) {
		if (Room* owner = GetStorageRoom(it->second)) roomStorages.erase(owner);
		storageRooms.erase(it->second);
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
	if (Room* owner = GetStorageRoom(it->second)) roomStorages.erase(owner);
	storageRooms.erase(it->second);
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
		manufactureRooms.erase(it->second);
		storageRooms.erase(it->second->GetInputCache());
		storageRooms.erase(it->second->GetOutputCache());
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
	manufactureRooms.erase(it->second);
	storageRooms.erase(it->second->GetInputCache());
	storageRooms.erase(it->second->GetOutputCache());
	delete it->second;
	manufactures.erase(it);
}

Manufacture* Industry::FindManufactureByName(const string& name) const {
	auto it = manufactures.find(name);
	return it != manufactures.end() ? it->second : nullptr;
}

void Industry::Tick(const Time& currentTime, bool crossedDay, PostHandle* post) {
	if (!crossedDay) return;
	flows.clear();

	// 阶段A：全体工坊先把上一轮算出的产量结算进各自的outputCache。
	dailyProduction.clear();
	for (auto& [name, manufacture] : manufactures) {
		manufacture->WorkAccount();
		for (const auto& [type, amount] : manufacture->GetLastProduced()) dailyProduction[type] += amount;
	}

	// 阶段B：全城物流，先需求侧再供给侧。
	Distribute();

	// 阶段C：全体工坊用调配完之后的inputCache存量+outputCache剩余空间，算今天能产
	// 几批，为下一次跨天的WorkAccount()做准备。
	for (auto& [name, manufacture] : manufactures) {
		if (closedRooms.count(GetManufactureRoom(manufacture))) continue; // 停业的工坊不开工
		float staffing = 1.f;
		auto staffingIt = staffingRatios.find(GetManufactureRoom(manufacture));
		if (staffingIt != staffingRatios.end()) staffing = staffingIt->second;
		manufacture->StartProduce(0.5f * (1.f + staffing));
	}
}

bool Industry::IsClosed(const Storage* storage) const {
	return closedRooms.count(GetStorageRoom(storage)) > 0;
}

bool Industry::ApplyChange(const Change* change, const ScriptContext& context) {
	// 占位：Industry域目前没有需要处理的Change子类。
	return false;
}

namespace {
	// 浮点存量转成整数份数：货物按整份流转，+eps避免0.9999999这类累加误差少算一份。
	int Whole(float value) {
		return value > 0.f ? static_cast<int>(floor(value + 1e-4f)) : 0;
	}
}

void Industry::Record(Storage* from, Storage* to, const string& product, int quantity) {
	if (quantity <= 0) return;
	flows.push_back({ from, to, product, quantity });
}

void Industry::Distribute() {
	// 遍历顺序固定：先storages(按name)，再manufactures(按name)的input、output。
	vector<Storage*> all;
	for (auto& [name, storage] : storages) all.push_back(storage);
	for (auto& [name, manufacture] : manufactures) {
		all.push_back(manufacture->GetInputCache());
		all.push_back(manufacture->GetOutputCache());
	}

	vector<Storage*> putOnly, takeOnly, takePut;
	for (Storage* storage : all) {
		if (IsClosed(storage)) continue; // 停业房间的仓库不参与物流
		if (storage->CanPut() && !storage->CanTake()) putOnly.push_back(storage);
		else if (storage->CanTake() && !storage->CanPut()) takeOnly.push_back(storage);
		else if (storage->CanTake() && storage->CanPut()) takePut.push_back(storage);
	}

	// 取货顺序：先从只能取的仓库扣，再从可取可放的仓库扣。
	vector<Storage*> sources = takeOnly;
	sources.insert(sources.end(), takePut.begin(), takePut.end());

	DistributeDemand(putOnly, sources);
	DistributeSupply(takeOnly, takePut);
}

void Industry::DistributeDemand(const vector<Storage*>& putOnly, const vector<Storage*>& sources) {
	// 先精确product需求：product id -> 各需求方的缺口。
	map<string, vector<Demand>> precise;
	for (Storage* dest : putOnly) {
		for (const auto& [productId, ratio] : dest->GetProductDemand()) {
			int target = Whole(ratio * dest->GetCapacity());
			int have = Whole(dest->GetAmount(productId));
			int amount = min(max(0, target - have), Whole(dest->GetSpace()));
			if (amount > 0) precise[productId].push_back({ dest, amount });
		}
	}
	for (auto& [productId, demands] : precise) {
		Fulfil({ productId }, productId, demands, sources);
	}

	// 再服务类型需求：某类服务下所有product的存量合计与目标的差额。
	vector<Product*> products = GetProducts();
	map<string, vector<Demand>> byType;
	for (Storage* dest : putOnly) {
		for (const auto& [serviceType, ratio] : dest->GetServiceTypeDemand()) {
			int have = 0;
			for (Product* product : products) {
				if (product->GetServiceType() == serviceType) have += Whole(dest->GetAmount(product->GetType()));
			}
			int target = Whole(ratio * dest->GetCapacity());
			int amount = min(max(0, target - have), Whole(dest->GetSpace()));
			if (amount > 0) byType[serviceType].push_back({ dest, amount });
		}
	}
	for (auto& [serviceType, demands] : byType) {
		vector<string> candidates;
		string importProduct;
		int64_t cheapest = 0;
		for (Product* product : products) {
			if (product->GetServiceType() != serviceType) continue;
			candidates.push_back(product->GetType());
			// 缺口进口买最便宜的那一种。
			if (importProduct.empty() || product->GetImportPrice() < cheapest) {
				importProduct = product->GetType();
				cheapest = product->GetImportPrice();
			}
		}
		if (candidates.empty()) continue;
		Fulfil(candidates, importProduct, demands, sources);
	}
}

void Industry::Fulfil(const vector<string>& candidates, const string& importProduct,
	vector<Demand>& demands, const vector<Storage*>& sources) {
	long long total = 0;
	for (const Demand& d : demands) total += d.amount;
	if (total <= 0) return;

	long long supply = 0;
	for (Storage* src : sources) {
		for (const string& product : candidates) supply += Whole(src->GetAmount(product));
	}
	long long internal = min(total, supply);

	// 内部供给按统一满足比例分给各需求方：先取整，剩下的份数按小数部分从大到小逐份补(同分按顺序)。
	vector<long long> share(demands.size(), 0);
	vector<pair<long long, size_t>> remainders;
	long long assigned = 0;
	for (size_t i = 0; i < demands.size(); i++) {
		long long scaled = demands[i].amount * internal;
		share[i] = scaled / total;
		assigned += share[i];
		remainders.push_back({ scaled % total, i });
	}
	stable_sort(remainders.begin(), remainders.end(),
		[](const auto& a, const auto& b) { return a.first > b.first; });
	for (size_t k = 0; assigned < internal && k < remainders.size(); k++, assigned++) {
		share[remainders[k].second]++;
	}

	for (size_t i = 0; i < demands.size(); i++) {
		Storage* dest = demands[i].dest;
		int need = static_cast<int>(share[i]);
		for (Storage* src : sources) {
			for (const string& product : candidates) {
				if (need <= 0) break;
				int give = min({ need, Whole(src->GetAmount(product)), Whole(dest->GetSpace()) });
				if (give <= 0) continue;
				src->Output(product, static_cast<float>(give));
				dest->Input(product, static_cast<float>(give));
				Record(src, dest, product, give);
				need -= give;
			}
		}

		// 内部没满足的部分直接进口。
		int toImport = min(demands[i].amount - static_cast<int>(share[i]) + need, Whole(dest->GetSpace()));
		if (toImport > 0) {
			dest->Input(importProduct, static_cast<float>(toImport));
			Record(nullptr, dest, importProduct, toImport);
		}
	}
}

void Industry::DistributeSupply(const vector<Storage*>& takeOnly, const vector<Storage*>& takePut) {
	// 只处理超出exportKeepRatio*容量的部分(内销取完之后还剩的)，各货物按存量占比分摊这份超出量。
	for (Storage* src : takeOnly) {
		vector<string> types;
		float total = 0.f;
		for (const auto& [type, amount] : src->GetStock()) {
			types.push_back(type);
			total += amount;
		}
		sort(types.begin(), types.end());

		float movableRatio = total > 0.f ? max(0.f, (total - exportKeepRatio * src->GetCapacity()) / total) : 0.f;
		if (movableRatio <= 0.f) continue;

		for (const string& type : types) {
			int remaining = Whole(src->GetAmount(type) * movableRatio);
			if (remaining <= 0) continue;

			Product* product = FindProduct(type);
			if (product) {
				for (Storage* dst : takePut) {
					if (remaining <= 0) break;
					if (dst == src || !dst->AcceptsCategory(product->GetCategories())) continue;
					int give = min(remaining, Whole(dst->GetSpace()));
					if (give <= 0) continue;
					src->Output(type, static_cast<float>(give));
					dst->Input(type, static_cast<float>(give));
					Record(src, dst, type, give);
					remaining -= give;
				}
			}

			if (remaining > 0) {
				src->Output(type, static_cast<float>(remaining));
				Record(src, nullptr, type, remaining);
			}
		}
	}
}
