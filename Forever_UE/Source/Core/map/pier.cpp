#include "map/pier.h"

#include "map/map.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

using namespace std;

namespace {
	// 桥墩选位参数(地图单位，1=10m)。
	constexpr float PIER_SPACING_STEP = 0.5f;   // 墩距一档=5m
	constexpr float PIER_MIN_HEIGHT = 0.05f;    // 墩顶离地不足0.5m(贴地/隧道内/过山抬升段)不放墩
	constexpr float PIER_SAMPLE_STEP = 0.25f;   // 障碍线按这个间距采样成折线

	float PointSegmentDistSq(float px, float py, float ax, float ay, float bx, float by) {
		float dx = bx - ax, dy = by - ay;
		float lengthSq = dx * dx + dy * dy;
		float t = lengthSq > 1e-12f ? max(0.f, min(1.f, ((px - ax) * dx + (py - ay) * dy) / lengthSq)) : 0.f;
		float cx = ax + dx * t - px, cy = ay + dy * t - py;
		return cx * cx + cy * cy;
	}
}

PierPlacer::Obstacle PierPlacer::SampleObstacle(const Connection& connection, float halfWidth) {
	Obstacle obstacle;
	obstacle.minX = FLT_MAX; obstacle.minY = FLT_MAX; obstacle.maxX = -FLT_MAX; obstacle.maxY = -FLT_MAX;
	obstacle.halfWidth = halfWidth;
	float length = connection.CalcDistance();
	int count = max(1, static_cast<int>(ceilf(length / PIER_SAMPLE_STEP)));
	for (int i = 0; i <= count; i++) {
		Node point = connection.GetPoint(static_cast<float>(i) / static_cast<float>(count));
		float x = point.GetX(), y = point.GetY();
		obstacle.points.emplace_back(x, y);
		obstacle.minX = min(obstacle.minX, x); obstacle.maxX = max(obstacle.maxX, x);
		obstacle.minY = min(obstacle.minY, y); obstacle.maxY = max(obstacle.maxY, y);
	}
	return obstacle;
}

bool PierPlacer::HitsObstacle(const Obstacle& obstacle, float x, float y, float radius) {
	float reach = radius + obstacle.halfWidth;
	if (x < obstacle.minX - reach || x > obstacle.maxX + reach || y < obstacle.minY - reach || y > obstacle.maxY + reach) return false;
	float reachSq = reach * reach;
	if (obstacle.points.size() == 1) {
		float dx = obstacle.points[0].first - x, dy = obstacle.points[0].second - y;
		return dx * dx + dy * dy < reachSq;
	}
	for (size_t i = 0; i + 1 < obstacle.points.size(); i++) {
		if (PointSegmentDistSq(x, y, obstacle.points[i].first, obstacle.points[i].second,
			obstacle.points[i + 1].first, obstacle.points[i + 1].second) < reachSq) return true;
	}
	return false;
}

void PierPlacer::PlaceAlongChains(Map* map, const PierRequest& request, const vector<vector<Connection>>& chains,
	const function<bool(float, float)>& blocked, vector<Pier>& out) {
	float minSpacing = max(request.minSpacing, PIER_SPACING_STEP);
	float defaultSpacing = max(request.defaultSpacing, minSpacing);
	for (const vector<Connection>& chain : chains) {
		if (chain.empty()) continue;
		vector<float> cumulative(chain.size() + 1, 0.f);
		for (size_t i = 0; i < chain.size(); i++) cumulative[i + 1] = cumulative[i] + chain[i].CalcDistance();
		float total = cumulative.back();
		auto pointAt = [&](float s, float& x, float& y, float& z) {
			size_t i = 0;
			while (i + 1 < chain.size() && s > cumulative[i + 1]) i++;
			float length = cumulative[i + 1] - cumulative[i];
			Node point = chain[i].GetPoint(length > 1e-6f ? max(0.f, min(1.f, (s - cumulative[i]) / length)) : 0.f);
			x = point.GetX(); y = point.GetY(); z = point.GetZ();
		};

		// 链首是车站/路口，视为已有支撑：从那里往后一根根放，剩余长度不足默认墩距时收尾(尾端也是车站/落地)。
		float s = 0.f;
		while (total - s > defaultSpacing) {
			// 候选墩距：默认→每次缩短一档到最短；都放不下再从默认每次加长一档，直到超出链尾。
			vector<float> candidates;
			for (float d = defaultSpacing; d >= minSpacing - 1e-4f; d -= PIER_SPACING_STEP) candidates.push_back(d);
			for (float d = defaultSpacing + PIER_SPACING_STEP; s + d <= total; d += PIER_SPACING_STEP) candidates.push_back(d);

			bool found = false;
			for (float d : candidates) {
				if (s + d > total) continue;
				float x, y, z;
				pointAt(s + d, x, y, z);
				bool needPier = z - map->SampleHeight(x, y) >= PIER_MIN_HEIGHT;
				if (needPier && blocked(x, y)) continue;
				if (needPier) {
					Pier pier;
					pier.x = x; pier.y = y; pier.topZ = z; pier.radius = request.radius; pier.mesh = request.mesh;
					out.push_back(std::move(pier));
				}
				s += d;
				found = true;
				break;
			}
			if (!found) break;
		}
	}
}
