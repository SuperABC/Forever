#include "station_train.h"

#include "station_common.h"

#include "map/general_geometry.h"

using namespace std;

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
	// 同一条轨道的两个接口朝向必须相同。kInterfaceZ的含义见station_train.md。
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
