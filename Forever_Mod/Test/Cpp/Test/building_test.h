#pragma once

#include <string>

#include "map/building_mod.h"

// 阶段3示例mod:只实现GetId/GetType/GetName三个身份接口,用于验证Mod发现/加载/
// 注册链路。真正的建筑玩法逻辑(PlaceConstruction/LayoutBuilding/PlacePivots/
// GetPowers/BuildingAssigner等)留到阶段4迁移map系统时,对照旧工程
// E:\Projects\Forever_Mods\Test\Cpp\Xiaohua\building_xiaohua.h 补上。
//
// 这次会话改回"一个本体独占一个mod实例"模型：RandomAcreage/GetAcreageMin/GetAcreageMax/
// GetPower/Assign全部改成不需要实例的static方法（纯占位，返回0.f/空实现，不涉及任何真实
// 行为迁移），寻址用的唯一名字计数器直接写在构造函数里。
class PengzhanBuilding : public BuildingMod {
public:
	PengzhanBuilding() { lastName = std::string("pengzhan") + std::to_string(count++); }

	static const char* GetId() { return "pengzhan"; }
	virtual const char* GetType() const override { return "pengzhan"; }
	virtual const char* GetName() override { return lastName.c_str(); }

	static void Assign(const std::vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {}
	static float RandomAcreage() { return 0.f; }
	static float GetAcreageMin() { return 0.f; }
	static float GetAcreageMax() { return 0.f; }
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

class YizhongBuilding : public BuildingMod {
public:
	YizhongBuilding() { lastName = std::string("yizhong") + std::to_string(count++); }

	static const char* GetId() { return "yizhong"; }
	virtual const char* GetType() const override { return "yizhong"; }
	virtual const char* GetName() override { return lastName.c_str(); }

	static void Assign(const std::vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {}
	static float RandomAcreage() { return 0.f; }
	static float GetAcreageMin() { return 0.f; }
	static float GetAcreageMax() { return 0.f; }
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

// SuperGPBuilding：指定位置的测试建筑——只在井字路网中心lot(AREA_OFFICIAL_HIGH)里自适应找一块
// 6x6的位置放置，不参与权重随机填充(GetPower恒0)。对标旧工程Test的PengzhanBuilding
// (12层、每层0.4高、1楼+上层两套模板、一部电梯)，模板文件见Test/Resource/supergp_fg+.layout
// (1楼)、supergp_fu+-.layout(2楼及以上)，没有地下室，墙材质先不指定(用组件默认值)。
// 两个模板没有pedestrianNavigation块，所以楼内暂时没有行人导航图，见Test/Resource/README.md。
class SuperGPBuilding : public BuildingMod {
public:
	SuperGPBuilding() { lastName = std::string("supergp") + std::to_string(count++); }

	static const char* GetId() { return "supergp"; }
	virtual const char* GetType() const override { return "supergp"; }
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
		layers = 12;
		// 挂示例脚本entity_event：进出这栋楼的事件只发给它，见Forever_UE/Resource/Story/entity_event.script。
		milestoneNames = { "entity_event" };
		floorHeights.assign(layers, 0.4f);

		const std::string component = "empty";
		AssignFloor(0, "supergp_fg+", face);
		for (int slot = 0; slot < 5; ++slot) AssignRoom(0, slot, "empty", component, 0);
		for (int slot = 0; slot < 2; ++slot) ArrangeRow(0, slot, "empty", 200.f, component, 0);
		for (int level = 1; level < layers; ++level) {
			AssignFloor(level, "supergp_fu+-", face);
			for (int slot = 0; slot < 2; ++slot) AssignRoom(level, slot, "empty", component, 0);
			for (int slot = 0; slot < 2; ++slot) ArrangeRow(level, slot, "empty", 200.f, component, 0);
		}

		// 两个模板各自在elevators数组里只有一口井(下标0)，一部电梯从1楼跑到顶层。
		AssignElevatorCabin(0, 0, layers - 1, "", "empty", { "elevator_basic" });
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
