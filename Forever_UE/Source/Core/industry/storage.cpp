#include "industry/storage.h"

#include <algorithm>

using namespace std;

Storage::Storage(StorageFactory* factory, const string& id, const string& name) :
	factory(factory), name(name) {
	mod = factory->CreateStorage(id);
	valid = (mod != nullptr);
	if (!mod) return;

	mod->SetProperty();
	categories = mod->categories;
	capacity = mod->capacity;
}

Storage::Storage(const string& name, const vector<string>& categories, float capacity) :
	name(name), categories(categories), capacity(capacity) {
}

Storage::~Storage() {
	if (mod) factory->DestroyStorage(mod);
}

bool Storage::IsValid() const { return valid; }

const string& Storage::GetName() const { return name; }

bool Storage::AcceptsCategory(const vector<string>& productCategories) const {
	for (const string& c : categories) {
		for (const string& p : productCategories) {
			if (c == p) return true;
		}
	}
	return false;
}

float Storage::GetAmount(const string& type) const {
	auto it = stock.find(type);
	return it != stock.end() ? it->second : 0.f;
}

float Storage::GetSpace() const {
	float occupied = 0.f;
	for (const auto& [type, amount] : stock) occupied += amount;
	return capacity - occupied;
}

void Storage::SetCapacity(float newCapacity) { capacity = newCapacity; }

float Storage::Input(const string& type, float amount) {
	if (amount <= 0.f) return 0.f;
	float actual = min(amount, max(GetSpace(), 0.f));
	if (actual <= 0.f) return 0.f;
	stock[type] += actual;
	return actual;
}

float Storage::Output(const string& type, float amount) {
	if (amount <= 0.f) return 0.f;
	auto it = stock.find(type);
	if (it == stock.end()) return 0.f;
	float actual = min(amount, it->second);
	if (actual <= 0.f) return 0.f;
	it->second -= actual;
	return actual;
}
