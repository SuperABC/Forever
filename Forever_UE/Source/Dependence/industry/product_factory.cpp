#include "product_factory.h"


using namespace std;

void ProductFactory::RegisterProduct(const string& id, CreateFunc creator, DestroyFunc deleter) {
	registries[id] = { creator, deleter };
}

ProductMod* ProductFactory::CreateProduct(const string& id) {
	auto it = registries.find(id);
	if (it == registries.end())
		return nullptr;

	auto argsIt = configuredArgs.find(id);
	ProductMod* instance = it->second.creator(argsIt != configuredArgs.end() ? argsIt->second : string());
	if (instance) {
		liveInstances[instance] = id;
	}
	return instance;
}

void ProductFactory::DestroyProduct(ProductMod* instance) {
	if (!instance)
		return;

	auto liveIt = liveInstances.find(instance);
	if (liveIt == liveInstances.end())
		return;

	auto regIt = registries.find(liveIt->second);
	if (regIt != registries.end())
		regIt->second.deleter(instance);

	liveInstances.erase(liveIt);
}

bool ProductFactory::CheckRegistered(const string& id) const {
	return registries.find(id) != registries.end();
}

vector<string> ProductFactory::GetRegisteredIds() const {
	vector<string> ids;
	for (const auto& [id, entry] : registries) {
		ids.push_back(id);
	}
	return ids;
}

void ProductFactory::SetModArgs(const unordered_map<string, string>& argsById) {
	configuredArgs = argsById;
}
