#include "player/asset.h"

#include "common/error.h"

#include "common/registry.h"
#include "story/script.h"

#include <algorithm>
#include <vector>

using namespace std;

Asset::Asset(AssetFactory* factory, const string& id) :
	factory(factory), mod(nullptr) {
	mod = factory->CreateAsset(id);
	if (!mod) return;

	MOD_CALL(mod, mod->SetProperty());
	name = mod->GetName(); // mod保证全局唯一，拷贝一份：GetName()返回的是mod内部缓冲
	type = mod->GetType();
	mobility = mod->mobility;
	weight = mod->weight;
	size = mod->size;
	volume = mod->volume;
	backpack = mod->backpack;
	weaponFlag = mod->weapon;
	weaponType = mod->weaponType;
	usableUses = mod->usable;
	usesLeft = mod->usable > 0 ? mod->usable : 0;
	productType = mod->productType;
	throwableFlag = mod->throwable;
	meshPath = mod->meshPath;
	meshScale = mod->meshScale;

	InitScript(mod->scriptModName, mod->milestoneNames);
}

Asset::Asset(const AssetFixtureDesc& desc, const string& uniqueName) :
	factory(nullptr), mod(nullptr) {
	builtin = true;
	mobility = AssetMobility::Fixed;
	name = uniqueName;
	type = desc.type;
	meshPath = desc.meshPath;
	InitScript(desc.scriptModName, desc.milestoneNames); // 可能抛(脚本mod没启用/里程碑文件有错)，所以放在分配fixture之前
	fixture = new AssetFixtureDesc(desc);
}

void Asset::InitScript(const string& scriptModName, const vector<string>& milestoneNames) {
	script = MakeScript(&Registry::Get().GetScriptFactory(), scriptModName, milestoneNames);
	if (script) {
		script->SetOwner(ScriptOwnerKind::Asset, this);
		script->SetStableKey("asset:" + name);
		script->SetValue("name", ValueType(name));
		script->SetValue("type", ValueType(type));
	}
}

Asset::Asset(AssetMobility builtinMobility, const string& builtinName, const string& builtinType) :
	factory(nullptr), mod(nullptr) {
	builtin = true;
	mobility = builtinMobility;
	name = builtinName;
	type = builtinType;
}

Asset::~Asset() {
	for (auto& [name, content] : contents) delete content;
	delete script;
	delete fixture;
	if (mod) factory->DestroyAsset(mod);
}

bool Asset::IsValid() const { return builtin || mod != nullptr; }

const string& Asset::GetName() const { return name; }
const string& Asset::GetType() const { return type; }

AssetMobility Asset::GetMobility() const { return mobility; }
float Asset::GetWeight() const { return weight; }
float Asset::GetSize() const { return size; }
float Asset::GetVolume() const { return volume; }
bool Asset::GetBackpack() const { return backpack; }
bool Asset::IsWeapon() const { return weaponFlag; }
const string& Asset::GetWeaponType() const { return weaponType; }
bool Asset::IsUsable() const { return usableUses != 0; }
int Asset::GetUsable() const { return usableUses; }
int Asset::GetUsesLeft() const { return usesLeft; }
const std::string& Asset::GetProductType() const { return productType; }
bool Asset::IsThrowable() const { return throwableFlag; }
const string& Asset::GetMeshPath() const { return meshPath; }
float Asset::GetMeshScale() const { return meshScale; }
bool Asset::IsContainer() const { return volume > 0.f; }
bool Asset::IsFixture() const { return mobility == AssetMobility::Fixed; }

void Asset::GetMeshScale3(float& outX, float& outY, float& outZ) const {
	if (fixture) { outX = fixture->scaleX; outY = fixture->scaleY; outZ = fixture->scaleZ; }
	else { outX = outY = outZ = meshScale; }
}

const AssetFixtureDesc* Asset::GetFixtureDesc() const { return fixture; }
const AssetGround& Asset::GetGround() const { return ground; }
void Asset::SetGround(const AssetGround& value) { ground = value; }

Script* Asset::GetScript() const { return script; }
const vector<string>& Asset::GetOptions() const { return options; }

void Asset::AddOption(const string& option) {
	if (find(options.begin(), options.end(), option) == options.end()) options.push_back(option);
}

void Asset::RemoveOption(const string& option) {
	options.erase(remove(options.begin(), options.end(), option), options.end());
}

bool Asset::IsScriptStarted() const { return scriptStarted; }
void Asset::SetScriptStarted(bool value) { scriptStarted = value; }

Asset* Asset::FindDescendant(const string& childName) const {
	auto it = contents.find(childName);
	if (it != contents.end()) return it->second;
	for (const auto& [key, content] : contents) {
		if (Asset* found = content->FindDescendant(childName)) return found;
	}
	return nullptr;
}

int Asset::GetCount() const { return count; }
void Asset::SetCount(int value) { count = value; }
int Asset::GetMagazineAmmo() const { return magazineAmmo; }
void Asset::SetMagazineAmmo(int value) { magazineAmmo = value; }

bool Asset::Use() {
	if (usableUses == 0 || count <= 0) return false;
	if (usableUses < 0) return false; // 无限次：不消耗
	if (--usesLeft > 0) return false;
	// 这一件用完了：数量-1，下一件的剩余次数回满。
	count--;
	usesLeft = usableUses;
	return count <= 0;
}

float Asset::GetSpace() const {
	float occupied = 0.f;
	for (const auto& [name, content] : contents) occupied += content->size * content->count;
	return volume - occupied;
}

bool Asset::AddContent(Asset* content) {
	if (!content) return false;
	if (content->IsFixture()) return false; // 固定物品不能被装进任何容器
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
