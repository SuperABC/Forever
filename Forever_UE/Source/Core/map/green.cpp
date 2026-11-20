#include "green.h"
#include "common/random.h"
#include "common/error.h"
#include "common/utility.h"

#include "map/polygon_util.h"

#include <algorithm>
#include <cmath>


using namespace std;

namespace {
	constexpr float kPi = 3.14159265358979323846f;
}

Green::Green(GreenFactory* factory, GreenMod* mod) :
	mod(mod),
	factory(factory),
	type(),
	name() {
	if (!mod) {
		THROW_EXCEPTION(NullPointerException, "Green mod is null.\n");
	}
	type = mod->GetType();
	name = mod->GetName();
}

Green::~Green() {
	factory->DestroyGreen(mod);
}

string Green::GetType() const { return type; }
string Green::GetName() const { return name; }
GreenMod* Green::GetMod() const { return mod; }
const vector<float>& Green::GetPolygon() const { return polygon; }
const vector<Green::Link>& Green::GetLinks() const { return links; }
const vector<Green::Circle>& Green::GetCircles() const { return circles; }
const vector<DecorationInstance>& Green::GetDecorations() const { return decorations; }
bool Green::IsLinked() const { return !links.empty(); }

void Green::GetBounds(float& outMinX, float& outMinY, float& outMaxX, float& outMaxY) const {
	outMinX = minX;
	outMinY = minY;
	outMaxX = maxX;
	outMaxY = maxY;
}

string Green::GetAddress() const {
	if (links.empty() || !links.front().road) return name;
	return links.front().road->GetName() + " -1 " + name;
}

string Green::GetCircleAddress(int circleIndex) const {
	if (links.empty()) return "";
	return GetAddress() + " " + to_string(circleIndex);
}

void Green::Layout(const vector<float>& xy, const vector<Link>& inLinks) {
	polygon = xy;
	links = inLinks;
	PolygonBounds(polygon, minX, minY, maxX, maxY);

	GreenShape shape;
	shape.xy = polygon;
	shape.minX = minX;
	shape.minY = minY;
	shape.maxX = maxX;
	shape.maxY = maxY;
	shape.linked = !links.empty();

	{
		RandomScope scope(RandomRegistry::GetInstanceStream(mod));
		mod->Layout(shape);
	}

	// 装饰物：包围盒系 -> 世界坐标，多边形外的丢弃。
	decorations.clear();
	float w = maxX - minX, h = maxY - minY;
	for (const DecorationSpec& spec : mod->decorations) {
		float x = minX + spec.position[0] * w + spec.position[1];
		float y = minY + spec.position[2] * h + spec.position[3];
		if (!PointInPolygon(polygon, x, y)) continue;
		DecorationInstance decoration;
		decoration.meshPath = spec.meshPath;
		decoration.x = x;
		decoration.y = y;
		decoration.z = spec.z;
		decoration.yaw = spec.yaw * kPi / 180.f;
		decoration.pitch = spec.pitch;
		decoration.roll = spec.roll;
		decoration.scaleX = spec.scaleX;
		decoration.scaleY = spec.scaleY;
		decoration.scaleZ = spec.scaleZ;
		decorations.push_back(decoration);
	}

	// 活动圆过滤并重新编号。
	circles.clear();
	int n = static_cast<int>(polygon.size() / 2);
	for (const GreenCircle& source : mod->circles) {
		if (source.r <= 0.f) continue;
		if (!PointInPolygon(polygon, source.x, source.y)) continue;

		Circle circle;
		circle.x = source.x;
		circle.y = source.y;
		circle.r = source.r;

		if (links.empty()) {
			circles.push_back(circle);
			continue;
		}

		// 按圆心到每条连路边(线段)的最短距离从近到远试。
		struct Candidate { float dist; int linkIndex; float footX, footY; };
		vector<Candidate> candidates;
		for (int i = 0; i < static_cast<int>(links.size()); i++) {
			int edge = links[i].edge;
			if (edge < 0 || edge >= n) continue;
			int next = (edge + 1) % n;
			float fx, fy;
			float dist = ClosestPointOnSegment(polygon[2 * edge], polygon[2 * edge + 1],
				polygon[2 * next], polygon[2 * next + 1], source.x, source.y, fx, fy);
			candidates.push_back({ dist, i, fx, fy });
		}
		stable_sort(candidates.begin(), candidates.end(),
			[](const Candidate& a, const Candidate& b) { return a.dist < b.dist; });

		bool accepted = false;
		for (const Candidate& candidate : candidates) {
			if (!SegmentInsidePolygon(polygon, source.x, source.y, candidate.footX, candidate.footY)) continue;
			circle.edge = links[candidate.linkIndex].edge;
			circle.footX = candidate.footX;
			circle.footY = candidate.footY;
			accepted = true;
			break;
		}
		if (accepted) circles.push_back(circle);
	}
}

pair<float, float> Green::RandomPointInCircle(int circleIndex) const {
	if (circleIndex < 0 || circleIndex >= static_cast<int>(circles.size())) return { 0.f, 0.f };
	const Circle& c = circles[circleIndex];
	// 圆内均匀：半径取sqrt。随机数范围1e6精度远小于圆的尺寸。
	float u = static_cast<float>(GetRandom(1000000)) / 1000000.f;
	float v = static_cast<float>(GetRandom(1000000)) / 1000000.f;
	float radius = c.r * sqrtf(u);
	float angle = v * 2.f * kPi;
	return { c.x + radius * cosf(angle), c.y + radius * sinf(angle) };
}
