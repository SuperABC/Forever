#include "save.h"

#include "common/config.h"
#include "common/registry.h"
#include "common/utility.h"
#include "map/map.h"
#include "map/map_save.h"
#include "populace/populace_save.h"
#include "society/society_save.h"
#include "traffic/traffic_save.h"
#include "story/script_save.h"
#include "player/player_save.h"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <unordered_map>
#include <unordered_set>


using namespace std;

const vector<SaveSectionVersion>& GetSaveSectionVersions() {
	static const vector<SaveSectionVersion> versions = {
		{ SAVE_SECTION_MANIFEST, 1, 1 },
		{ SAVE_SECTION_MAP_TERRAIN, 1, 1 },
		{ SAVE_SECTION_MAP_ROADNET, 1, 1 },
		{ SAVE_SECTION_MAP_STRUCTURES, 1, 1 },
		{ SAVE_SECTION_MAP_VEGETATION, 1, 1 },
		// 人口/社会段可以缺省：没有时(P2阶段的存档)这两个域在恢复出来的地图上重新生成。
		{ SAVE_SECTION_POPULACE, 1, 1, false },
		{ SAVE_SECTION_SOCIETY, 1, 1, false },
		// 交通/脚本状态/UE侧状态同理(P4及更早的存档没有)：交通没有时重新生成；脚本状态没有时
		// 照新开局广播game_start；UE段没有时由game_start决定玩家操控谁。
		{ SAVE_SECTION_TRAFFIC, 1, 1, false },
		{ SAVE_SECTION_SCRIPTS, 1, 1, false },
		{ SAVE_SECTION_UE, 1, 1, false },
		{ SAVE_SECTION_PLAYER, 1, 1, false },
	};
	return versions;
}

namespace {
	// ---- base64(SetBytes/GetBytes用) ----
	const char kBase64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	string Base64Encode(const uint8_t* data, size_t size) {
		string out;
		out.reserve((size + 2) / 3 * 4);
		for (size_t i = 0; i < size; i += 3) {
			uint32_t n = static_cast<uint32_t>(data[i]) << 16;
			if (i + 1 < size) n |= static_cast<uint32_t>(data[i + 1]) << 8;
			if (i + 2 < size) n |= data[i + 2];
			out.push_back(kBase64[(n >> 18) & 63]);
			out.push_back(kBase64[(n >> 12) & 63]);
			out.push_back(i + 1 < size ? kBase64[(n >> 6) & 63] : '=');
			out.push_back(i + 2 < size ? kBase64[n & 63] : '=');
		}
		return out;
	}

	vector<uint8_t> Base64Decode(const string& text) {
		int table[256];
		fill(begin(table), end(table), -1);
		for (int i = 0; i < 64; i++) table[static_cast<unsigned char>(kBase64[i])] = i;
		vector<uint8_t> out;
		uint32_t buffer = 0;
		int bits = 0;
		for (char c : text) {
			int v = table[static_cast<unsigned char>(c)];
			if (v < 0) continue; // '='和非法字符直接跳过
			buffer = (buffer << 6) | static_cast<uint32_t>(v);
			bits += 6;
			if (bits >= 8) {
				bits -= 8;
				out.push_back(static_cast<uint8_t>((buffer >> bits) & 0xFF));
			}
		}
		return out;
	}

	JsonValue StringArray(const vector<string>& items) {
		JsonValue array(DATA_ARRAY);
		for (const string& item : items) array.append(JsonValue(item));
		return array;
	}

	vector<string> ReadStringArray(const JsonValue& array) {
		vector<string> out;
		if (!array.IsArray()) return out;
		for (unsigned int i = 0; i < array.size(); i++) out.push_back(array[i].AsString());
		return out;
	}

	string FileStem(const string& path) {
		return filesystem::path(path).stem().string();
	}

