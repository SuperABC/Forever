#pragma once

#include "map/general_geometry.h"


// alongRatio沿道路方向定位，depthRatio沿进深方向(0=贴路边，1=远离)，换算成quad局部PointParams。
PointParams NearRoadPoint(int direction, float alongRatio, float depthRatio);

// 沿frontage轴(alongRatio增大方向)的朝向角度；towardIncreasing=false时转180度。
float AlongAxisHeadingDegrees(int direction, bool towardIncreasing);
