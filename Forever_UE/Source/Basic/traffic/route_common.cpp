#include "route_common.h"

#include <algorithm>
#include <cmath>

using namespace std;

// 射线从(x,y)沿(dirX,dirY)方向和[0,sizeX]x[0,sizeY]地图边界的交点——dirX/dirY不会同时为0
// (接口朝向不可能是零向量)。
pair<float, float> RayBoxIntersection(float x, float y, float dirX, float dirY, int sizeX, int sizeY) {
	float tBest = 1e9f;
	if (dirX > 0.f) tBest = min(tBest, (static_cast<float>(sizeX) - x) / dirX);
	else if (dirX < 0.f) tBest = min(tBest, (0.f - x) / dirX);
	if (dirY > 0.f) tBest = min(tBest, (static_cast<float>(sizeY) - y) / dirY);
	else if (dirY < 0.f) tBest = min(tBest, (0.f - y) / dirY);
	if (tBest < 0.f || tBest > 1e8f) tBest = 0.f; // 理论不会发生，防御性兜底(站点已经在图内)
	return { x + dirX * tBest, y + dirY * tBest };
}

TrackSegment MakeBezierSegment(float x1, float y1, float z1, float h1x, float h1y,
	float x2, float y2, float z2, float h2x, float h2y, float arm, bool terrainFollow) {
	TrackSegment seg;
	seg.x1 = x1; seg.y1 = y1; seg.z1 = z1;
	seg.x2 = x2; seg.y2 = y2; seg.z2 = z2;
	if (arm <= 0.f) {
		seg.c1x = x1 + (x2 - x1) / 3.f; seg.c1y = y1 + (y2 - y1) / 3.f;
		seg.c2x = x1 + (x2 - x1) * 2.f / 3.f; seg.c2y = y1 + (y2 - y1) * 2.f / 3.f;
	}
	else {
		seg.c1x = x1 + h1x * arm; seg.c1y = y1 + h1y * arm;
		seg.c2x = x2 - h2x * arm; seg.c2y = y2 - h2y * arm;
	}
	seg.c1z = z1; seg.c2z = z2;
	seg.terrainFollow = terrainFollow;
	return seg;
}
