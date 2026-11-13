#pragma once

#include "traffic/route_mod.h"

#include <utility>
#include <vector>


// 射线从(x,y)沿(dirX,dirY)方向和[0,sizeX]x[0,sizeY]地图边界的交点，dirX/dirY不同时为0。
std::pair<float, float> RayBoxIntersection(float x, float y, float dirX, float dirY, int sizeX, int sizeY);

// 单站环线追加的8个边缘站点里，跑道/轨道1、2中点(停靠点)的相对下标。
constexpr int kDualTrackStop1 = 6;
constexpr int kDualTrackStop2 = 7;
constexpr int kDualTrackEdgeCount = 8;

// TrainRoute/AirRoute共用的单站双跑道(轨道)四接口环线拓扑，见route_common.md。
// interfaces是同一个站点的4个接口(leftIn,rightOut,rightIn,leftOut)；edgeBase是本站边缘站点在Core站点总列表里的起始下标
// (真实站点数+已追加的边缘站点数)；overrideEdgeZ为true时边缘站点的z取edgeHeight(AirRoute用巡航高度)。结果追加到out*。
void LayoutDualTrackStationLoop(const std::vector<RouteStationInfo>& interfaces, int sizeX, int sizeY,
	bool overrideEdgeZ, float edgeHeight, int edgeBase,
	std::vector<RouteStationInfo>& outEdgeStations, std::vector<RouteLink>& outLinks,
	std::vector<std::vector<RouteStop>>& outLines);
