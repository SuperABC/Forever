#pragma once

#include "traffic/route_mod.h"

#include <vector>


// TrainRoute/AirRoute共用的单站双跑道(轨道)四接口环线拓扑，见route_train.md。
// overrideEdgeZ为true时边缘站点的z取edgeHeight(AirRoute用巡航高度)，否则用接口原本的z。
void LayoutDualTrackStationLoop(const std::vector<RouteStationInfo>& interfaces, int sizeX, int sizeY,
	bool overrideEdgeZ, float edgeHeight,
	std::vector<RouteStationInfo>& outEdgeStations, std::vector<RouteLink>& outLinks,
	std::vector<std::vector<RouteStop>>& outLines);
