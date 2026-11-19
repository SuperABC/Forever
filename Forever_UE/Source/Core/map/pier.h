#pragma once

#include "map/general_geometry.h"

#include <functional>
#include <string>
#include <vector>

class Map;

// 桥墩：墩顶(x,y,topZ)是链上该点；墩底不存，UE画时贴当时的地形(水底)。mesh空串=默认圆柱。
// 火车线路(Route)和公路(Roadnet)共用，选位算法见pier.md。
struct Pier {
	float x = 0.f, y = 0.f, topZ = 0.f, radius = 0.f;
	std::string mesh;
};

namespace PierPlacer {
	// 一条障碍折线：采样点+AABB(预筛)+半宽。
	struct Obstacle {
		std::vector<std::pair<float, float>> points;
		float minX = 0.f, minY = 0.f, maxX = 0.f, maxY = 0.f;
		float halfWidth = 0.f;
	};

	// 把connection按固定间距采样成障碍折线，halfWidth是它的半宽。
	Obstacle SampleObstacle(const Connection& connection, float halfWidth);

	// (x,y)处半径radius的墩是否碰到这条障碍(到折线距离<radius+halfWidth)。
	bool HitsObstacle(const Obstacle& obstacle, float x, float y, float radius);

	// 沿chains(必须是调用方在Core里深拷贝出来的，不能直接用mod DLL分配的链，见Core/traffic/route.cpp的注释)里每条链放墩，
	// 追加进out。blocked(x,y)返回候选点是否被障碍挡住：只有真的需要墩(墩顶离地>=PIER_MIN_HEIGHT)的点才判断。
	void PlaceAlongChains(Map* map, const PierRequest& request, const std::vector<std::vector<Connection>>& chains,
		const std::function<bool(float, float)>& blocked, std::vector<Pier>& out);
}
