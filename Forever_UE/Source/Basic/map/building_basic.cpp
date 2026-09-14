#include "building_basic.h"
#include "room_basic.h"

#include "common/utility.h"

#include <cmath>

using namespace std;

int BuildingBasic::count = 0;

BuildingBasic::BuildingBasic() {
	lastName = string(GetType()) + std::to_string(count++);
}

void BuildingBasic::Layout(int& direction, const Quad& quad,
	const std::unordered_map<int, Road*>& boundaryRoads) {
	// direction对显式占位是真实方向，对权重CDF/FillRemainder落地传进来是-1——这个类型的
	// 楼层内部布局(AssignFloor选的模板)按direction摆朝向，所以-1时要从boundaryRoads里随机
	// 挑一个有真实边界路的方向回写，保证这栋building最终有确定方向可用(行人导航的"outside"
	// 端点要靠这个方向找到该连去哪条路，见building_mod.h的Layout()注释)。
	if (direction < 0) {
		std::vector<int> candidates;
		for (auto& [dir, road] : boundaryRoads) {
			if (road) candidates.push_back(dir);
		}
		if (!candidates.empty()) {
			direction = candidates[GetRandom(static_cast<int>(candidates.size()))];
		}
	}

	footprint = BuildingFootprintSpec{ 0.5f, 0.5f, 0.8f, 0.8f }; // 楼体占地块80%，居中
	basements = 1;
	layers = 3 + GetRandom(5); // 地上楼层数3~7层随机(GetRandom(5)取[0,5))
	floorHeights.clear();
	floorHeights.push_back(0.35f); // 地下室
	constexpr float kFloorHeightCycle[] = { 0.4f, 0.45f, 0.5f, 0.42f }; // 循环取值，保证每层
		// 高度不完全一样(测试渲染层按floorHeights逐层堆叠、不是所有楼层同一高度)
	for (int i = 0; i < layers; i++) {
		floorHeights.push_back(kFloorHeightCycle[i % 4]);
	}
	// lodMaterial留空 -> 用渲染层默认灰色Pure MID

	// 楼层内部布局：地下室用"有上行楼梯"的模板，1楼用"上下都有"的模板，2楼及以上用
	// "有下行楼梯"的模板——照抄老工程ResidentialBuilding::LayoutBuilding的调用模式，
	// 复用从老工程迁移过来的.layout模板(见Resource/Layouts/)。每层两个row槽位都归到
	// 同一个(component_basic,0)组合下，模拟"一栋居民楼是一个整体"。
	constexpr const char* kComponent = "component_basic";
	constexpr int kComponentId = 0;
	constexpr float kRoomAcreage = 800.f;

	AssignFloor(-1, "preset_straight_linear_bg+", direction);
	ArrangeRow(-1, 0, RoomBasic::GetId(), kRoomAcreage, kComponent, kComponentId);
	ArrangeRow(-1, 1, RoomBasic::GetId(), kRoomAcreage, kComponent, kComponentId);

	AssignFloor(0, "preset_straight_linear_fg+-", direction);
	ArrangeRow(0, 0, RoomBasic::GetId(), kRoomAcreage, kComponent, kComponentId);
	ArrangeRow(0, 1, RoomBasic::GetId(), kRoomAcreage, kComponent, kComponentId);

	for (int level = 1; level < layers; level++) {
		AssignFloor(level, "preset_straight_linear_fu+-", direction);
		ArrangeRow(level, 0, RoomBasic::GetId(), kRoomAcreage, kComponent, kComponentId);
		ArrangeRow(level, 1, RoomBasic::GetId(), kRoomAcreage, kComponent, kComponentId);
	}
}

void BuildingBasic::Assign(const vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {
	// 不用显式占位。
}

// 老工程ResidentialBuilding::RandomAcreage/minAcreage/maxAcreage原始数值，照抄不改。
float BuildingBasic::RandomAcreage() {
	return 2000.f * powf(1.f + GetRandom(1000) / 1000.f * 2.f, 2);
}

float BuildingBasic::GetAcreageMin() {
	return 2000.f;
}

float BuildingBasic::GetAcreageMax() {
	return 18000.f;
}

float BuildingBasic::GetPower(AREA_TYPE area) {
	// RoadnetMod(JingRoadnet::DistributeRoadnet)已经会给每个lot调用Lot::SetArea()标好实际的
	// 分区类型，不是AREA_NONE——之前这里错误地假设所有lot都是默认值AREA_NONE，导致GetPower对
	// 每个真实lot都返回0，FillRemainder链路上的候选权重从未被登记过，一个独立building都生成
	// 不出来(PIE验证发现)。这个通用占位类型暂时不按分区类型区分权重，对所有分区一视同仁给
	// 权重1(照抄老工程改造前candidateWeights.push_back({lot,1.f})的行为)，等以后设计具体
	// 建筑类型时再按area细化。
	return 1.f;
}