	// 21个concept的统一操作表：按jsonKey枚举存活实例(id+计数器+状态版本)、临时创建实例取状态
	// 版本、恢复计数器。每个concept的Factory/Mod类型不同，用模板+成员函数指针生成，避免手写21遍。
	struct ConceptOps {
		string jsonKey;
		function<void(vector<tuple<string, int, int>>&)> collectLive; // (id, counter, stateVersion)
		function<int(const string&)> probeStateVersion;               // 创建失败返回-1
		function<void(const string&, int)> setCounter;
		function<bool(const string&)> isRegistered;
	};

	template <class Factory, class Mod>
	ConceptOps MakeOps(const char* jsonKey, Factory& factory, Mod* (Factory::* create)(const string&),
		void (Factory::* destroy)(Mod*)) {
		ConceptOps ops;
		ops.jsonKey = jsonKey;
		ops.collectLive = [&factory](vector<tuple<string, int, int>>& out) {
			for (auto& [id, instance] : factory.GetLiveInstances()) {
				if (instance) out.emplace_back(id, instance->GetInstanceCounter(), instance->GetStateVersion());
			}
			};
		ops.probeStateVersion = [&factory, create, destroy](const string& id) {
			Mod* instance = (factory.*create)(id);
			if (!instance) return -1;
			int version = instance->GetStateVersion();
			(factory.*destroy)(instance);
			return version;
			};
		ops.isRegistered = [&factory](const string& id) { return factory.CheckRegistered(id); };
		ops.setCounter = [&factory, create, destroy](const string& id, int value) {
			// 临时实例的构造会让计数器加一，所以先建、再设、再销毁(销毁不改计数器)。
			Mod* instance = (factory.*create)(id);
			if (!instance) return;
			instance->SetInstanceCounter(value);
			(factory.*destroy)(instance);
			};
		return ops;
	}

	const vector<ConceptOps>& GetConceptOps() {
		static vector<ConceptOps> table = [] {
			Registry& r = Registry::Get();
			vector<ConceptOps> t;
			t.push_back(MakeOps("app_mods", r.GetAppFactory(), &AppFactory::CreateApp, &AppFactory::DestroyApp));
			t.push_back(MakeOps("asset_mods", r.GetAssetFactory(), &AssetFactory::CreateAsset, &AssetFactory::DestroyAsset));
			t.push_back(MakeOps("building_mods", r.GetBuildingFactory(), &BuildingFactory::CreateBuilding, &BuildingFactory::DestroyBuilding));
			t.push_back(MakeOps("component_mods", r.GetComponentFactory(), &ComponentFactory::CreateComponent, &ComponentFactory::DestroyComponent));
			t.push_back(MakeOps("job_mods", r.GetJobFactory(), &JobFactory::CreateJob, &JobFactory::DestroyJob));
			t.push_back(MakeOps("manufacture_mods", r.GetManufactureFactory(), &ManufactureFactory::CreateManufacture, &ManufactureFactory::DestroyManufacture));
			t.push_back(MakeOps("name_mods", r.GetNameFactory(), &NameFactory::CreateName, &NameFactory::DestroyName));
			t.push_back(MakeOps("organization_mods", r.GetOrganizationFactory(), &OrganizationFactory::CreateOrganization, &OrganizationFactory::DestroyOrganization));
			t.push_back(MakeOps("product_mods", r.GetProductFactory(), &ProductFactory::CreateProduct, &ProductFactory::DestroyProduct));
			t.push_back(MakeOps("puzzle_mods", r.GetPuzzleFactory(), &PuzzleFactory::CreatePuzzle, &PuzzleFactory::DestroyPuzzle));
			t.push_back(MakeOps("roadnet_mods", r.GetRoadnetFactory(), &RoadnetFactory::CreateRoadnet, &RoadnetFactory::DestroyRoadnet));
			t.push_back(MakeOps("room_mods", r.GetRoomFactory(), &RoomFactory::CreateRoom, &RoomFactory::DestroyRoom));
			t.push_back(MakeOps("route_mods", r.GetRouteFactory(), &RouteFactory::CreateRoute, &RouteFactory::DestroyRoute));
			t.push_back(MakeOps("scheduler_mods", r.GetSchedulerFactory(), &SchedulerFactory::CreateScheduler, &SchedulerFactory::DestroyScheduler));
			t.push_back(MakeOps("script_mods", r.GetScriptFactory(), &ScriptFactory::CreateScript, &ScriptFactory::DestroyScript));
			t.push_back(MakeOps("station_mods", r.GetStationFactory(), &StationFactory::CreateStation, &StationFactory::DestroyStation));
			t.push_back(MakeOps("storage_mods", r.GetStorageFactory(), &StorageFactory::CreateStorage, &StorageFactory::DestroyStorage));
			t.push_back(MakeOps("terrain_mods", r.GetTerrainFactory(), &TerrainFactory::CreateTerrain, &TerrainFactory::DestroyTerrain));
			t.push_back(MakeOps("vehicle_mods", r.GetVehicleFactory(), &VehicleFactory::CreateVehicle, &VehicleFactory::DestroyVehicle));
			t.push_back(MakeOps("weapon_mods", r.GetWeaponFactory(), &WeaponFactory::CreateWeapon, &WeaponFactory::DestroyWeapon));
			t.push_back(MakeOps("zone_mods", r.GetZoneFactory(), &ZoneFactory::CreateZone, &ZoneFactory::DestroyZone));
			return t;
		}();
		return table;
	}

