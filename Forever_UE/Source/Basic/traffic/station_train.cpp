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
	// 两条轨道t=0/1(航向相反)，每条轨道一个进站口、一个出站口、一个停靠位(轨道中点)，下标都等于t，被TrainRoute::LayoutRoute按下标依赖；
	// 同一条轨道的三个端口朝向必须相同。kPortZ的含义见station_train.md。
	constexpr float kTrackDepth1 = 0.55f;   // 距离门口(贴路一侧)的深度比例
	constexpr float kTrackDepth2 = 0.65f;
	constexpr float kPortZ = 0.001f;

	// 靠右行驶：沿航向走时另一条轨道必须在左手边，这样顺时针环(靠内侧)/逆时针环(靠外侧)在每个站才一致。
	// 局部坐标+X东+Y南，NORTH/EAST朝向的"沿路增大方向"轨道1在远离道路一侧时会落在航向左侧，所以这两个方向交换两条轨道的深度。
	bool swapDepths = (direction == FACE_NORTH || direction == FACE_EAST);
	float depths[2] = { swapDepths ? kTrackDepth2 : kTrackDepth1, swapDepths ? kTrackDepth1 : kTrackDepth2 };
	float headings[2] = { AlongAxisHeadingDegrees(direction, true), AlongAxisHeadingDegrees(direction, false) };
	// 轨道0沿路增大方向，进站口在0.3、出站口在0.7；轨道1反向，进站口在0.7、出站口在0.3。
	float inAlong[2] = { 0.3f, 0.7f }, outAlong[2] = { 0.7f, 0.3f };

	inlets.clear(); outlets.clear(); stops.clear(); connections.clear();
	for (int t = 0; t < 2; t++) {
		StationPortSpec in, out, stop;
		in.position = NearRoadPoint(direction, inAlong[t], depths[t]);
		out.position = NearRoadPoint(direction, outAlong[t], depths[t]);
		stop.position = NearRoadPoint(direction, 0.5f, depths[t]);
		in.headingDegrees = out.headingDegrees = stop.headingDegrees = headings[t];
		in.z = out.z = stop.z = kPortZ;
		inlets.push_back(in); outlets.push_back(out); stops.push_back(stop);

		// 进站口->停靠位、停靠位->出站口铺轨；进站口->出站口直通(不停站通过)和前两条重叠，不重复铺。
		connections.push_back(StraightConnection(PortKind::Inlet, t, in.position, PortKind::Stop, t, stop.position,
			kPortZ, kTrainTrackMesh, kTrainTrackUnit));
		connections.push_back(StraightConnection(PortKind::Stop, t, stop.position, PortKind::Outlet, t, out.position,
			kPortZ, kTrainTrackMesh, kTrainTrackUnit));
		connections.push_back(StraightConnection(PortKind::Inlet, t, in.position, PortKind::Outlet, t, out.position, kPortZ));
	}
}
