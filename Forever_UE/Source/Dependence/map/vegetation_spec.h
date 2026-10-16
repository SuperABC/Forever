#pragma once

#include "map/geometry.h"

#include <string>


// 植被请求——mod只描述"在哪里种几棵、用什么mesh、基础缩放多少"，每棵树具体的位置/随机
// 旋转/随机缩放统一由Core的Vegetation展开(见Source/Core/map/vegetation.md)，规则只有一份。
// 坐标单位都是地图element(1=10m)。详见vegetation_spec.md。

// 沿from->to线段均匀种num棵(含两端，num==1时种在中点)。由RoadnetMod::AddTreeLine(在
// RoadnetMod::LayoutTrees里)记录，
// 坐标是世界坐标。
struct TreeLineRequest {
	float fromX = 0.f, fromY = 0.f;
	float toX = 0.f, toY = 0.f;
	int num = 0;
	float scale = 1.f;
	std::string meshPath;
};

// 在quad范围内均匀随机种num棵。由BuildingMod::AddTreeArea(在BuildingMod::LayoutTrees里)记录，
// quad就是LayoutTrees()拿到的那个
// building quad(中心是世界坐标，尺寸沿楼体局部轴向)，Core展开时再叠加building的旋转。
struct TreeAreaRequest {
	Quad quad;
	int num = 0;
	float scale = 1.f;
	std::string meshPath;
};