	const ConceptOps* FindOps(const string& jsonKey) {
		for (const ConceptOps& ops : GetConceptOps()) {
			if (ops.jsonKey == jsonKey) return &ops;
		}
		return nullptr;
	}
}

// ---- SaveManifest ----

JsonValue SaveManifest::ToJson() const {
	JsonValue json(DATA_OBJECT);
	json["gameVersion"] = JsonValue(gameVersion);
	json["mainStory"] = JsonValue(mainStory);
	json["storyScripts"] = StringArray(storyScripts);
	JsonValue modArray(DATA_ARRAY);
	for (const SaveModRecord& mod : mods) {
		JsonValue item(DATA_OBJECT);
		item["concept"] = JsonValue(mod.conceptKey);
		item["id"] = JsonValue(mod.modId);
		item["args"] = JsonValue(mod.args);
		item["stateVersion"] = JsonValue(mod.stateVersion);
		item["counter"] = JsonValue(mod.counter);
		modArray.append(item);
	}
	json["mods"] = modArray;
	json["scripts"] = StringArray(scripts);
	json["layouts"] = StringArray(layouts);
	return json;
}

bool SaveManifest::FromJson(const JsonValue& json, SaveManifest& out, string& error) {
	if (!json.IsObject() || !json["mods"].IsArray()) {
		error = "manifest is malformed";
		return false;
	}
	out = SaveManifest();
	out.gameVersion = json["gameVersion"].AsString();
	out.mainStory = json["mainStory"].AsString();
	out.storyScripts = ReadStringArray(json["storyScripts"]);
	const JsonValue& modArray = json["mods"];
	for (unsigned int i = 0; i < modArray.size(); i++) {
		const JsonValue& item = modArray[i];
		SaveModRecord mod;
		mod.conceptKey = item["concept"].AsString();
		mod.modId = item["id"].AsString();
		mod.args = item["args"].AsString();
		mod.stateVersion = item["stateVersion"].AsInt();
		mod.counter = item["counter"].AsInt();
		out.mods.push_back(mod);
	}
	out.scripts = ReadStringArray(json["scripts"]);
	out.layouts = ReadStringArray(json["layouts"]);
	return true;
}

// ---- JsonStateWriter ----

JsonStateWriter::JsonStateWriter() {
	stack.emplace_back(string(), JsonValue(DATA_OBJECT));
}

void JsonStateWriter::Put(const char* key, const JsonValue& value) {
	JsonValue& node = stack.back().second;
	if (node.IsArray()) node.append(value);
	else node[key ? key : ""] = value;
}

