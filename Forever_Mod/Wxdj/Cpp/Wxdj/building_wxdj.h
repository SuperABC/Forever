#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "map/building_mod.h"

// WxdjBuilding：五行斗诀开局的全局唯一建筑——只在井字路网中心lot(AREA_OFFICIAL_HIGH)里自适应贴路放一块
// 6x6，不参与权重随机填充(GetPower恒0)。3层，1楼用wxdj_fg+.layout(5个single房间，房间号00001~00005)，
// 2楼及以上用wxdj_fu+-.layout；layout是从Test组yuanshen模板拷贝的，没有电梯、没有挂脚本。
// 剧情通过Wxdj.script里的record_object(kind=building_wxdj)拿到它的地址，不需要C++。
class WxdjBuilding : public BuildingMod {
public:
	WxdjBuilding() { lastName = std::string("wxdj") + std::to_string(count++); }

	static const char* GetId() { return "building_wxdj"; }
	virtual const char* GetType() const override { return "building_wxdj"; }
	virtual const char* GetName() override { return lastName.c_str(); }

	virtual void Layout(int& direction, const Quad& quad,
		const std::unordered_map<int, Road*>& boundaryRoads) override {
		// Assign产出的显式占位总会带真实方向；传进来是-1(兜底)时从有真实边界路的方向里随机挑一个回写。
		if (direction < 0) {
			std::vector<int> candidates;
			for (auto& [dir, road] : boundaryRoads) {
				if (road) candidates.push_back(dir);
			}
			if (!candidates.empty()) {
				direction = candidates[GetRandom(static_cast<int>(candidates.size()))];
			}
		}
		int face = direction >= 0 ? direction : FACE_NORTH;

		footprint = { 0.5f, 0.5f, 1.f, 1.f };
		basements = 0;
		layers = 3;
		floorHeights.assign(layers, 0.4f);

		const std::string component = "component_empty";
		AssignFloor(0, "wxdj_fg+", face);
		for (int slot = 0; slot < 5; ++slot) AssignRoom(0, slot, "room_empty", component, 0);
		for (int slot = 0; slot < 2; ++slot) ArrangeRow(0, slot, "room_empty", 200.f, component, 0);
		for (int level = 1; level < layers; ++level) {
			AssignFloor(level, "wxdj_fu+-", face);
			for (int slot = 0; slot < 2; ++slot) AssignRoom(level, slot, "room_empty", component, 0);
			for (int slot = 0; slot < 2; ++slot) ArrangeRow(level, slot, "room_empty", 200.f, component, 0);
		}
	}

	// 同YuanshenBuilding::Assign：在中心lot里按剩余自由空间自适应选一块贴路的6x6。
	static void Assign(const std::vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {
		constexpr float kSize = 6.f;
		for (Lot* lot : lots) {
			if (!lot || lot->GetArea() != AREA_OFFICIAL_HIGH) continue;

			for (int dir = FACE_WEST; dir <= FACE_SOUTH; dir++) {
				Lot* targetLot;
				float marginStart, marginEnd, depth;
				if (!lot->FindAdaptivePlacement(dir, kSize, kSize, targetLot, marginStart, marginEnd, depth)) continue;

				marginStart += marginEnd;
				marginEnd = 0.f;

				LotPlacementRequest request;
				request.lot = targetLot;
				request.adaptiveParent = (targetLot != lot) ? lot : nullptr;
				request.direction = dir;
				request.marginStart = marginStart;
				request.marginEnd = marginEnd;
				request.depth = depth;
				emit(context, request);
				return;
			}
			return; // 中心lot只有一个
		}
	}
	static float RandomAcreage() { return 36.f; }
	static float GetAcreageMin() { return 36.f; }
	static float GetAcreageMax() { return 36.f; }
	static float GetPower(AREA_TYPE area) { return 0.f; }

	// 存档保存/恢复命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }

private:
	std::string lastName;
	static int count;
};
