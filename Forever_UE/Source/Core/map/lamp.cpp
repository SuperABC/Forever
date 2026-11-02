#include "lamp.h"

#include "common/utility.h"

#include <cmath>


using namespace std;

namespace {
	const float kDegToRad = 0.01745329252f;

	float RandomUnit() {
		return GetRandom(100000) / 100000.f;
	}
}

int Lamps::RegisterBlueprint(const string& blueprintPath) {
	auto found = blueprintIndices.find(blueprintPath);
	if (found != blueprintIndices.end()) return found->second;
	int index = static_cast<int>(blueprintPaths.size());
	blueprintPaths.push_back(blueprintPath);
	blueprintIndices[blueprintPath] = index;
	return index;
}

int Lamps::ExpandRequests(const LampRequests& requests, const TreeFrame& frame, const PlaceFilter& filter) {
	auto toWorld = [&frame](const PointParams& p) {
		auto [lx, ly] = frame.toLocal(p);
		return frame.localToWorld(lx, ly);
		};
	// 请求里的朝向是宿主局部系的度数，叠加宿主旋转(弧度)后就是世界朝向。
	auto worldYaw = [&frame](float rotationDegrees) {
		return rotationDegrees * kDegToRad + frame.rotation;
		};
	int placed = 0;
	for (const LampSingleSpec& spec : requests.singles) {
		auto [x, y] = toWorld(spec.position);
		placed += ExpandSingle(x, y, worldYaw(spec.rotationDegrees), spec.blueprintPath, filter);
	}
	for (const LampLineSpec& spec : requests.lines) {
		auto [fromX, fromY] = toWorld(spec.from);
		auto [toX, toY] = toWorld(spec.to);
		placed += ExpandLine(fromX, fromY, toX, toY, spec.num, worldYaw(spec.rotationDegrees), spec.blueprintPath, filter);
	}
	for (const LampAreaSpec& spec : requests.areas) {
		// 同Vegetation::ExpandRequests：两个对角先换到宿主局部坐标取中心/尺寸，中心再换世界坐标。
		auto [ax, ay] = frame.toLocal(spec.from);
		auto [bx, by] = frame.toLocal(spec.to);
		auto [cx, cy] = frame.localToWorld((ax + bx) * 0.5f, (ay + by) * 0.5f);
		Quad quad(cx, cy, fabsf(bx - ax), fabsf(by - ay));
		placed += ExpandArea(quad, frame.rotation, spec.num, worldYaw(spec.rotationDegrees), spec.blueprintPath, filter);
	}
	return placed;
}

int Lamps::ExpandSingle(float x, float y, float yaw, const string& blueprintPath, const PlaceFilter& filter) {
	if (blueprintPath.empty()) return 0;
	if (filter && !filter(x, y)) return 0;
	AddLamp(x, y, yaw, RegisterBlueprint(blueprintPath));
	return 1;
}

int Lamps::ExpandLine(float fromX, float fromY, float toX, float toY, int num, float yaw,
	const string& blueprintPath, const PlaceFilter& filter) {
	if (num <= 0 || blueprintPath.empty()) return 0;
	int blueprintIndex = RegisterBlueprint(blueprintPath);
	int placed = 0;
	for (int i = 0; i < num; i++) {
		float t = num == 1 ? 0.5f : float(i) / float(num - 1);
		float x = fromX + (toX - fromX) * t;
		float y = fromY + (toY - fromY) * t;
		if (filter && !filter(x, y)) continue;
		AddLamp(x, y, yaw, blueprintIndex);
		placed++;
	}
	return placed;
}

int Lamps::ExpandArea(const Quad& quad, float rotation, int num, float yaw,
	const string& blueprintPath, const PlaceFilter& filter) {
	if (num <= 0 || blueprintPath.empty()) return 0;
	int blueprintIndex = RegisterBlueprint(blueprintPath);
	float c = cosf(rotation), s = sinf(rotation);
	int placed = 0;
	for (int i = 0; i < num; i++) {
		float relX = (RandomUnit() - 0.5f) * quad.GetSizeX();
		float relY = (RandomUnit() - 0.5f) * quad.GetSizeY();
		float x = quad.GetPosX() + relX * c - relY * s;
		float y = quad.GetPosY() + relX * s + relY * c;
		if (filter && !filter(x, y)) continue;
		AddLamp(x, y, yaw, blueprintIndex);
		placed++;
	}
	return placed;
}

const vector<string>& Lamps::GetBlueprintPaths() const {
	return blueprintPaths;
}

const vector<LampInstance>& Lamps::GetLamps() const {
	return lamps;
}

void Lamps::AddLamp(float x, float y, float yaw, int blueprintIndex) {
	LampInstance lamp;
	lamp.id = nextId++;
	lamp.x = x;
	lamp.y = y;
	lamp.yaw = yaw;
	lamp.blueprintIndex = blueprintIndex;
	lamps.push_back(lamp);
}