void JsonStateWriter::SetInt(const char* key, int64_t value) { Put(key, JsonValue(static_cast<long long>(value))); }
void JsonStateWriter::SetDouble(const char* key, double value) { Put(key, JsonValue(value)); }
void JsonStateWriter::SetBool(const char* key, bool value) { Put(key, JsonValue(value)); }
void JsonStateWriter::SetString(const char* key, const char* value) { Put(key, JsonValue(value ? value : "")); }
void JsonStateWriter::SetBytes(const char* key, const void* data, int64_t size) {
	Put(key, JsonValue(Base64Encode(static_cast<const uint8_t*>(data), size > 0 ? static_cast<size_t>(size) : 0)));
}

void JsonStateWriter::BeginObject(const char* key) { stack.emplace_back(key ? key : "", JsonValue(DATA_OBJECT)); }
void JsonStateWriter::BeginArray(const char* key) { stack.emplace_back(key ? key : "", JsonValue(DATA_ARRAY)); }

void JsonStateWriter::Close() {
	if (stack.size() <= 1) return;
	auto [key, child] = stack.back();
	stack.pop_back();
	Put(key.c_str(), child);
}

void JsonStateWriter::EndObject() { Close(); }
void JsonStateWriter::EndArray() { Close(); }

JsonValue JsonStateWriter::Finish() {
	while (stack.size() > 1) Close();
	return stack.back().second;
}

// ---- JsonStateReader ----

JsonStateReader::JsonStateReader(const JsonValue& inRoot, int inSavedVersion) : root(inRoot), savedVersion(inSavedVersion) {
	stack.push_back(&root);
}

const JsonValue* JsonStateReader::Current() const { return stack.back(); }

const JsonValue* JsonStateReader::Member(const char* key) const {
	const JsonValue* node = Current();
	if (!key || !node->IsObject() || !node->ValidMember(key)) return nullptr;
	return &(*node)[key];
}

const JsonValue* JsonStateReader::Element(int index) const {
	const JsonValue* node = Current();
	if (!node->IsArray() || index < 0 || !node->ValidIndex(static_cast<unsigned int>(index))) return nullptr;
	return &(*node)[index];
}

int JsonStateReader::GetSavedVersion() const { return savedVersion; }
bool JsonStateReader::Has(const char* key) const { return Member(key) != nullptr; }

int64_t JsonStateReader::GetInt(const char* key, int64_t fallback) const {
	const JsonValue* v = Member(key);
	return v && v->IsNumeric() ? static_cast<int64_t>(v->AsDouble()) : fallback;
}

double JsonStateReader::GetDouble(const char* key, double fallback) const {
	const JsonValue* v = Member(key);
	return v && v->IsNumeric() ? v->AsDouble() : fallback;
}

bool JsonStateReader::GetBool(const char* key, bool fallback) const {
	const JsonValue* v = Member(key);
	return v && v->IsBool() ? v->AsBool() : fallback;
}

const char* JsonStateReader::GetString(const char* key, const char* fallback) const {
	const JsonValue* v = Member(key);
	if (!v || !v->IsString()) return fallback;
	scratch = v->AsString();
	return scratch.c_str();
}

int64_t JsonStateReader::GetBytes(const char* key, void* out, int64_t outCapacity) const {
	const JsonValue* v = Member(key);
	if (!v || !v->IsString()) return 0;
	vector<uint8_t> bytes = Base64Decode(v->AsString());
	if (out && outCapacity > 0) {
		size_t n = min(bytes.size(), static_cast<size_t>(outCapacity));
		copy(bytes.begin(), bytes.begin() + n, static_cast<uint8_t*>(out));
	}
	return static_cast<int64_t>(bytes.size());
}

bool JsonStateReader::EnterObject(const char* key) {
	const JsonValue* v = Member(key);
	if (!v || !v->IsObject()) return false;
	stack.push_back(v);
	return true;
}

