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
	// 两条跑道，端口规则同TrainStation::Layout(下标=跑道号)，见station_air.md；停靠位在跑道中点。
	constexpr float kRunwayDepth[2] = { 0.65f, 0.85f };
	constexpr float kPortZ = 0.001f;

	float headings[2] = { AlongAxisHeadingDegrees(direction, true), AlongAxisHeadingDegrees(direction, false) };
	float inAlong[2] = { 0.3f, 0.7f }, outAlong[2] = { 0.7f, 0.3f };

	inlets.clear(); outlets.clear(); stops.clear(); connections.clear();
	for (int r = 0; r < 2; r++) {
		StationPortSpec in, out, stop;
		in.position = NearRoadPoint(direction, inAlong[r], kRunwayDepth[r]);
		out.position = NearRoadPoint(direction, outAlong[r], kRunwayDepth[r]);
		stop.position = NearRoadPoint(direction, 0.5f, kRunwayDepth[r]);
		in.headingDegrees = out.headingDegrees = stop.headingDegrees = headings[r];
		in.z = out.z = stop.z = kPortZ;
		inlets.push_back(in); outlets.push_back(out); stops.push_back(stop);

		connections.push_back(StraightConnection(PortKind::Inlet, r, in.position, PortKind::Stop, r, stop.position, kPortZ));
		connections.push_back(StraightConnection(PortKind::Stop, r, stop.position, PortKind::Outlet, r, out.position, kPortZ));
		connections.push_back(StraightConnection(PortKind::Inlet, r, in.position, PortKind::Outlet, r, out.position, kPortZ));
	}
}
