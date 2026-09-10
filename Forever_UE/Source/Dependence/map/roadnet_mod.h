#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

#include "map/geometry.h"

// RoadnetMod:路网布局方案的Mod扩展点。和Terrain(可以多个mod叠加分发)不同,一次只应该有
// 一个路网布局方案生效,由RoadnetFactory::SetConfig/GetRoadnet做单选,详见roadnet_factory.h。
class RoadnetMod {
public:
	RoadnetMod() = default;
	virtual ~RoadnetMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;
	virtual void ApplyArgs(const std::string& args) {}

	// 构建路网:根据地图宽高及地形/水域采样回调,把结果填进下面externs/intersections/roads/lots
	// 四个成员。nodeStaticCount是宿主传入的当前Node::count计数器值,实现开头必须先
	// Node::SetCount(nodeStaticCount)再开始构造任何Node/Intersection/Road,否则跨DLL的id会冲突;
	// 不要在mod里留下未通过这四个成员返回的Node实例。
	// 道路高度固定为0,不采样地形高度(这次范围裁剪,详见roadnet.md)。
	virtual void DistributeRoadnet(int width, int height,
		const std::function<std::string(int, int)>& getTerrain,
		const std::function<std::pair<bool, float>(int, int)>& getWater,
		int nodeStaticCount) = 0;

	std::vector<Node> externs;
	std::vector<Intersection> intersections;
	std::vector<Road> roads;

	// 每个lot连同它的边界Road映射(int键=FACE_DIRECTION 0-3,缺失/找不到表示那一侧没有路)。
	// 不含边界Intersection映射——车行/行人导航图直接挂在Road/Intersection上,不需要通过lot中转。
	std::vector<std::pair<Lot, std::unordered_map<int, Road>>> lots;
};