void JsonStateReader::ExitObject() { if (stack.size() > 1) stack.pop_back(); }

bool JsonStateReader::EnterArray(const char* key) {
	const JsonValue* v = Member(key);
	if (!v || !v->IsArray()) return false;
	stack.push_back(v);
	return true;
}

void JsonStateReader::ExitArray() { if (stack.size() > 1) stack.pop_back(); }

int JsonStateReader::GetArraySize() const {
	const JsonValue* node = Current();
	return node->IsArray() ? static_cast<int>(node->size()) : 0;
}

bool JsonStateReader::EnterArrayElement(int index) {
	const JsonValue* v = Element(index);
	if (!v) return false;
	stack.push_back(v);
	return true;
}

int64_t JsonStateReader::GetIntAt(int index, int64_t fallback) const {
	const JsonValue* v = Element(index);
	return v && v->IsNumeric() ? static_cast<int64_t>(v->AsDouble()) : fallback;
}

double JsonStateReader::GetDoubleAt(int index, double fallback) const {
	const JsonValue* v = Element(index);
	return v && v->IsNumeric() ? v->AsDouble() : fallback;
}

const char* JsonStateReader::GetStringAt(int index, const char* fallback) const {
	const JsonValue* v = Element(index);
	if (!v || !v->IsString()) return fallback;
	scratch = v->AsString();
	return scratch.c_str();
}

// ---- SaveGame ----

SaveManifest SaveGame::BuildManifest(const SaveWorld& world) {
	const Map* map = world.map;
	SaveManifest manifest;
	manifest.gameVersion = SAVE_GAME_VERSION;
	manifest.mainStory = Config::GetMainStoryScriptModName();

	// 存活实例的计数器(同一个id取最大值)和状态版本。
	unordered_map<string, pair<int, int>> liveInfo; // "jsonKey/id" -> (counter, stateVersion)
	for (const ConceptOps& ops : GetConceptOps()) {
		vector<tuple<string, int, int>> live;
		ops.collectLive(live);
		for (auto& [id, counter, stateVersion] : live) {
			auto [it, inserted] = liveInfo.emplace(ops.jsonKey + "/" + id, make_pair(counter, stateVersion));
			if (!inserted) {
				it->second.first = max(it->second.first, counter);
				it->second.second = max(it->second.second, stateVersion);
			}
		}
	}

	// 存档用到的mod集合 = 当前启用的mod集合(按config.json的"<concept>_mods"，含参数字符串)。
	vector<string> keys = Config::GetConceptJsonKeys();
	sort(keys.begin(), keys.end());
	for (const string& key : keys) {
		for (auto& [id, args] : Config::GetConceptMods(key)) {
			SaveModRecord record;
			record.conceptKey = key;
			record.modId = id;
			record.args = args;
			auto it = liveInfo.find(key + "/" + id);
			if (it != liveInfo.end()) {
				record.counter = it->second.first;
				record.stateVersion = it->second.second;
			}
			manifest.mods.push_back(record);
		}
	}

	// 脚本：Story screen的脚本 + 主线脚本，只存名字(不含路径/扩展名)。
	unordered_set<string> scriptSet;
	for (const string& path : Config::GetStoryScripts()) {
		string stem = FileStem(path);
		manifest.storyScripts.push_back(stem);
		if (scriptSet.insert(stem).second) manifest.scripts.push_back(stem);
	}
	string mainScript = Config::GetMainStoryScriptPath();
	if (!mainScript.empty()) {
		string stem = FileStem(mainScript);
		if (scriptSet.insert(stem).second) manifest.scripts.push_back(stem);
	}

	// 布局：所有建筑登记的楼层模板名。读Building自己的快照，不读mod->floors——读档恢复出来的
	// 建筑mod是新建的空实例，floors是空的(见map_save.md)。
	unordered_set<string> layoutSet;
	if (map) {
		for (auto& [name, building] : map->GetBuildings()) {
			if (!building) continue;
			for (auto& [level, spec] : building->GetFloorSpecs()) {
				if (!spec.templateName.empty() && layoutSet.insert(spec.templateName).second) {
					manifest.layouts.push_back(spec.templateName);
				}
			}
		}
	}
	sort(manifest.layouts.begin(), manifest.layouts.end());
	return manifest;
}

