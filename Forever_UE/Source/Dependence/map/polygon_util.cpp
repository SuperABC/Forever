#include "polygon_util.h"

#include <algorithm>
#include <cmath>


using namespace std;

namespace {
	constexpr double kEps = 1e-9;
	// 点落在边界上的容差(地图单位，1=10m)，0.0001=1mm。
	constexpr double kBoundaryTol = 1e-4;

	inline int VertexCount(const vector<float>& xy) { return static_cast<int>(xy.size() / 2); }

	inline double Cross(double ax, double ay, double bx, double by) { return ax * by - ay * bx; }

	// 点p相对有向线段a->b的方向：>0 / <0 / 0。
	inline int Orient(double ax, double ay, double bx, double by, double px, double py) {
		double c = Cross(bx - ax, by - ay, px - ax, py - ay);
		if (c > kEps) return 1;
		if (c < -kEps) return -1;
		return 0;
	}

	// 共线时p是否落在线段a-b的包围盒内。
	inline bool OnSegmentBox(double ax, double ay, double bx, double by, double px, double py) {
		return min(ax, bx) - kEps <= px && px <= max(ax, bx) + kEps
			&& min(ay, by) - kEps <= py && py <= max(ay, by) + kEps;
	}
}

float PolygonSignedArea(const vector<float>& xy) {
	int n = VertexCount(xy);
	double sum = 0.0;
	for (int i = 0; i < n; i++) {
		int j = (i + 1) % n;
		sum += static_cast<double>(xy[2 * i]) * xy[2 * j + 1] - static_cast<double>(xy[2 * j]) * xy[2 * i + 1];
	}
	return static_cast<float>(sum * 0.5);
}

bool EnsurePositiveArea(vector<float>& xy) {
	if (PolygonSignedArea(xy) >= 0.f) return false;
	int n = VertexCount(xy);
	for (int i = 0, j = n - 1; i < j; i++, j--) {
		swap(xy[2 * i], xy[2 * j]);
		swap(xy[2 * i + 1], xy[2 * j + 1]);
	}
	return true;
}

void PolygonBounds(const vector<float>& xy, float& minX, float& minY, float& maxX, float& maxY) {
	int n = VertexCount(xy);
	if (n == 0) {
		minX = minY = maxX = maxY = 0.f;
		return;
	}
	minX = maxX = xy[0];
	minY = maxY = xy[1];
	for (int i = 1; i < n; i++) {
		minX = min(minX, xy[2 * i]);
		maxX = max(maxX, xy[2 * i]);
		minY = min(minY, xy[2 * i + 1]);
		maxY = max(maxY, xy[2 * i + 1]);
	}
}

float ClosestPointOnSegment(float ax, float ay, float bx, float by, float px, float py, float& cx, float& cy) {
	double dx = static_cast<double>(bx) - ax, dy = static_cast<double>(by) - ay;
	double len2 = dx * dx + dy * dy;
	double t = 0.0;
	if (len2 > kEps) {
		t = ((static_cast<double>(px) - ax) * dx + (static_cast<double>(py) - ay) * dy) / len2;
		t = max(0.0, min(1.0, t));
	}
	cx = static_cast<float>(ax + dx * t);
	cy = static_cast<float>(ay + dy * t);
	double ex = static_cast<double>(px) - cx, ey = static_cast<double>(py) - cy;
	return static_cast<float>(sqrt(ex * ex + ey * ey));
}

float DistanceToPolygonBoundary(const vector<float>& xy, float x, float y) {
	int n = VertexCount(xy);
	float best = 1e30f;
	for (int i = 0; i < n; i++) {
		int j = (i + 1) % n;
		float cx, cy;
		best = min(best, ClosestPointOnSegment(xy[2 * i], xy[2 * i + 1], xy[2 * j], xy[2 * j + 1], x, y, cx, cy));
	}
	return best;
}

static bool RayCastInside(const vector<float>& xy, double x, double y) {
	int n = VertexCount(xy);
	bool inside = false;
	for (int i = 0, j = n - 1; i < n; j = i++) {
		double xi = xy[2 * i], yi = xy[2 * i + 1], xj = xy[2 * j], yj = xy[2 * j + 1];
		if ((yi > y) != (yj > y) && x < (xj - xi) * (y - yi) / (yj - yi) + xi) inside = !inside;
	}
	return inside;
}

bool PointInPolygon(const vector<float>& xy, float x, float y) {
	if (VertexCount(xy) < 3) return false;
	if (DistanceToPolygonBoundary(xy, x, y) <= kBoundaryTol) return true;
	return RayCastInside(xy, x, y);
}

float SignedDistanceToPolygon(const vector<float>& xy, float x, float y) {
	float d = DistanceToPolygonBoundary(xy, x, y);
	return RayCastInside(xy, x, y) ? d : -d;
}

bool SegmentsIntersect(float ax, float ay, float bx, float by, float cx, float cy, float dx, float dy) {
	int o1 = Orient(ax, ay, bx, by, cx, cy);
	int o2 = Orient(ax, ay, bx, by, dx, dy);
	int o3 = Orient(cx, cy, dx, dy, ax, ay);
	int o4 = Orient(cx, cy, dx, dy, bx, by);
	if (o1 != o2 && o3 != o4) return true;
	if (o1 == 0 && OnSegmentBox(ax, ay, bx, by, cx, cy)) return true;
	if (o2 == 0 && OnSegmentBox(ax, ay, bx, by, dx, dy)) return true;
	if (o3 == 0 && OnSegmentBox(cx, cy, dx, dy, ax, ay)) return true;
	if (o4 == 0 && OnSegmentBox(cx, cy, dx, dy, bx, by)) return true;
	return false;
}

