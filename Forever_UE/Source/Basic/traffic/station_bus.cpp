#include "station_bus.h"

#include "map/general_geometry.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <map>

using namespace std;

int BusStation::count = 0;

BusStation::BusStation() : id(count++) {
}

const char* BusStation::GetName() {
	name = "BusStation" + to_string(id);
	return name.data();
}

void BusStation::SetProperty() {
	stationType = "bus";
}

void BusStation::AssignRoads(const vector<Road*>& roads, RoadStationEmitFunc emit, void* context) {
	// 棋盘格路网的4车道大路(每侧>=2条车行道)，长度在[kMinLength,kMaxLength]的(跨1~2格；跨3格的会误加，不管)靠中点两侧各放一个站，
	// 井字路网没有4车道大路，所以没有公交。站在每侧最外侧车道上(Core的ResolveAccessLane保证)。见station_bus.md。
	constexpr float kMinLength = 24.f;
	constexpr float kMaxLength = 96.f;
	constexpr float kMinFreeLength = 2.f;  // 站所在的"被路口/出入口切开的路段"至少这么长
	constexpr float kEndMargin = 1.f;      // 站离所在路段两端至少这么远
	constexpr float kFlatTolerance = 0.05f;
	map<string, int> counters; // 同名路(棋盘格里同一条横/竖线被路口切成多段Road)上的站按名字计数

	for (Road* road : roads) {
		if (!road) continue;
		if (road->GetVehicleLanes(0).size() < 2 || road->GetVehicleLanes(1).size() < 2) continue;
		float length = road->CalcDistance();
		if (length < kMinLength || length > kMaxLength) continue;

		// 只放在平直的地面段：隧道(资产名以_T结尾，或端点在地面以下)、坡道(两端高度不同)不放。
		string mesh = road->GetMesh();
		float zStart = road->GetPoint(0.f).GetZ(), zEnd = road->GetPoint(1.f).GetZ();
		if ((mesh.size() >= 2 && mesh.compare(mesh.size() - 2, 2, "_T") == 0) || zStart < 0.f || zEnd < 0.f || fabsf(zStart - zEnd) > kFlatTolerance) continue;

		// 每侧单独找：被路口缩进和本侧车辆开口(含小路接入)切开后长度>2的路段里，离中点最近的位置。两侧都找到才放(成对才能掉头)。
		float positions[2];
		bool found = true;
		for (int side = 0; side < 2 && found; side++) {
			vector<pair<float, float>> blocked; // 沿路距离区间
			blocked.push_back({ 0.f, road->GetSetback(true) });
			blocked.push_back({ length - road->GetSetback(false), length });
			for (const RoadOpening& opening : road->GetOpenings()) {
				if (!opening.isVehicle || opening.forwardSide != (side == 0)) continue;
				float center = opening.t * length;
				blocked.push_back({ center - opening.width * 0.5f, center + opening.width * 0.5f });
			}
			sort(blocked.begin(), blocked.end());

			float target = length * 0.5f, bestDistance = FLT_MAX, cursor = 0.f;
			bool hasBest = false;
			auto consider = [&](float lo, float hi) {
				if (hi - lo <= kMinFreeLength) return;
				float position = max(lo + kEndMargin, min(hi - kEndMargin, target));
				float distance = fabsf(position - target);
				if (distance < bestDistance) { bestDistance = distance; positions[side] = position; hasBest = true; }
			};
			for (const auto& interval : blocked) {
				consider(cursor, interval.first);
				cursor = max(cursor, interval.second);
			}
			consider(cursor, length);
			found = hasBest;
		}
		if (!found) continue;

		// side0=沿Start->End的右手边(rightSide=true)，side1=另一侧；名字=路名+该路名下的序号。
		for (int side = 0; side < 2; side++) {
			RoadStationRequest request{ road, positions[side] / length, side == 0, road->GetName() + to_string(++counters[road->GetName()]) };
			emit(context, request);
		}
	}
}