SaveArchive SaveGame::BuildArchive(const SaveWorld& world) {
	SaveArchive archive;
	archive.SetJson(SAVE_SECTION_MANIFEST, 1, BuildManifest(world).ToJson().WriteCompact());
	if (world.map) {
		MapSaveCodec::Save(*world.map, archive);
		if (world.populace) {
			PopulaceSaveCodec::Save(*world.populace, *world.map, world.society, archive);
			if (world.society) SocietySaveCodec::Save(*world.society, *world.map, *world.populace, archive);
		}
		if (world.traffic) TrafficSaveCodec::Save(*world.traffic, *world.map, archive);
		ScriptSaveCodec::Save(world, archive);
		if (world.player) PlayerSaveCodec::Save(*world.player, *world.map, archive);
	}
	return archive;
}

vector<string> SaveGame::ValidateArchive(const SaveArchive& archive, SaveManifest& outManifest) {
	vector<string> errors;

	// 段版本：不认识的段、比当前新、比最低可读还旧的段都拒绝。
	for (const SaveSection& section : archive.GetSections()) {
		const SaveSectionVersion* known = nullptr;
		for (const SaveSectionVersion& v : GetSaveSectionVersions()) {
			if (section.name == v.name) { known = &v; break; }
		}
		if (!known) {
			errors.push_back("unknown save section '" + section.name + "' (save is from a newer version?)");
		}
		else if (section.version > known->current) {
			errors.push_back("section '" + section.name + "' version " + to_string(section.version) +
				" is newer than this game supports (" + to_string(known->current) + ")");
		}
		else if (section.version < known->minimum) {
			errors.push_back("section '" + section.name + "' version " + to_string(section.version) +
				" is too old (minimum " + to_string(known->minimum) + ")");
		}
	}

	for (const SaveSectionVersion& v : GetSaveSectionVersions()) {
		if (v.required && !archive.Find(v.name)) {
			errors.push_back(string("save has no section '") + v.name + "' (save is from an older version?)");
		}
	}
	const SaveSection* manifestSection = archive.Find(SAVE_SECTION_MANIFEST);
	if (!manifestSection) return errors;
	JsonValue manifestJson;
	JsonReader reader;
	string manifestText(manifestSection->data.begin(), manifestSection->data.end());
	string parseError;
	if (!reader.Parse(manifestText, manifestJson, false) || !SaveManifest::FromJson(manifestJson, outManifest, parseError)) {
		errors.push_back("manifest is corrupted" + (parseError.empty() ? string() : ": " + parseError));
		return errors;
	}

	// mod：每个(jsonKey, modId)都要有已发现的DLL提供。
	unordered_set<string> available;
	for (const Config::ModEntry& entry : Config::GetModEnables()) {
		available.insert(Config::ConceptKeyToJsonKey(entry.conceptKey) + "/" + entry.modId);
	}
	unordered_set<string> savedIds;
	for (const SaveModRecord& mod : outManifest.mods) savedIds.insert(mod.modId);
	for (const SaveModRecord& mod : outManifest.mods) {
		if (!available.count(mod.conceptKey + "/" + mod.modId)) {
			errors.push_back("missing mod: " + mod.conceptKey + " / " + mod.modId);
		}
		// 依赖：每个mod声明依赖的id也必须在存档的mod集合里。
		for (const string& dependence : Config::GetModDependences(mod.modId)) {
			if (!savedIds.count(dependence)) {
				errors.push_back("mod " + mod.modId + " depends on " + dependence + ", which the save does not use");
			}
		}
	}

	// 资源：脚本名、布局模板名都要能在配置的资源目录里找到。先重扫一遍，玩家运行中补回/删掉的文件才算数。
	Config::RescanFiles();
	for (const string& name : Config::FindMissingScripts(outManifest.scripts)) {
		errors.push_back("missing script: " + name + " (add it to a configured resource path)");
	}
	for (const string& name : Config::FindMissingLayouts(outManifest.layouts)) {
		errors.push_back("missing layout: " + name + " (add it to a configured layout/resource path)");
	}
	return errors;
}

