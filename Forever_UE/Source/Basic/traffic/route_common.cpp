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

vector<CurveSample> SampleBezier(const TrackSegment& curve, float step, float fallbackX, float fallbackY) {
	auto pointAt = [&](float t, float& x, float& y) {
		float m = 1.f - t;
		x = m * m * m * curve.x1 + 3.f * m * m * t * curve.c1x + 3.f * m * t * t * curve.c2x + t * t * t * curve.x2;
		y = m * m * m * curve.y1 + 3.f * m * m * t * curve.c1y + 3.f * m * t * t * curve.c2y + t * t * t * curve.y2;
	};
	auto tangentAt = [&](float t, float& tx, float& ty) {
		float m = 1.f - t;
		tx = 3.f * m * m * (curve.c1x - curve.x1) + 6.f * m * t * (curve.c2x - curve.c1x) + 3.f * t * t * (curve.x2 - curve.c2x);
		ty = 3.f * m * m * (curve.c1y - curve.y1) + 6.f * m * t * (curve.c2y - curve.c1y) + 3.f * t * t * (curve.y2 - curve.c2y);
		float length = sqrtf(tx * tx + ty * ty);
		if (length < 1e-6f) { tx = fallbackX; ty = fallbackY; length = sqrtf(tx * tx + ty * ty); }
		if (length > 1e-6f) { tx /= length; ty /= length; }
	};

	float chord = sqrtf((curve.x2 - curve.x1) * (curve.x2 - curve.x1) + (curve.y2 - curve.y1) * (curve.y2 - curve.y1));
	int fine = max(64, static_cast<int>(ceilf(chord * 16.f)));
	vector<float> fineArc(fine + 1, 0.f);
	float px, py;
	pointAt(0.f, px, py);
	for (int i = 1; i <= fine; i++) {
		float x, y;
		pointAt(static_cast<float>(i) / static_cast<float>(fine), x, y);
		fineArc[i] = fineArc[i - 1] + sqrtf((x - px) * (x - px) + (y - py) * (y - py));
		px = x; py = y;
	}
	float total = fineArc[fine];
	int count = max(1, static_cast<int>(ceilf(total / max(step, 1e-3f))));

	vector<CurveSample> samples(count + 1);
	int cursor = 0;
	for (int i = 0; i <= count; i++) {
		float target = total * static_cast<float>(i) / static_cast<float>(count);
		while (cursor + 1 < fine && fineArc[cursor + 1] < target) cursor++;
		float span = fineArc[cursor + 1] - fineArc[cursor];
		float f = span > 1e-9f ? (target - fineArc[cursor]) / span : 0.f;
		float t = (static_cast<float>(cursor) + f) / static_cast<float>(fine);
		pointAt(t, samples[i].x, samples[i].y);
		tangentAt(t, samples[i].tx, samples[i].ty);
	}
	samples[0].x = curve.x1; samples[0].y = curve.y1;
	samples[count].x = curve.x2; samples[count].y = curve.y2;
	return samples;
}

vector<TrackSegment> OffsetRail(const vector<CurveSample>& samples, float offset, bool reverse,
	float z1, float z2, bool terrainFollow) {
	size_t n = samples.size();
	vector<TrackSegment> segments;
	if (n < 2) return segments;

	// 按行驶顺序排好的偏移点和行驶方向。
	vector<float> px(n), py(n), hx(n), hy(n);
	for (size_t k = 0; k < n; k++) {
		const CurveSample& s = samples[reverse ? n - 1 - k : k];
		px[k] = s.x - s.ty * offset;
		py[k] = s.y + s.tx * offset;
		hx[k] = reverse ? -s.tx : s.tx;
		hy[k] = reverse ? -s.ty : s.ty;
	}
	for (size_t k = 0; k + 1 < n; k++) {
		float za = z1 + (z2 - z1) * static_cast<float>(k) / static_cast<float>(n - 1);
		float zb = z1 + (z2 - z1) * static_cast<float>(k + 1) / static_cast<float>(n - 1);
		float arm = sqrtf((px[k + 1] - px[k]) * (px[k + 1] - px[k]) + (py[k + 1] - py[k]) * (py[k + 1] - py[k])) / 3.f;
		segments.push_back(MakeBezierSegment(px[k], py[k], za, hx[k], hy[k], px[k + 1], py[k + 1], zb, hx[k + 1], hy[k + 1], arm, terrainFollow));
	}
	return segments;
}
