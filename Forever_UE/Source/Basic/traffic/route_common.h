#pragma once

#include "traffic/station_mod.h"

#include <utility>


// 射线从(x,y)沿(dirX,dirY)方向和[0,sizeX]x[0,sizeY]地图边界的交点，dirX/dirY不同时为0。
std::pair<float, float> RayBoxIntersection(float x, float y, float dirX, float dirY, int sizeX, int sizeY);

// 一段三次贝塞尔(世界坐标)：起点(x1,y1,z1)沿航向(h1x,h1y)出发，终点(x2,y2,z2)沿航向(h2x,h2y)到达，
// 两个控制点各在端点沿航向arm处(arm<=0=退化成直线，控制点取三等分点)，控制点z取端点z。terrainFollow只给mod自己的BuildTrack看。
TrackSegment MakeBezierSegment(float x1, float y1, float z1, float h1x, float h1y,
	float x2, float y2, float z2, float h2x, float h2y, float arm, bool terrainFollow = false);
