#pragma once

#include <string>

#include "map/building_mod.h"

// YuanshenBuilding：指定位置的测试建筑——只在井字路网中心lot(AREA_OFFICIAL_HIGH)里自适应找一块
// 6x6的位置放置，不参与权重随机填充(GetPower恒0)。对标旧工程Test的PengzhanBuilding
// (12层、每层0.4高、1楼+上层两套模板、一部电梯)，模板文件见Test/Resource/yuanshen_fg+.layout
// (1楼)、yuanshen_fu+-.layout(2楼及以上)，没有地下室，墙材质先不指定(用组件默认值)。
// 两个模板没有pedestrianNavigation块，所以楼内暂时没有行人导航图，见Test/Resource/README.md。
class YuanshenBuilding : public BuildingMod {
public:
	YuanshenBuilding() { lastName = std::string("yuanshen") + std::to_string(count++); }

	static const char* GetId() { return "building_yuanshen"; }
	virtual const char* GetType() const override { return "building_yuanshen"; }
	virtual const char* GetName() override { return lastName.c_str(); }

	virtual void Layout(int& direction, const Quad& quad,
		const std::unordered_map<int, Road*>& boundaryRoads) override {
		// Assign产出的显式占位总会带真实方向；传进来是-1(兜底)时从有真实边界路的方向里随机挑一个
		// 回写，和MoharBuilding同一个做法。
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

		// 楼体占满整个6x6矩形(BuildingFootprintSpec默认值，显式写出来强调)。
		footprint = { 0.5f, 0.5f, 1.f, 1.f };
		basements = 0;
		// 14层：YuanshenScript把30个角色放进来——1楼4个(房间00001~00004)，之后每层2个，用到14层。
		layers = 14;
		// 挂示例脚本entity_event：进出这栋楼的事件只发给它，见Forever_UE/Resource/Story/entity_event.script。
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

		// 两个模板各自在elevators数组里只有一口井(下标0)，一部电梯从1楼跑到顶层。
		AssignElevatorCabin(0, 0, layers - 1, "", "script_empty", { "elevator_basic" });
	}

	// 和TrainStationBuilding::Assign同一个思路：在中心lot里用Lot::FindAdaptivePlacement按当前
	// 实际剩余的自由空间自适应选一块贴路的6x6(它返回的margin/depth正好是minFrontage x
	// minDepth、沿路居中)，4个方向哪个先找到可用空间就用哪个；放不下就静默不放。
	static void Assign(const std::vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {
		constexpr float kSize = 6.f;
		for (Lot* lot : lots) {
			if (!lot || lot->GetArea() != AREA_OFFICIAL_HIGH) continue;

			for (int dir = FACE_WEST; dir <= FACE_SOUTH; dir++) {
				Lot* targetLot;
				float marginStart, marginEnd, depth;
				if (!lot->FindAdaptivePlacement(dir, kSize, kSize, targetLot, marginStart, marginEnd, depth)) continue;

				// 贴到命中子块这条边"终点"那个角(余量全部挪到marginStart)，和园区/火车站的起点角
				// 错开，别居中占掉整块连续空地。
				marginStart += marginEnd;
				marginEnd = 0.f;

				// request.lot是命中的自由子块，不是lot本身；targetLot!=lot时要让Core侧把子块的残余
				// 空间搬回lot，所以设adaptiveParent，见TrainStationBuilding::Assign的说明。
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

private:
	std::string lastName;
public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
};
