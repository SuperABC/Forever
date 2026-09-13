#include "building_basic.h"

#include "common/utility.h"

#include <cmath>

using namespace std;

int BuildingBasic::count = 0;

BuildingBasic::BuildingBasic() {
	lastName = string(GetType()) + std::to_string(count++);
}

void BuildingBasic::Layout(int direction, const Quad& quad,
	const std::unordered_map<int, Road*>& boundaryRoads) {
	// direction对显式占位是真实方向，对权重CDF/FillRemainder落地是-1——这个测试类型的楼体/
	// 楼层配置不需要跟着方向变化，也不需要看quad/boundaryRoads，两种情况都设成同一份固定数据
	// 即可(更复杂的类型可以用quad.GetSizeX()/GetSizeY()按实际落地尺寸调整楼层数等)。
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
