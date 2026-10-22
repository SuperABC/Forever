#include "player/player.h"

#include "common/utility.h"
#include "common/registry.h"
#include "map/room.h"

#include <algorithm>

using namespace std;

Player::Player() : assetFactory(Registry::Get().GetAssetFactory()) {
}

Player::~Player() {
	delete time;

	delete leftHand;
	delete rightHand;
	delete backPack;
	delete leftShoulder;
	delete rightShoulder;
	for (auto& [name, asset] : estateAssets) delete asset;
	for (auto& [name, entry] : worldAssets) delete entry.asset;
}

void Player::Init() {
	time = new Time();
	time->SetHour(8);
}

void Player::Tick(float delta) {
	day = time->GetDay();
	time->AddMilliseconds(static_cast<int>(delta * 60 * 1000 * timeFlowRatio));
}

void Player::SetTimeFlowRatio(double ratio) {
	timeFlowRatio = ratio;
}

Time* Player::GetTime() const {
	return time;
}

void Player::SetTime(const Time& newTime) {
	day = time->GetDay();
	*time = newTime;
}

bool Player::CrossDay() {
	return day != time->GetDay();
}

float Player::GetHealth() const { return health; }
float Player::GetMaxHealth() const { return maxHealth; }

float Player::TakeDamage(float amount) {
	health = std::max(0.f, health - amount);
	return health;
}

float Player::Heal(float amount) {
	health = std::min(maxHealth, health + amount);
	return health;
}

bool Player::IsDead() const { return health <= 0.f; }

Asset* Player::CreateAsset(const string& id) {
	Asset* asset = new Asset(&assetFactory, id);
	if (!asset->IsValid()) {
		delete asset;
		return nullptr;
	}
	return asset;
}

Asset* Player::SplitAsset(Asset* src, int n) {
	if (!src || n <= 0 || n >= src->GetCount()) return nullptr;
	Asset* piece = CreateAsset(src->GetType());
	if (!piece) return nullptr;
	piece->SetCount(n);
	src->SetCount(src->GetCount() - n);
	return piece;
}

void Player::DestroyAsset(Asset* asset) {
	delete asset;
}

vector<string> Player::SplitPath(const string& path) {
	vector<string> parts;
	size_t start = 0;
	while (start <= path.size()) {
		size_t pos = path.find('/', start);
		if (pos == string::npos) {
			parts.push_back(path.substr(start));
			break;
		}
		parts.push_back(path.substr(start, pos - start));
		start = pos + 1;
	}
	return parts;
}

Asset* Player::GetByPath(const string& path) const {
	vector<string> parts = SplitPath(path);
	if (parts.empty()) return nullptr;

	Asset* current = nullptr;
	size_t nextIndex = 1;

	if (parts[0] == "left") current = leftHand;
	else if (parts[0] == "right") current = rightHand;
	else if (parts[0] == "back") current = backPack;
	else if (parts[0] == "leftShoulder") current = leftShoulder;
	else if (parts[0] == "rightShoulder") current = rightShoulder;
	else if (parts[0] == "room") {
		if (!currentRoom || parts.size() < 2) return nullptr;
		const auto& roomAssets = currentRoom->GetAssets();
		auto it = roomAssets.find(parts[1]);
		current = (it != roomAssets.end()) ? it->second : nullptr;
		nextIndex = 2;
	} else {
		return nullptr;
	}

	for (size_t i = nextIndex; i < parts.size() && current; ++i) {
		const auto& contents = current->GetContents();
		auto it = contents.find(parts[i]);
		current = (it != contents.end()) ? it->second : nullptr;
	}
	return current;
}

Asset* Player::RemoveByPath(const string& path) {
	vector<string> parts = SplitPath(path);
	if (parts.empty()) return nullptr;

	if (parts.size() == 1) {
		if (parts[0] == "left") { Asset* a = leftHand; leftHand = nullptr; return a; }
		if (parts[0] == "right") { Asset* a = rightHand; rightHand = nullptr; return a; }
		if (parts[0] == "back") { Asset* a = backPack; backPack = nullptr; return a; }
		if (parts[0] == "leftShoulder") { Asset* a = leftShoulder; leftShoulder = nullptr; return a; }
		if (parts[0] == "rightShoulder") { Asset* a = rightShoulder; rightShoulder = nullptr; return a; }
		return nullptr; // "room"单独一段不是具体某个资产，没法摘除
	}

	if (parts[0] == "room" && parts.size() == 2) {
		if (!currentRoom) return nullptr;
		return currentRoom->RemoveAsset(parts[1]);
	}

	string parentPath;
	for (size_t i = 0; i + 1 < parts.size(); ++i) {
		if (i > 0) parentPath += "/";
		parentPath += parts[i];
	}
	Asset* parent = GetByPath(parentPath);
	if (!parent) return nullptr;
	return parent->RemoveContent(parts.back());
}

