#include "zone_basic.h"

#include <cmath>

using namespace std;

void ZoneBasic::Distribute(const vector<Lot*>& lots) {
	// 老工程ResidentialZone::LayoutZone的常量，Quad::acreage换算系数两边工程完全一致，
	// 不需要单位换算；换算成一个近似正方形的边长，贴着lot任意一条有路的边居中摆放。
	constexpr float TARGET_ACREAGE = 20000.f;
	float side = sqrtf(TARGET_ACREAGE / ACREAGE_SCALE_FACTOR);

	for (Lot* lot : lots) {
		if (!lot) continue;
		for (int dir = 0; dir < 4; dir++) {
			if (!lot->GetBoundaryRoad(dir)) continue;
			bool alongY = (dir == FACE_WEST || dir == FACE_EAST);
			float frontage = alongY ? lot->GetSizeY() : lot->GetSizeX();
			if (frontage < side) continue;

			LotPlacementRequest request;
			request.lot = lot;
			request.direction = dir;
			request.marginStart = (frontage - side) / 2.f;
			request.marginEnd = (frontage - side) / 2.f;
			request.depth = side;
			explicitPlacements.push_back(request);
			break; // 这个lot已经安排了一个Zone，换下一个lot
		}
	}
}