bool SegmentInsidePolygon(const vector<float>& xy, float x0, float y0, float x1, float y1) {
	int n = VertexCount(xy);
	if (n < 3) return false;
	double dx = static_cast<double>(x1) - x0, dy = static_cast<double>(y1) - y0;
	double len2 = dx * dx + dy * dy;
	if (len2 < kEps) return PointInPolygon(xy, x0, y0);

	// 线段与每条边的交点参数t(沿线段0..1)；共线重叠时取边端点在线段上的投影。
	vector<double> ts = { 0.0, 1.0 };
	for (int i = 0; i < n; i++) {
		int j = (i + 1) % n;
		double ex = xy[2 * j] - static_cast<double>(xy[2 * i]);
		double ey = xy[2 * j + 1] - static_cast<double>(xy[2 * i + 1]);
		double denom = Cross(dx, dy, ex, ey);
		double fx = xy[2 * i] - static_cast<double>(x0), fy = xy[2 * i + 1] - static_cast<double>(y0);
		if (fabs(denom) < kEps) {
			// 平行：共线才可能重叠，把边的两个端点投影到线段上。
			if (fabs(Cross(dx, dy, fx, fy)) / sqrt(len2) > kBoundaryTol) continue;
			for (int k : { i, j }) {
				double t = ((xy[2 * k] - static_cast<double>(x0)) * dx + (xy[2 * k + 1] - static_cast<double>(y0)) * dy) / len2;
				if (t > 0.0 && t < 1.0) ts.push_back(t);
			}
			continue;
		}
		double t = Cross(fx, fy, ex, ey) / denom;
		double u = Cross(fx, fy, dx, dy) / denom;
		if (t > 0.0 && t < 1.0 && u >= -1e-7 && u <= 1.0 + 1e-7) ts.push_back(t);
	}
	sort(ts.begin(), ts.end());
	for (size_t i = 0; i + 1 < ts.size(); i++) {
		if (ts[i + 1] - ts[i] < 1e-7) continue;
		double tm = (ts[i] + ts[i + 1]) * 0.5;
		if (!PointInPolygon(xy, static_cast<float>(x0 + dx * tm), static_cast<float>(y0 + dy * tm))) return false;
	}
	return true;
}

bool PolygonSelfIntersects(const vector<float>& xy) {
	int n = VertexCount(xy);
	if (n < 4) return false;
	for (int i = 0; i < n; i++) {
		int i2 = (i + 1) % n;
		for (int j = i + 2; j < n; j++) {
			if (i == 0 && j == n - 1) continue; // 首尾边相邻
			int j2 = (j + 1) % n;
			if (SegmentsIntersect(xy[2 * i], xy[2 * i + 1], xy[2 * i2], xy[2 * i2 + 1],
				xy[2 * j], xy[2 * j + 1], xy[2 * j2], xy[2 * j2 + 1])) return true;
		}
	}
	return false;
}

bool TriangulatePolygon(const vector<float>& xy, vector<int>& triangles) {
	triangles.clear();
	int n = VertexCount(xy);
	if (n < 3) return false;
	double area = PolygonSignedArea(xy);
	if (fabs(area) < kEps) return false;
	double sign = area > 0.0 ? 1.0 : -1.0;

	vector<int> idx(n);
	for (int i = 0; i < n; i++) idx[i] = i;
	auto px = [&](int v) { return static_cast<double>(xy[2 * v]); };
	auto py = [&](int v) { return static_cast<double>(xy[2 * v + 1]); };

	int guard = 0;
	while (idx.size() > 3 && guard++ < 10 * n * n) {
		int m = static_cast<int>(idx.size());
		bool clipped = false;
		for (int k = 0; k < m && !clipped; k++) {
			int a = idx[(k + m - 1) % m], b = idx[k], c = idx[(k + 1) % m];
			double cr = Cross(px(b) - px(a), py(b) - py(a), px(c) - px(a), py(c) - py(a)) * sign;
			if (cr <= kEps) continue; // 凹角或共线，不是耳朵
			bool ok = true;
			for (int q = 0; q < m && ok; q++) {
				int v = idx[q];
				if (v == a || v == b || v == c) continue;
				// 与三角形顶点重合的点(多边形在同一点相碰)不算挡住
				if ((px(v) == px(a) && py(v) == py(a)) || (px(v) == px(b) && py(v) == py(b))
					|| (px(v) == px(c) && py(v) == py(c))) continue;
				double d1 = Cross(px(b) - px(a), py(b) - py(a), px(v) - px(a), py(v) - py(a)) * sign;
				double d2 = Cross(px(c) - px(b), py(c) - py(b), px(v) - px(b), py(v) - py(b)) * sign;
				double d3 = Cross(px(a) - px(c), py(a) - py(c), px(v) - px(c), py(v) - py(c)) * sign;
				if (d1 >= -kEps && d2 >= -kEps && d3 >= -kEps) ok = false;
			}
			if (!ok) continue;
			triangles.push_back(a);
			triangles.push_back(b);
			triangles.push_back(c);
			idx.erase(idx.begin() + k);
			clipped = true;
		}
		if (clipped) continue;
		// 找不到耳朵：多半是共线退化点，丢掉一个近似共线的顶点继续。
		int drop = -1;
		for (int k = 0; k < m; k++) {
			int a = idx[(k + m - 1) % m], b = idx[k], c = idx[(k + 1) % m];
			if (fabs(Cross(px(b) - px(a), py(b) - py(a), px(c) - px(a), py(c) - py(a))) <= 1e-7) { drop = k; break; }
		}
		if (drop < 0) return false;
		idx.erase(idx.begin() + drop);
	}
	if (idx.size() == 3) {
		triangles.push_back(idx[0]);
		triangles.push_back(idx[1]);
		triangles.push_back(idx[2]);
	}
	return !triangles.empty();
}
