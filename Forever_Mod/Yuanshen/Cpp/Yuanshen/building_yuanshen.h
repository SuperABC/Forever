#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "map/building_mod.h"

// 设计说明见building_yuanshen.md。
class YuanshenBuilding : public BuildingMod {
public:
	YuanshenBuilding() : id(count++) { }

	static const char* GetId() { return "building_yuanshen"; }
	virtual const char* GetType() const override { return "building_yuanshen"; }
	virtual const char* GetName() override {
		name = "yuanshen" + std::to_string(id);
		return name.data();
	}

	virtual void Layout(int& direction, const Quad& quad,
		const std::unordered_map<int, Road*>& boundaryRoads) override {
		// direction<0是兜底：从有真实边界路的方向里随机挑一个回写。
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
		// 层数由ys.script要放进来的角色数决定，见md。
		layers = 14;
		milestoneNames = { "entity_event" };
		floorHeights.assign(layers, 0.4f);

		const std::string component = "component_empty";
		AssignFloor(0, "yuanshen_fg+", face);
		for (int slot = 0; slot < 5; ++slot) AssignRoom(0, slot, "room_empty", component, 0);
		for (int slot = 0; slot < 2; ++slot) ArrangeRow(0, slot, "room_empty", 200.f, component, 0);
		for (int level = 1; level < layers; ++level) {
			AssignFloor(level, "yuanshen_fu+-", face);
			for (int slot = 0; slot < 2; ++slot) AssignRoom(level, slot, "room_empty", component, 0);
			for (int slot = 0; slot < 2; ++slot) ArrangeRow(level, slot, "room_empty", 200.f, component, 0);
		}

		// 两个模板的elevators数组里都只有一口井(下标0)。
		AssignElevatorCabin(0, 0, layers - 1, "", "script_empty", { "elevator_basic" });
	}

	static void Assign(const std::vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {
		constexpr float kSize = 6.f;
		for (Lot* lot : lots) {
			if (!lot || lot->GetArea() != AREA_TEST) continue;

			for (int dir = FACE_WEST; dir <= FACE_SOUTH; dir++) {
				Lot* targetLot;
				float marginStart, marginEnd, depth;
				if (!lot->FindAdaptivePlacement(dir, kSize, kSize, targetLot, marginStart, marginEnd, depth)) continue;

				// 余量全挪到marginStart，贴到边的终点角，避免居中占掉整块连续空地。
				marginStart += marginEnd;
				marginEnd = 0.f;

				// request.lot是命中的自由子块；targetLot!=lot时必须设adaptiveParent，让Core把残余空间搬回lot。
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

public:
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }

private:
	static inline int count = 0;
	int id;
	std::string name;
};
