#include "station_bus.h"

#include "bus_stop_name.h"
#include "map/general_geometry.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <map>

using namespace std;

namespace {
	constexpr float kBusMinFreeLength = 2.f;  // 站所在的"被路口/出入口切开的路段"至少这么长
	constexpr float kBusEndMargin = 1.f;      // 站离所在路段两端至少这么远

	// 在本侧(side0=沿Start->End的右手边)被路口缩进和车辆开口切开后长度>kBusMinFreeLength的路段里，找离target(沿路距离)最近的位置。
	// 目标点在可用段内就直接用目标点；找不到可用段返回false。自动/手动两条路共用。
	bool BusFindStopPosition(const Road* road, int side, float length, float target, float& position) {
		vector<pair<float, float>> blocked; // 沿路距离区间
		blocked.push_back({ 0.f, road->GetSetback(true) });
		blocked.push_back({ length - road->GetSetback(false), length });
		for (const RoadOpening& opening : road->GetOpenings()) {
			if (!opening.isVehicle || opening.forwardSide != (side == 0)) continue;
			float center = opening.t * length;
			blocked.push_back({ center - opening.width * 0.5f, center + opening.width * 0.5f });
		}
		sort(blocked.begin(), blocked.end());

		float bestDistance = FLT_MAX, cursor = 0.f;
		bool hasBest = false;
		auto consider = [&](float lo, float hi) {
			if (hi - lo <= kBusMinFreeLength) return;
			float candidate = max(lo + kBusEndMargin, min(hi - kBusEndMargin, target));
			float distance = fabsf(candidate - target);
			if (distance < bestDistance) { bestDistance = distance; position = candidate; hasBest = true; }
		};
		for (const auto& interval : blocked) {
			consider(cursor, interval.first);
			cursor = max(cursor, interval.second);
		}
		consider(cursor, length);
		return hasBest;
	}

	// 点(x,y)在路中线上的垂足的归一化t(Road的t是弧长参数)：均匀采样找最近点后在相邻两样本间线性插值。
	float BusProjectToRoad(const Road* road, float x, float y) {
		constexpr int kSamples = 64;
		float bestT = 0.5f, bestDistance = FLT_MAX;
		Node previous = road->GetPoint(0.f);
		for (int i = 1; i <= kSamples; i++) {
			float t1 = static_cast<float>(i) / kSamples;
			Node current = road->GetPoint(t1);
			float ex = current.GetX() - previous.GetX(), ey = current.GetY() - previous.GetY();
			float lengthSquared = ex * ex + ey * ey;
			float u = lengthSquared > 1e-12f ? ((x - previous.GetX()) * ex + (y - previous.GetY()) * ey) / lengthSquared : 0.f;
			u = max(0.f, min(1.f, u));
			float fx = previous.GetX() + ex * u - x, fy = previous.GetY() + ey * u - y;
			float distance = fx * fx + fy * fy;
			if (distance < bestDistance) { bestDistance = distance; bestT = (static_cast<float>(i - 1) + u) / kSamples; }
			previous = current;
		}
		return bestT;
	}
}

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
			found = BusFindStopPosition(road, side, length, length * 0.5f, positions[side]);
		}
		if (!found) continue;

		// side0=沿Start->End的右手边(rightSide=true)，side1=另一侧；名字=路名+该路名下的序号。
		for (int side = 0; side < 2; side++) {
			RoadStationRequest request{ road, positions[side] / length, side == 0, road->GetName() + to_string(++counters[road->GetName()]) };
			emit(context, request);
		}
	}
}

void BusStation::AssignManualRoads(const vector<Road*>& roads, const TransitData& data, RoadStationEmitFunc emit, void* context) {
	// 编辑器"路网路线"的站：只处理station==本mod的站，站名=BusStopName(线下标,站下标)，route_bus用同一个名字连线。
	// 目标位置=站点垂足的t，从它向两侧找离它最近的、避开本侧车辆开口和路口缩进的位置；整条路都没有合适位置就硬放在目标位置。
	for (size_t lineIndex = 0; lineIndex < data.roadLines.size(); lineIndex++) {
		const TransitData::RoadLine& line = data.roadLines[lineIndex];
		for (size_t stopIndex = 0; stopIndex < line.stops.size(); stopIndex++) {
			const TransitData::RoadStop& stop = line.stops[stopIndex];
			if (stop.station != GetId()) continue;
			if (stop.road < 0 || stop.road >= static_cast<int>(roads.size()) || !roads[stop.road]) {
				debugf(DebugLevel::Warning, "BusStation manual stop L%d S%d references missing road %d, skipped.\n", static_cast<int>(lineIndex), static_cast<int>(stopIndex), stop.road);
				continue;
			}
			Road* road = roads[stop.road];
			float length = road->CalcDistance();
			if (length < 1e-4f) continue;

			// 单行道只有一侧有车行道：编辑器不允许放在空侧，这里再兜底换到有车行道的一侧。
			bool rightSide = stop.rightSide;
			if (road->GetVehicleLanes(rightSide ? 0 : 1).empty()) {
				if (road->GetVehicleLanes(rightSide ? 1 : 0).empty()) {
					debugf(DebugLevel::Warning, "BusStation manual stop L%d S%d: road '%s' has no vehicle lane, skipped.\n", static_cast<int>(lineIndex), static_cast<int>(stopIndex), road->GetName().c_str());
					continue;
				}
				rightSide = !rightSide;
			}

			float target = BusProjectToRoad(road, stop.x, stop.y) * length;
			float position = target;
			if (!BusFindStopPosition(road, rightSide ? 0 : 1, length, target, position)) {
				debugf(DebugLevel::Warning, "BusStation manual stop L%d S%d: no free spot on road '%s', placed at the requested position.\n", static_cast<int>(lineIndex), static_cast<int>(stopIndex), road->GetName().c_str());
				position = target;
			}
			emit(context, RoadStationRequest{ road, position / length, rightSide, BusStopName(static_cast<int>(lineIndex), static_cast<int>(stopIndex)) });
		}
	}
}
