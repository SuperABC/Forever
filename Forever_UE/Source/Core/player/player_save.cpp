#include "player_save.h"

#include "player/player.h"
#include "player/asset.h"
#include "map/map.h"
#include "map/room.h"

#include "common/save.h"
#include "common/save_refs.h"
#include "common/utility.h"

#include <algorithm>
#include <unordered_map>


using namespace std;

namespace {
	int VersionOf(const char* name) {
		for (const SaveSectionVersion& v : GetSaveSectionVersions()) {
			if (string(v.name) == name) return v.current;
		}
		return 1;
	}

	vector<const Asset*> SortedAssets(const unordered_map<string, Asset*>& m) {
		vector<string> names;
		for (auto& [name, asset] : m) names.push_back(name);
		sort(names.begin(), names.end());
		vector<const Asset*> assets;
		for (const string& name : names) assets.push_back(m.at(name));
		return assets;
	}
}

void PlayerSaveCodec::WriteAsset(const Asset* asset, ByteWriter& w) {
	w.Str(asset->type);
	w.Str(asset->name);
	w.I32(asset->count);
	SaveRefs::WriteModState(asset->mod, w);
	vector<const Asset*> contents = SortedAssets(asset->contents);
	w.U32(static_cast<uint32_t>(contents.size()));
	for (const Asset* content : contents) WriteAsset(content, w);
}

Asset* PlayerSaveCodec::ReadAsset(AssetFactory& factory, ByteReader& r) {
	string type = r.Str();
	string name = r.Str();
	int count = r.I32();
	Asset* asset = new Asset(&factory, type);
	bool valid = asset->IsValid();
	asset->name = name; // 覆写回存档里的名字，容器/房间/worldAssets的key都按它索引
	if (!SaveRefs::ReadModState(valid ? asset->mod : static_cast<AssetMod*>(nullptr), r)) {
		debugf("Warning: failed to parse saved state of asset mod %s.\n", type.c_str());
	}
	asset->count = count;
	uint32_t contentCount = r.U32();
	for (uint32_t i = 0; i < contentCount && !r.Failed(); i++) {
		Asset* content = ReadAsset(factory, r);
		// 直接放回去，不走AddContent的空间检查：存档里本来就是装得下的(mod改了尺寸也不丢东西)。
		if (content) asset->contents[content->name] = content;
	}
	if (!valid) {
		debugf("Warning: asset mod %s is not available, item %s dropped.\n", type.c_str(), name.c_str());
		delete asset;
		return nullptr;
	}
	return asset;
}

void PlayerSaveCodec::Save(const Player& player, const Map& map, SaveArchive& archive) {
	ByteWriter w;

	w.U8(player.time ? 1 : 0);
	if (player.time) SaveRefs::WriteTime(*player.time, w);
	w.I32(player.day);
	w.F64(player.timeFlowRatio);
	w.F32(player.health);
	w.F32(player.maxHealth);

	const Asset* slots[] = { player.leftHand, player.rightHand, player.backPack, player.leftShoulder, player.rightShoulder };
	for (const Asset* slot : slots) {
		w.U8(slot ? 1 : 0);
		if (slot) WriteAsset(slot, w);
	}
	vector<const Asset*> estate = SortedAssets(player.estateAssets);
	w.U32(static_cast<uint32_t>(estate.size()));
	for (const Asset* asset : estate) WriteAsset(asset, w);
	SaveRefs::WriteRoom(player.currentRoom, w);

	// 房间里的物品：只写有物品的房间，按(建筑名, 房间下标)排序。
	vector<string> buildingNames;
	for (auto& [name, building] : map.GetBuildings()) buildingNames.push_back(name);
	sort(buildingNames.begin(), buildingNames.end());
	vector<const Room*> rooms;
	for (const string& name : buildingNames) {
		for (const Room* room : map.GetBuilding(name)->GetRooms()) {
			if (room && !room->GetAssets().empty()) rooms.push_back(room);
		}
	}
	w.U32(static_cast<uint32_t>(rooms.size()));
	for (const Room* room : rooms) {
		SaveRefs::WriteRoom(room, w);
		vector<const Asset*> assets = SortedAssets(room->GetAssets());
		w.U32(static_cast<uint32_t>(assets.size()));
		for (const Asset* asset : assets) WriteAsset(asset, w);
	}

	// 房间外的物品(被扔出去的投掷物等)：位置朝向+整棵Asset树，不存速度，读档后从静止开始模拟。
	vector<string> worldNames;
	for (auto& [name, entry] : player.worldAssets) worldNames.push_back(name);
	sort(worldNames.begin(), worldNames.end());
	w.U32(static_cast<uint32_t>(worldNames.size()));
	for (const string& name : worldNames) {
		const Player::WorldAsset& entry = player.worldAssets.at(name);
		w.F32(entry.transform.x);
		w.F32(entry.transform.y);
		w.F32(entry.transform.z);
		w.F32(entry.transform.pitch);
		w.F32(entry.transform.yaw);
		w.F32(entry.transform.roll);
		WriteAsset(entry.asset, w);
	}

	archive.SetBinary(SAVE_SECTION_PLAYER, VersionOf(SAVE_SECTION_PLAYER), w.Take());
}

