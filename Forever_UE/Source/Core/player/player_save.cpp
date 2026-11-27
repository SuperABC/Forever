#include "player_save.h"

#include "common/error.h"

#include "player/player.h"
#include "player/asset.h"
#include "story/script.h"
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

	// 物品的地面摆位(房间里的和室外的都写)：6个浮点+restOnBottom。
	void WriteGround(const AssetGround& ground, ByteWriter& w) {
		w.F32(ground.x); w.F32(ground.y); w.F32(ground.z);
		w.F32(ground.pitch); w.F32(ground.yaw); w.F32(ground.roll);
		w.U8(ground.restOnBottom ? 1 : 0);
	}

	AssetGround ReadGround(ByteReader& r) {
		AssetGround ground;
		ground.x = r.F32(); ground.y = r.F32(); ground.z = r.F32();
		ground.pitch = r.F32(); ground.yaw = r.F32(); ground.roll = r.F32();
		ground.restOnBottom = r.U8() != 0;
		return ground;
	}
}

void PlayerSaveCodec::WriteAsset(const Asset* asset, ByteWriter& w) {
	// 第一个字节区分普通物品(0)和固定物品(1)。固定物品没有AssetMod，把规格整份写出来，读档时按规格重建。
	w.U8(asset->fixture ? 1 : 0);
	if (asset->fixture) {
		const AssetFixtureDesc& desc = *asset->fixture;
		w.Str(desc.type);
		w.Str(asset->name);
		w.Str(desc.meshPath);
		w.F32(desc.scaleX); w.F32(desc.scaleY); w.F32(desc.scaleZ);
		w.Str(desc.scriptModName);
		w.U32(static_cast<uint32_t>(desc.milestoneNames.size()));
		for (const string& milestone : desc.milestoneNames) w.Str(milestone);
		w.U32(static_cast<uint32_t>(asset->options.size()));
		for (const string& option : asset->options) w.Str(option);
		w.U8(asset->scriptStarted ? 1 : 0);
		return;
	}
	w.Str(asset->type);
	w.Str(asset->name);
	w.I32(asset->count);
	w.I32(asset->usesLeft); // 多次使用物品当前这一件的剩余次数
	w.I32(asset->magazineAmmo); // 武器弹匣余量，-1=没用过
	SaveRefs::WriteModState(asset->mod, w);
	w.U32(static_cast<uint32_t>(asset->options.size()));
	for (const string& option : asset->options) w.Str(option);
	w.U8(asset->scriptStarted ? 1 : 0);
	vector<const Asset*> contents = SortedAssets(asset->contents);
	w.U32(static_cast<uint32_t>(contents.size()));
	for (const Asset* content : contents) WriteAsset(content, w);
}

Asset* PlayerSaveCodec::ReadAsset(AssetFactory& factory, ByteReader& r) {
	if (r.U8() != 0) {
		// 固定物品：按存档里的规格重建(脚本的name/type变量构造时就是存档里的名字)。
		AssetFixtureDesc desc;
		desc.type = r.Str();
		string fixtureName = r.Str();
		desc.meshPath = r.Str();
		desc.scaleX = r.F32(); desc.scaleY = r.F32(); desc.scaleZ = r.F32();
		desc.scriptModName = r.Str();
		uint32_t milestoneCount = r.U32();
		for (uint32_t i = 0; i < milestoneCount && !r.Failed(); i++) desc.milestoneNames.push_back(r.Str());
		vector<string> savedOptions;
		uint32_t savedOptionCount = r.U32();
		for (uint32_t i = 0; i < savedOptionCount && !r.Failed(); i++) savedOptions.push_back(r.Str());
		bool started = r.U8() != 0;
		if (r.Failed()) return nullptr;
		Asset* fixtureAsset = new Asset(desc, fixtureName);
		fixtureAsset->options = std::move(savedOptions);
		fixtureAsset->scriptStarted = started;
		return fixtureAsset;
	}
	string type = r.Str();
	string name = r.Str();
	int count = r.I32();
	int usesLeft = r.I32();
	int magazineAmmo = r.I32();
	Asset* asset = new Asset(&factory, type);
	bool valid = asset->IsValid();
	asset->name = name; // 覆写回存档里的名字，容器/房间/worldAssets的key都按它索引
	if (!SaveRefs::ReadModState(valid ? asset->mod : static_cast<AssetMod*>(nullptr), r)) {
		debugf(DebugLevel::Warning, "failed to parse saved state of asset mod %s.\n", type.c_str());
	}
	asset->count = count;
	asset->usesLeft = usesLeft;
	asset->magazineAmmo = magazineAmmo;
	uint32_t optionCount = r.U32();
	for (uint32_t i = 0; i < optionCount && !r.Failed(); i++) asset->options.push_back(r.Str());
	asset->scriptStarted = r.U8() != 0;
	// 构造时脚本里的name是mod生成的默认名，覆写成存档里的名字。
	if (asset->script) asset->script->SetValue("name", ValueType(name));
	uint32_t contentCount = r.U32();
	for (uint32_t i = 0; i < contentCount && !r.Failed(); i++) {
		Asset* content = ReadAsset(factory, r);
		// 直接放回去，不走AddContent的空间检查：存档里本来就是装得下的(mod改了尺寸也不丢东西)。
		if (content) asset->contents[content->name] = content;
	}
	if (!valid) {
		debugf(DebugLevel::Warning, "asset mod %s is not available, item %s dropped.\n", type.c_str(), name.c_str());
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
		for (const Asset* asset : assets) {
			WriteGround(asset->GetGround(), w);
			WriteAsset(asset, w);
		}
	}

	// 房间外的物品(被扔出去的投掷物等)：位置朝向+整棵Asset树，不存速度，读档后从静止开始模拟。
	vector<string> worldNames;
	for (auto& [name, entry] : player.worldAssets) worldNames.push_back(name);
	sort(worldNames.begin(), worldNames.end());
	w.U32(static_cast<uint32_t>(worldNames.size()));
	for (const string& name : worldNames) {
		const Player::WorldAsset& entry = player.worldAssets.at(name);
		WriteGround(entry.asset->GetGround(), w);
		WriteAsset(entry.asset, w);
	}

	// 天气：当前类型+距下次自动换天气的剩余游戏毫秒。段尾后加的，更早的存档没有它。
	w.U8(static_cast<uint8_t>(player.weather.type));
	w.F64(static_cast<double>(player.weather.remainingMs));

	// 音乐音轨：轨道名、资产、循环、暂停、进度(UE回写的实际进度)。段尾后加的，更早的存档没有它。
	w.U32(static_cast<uint32_t>(player.music.tracks.size()));
	for (const auto& [name, track] : player.music.tracks) {
		w.Str(name);
		w.Str(track.asset);
		w.U8(track.loop ? 1 : 0);
		w.U8(track.paused ? 1 : 0);
		w.F64(track.position);
	}

	archive.SetBinary(SAVE_SECTION_PLAYER, VersionOf(SAVE_SECTION_PLAYER), w.Take());
}

