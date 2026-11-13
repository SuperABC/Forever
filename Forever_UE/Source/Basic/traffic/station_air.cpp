#include "station_air.h"

#include "station_common.h"

#include "map/general_geometry.h"

using namespace std;

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
	// 两条跑道、四个接口，顺序/分组规则同TrainStation::Layout，见station_air.md。
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