Player* PlayerSaveCodec::Load(const SaveArchive& archive, Map& map, string& error) {
	const SaveSection* section = archive.Find(SAVE_SECTION_PLAYER);
	if (!section) return nullptr;

	try {
		ByteReader r(section->data);
		Player* player = new Player();
		if (r.U8() != 0) player->time = new Time(SaveRefs::ReadTime(r));
		else player->time = new Time();
		player->day = r.I32();
		player->timeFlowRatio = r.F64();
		player->health = r.F32();
		player->maxHealth = r.F32();

		Asset** slots[] = { &player->leftHand, &player->rightHand, &player->backPack, &player->leftShoulder, &player->rightShoulder };
		for (Asset** slot : slots) {
			if (r.U8() != 0) *slot = ReadAsset(player->assetFactory, r);
		}
		uint32_t estateCount = r.U32();
		for (uint32_t i = 0; i < estateCount && !r.Failed(); i++) {
			if (Asset* asset = ReadAsset(player->assetFactory, r)) player->estateAssets[asset->name] = asset;
		}
		player->currentRoom = SaveRefs::ReadRoom(map, r);

		uint32_t roomCount = r.U32();
		for (uint32_t i = 0; i < roomCount && !r.Failed(); i++) {
			Room* room = SaveRefs::ReadRoom(map, r);
			uint32_t assetCount = r.U32();
			for (uint32_t k = 0; k < assetCount && !r.Failed(); k++) {
				Asset* asset = ReadAsset(player->assetFactory, r);
				if (asset && room) room->AddAsset(asset);
				else delete asset;
			}
		}

		// 房间外的物品。这一块是后加的，更早的存档没有它，读到末尾就当没有。
		if (!r.AtEnd()) {
			uint32_t worldCount = r.U32();
			for (uint32_t i = 0; i < worldCount && !r.Failed(); i++) {
				Player::WorldAssetTransform transform;
				transform.x = r.F32();
				transform.y = r.F32();
				transform.z = r.F32();
				transform.pitch = r.F32();
				transform.yaw = r.F32();
				transform.roll = r.F32();
				Asset* asset = ReadAsset(player->assetFactory, r);
				if (asset && !player->AddWorldAsset(asset, transform)) delete asset;
			}
		}

		if (r.Failed()) {
			error = "player section is truncated";
			return nullptr;
		}
		return player;
	}
	catch (const exception& e) {
		error = string("player load failed: ") + e.what();
		return nullptr;
	}
	catch (...) {
		error = "player load failed with an unknown error";
		return nullptr;
	}
}
