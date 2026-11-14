#pragma once

#include "traffic/station_mod.h"

#include <utility>
#include <vector>


// 射线从(x,y)沿(dirX,dirY)方向和[0,sizeX]x[0,sizeY]地图边界的交点，dirX/dirY不同时为0。
std::pair<float, float> RayBoxIntersection(float x, float y, float dirX, float dirY, int sizeX, int sizeY);

// 一段三次贝塞尔(世界坐标)：起点(x1,y1,z1)沿航向(h1x,h1y)出发，终点(x2,y2,z2)沿航向(h2x,h2y)到达，
// 两个控制点各在端点沿航向arm处(arm<=0=退化成直线，控制点取三等分点)，控制点z取端点z。terrainFollow只给mod自己的BuildTrack看。
TrackSegment MakeBezierSegment(float x1, float y1, float z1, float h1x, float h1y,
	float x2, float y2, float z2, float h2x, float h2y, float arm, bool terrainFollow = false);

// 平面曲线上的一个采样点：位置 + 单位切线(行驶方向)。
struct CurveSample {
	float x = 0.f, y = 0.f;
	float tx = 1.f, ty = 0.f;
};

// 把一段三次贝塞尔(只看平面)按弧长大约每step采样，返回含两端的采样点(至少2个)。
// 某点导数退化成零向量时切线取(fallbackX,fallbackY)。
std::vector<CurveSample> SampleBezier(const TrackSegment& curve, float step, float fallbackX, float fallbackY);

// 把一组采样点沿右手法线(-ty,tx，+Y向南)偏移offset(正=右、负=左)，相邻点之间用Hermite三次贝塞尔连起来(控制臂=偏移弦长/3)，
// z从z1线性过渡到z2。reverse=true时倒着走(切线取反)，用来取同一条中轴线另一个方向的铁轨。
// 同一组采样点的两个方向共用偏移基准，所以两条铁轨永远平行、不会交叉。
std::vector<TrackSegment> OffsetRail(const std::vector<CurveSample>& samples, float offset, bool reverse,
	float z1, float z2, bool terrainFollow);