Player* PlayerSaveCodec::Load(const SaveArchive& archive, Map& map, string& error) {
	const SaveSection* section = archive.Find(SAVE_SECTION_PLAYER);
	if (!section) {
		error = "the save has no section '" SAVE_SECTION_PLAYER "'";
		return nullptr;
	}

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
		player->currentRoom = SaveRefs::ReadRoom(map, r);

		uint32_t roomCount = r.U32();
		for (uint32_t i = 0; i < roomCount && !r.Failed(); i++) {
			Room* room = SaveRefs::ReadRoom(map, r);
			uint32_t assetCount = r.U32();
			for (uint32_t k = 0; k < assetCount && !r.Failed(); k++) {
				AssetGround ground = ReadGround(r);
				Asset* asset = ReadAsset(player->assetFactory, r);
				if (asset) asset->SetGround(ground);
				// 没有房间、或者房间里已经有同名物品(存档被改过)：这件没处放，丢掉。
				if (!(asset && room && room->AddAsset(asset))) delete asset;
			}
		}

		// 房间外的物品。这一块是后加的，更早的存档没有它，读到末尾就当没有。
		if (!r.AtEnd()) {
			uint32_t worldCount = r.U32();
			for (uint32_t i = 0; i < worldCount && !r.Failed(); i++) {
				Player::WorldAssetTransform transform = ReadGround(r);
				Asset* asset = ReadAsset(player->assetFactory, r);
				if (asset && !player->AddWorldAsset(asset, transform)) delete asset;
			}
		}

		// 天气。也是后加的，读到末尾就当没有(保持晴天+新抽的倒计时)；读档不触发weather_change。
		if (!r.AtEnd()) {
			uint8_t weatherType = r.U8();
			double remaining = r.F64();
			if (!r.Failed() && weatherType <= static_cast<uint8_t>(Weather::Type::HeavySnow)) {
				player->weather.type = static_cast<Weather::Type>(weatherType);
				player->weather.remainingMs = static_cast<long long>(remaining);
			}
		}

		// 音乐音轨。读档后都是"加载中"、带新的generation，UE层第一次对照时从存下的进度接着放。
		if (!r.AtEnd()) {
			uint32_t trackCount = r.U32();
			for (uint32_t i = 0; i < trackCount && !r.Failed(); i++) {
				string name = r.Str();
				MusicTracks::Track track;
				track.asset = r.Str();
				track.loop = r.U8() != 0;
				track.paused = r.U8() != 0;
				track.position = r.F64();
				if (r.Failed() || name.empty() || track.asset.empty()) continue;
				track.generation = player->music.nextGeneration++;
				player->music.tracks[name] = track;
			}
		}

		if (r.Failed()) {
			error = "player section is truncated";
			return nullptr;
		}
		return player;
	}
	catch (...) {
		error = "player load failed: " + DescribeCurrentException().ToLine();
		return nullptr;
	}
}