bool Player::AddByPath(const string& path, Asset* asset) {
	if (!asset) return false;
	vector<string> parts = SplitPath(path);
	if (parts.empty()) return false;

	if (parts.size() == 1) {
		if (parts[0] == "left") {
			if (leftHand || asset->GetMobility() != AssetMobility::Object || asset->GetCount() != 1) return false;
			leftHand = asset;
			return true;
		}
		if (parts[0] == "right") {
			if (rightHand || asset->GetMobility() != AssetMobility::Object || asset->GetCount() != 1) return false;
			rightHand = asset;
			return true;
		}
		if (parts[0] == "back") {
			if (backPack || asset->GetMobility() != AssetMobility::Object || !asset->GetBackpack()) return false;
			backPack = asset;
			return true;
		}
		if (parts[0] == "leftShoulder") {
			if (leftShoulder || !asset->IsWeapon()) return false;
			leftShoulder = asset;
			return true;
		}
		if (parts[0] == "rightShoulder") {
			if (rightShoulder || !asset->IsWeapon()) return false;
			rightShoulder = asset;
			return true;
		}
		if (parts[0] == "room") {
			if (!currentRoom) return false;
			currentRoom->AddAsset(asset);
			return true;
		}
		return false;
	}

	Asset* container = GetByPath(path);
	if (!container) return false;
	return container->AddContent(asset);
}

bool Player::AddEstateAsset(Asset* asset) {
	if (!asset) return false;
	if (asset->GetMobility() != AssetMobility::Estate && asset->GetMobility() != AssetMobility::Vehicle) return false;
	estateAssets.insert_or_assign(asset->GetName(), asset);
	return true;
}

Asset* Player::GetEstateAsset(const string& name) const {
	auto it = estateAssets.find(name);
	return it != estateAssets.end() ? it->second : nullptr;
}

void Player::RemoveEstateAsset(const string& name) {
	estateAssets.erase(name);
}

int Player::CountByType(const string& type) const {
	return backPack ? backPack->CountByType(type) : 0;
}

bool Player::ConsumeByType(const string& type, int amount) {
	return backPack ? backPack->ConsumeByType(type, amount) : false;
}

namespace {
	// 子项按name排序，unordered_map的遍历顺序不确定，投掷物的扫描顺序必须稳定。
	vector<Asset*> SortedContents(const Asset* asset) {
		vector<Asset*> children;
		for (const auto& [name, content] : asset->GetContents()) children.push_back(content);
		sort(children.begin(), children.end(),
			[](const Asset* a, const Asset* b) { return a->GetName() < b->GetName(); });
		return children;
	}

	void CollectThrowablesFrom(const Asset* asset, vector<Player::ThrowableEntry>& out) {
		if (asset->IsThrowable() && asset->GetCount() > 0) {
			auto it = find_if(out.begin(), out.end(),
				[&](const Player::ThrowableEntry& e) { return e.type == asset->GetType(); });
			if (it == out.end()) out.push_back({ asset->GetType(), asset->GetCount(), asset->GetMeshPath(), asset->GetMeshScale() });
			else it->count += asset->GetCount();
		}
		for (const Asset* child : SortedContents(asset)) CollectThrowablesFrom(child, out);
	}

	// 深度优先找第一个该type的投掷物堆叠，outParent=它所在的容器(直接在手槽/背包槽上为nullptr)。
	Asset* FindThrowable(Asset* asset, Asset* parent, const string& type, Asset*& outParent) {
		if (asset->IsThrowable() && asset->GetType() == type && asset->GetCount() > 0) {
			outParent = parent;
			return asset;
		}
		for (Asset* child : SortedContents(asset)) {
			if (Asset* found = FindThrowable(child, asset, type, outParent)) return found;
		}
		return nullptr;
	}
}

vector<Player::ThrowableEntry> Player::CollectThrowables() const {
	vector<ThrowableEntry> entries;
	for (const Asset* root : { leftHand, rightHand, backPack }) {
		if (root) CollectThrowablesFrom(root, entries);
	}
	return entries;
}

Asset* Player::TakeOneThrowable(const string& type) {
	for (Asset** root : { &leftHand, &rightHand, &backPack }) {
		if (!*root) continue;

		Asset* parent = nullptr;
		Asset* found = FindThrowable(*root, nullptr, type, parent);
		if (!found) continue;

		if (found->GetCount() > 1) {
			return SplitAsset(found, 1);
		}

		// 整个堆叠只剩一个：直接摘出来交给调用方。
		if (parent) parent->RemoveContent(found->GetName());
		else *root = nullptr;
		return found;
	}
	return nullptr;
}

bool Player::AddWorldAsset(Asset* asset, const WorldAssetTransform& transform) {
	if (!asset || worldAssets.count(asset->GetName())) return false;
	worldAssets[asset->GetName()] = { asset, transform };
	return true;
}

Asset* Player::RemoveWorldAsset(const string& name) {
	auto it = worldAssets.find(name);
	if (it == worldAssets.end()) return nullptr;
	Asset* removed = it->second.asset;
	worldAssets.erase(it);
	return removed;
}

void Player::SetWorldAssetTransform(const string& name, const WorldAssetTransform& transform) {
	auto it = worldAssets.find(name);
	if (it != worldAssets.end()) it->second.transform = transform;
}

bool Player::HasThrowableInHand(const string& type) const {
	for (const Asset* hand : { leftHand, rightHand }) {
		if (hand && hand->IsThrowable() && hand->GetType() == type) return true;
	}
	return false;
}

bool Player::ApplyChange(const Change* change, const ScriptContext& context) {
	// 占位，等玩家相关的Change子类被点名实现时再补。
	return false;
}
