#include "industry_save.h"

#include "industry/industry.h"
#include "map/map.h"

#include "common/save.h"

#include <algorithm>
#include <vector>


using namespace std;

namespace {
	int VersionOf(const char* name) {
		for (const SaveSectionVersion& v : GetSaveSectionVersions()) {
			if (string(v.name) == name) return v.current;
		}
		return 1;
	}

	void WriteStock(const unordered_map<string, float>& stock, ByteWriter& w) {
		vector<string> types;
		for (const auto& [type, amount] : stock) types.push_back(type);
		sort(types.begin(), types.end());
		w.U32(static_cast<uint32_t>(types.size()));
		for (const string& type : types) {
			w.Str(type);
			w.F32(stock.at(type));
		}
	}

	void ReadStock(unordered_map<string, float>& stock, ByteReader& r) {
		stock.clear();
		uint32_t count = r.U32();
		for (uint32_t i = 0; i < count && !r.Failed(); i++) {
			string type = r.Str();
			stock[type] = r.F32();
		}
	}
}

void IndustrySaveCodec::Save(const Industry& industry, SaveArchive& archive) {
	ByteWriter w;

	w.U32(static_cast<uint32_t>(industry.products.size()));
	for (const auto& [type, product] : industry.products) {
		w.Str(type);
		w.I64(product->GetWholesalePrice());
	}

	w.U32(static_cast<uint32_t>(industry.storages.size()));
	for (const auto& [name, storage] : industry.storages) {
		w.Str(name);
		WriteStock(storage->stock, w);
	}

	w.U32(static_cast<uint32_t>(industry.manufactures.size()));
	for (const auto& [name, manufacture] : industry.manufactures) {
		w.Str(name);
		WriteStock(manufacture->inputCache->stock, w);
		WriteStock(manufacture->outputCache->stock, w);

		vector<string> targets;
		for (const auto& [target, batches] : manufacture->pendingProduction) targets.push_back(target);
		sort(targets.begin(), targets.end());
		w.U32(static_cast<uint32_t>(targets.size()));
		for (const string& target : targets) {
			w.Str(target);
			w.I32(manufacture->pendingProduction.at(target));
		}
	}

	archive.SetBinary(SAVE_SECTION_INDUSTRY, VersionOf(SAVE_SECTION_INDUSTRY), w.Take());
}

Industry* IndustrySaveCodec::Load(const SaveArchive& archive, const Map& map, string& error) {
	const SaveSection* section = archive.Find(SAVE_SECTION_INDUSTRY);
	if (!section) {
		error = "the save has no section '" SAVE_SECTION_INDUSTRY "'";
		return nullptr;
	}

	Industry* industry = new Industry();
	industry->Init(map.GetAllComponents());

	try {
		ByteReader r(section->data);

		uint32_t productCount = r.U32();
		for (uint32_t i = 0; i < productCount && !r.Failed(); i++) {
			string type = r.Str();
			int64_t price = r.I64();
			if (Product* product = industry->FindProduct(type)) product->SetWholesalePrice(price);
		}

		// 存档里有、当前世界已经没有的仓库/工坊（mod变了）直接丢弃库存。
		uint32_t storageCount = r.U32();
		for (uint32_t i = 0; i < storageCount && !r.Failed(); i++) {
			string name = r.Str();
			Storage* storage = industry->FindStorageByName(name);
			unordered_map<string, float> stock;
			ReadStock(stock, r);
			if (storage) storage->stock = stock;
		}

		uint32_t manufactureCount = r.U32();
		for (uint32_t i = 0; i < manufactureCount && !r.Failed(); i++) {
			string name = r.Str();
			Manufacture* manufacture = industry->FindManufactureByName(name);
			unordered_map<string, float> input, output;
			ReadStock(input, r);
			ReadStock(output, r);
			unordered_map<string, int> pending;
			uint32_t pendingCount = r.U32();
			for (uint32_t k = 0; k < pendingCount && !r.Failed(); k++) {
				string target = r.Str();
				pending[target] = r.I32();
			}
			if (manufacture) {
				manufacture->inputCache->stock = input;
				manufacture->outputCache->stock = output;
				manufacture->pendingProduction = pending;
			}
		}

		if (r.Failed()) {
			error = "the industry section is truncated";
			delete industry;
			return nullptr;
		}
	}
	catch (const exception& e) {
		error = e.what();
		delete industry;
		return nullptr;
	}
	return industry;
}
