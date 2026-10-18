#pragma once

#include "common/save_archive.h"
#include "common/save.h"
#include "common/json.h"
#include "common/utility.h"

#include "map/map.h"
#include "map/building.h"
#include "map/room.h"
#include "map/component.h"

#include <string>
#include <unordered_map>
#include <vector>


// 跨域存档引用的统一编码，人口/社会/交通等域的存档都用这一套(只用各类的公开接口)：
// - Room/Component：(所属Building名字, 在building->GetRooms()/GetComponents()里的下标)。地图段按
//   存档原样恢复，读回来的下标和保存时一致。
// - Time：年月日时分秒毫秒逐个写。
// 详见save.md"跨域引用"一节。
namespace SaveRefs {
	template <typename T>
	int IndexOf(const std::vector<T*>& list, const T* item) {
		for (size_t i = 0; i < list.size(); i++) {
			if (list[i] == item) return static_cast<int>(i);
		}
		return -1;
	}

	inline void WriteRoom(const Room* room, ByteWriter& w) {
		const Building* building = room ? room->GetParentBuilding() : nullptr;
		w.Str(building ? building->GetName() : std::string());
		w.I32(building ? IndexOf(building->GetRooms(), room) : -1);
	}

	inline Room* ReadRoom(const Map& map, ByteReader& r) {
		std::string buildingName = r.Str();
		int index = r.I32();
		if (buildingName.empty() || index < 0) return nullptr;
		Building* building = map.GetBuilding(buildingName);
		if (!building || index >= static_cast<int>(building->GetRooms().size())) return nullptr;
		return building->GetRooms()[index];
	}

	inline void WriteComponent(const Component* component, ByteWriter& w) {
		const Building* building = component ? component->GetParentBuilding() : nullptr;
		w.Str(building ? building->GetName() : std::string());
		w.I32(building ? IndexOf(building->GetComponents(), component) : -1);
	}

	inline Component* ReadComponent(const Map& map, ByteReader& r) {
		std::string buildingName = r.Str();
		int index = r.I32();
		if (buildingName.empty() || index < 0) return nullptr;
		Building* building = map.GetBuilding(buildingName);
		if (!building || index >= static_cast<int>(building->GetComponents().size())) return nullptr;
		return building->GetComponents()[index];
	}

	inline void WriteTime(const Time& t, ByteWriter& w) {
		w.I32(t.GetYear()); w.I32(t.GetMonth()); w.I32(t.GetDay());
		w.I32(t.GetHour()); w.I32(t.GetMinute()); w.I32(t.GetSecond()); w.I32(t.GetMillisecond());
	}

	// mod实例 -> 注册id(Factory::Create用的那个id，不一定等于mod->GetType())，从Factory的存活实例表里查。
	template <typename F, typename M>
	std::unordered_map<const M*, std::string> LiveIds(const F& factory) {
		std::unordered_map<const M*, std::string> ids;
		for (auto& [id, mod] : factory.GetLiveInstances()) ids[mod] = id;
		return ids;
	}
	template <typename M>
	std::string LookupId(const std::unordered_map<const M*, std::string>& ids, const M* mod) {
		auto it = ids.find(mod);
		return it != ids.end() ? it->second : std::string();
	}

	// mod自己的状态(SaveState/LoadState)：状态版本+JSON文本。mod没有实现时是"{}"。
	template <typename M>
	void WriteModState(const M* mod, ByteWriter& w) {
		JsonStateWriter writer;
		int version = 0;
		if (mod) {
			mod->SaveState(writer);
			version = mod->GetStateVersion();
		}
		w.I32(version);
		w.Str(writer.Finish().WriteCompact());
	}

	// 读回并交给mod->LoadState。文本解析失败返回false(调用方打警告，mod保持新建时的状态)。
	template <typename M>
	bool ReadModState(M* mod, ByteReader& r) {
		int version = r.I32();
		std::string text = r.Str();
		if (!mod || text.empty()) return true;
		JsonValue root;
		JsonReader reader;
		if (!reader.Parse(text, root, false)) return false;
		JsonStateReader stateReader(root, version);
		mod->LoadState(stateReader);
		return true;
	}

	inline Time ReadTime(ByteReader& r) {
		int y = r.I32(), mon = r.I32(), d = r.I32(), h = r.I32(), min = r.I32(), s = r.I32(), ms = r.I32();
		return Time(y, mon, d, h, min, s, ms);
	}
}
