#include "roadnet_mod.h"

#include <cmath>
#include <algorithm>

// 贴合请求折线的采样间距(地图element单位)：路是平滑曲线，0.5对地形贴合(格点间距1/16)足够。
#define TERRAIN_FIT_SAMPLE_SPACING 0.5f


using namespace std;

void RoadnetMod::AddHatch(const Connection* connection, float t1, float t2, float width) {
	if (!connection) return;

	Node p1 = connection->GetPoint(t1);
	Node p2 = connection->GetPoint(t2);

	float dx = p2.GetX() - p1.GetX();
	float dy = p2.GetY() - p1.GetY();
	float length = connection->CalcDistance(t1, t2);
	if (length <= 0.f) return;

	float rotation = atan2(dy, dx);
	float cx = (p1.GetX() + p2.GetX()) * 0.5f;
	float cy = (p1.GetY() + p2.GetY()) * 0.5f;

	Quad quad(cx, cy, length, width);
	hatches.emplace_back(quad, rotation);
}

void RoadnetMod::AddTerrainFit(const Connection* connection, float t1, float t2, float width, float falloff) {
	if (!connection) return;

	float length = connection->CalcDistance(t1, t2);
	if (length <= 0.f || width <= 0.f) return;

	TerrainFitRequest request;
	request.width = width;
	request.falloff = max(falloff, 0.f);
	int segments = max(1, static_cast<int>(ceil(length / TERRAIN_FIT_SAMPLE_SPACING)));
	for (int i = 0; i <= segments; i++) {
		float t = t1 + (t2 - t1) * static_cast<float>(i) / static_cast<float>(segments);
		Node p = connection->GetPoint(t);
		request.centerline.push_back({ p.GetX(), p.GetY(), p.GetZ() });
	}
	terrainFits.push_back(std::move(request));
}

void RoadnetMod::AddTreeSingle(const PointParams& position, float scale, const string& meshPath) {
	trees.AddSingle(position, scale, meshPath);
}

void RoadnetMod::AddTreeLine(const PointParams& from, const PointParams& to, int num, float scale,
	const string& meshPath) {
	trees.AddLine(from, to, num, scale, meshPath);
}

void RoadnetMod::AddTreeArea(const PointParams& from, const PointParams& to, int num, float scale,
	const string& meshPath) {
	trees.AddArea(from, to, num, scale, meshPath);
}
