#include "green_mod.h"

#include "map/polygon_util.h"
#include "common/utility.h"

#include <algorithm>
#include <cmath>
#include <queue>

using namespace std;

namespace {
	// 找最大圆的精度(地图单位，0.01=10cm)。
	constexpr float kCirclePrecision = 0.01f;
	// 剩余空间小于这个半径就认为放不下。
	constexpr float kMinCircleRadius = 0.01f;
	constexpr int kMaxCells = 200000;

	struct Cell {
		float x, y, h;     // 中心、半边长
		float d;           // 中心处的距离函数值
		float potential;   // 这个格子里可能达到的最大值
	};
	struct CellLess {
		bool operator()(const Cell& a, const Cell& b) const { return a.potential < b.potential; }
	};

	inline float UniformUnit() {
		return static_cast<float>(GetRandom(1000000)) / 1000000.f;
	}
}

void GreenMod::Layout(const GreenShape& shape) {
	polygon = shape.xy;
	minX = shape.minX;
	minY = shape.minY;
	maxX = shape.maxX;
	maxY = shape.maxY;
	linked = shape.linked;
	circles.clear();
	decorations.clear();
	trees = TreeRequests();
	triangleIndices.clear();
	triangleCumulative.clear();
	triangulated = false;
	LayoutGreen();
}

void GreenMod::AddCircle(float x, float y, float r) {
	if (r <= 0.f) return;
	circles.push_back({ x, y, r });
}

void GreenMod::AddDecoration(const DecorationSpec& spec) {
	if (spec.meshPath.empty()) return;
	decorations.push_back(spec);
}

void GreenMod::AddTreeSingle(const PointParams& position, float scale, const string& meshPath) {
	trees.AddSingle(position, scale, meshPath);
}

void GreenMod::AddTreeLine(const PointParams& from, const PointParams& to, int num, float scale,
	const string& meshPath) {
	trees.AddLine(from, to, num, scale, meshPath);
}

void GreenMod::AddTreeArea(const PointParams& from, const PointParams& to, int num, float scale,
	const string& meshPath) {
	trees.AddArea(from, to, num, scale, meshPath);
}

float GreenMod::GetPolygonArea() const {
	return fabsf(PolygonSignedArea(polygon));
}

bool GreenMod::FindLargestCircle(float& cx, float& cy, float& r) {
	if (polygon.size() < 6) return false;
	// 距离函数：到多边形边界的有符号距离(内部为正)，和到每个已有圆边缘的距离，取小。
	// 两部分都是1-Lipschitz，所以min也是，已有圆不需要特殊处理，格子剪枝照样成立。
	auto distance = [&](float x, float y) {
		float d = SignedDistanceToPolygon(polygon, x, y);
		for (const GreenCircle& c : circles) {
			float dx = x - c.x, dy = y - c.y;
			d = min(d, sqrtf(dx * dx + dy * dy) - c.r);
		}
		return d;
	};

	float w = maxX - minX, h = maxY - minY;
	float cellSize = min(w, h);
	if (cellSize <= 0.f) return false;
	float half = cellSize * 0.5f;

	priority_queue<Cell, vector<Cell>, CellLess> queue;
	for (float x = minX; x < maxX; x += cellSize) {
		for (float y = minY; y < maxY; y += cellSize) {
			float px = x + half, py = y + half;
			float d = distance(px, py);
			queue.push({ px, py, half, d, d + half * 1.41421356f });
		}
	}

	// 初始最优取包围盒中心，保证至少有个可比较的下界。
	float bestX = (minX + maxX) * 0.5f, bestY = (minY + maxY) * 0.5f;
	float bestD = distance(bestX, bestY);
	int cells = 0;
	while (!queue.empty() && cells++ < kMaxCells) {
		Cell cell = queue.top();
		queue.pop();
		if (cell.d > bestD) {
			bestD = cell.d;
			bestX = cell.x;
			bestY = cell.y;
		}
		if (cell.potential - bestD <= kCirclePrecision) continue;
		float q = cell.h * 0.5f;
		for (int i = 0; i < 4; i++) {
			float px = cell.x + ((i & 1) ? q : -q), py = cell.y + ((i & 2) ? q : -q);
			float d = distance(px, py);
			queue.push({ px, py, q, d, d + q * 1.41421356f });
		}
	}

	if (bestD < kMinCircleRadius) return false;
	cx = bestX;
	cy = bestY;
	r = bestD;
	return true;
}

void GreenMod::RandomPoint(float& x, float& y) {
	if (!triangulated) {
		triangulated = true;
		triangleCumulative.clear();
		if (TriangulatePolygon(polygon, triangleIndices)) {
			float sum = 0.f;
			for (size_t i = 0; i + 2 < triangleIndices.size(); i += 3) {
				int a = triangleIndices[i], b = triangleIndices[i + 1], c = triangleIndices[i + 2];
				float area = fabsf((polygon[2 * b] - polygon[2 * a]) * (polygon[2 * c + 1] - polygon[2 * a + 1])
					- (polygon[2 * c] - polygon[2 * a]) * (polygon[2 * b + 1] - polygon[2 * a + 1])) * 0.5f;
				sum += area;
				triangleCumulative.push_back(sum);
			}
		}
	}

	if (triangleCumulative.empty() || triangleCumulative.back() <= 0.f) {
		// 三角化失败(退化多边形)的兜底：包围盒内拒绝采样，实在不行取包围盒中心。
		for (int i = 0; i < 64; i++) {
			float px = minX + UniformUnit() * (maxX - minX), py = minY + UniformUnit() * (maxY - minY);
			if (PointInPolygon(polygon, px, py)) { x = px; y = py; return; }
		}
		x = (minX + maxX) * 0.5f;
		y = (minY + maxY) * 0.5f;
		return;
	}

	float pick = UniformUnit() * triangleCumulative.back();
	size_t tri = lower_bound(triangleCumulative.begin(), triangleCumulative.end(), pick) - triangleCumulative.begin();
	if (tri >= triangleCumulative.size()) tri = triangleCumulative.size() - 1;
	int a = triangleIndices[tri * 3], b = triangleIndices[tri * 3 + 1], c = triangleIndices[tri * 3 + 2];
	float r1 = sqrtf(UniformUnit()), r2 = UniformUnit();
	float wa = 1.f - r1, wb = r1 * (1.f - r2), wc = r1 * r2;
	x = polygon[2 * a] * wa + polygon[2 * b] * wb + polygon[2 * c] * wc;
	y = polygon[2 * a + 1] * wa + polygon[2 * b + 1] * wb + polygon[2 * c + 1] * wc;
}
