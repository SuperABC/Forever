#include "building_basic.h"

#include "common/utility.h"

#include <cmath>

using namespace std;

void BuildingBasic::Distribute(const vector<Lot*>& lots) {
	// 不直接调用lot->AddCandidate(...)——那是Forever.dll分配/析构的对象，Basic.dll跨模块
	// 写它自己的std::vector会导致退出时析构崩溃，改成push进mod自己拥有的candidateWeights，
	// 由引擎(Map::InitBuildings)读回去后自己调用，见building_mod.h的注释。
	for (Lot* lot : lots) {
		if (!lot) continue;
		candidateWeights.push_back({ lot, 1.f });
	}
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
