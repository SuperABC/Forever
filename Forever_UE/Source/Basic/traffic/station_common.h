#pragma once

#include "map/general_geometry.h"
#include "traffic/station_mod.h"

#include <string>


// alongRatio沿道路方向定位，depthRatio沿进深方向(0=贴路边，1=远离)，换算成quad局部PointParams。
PointParams NearRoadPoint(int direction, float alongRatio, float depthRatio);

// 沿frontage轴(alongRatio增大方向)的朝向角度；towardIncreasing=false时转180度。
float AlongAxisHeadingDegrees(int direction, bool towardIncreasing);

// 占地矩形局部坐标里的一条直线连线(一段贝塞尔，控制点取三等分点)，两端高度都是z。mesh空=不铺轨。
StationCurveSpec StraightCurve(const PointParams& from, const PointParams& to, float z,
	const std::string& mesh = std::string(), float meshUnit = 0.f);

// 一条站内单向连线(单段直线)。
StationConnectionSpec StraightConnection(PortKind fromKind, int fromIndex, const PointParams& from,
	PortKind toKind, int toIndex, const PointParams& to, float z, const std::string& mesh = std::string(), float meshUnit = 0.f);
