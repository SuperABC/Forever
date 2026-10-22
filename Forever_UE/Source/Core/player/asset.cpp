#include "player/asset.h"

#include <algorithm>
#include <vector>

using namespace std;

Asset::Asset(AssetFactory* factory, const string& id) :
	factory(factory), mod(nullptr) {
	mod = factory->CreateAsset(id);
	if (!mod) return;

	mod->SetProperty();
	name = mod->GetName(); // mod保证全局唯一，拷贝一份：GetName()返回的是mod内部缓冲
	type = mod->GetType();
	mobility = mod->mobility;
	weight = mod->weight;
	size = mod->size;
	volume = mod->volume;
	backpack = mod->backpack;
	weaponFlag = mod->weapon;
	weaponType = mod->weaponType;
	usableFlag = mod->usable;
	throwableFlag = mod->throwable;
	meshPath = mod->meshPath;
	meshScale = mod->meshScale;
}

Asset::~Asset() {
	for (auto& [name, content] : contents) delete content;
	if (mod) factory->DestroyAsset(mod);
}

bool Asset::IsValid() const { return mod != nullptr; }

const string& Asset::GetName() const { return name; }
const string& Asset::GetType() const { return type; }

AssetMobility Asset::GetMobility() const { return mobility; }
float Asset::GetWeight() const { return weight; }
float Asset::GetSize() const { return size; }
float Asset::GetVolume() const { return volume; }
bool Asset::GetBackpack() const { return backpack; }
bool Asset::IsWeapon() const { return weaponFlag; }
const string& Asset::GetWeaponType() const { return weaponType; }
bool Asset::IsUsable() const { return usableFlag; }
bool Asset::IsThrowable() const { return throwableFlag; }
const string& Asset::GetMeshPath() const { return meshPath; }
float Asset::GetMeshScale() const { return meshScale; }
bool Asset::IsContainer() const { return volume > 0.f; }

int Asset::GetCount() const { return count; }
void Asset::SetCount(int value) { count = value; }

bool Asset::Use() {
	if (!usableFlag || count <= 0) return false;
	count--;
	return count <= 0;
}

float Asset::GetSpace() const {
	float occupied = 0.f;
	for (const auto& [name, content] : contents) occupied += content->size * content->count;
	return volume - occupied;
}

bool Asset::AddContent(Asset* content) {
	if (!content) return false;
	if (contents.count(content->GetName())) return false; // 名字全局唯一，正常不会撞；防止覆盖泄漏旧物品
	if (content->size * content->count > GetSpace()) return false;
	contents.insert_or_assign(content->GetName(), content);
	return true;
}

Asset* Asset::RemoveContent(const string& name) {
	auto it = contents.find(name);
	if (it == contents.end()) return nullptr;
	Asset* removed = it->second;
	contents.erase(it);
	return removed;
}

const unordered_map<string, Asset*>& Asset::GetContents() const { return contents; }

int Asset::CountByType(const string& queryType) const {
	int total = (type == queryType) ? count : 0;
	for (const auto& [name, content] : contents) total += content->CountByType(queryType);
	return total;
}

bool Asset::ConsumeByType(const string& queryType, int amount) {
	if (amount <= 0) return true;
	if (CountByType(queryType) < amount) return false;
	ConsumeUpTo(queryType, amount);
	return true;
}

int Asset::ConsumeUpTo(const string& queryType, int amount) {
	if (amount <= 0) return 0;

	int consumed = 0;
	vector<string> toRemove;
	for (auto& [name, content] : contents) {
		if (consumed >= amount) break;
		if (content->type != queryType) continue;
		int take = min(amount - consumed, content->count);
		content->count -= take;
		consumed += take;
		if (content->count <= 0) toRemove.push_back(name);
	}
	for (const string& name : toRemove) {
		Asset* removed = RemoveContent(name);
		delete removed;
	}

	for (auto& [name, content] : contents) {
		if (consumed >= amount) break;
		if (content->IsContainer()) consumed += content->ConsumeUpTo(queryType, amount - consumed);
	}

	return consumed;
}
