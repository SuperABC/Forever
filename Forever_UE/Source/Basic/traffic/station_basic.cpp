#include "station_basic.h"

#include "map/geometry.h"

using namespace std;

namespace {
	// 沿"道路方向"的坐标用alongRatio定位，沿"进深方向"(垂直于道路)用depthRatio定位
	// (0=贴着道路那条边，1=远离道路那条边)，换算成quad局部坐标系下的PointParams——每个
	// direction对应哪条边贴路、进深朝哪个轴走。
	PointParams NearRoadPoint(int direction, float alongRatio, float depthRatio) {
		switch (direction) {
			case FACE_WEST:  return { depthRatio, 0.f, alongRatio, 0.f };
			case FACE_EAST:  return { 1.f - depthRatio, 0.f, alongRatio, 0.f };
			case FACE_NORTH: return { alongRatio, 0.f, 1.f - depthRatio, 0.f };
			case FACE_SOUTH: return { alongRatio, 0.f, depthRatio, 0.f };
			default: return { alongRatio, 0.f, depthRatio, 0.f };
		}
	}

	// 沿frontage轴(alongRatio增大的方向)的朝向角度——NearRoadPoint的alongRatio无论
	// WEST/EAST/NORTH/SOUTH都不做翻转，直接映射到quad局部坐标系的Y轴(WEST/EAST)或X轴
	// (NORTH/SOUTH)，所以"朝alongRatio增大方向"只有两种可能(+Y=90度或+X=0度)。
	// towardIncreasing=false时转180度，指向alongRatio减小的方向。两条跑道/轨道的两端接口
	// 必须用这一对相反朝向，才能让TrainRoute/AirRoute的LayoutDualTrackStationLoop算出来的
	// 直线真的贴着两个接口连成一条直线（见route_basic.cpp）。
	float AlongAxisHeadingDegrees(int direction, bool towardIncreasing) {
		bool alongIsYAxis = (direction == FACE_WEST || direction == FACE_EAST);
		float baseDegrees = alongIsYAxis ? 90.f : 0.f;
		return towardIncreasing ? baseDegrees : baseDegrees + 180.f;
	}
}

int BusStation::count = 0;

BusStation::BusStation() : id(count++) {
	stationType = "bus";
}

const char* BusStation::GetName() {
	name = "BusStation" + to_string(id);
	return name.data();
}

void BusStation::AssignRoads(const vector<Road*>& roads, RoadStationEmitFunc emit, void* context) {
	// 测试布局：只贴井字路网中心正方形lot的四条边道路(JingRoadnet::DistributeRoadnet里固定
	// 这几个名字)，不是全图所有路都摆。每条路取中点(t=0.5)，两侧各摆一个。
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
	stationType = "train";
}

const char* TrainStation::GetName() {
	name = "TrainStation" + to_string(id);
	return name.data();
}

void TrainStation::Layout(int direction, float sizeX, float sizeY) {
	// 两条轨道、四个接口：左入站(leftIn)-右出站(rightOut)是depthRatio=0.65这条轨道，
	// 右入站(rightIn)-左出站(leftOut)是depthRatio=0.85那条——顺序必须是
	// [leftIn, rightOut, rightIn, leftOut]，TrainRoute::LayoutRoute(route_basic.cpp的
	// LayoutDualTrackStationLoop)按这个固定顺序把它们连成"左边界-leftIn-rightOut-右边界-
	// rightIn-leftOut-左边界"的环，同一条轨道的两个接口必须朝向相同(沿frontage轴同一方向)，
	// 这样Route::Build()算出来的贝塞尔控制点才会贴着两点连线、看起来是一条直的"轨道"而不是
	// 弯的。z比原来(0，贴地)抬高一点——PIE实测反馈接口太低、调试连线和地面/楼体糊在一起看
	// 不清楚。
	constexpr float kTrackDepth1 = 0.65f;
	constexpr float kTrackDepth2 = 0.85f;
	constexpr float kInterfaceZ = 0.1f;

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
	stationType = "plane";
}

const char* AirStation::GetName() {
	name = "AirStation" + to_string(id);
	return name.data();
}

void AirStation::Layout(int direction, float sizeX, float sizeY) {
	// 两条跑道、四个接口，和TrainStation::Layout同一套[leftIn, rightOut, rightIn, leftOut]
	// 顺序/同一套depthRatio分组，见那边的注释——跑道贴地只抬高一点点(0.1，和TrainStation同一个
	// 值)方便看清调试连线，不代表真实海拔；AirRoute::LayoutRoute生成的两段边缘段另外会抬到
	// 巡航高度，见route_basic.cpp。
	constexpr float kRunwayDepth1 = 0.65f;
	constexpr float kRunwayDepth2 = 0.85f;
	constexpr float kInterfaceZ = 0.1f;

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