SaveGame::ConfigSnapshot SaveGame::ApplyManifestConfig(const SaveManifest& manifest) {
	ConfigSnapshot snapshot;
	for (const string& key : Config::GetConceptJsonKeys()) {
		snapshot.conceptMods.emplace_back(key, Config::GetConceptMods(key));
	}
	snapshot.storyScripts = Config::GetStoryScripts();
	snapshot.mainStory = Config::GetMainStoryScriptModName();

	// 当前所有concept先清空，再按存档逐个填回——存档里没出现的concept就是"没启用任何mod"。
	unordered_map<string, vector<pair<string, string>>> mods;
	for (const string& key : Config::GetConceptJsonKeys()) mods[key];
	for (const SaveModRecord& mod : manifest.mods) mods[mod.conceptKey].emplace_back(mod.modId, mod.args);
	for (auto& [key, list] : mods) Config::SetConceptMods(key, list);

	vector<string> storyPaths;
	for (const string& name : manifest.storyScripts) {
		string path = Config::GetScriptPath(name);
		if (!path.empty()) storyPaths.push_back(path);
	}
	Config::SetStoryScripts(storyPaths);
	Config::SetMainStoryScriptModName(manifest.mainStory);

	Registry::Get().ReloadModArgs();
	return snapshot;
}

void SaveGame::RestoreConfig(const ConfigSnapshot& snapshot) {
	for (const string& key : Config::GetConceptJsonKeys()) Config::SetConceptMods(key, {});
	for (auto& [key, list] : snapshot.conceptMods) Config::SetConceptMods(key, list);
	Config::SetStoryScripts(snapshot.storyScripts);
	Config::SetMainStoryScriptModName(snapshot.mainStory);
	Registry::Get().ReloadModArgs();
}

vector<string> SaveGame::ValidateModsRegistered(const SaveManifest& manifest) {
	vector<string> errors;
	for (const SaveModRecord& mod : manifest.mods) {
		const ConceptOps* ops = FindOps(mod.conceptKey);
		if (!ops) continue;
		if (!ops->isRegistered(mod.modId)) {
			errors.push_back("mod " + mod.conceptKey + " / " + mod.modId +
				" is not loaded in this session (its DLL was not loaded at startup; restart the game and load again)");
		}
	}
	return errors;
}

vector<string> SaveGame::ValidateModStateVersions(const SaveManifest& manifest) {
	vector<string> errors;
	for (const SaveModRecord& mod : manifest.mods) {
		if (mod.stateVersion <= 0) continue; // 0=没声明版本，任何当前版本都能读
		const ConceptOps* ops = FindOps(mod.conceptKey);
		if (!ops) continue;
		int current = ops->probeStateVersion(mod.modId);
		if (current < 0) {
			errors.push_back("cannot create mod " + mod.conceptKey + " / " + mod.modId + " to check its state version");
		}
		else if (mod.stateVersion > current) {
			errors.push_back("mod " + mod.conceptKey + " / " + mod.modId + " state version " + to_string(mod.stateVersion) +
				" is newer than the installed mod supports (" + to_string(current) + ")");
		}
	}
	return errors;
}

void SaveGame::RestoreInstanceCounters(const SaveManifest& manifest) {
	for (const SaveModRecord& mod : manifest.mods) {
		if (mod.counter < 0) continue;
		const ConceptOps* ops = FindOps(mod.conceptKey);
		if (ops) ops->setCounter(mod.modId, mod.counter);
	}
}
