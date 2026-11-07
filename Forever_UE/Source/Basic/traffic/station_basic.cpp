#include "station_basic.h"

#include "map/general_geometry.h"

using namespace std;

namespace {
	// alongRatio沿道路方向定位，depthRatio沿进深方向(0=贴路边，1=远离)，换算成quad局部PointParams。
	PointParams NearRoadPoint(int direction, float alongRatio, float depthRatio) {
		switch (direction) {
			case FACE_WEST:  return { depthRatio, 0.f, alongRatio, 0.f };
			case FACE_EAST:  return { 1.f - depthRatio, 0.f, alongRatio, 0.f };
			case FACE_NORTH: return { alongRatio, 0.f, 1.f - depthRatio, 0.f };
			case FACE_SOUTH: return { alongRatio, 0.f, depthRatio, 0.f };
			default: return { alongRatio, 0.f, depthRatio, 0.f };
		}
	}

	// 沿frontage轴(alongRatio增大方向)的朝向角度；towardIncreasing=false时转180度。
	// 推导见station_basic.md。
	float AlongAxisHeadingDegrees(int direction, bool towardIncreasing) {
		bool alongIsYAxis = (direction == FACE_WEST || direction == FACE_EAST);
		float baseDegrees = alongIsYAxis ? 90.f : 0.f;
		return towardIncreasing ? baseDegrees : baseDegrees + 180.f;
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
	// 只贴井字路网中心正方形的四条边道路(JingRoadnet::DistributeRoadnet里固定的名字)，
	// 每条路取中点(t=0.5)，两侧各摆一个。
	static const char* kCenterRoadNames[] = { "中山西路", "中山东路", "中山北路", "中山南路" };

	for (Road* road : roads) {
		if (!road) continue;

		bool isCenterRoad = false;
		for (const char* roadName : kCenterRoadNames) {
			if (road->GetName() == roadName) { isCenterRoad = true; break; }
		}
		if (!isCenterRoad) continue;

		RoadStationRequest left{ road, 0.5f, false };
		RoadStationRequest right{ road, 0.5f, true };
		emit(context, left);
		emit(context, right);
	}
}

int TrainStation::count = 0;

TrainStation::TrainStation() : id(count++) {
}

const char* TrainStation::GetName() {
	name = "TrainStation" + to_string(id);
	return name.data();
}

void TrainStation::SetProperty() {
	stationType = "train";
}

void TrainStation::Layout(int direction, float sizeX, float sizeY) {
	// 接口顺序必须是[leftIn, rightOut, rightIn, leftOut]，被LayoutDualTrackStationLoop按下标依赖；
	// 同一条轨道的两个接口朝向必须相同。kInterfaceZ的含义见station_basic.md。
	constexpr float kTrackDepth1 = 0.55f;   // 距离门口(贴路一侧)的深度比例
	constexpr float kTrackDepth2 = 0.65f;
	constexpr float kInterfaceZ = 0.001f;

	float headingToRight = AlongAxisHeadingDegrees(direction, true);
	float headingToLeft = AlongAxisHeadingDegrees(direction, false);

	StationInterfaceSpec leftIn;
	leftIn.position = NearRoadPoint(direction, 0.1f, kTrackDepth1);
	leftIn.headingDegrees = headingToRight;
	leftIn.z = kInterfaceZ;

	StationInterfaceSpec rightOut;
	rightOut.position = NearRoadPoint(direction, 0.9f, kTrackDepth1);
	rightOut.headingDegrees = headingToRight; // 和leftIn同向，两点连线才是轨道1这条直线
	rightOut.z = kInterfaceZ;

	StationInterfaceSpec rightIn;
	rightIn.position = NearRoadPoint(direction, 0.9f, kTrackDepth2);
	rightIn.headingDegrees = headingToLeft;
	rightIn.z = kInterfaceZ;

	StationInterfaceSpec leftOut;
	leftOut.position = NearRoadPoint(direction, 0.1f, kTrackDepth2);
	leftOut.headingDegrees = headingToLeft; // 和rightIn同向，两点连线是轨道2这条直线
	leftOut.z = kInterfaceZ;

	interfaces = { leftIn, rightOut, rightIn, leftOut };
}

int AirStation::count = 0;

AirStation::AirStation() : id(count++) {
}

const char* AirStation::GetName() {
	name = "AirStation" + to_string(id);
	return name.data();
}

void AirStation::SetProperty() {
	stationType = "plane";
}

void AirStation::Layout(int direction, float sizeX, float sizeY) {
	// 两条跑道、四个接口，顺序/分组规则同TrainStation::Layout，见station_basic.md。
	constexpr float kRunwayDepth1 = 0.65f;
	constexpr float kRunwayDepth2 = 0.85f;
	constexpr float kInterfaceZ = 0.001f;

	float headingToRight = AlongAxisHeadingDegrees(direction, true);
	float headingToLeft = AlongAxisHeadingDegrees(direction, false);

	StationInterfaceSpec leftIn;
	leftIn.position = NearRoadPoint(direction, 0.1f, kRunwayDepth1);
	leftIn.headingDegrees = headingToRight;
	leftIn.z = kInterfaceZ;

	StationInterfaceSpec rightOut;
	rightOut.position = NearRoadPoint(direction, 0.9f, kRunwayDepth1);
	rightOut.headingDegrees = headingToRight; // 和leftIn同向，两点连线才是跑道1这条直线
	rightOut.z = kInterfaceZ;

	StationInterfaceSpec rightIn;
	rightIn.position = NearRoadPoint(direction, 0.9f, kRunwayDepth2);
	rightIn.headingDegrees = headingToLeft;
	rightIn.z = kInterfaceZ;

	StationInterfaceSpec leftOut;
	leftOut.position = NearRoadPoint(direction, 0.1f, kRunwayDepth2);
	leftOut.headingDegrees = headingToLeft; // 和rightIn同向，两点连线是跑道2这条直线
	leftOut.z = kInterfaceZ;

	interfaces = { leftIn, rightOut, rightIn, leftOut };
}
