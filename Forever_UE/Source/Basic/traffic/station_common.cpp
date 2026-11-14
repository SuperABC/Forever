#include "station_common.h"

#include "map/general_geometry.h"



using namespace std;

// alongRatio沿道路方向定位，depthRatio沿进深方向(0=贴路边，1=远离)，换算成quad局部PointParams。
PointParams NearRoadPoint(int direction, float alongRatio, float depthRatio) {
	switch (direction) {
		case FACE_WEST:  return { depthRatio, 0.f, alongRatio, 0.f };
		case FACE_EAST:  return { 1.f - depthRatio, 0.f, alongRatio, 0.f };
		case FACE_NORTH: return { alongRatio, 0.f, depthRatio, 0.f };
		case FACE_SOUTH: return { alongRatio, 0.f, 1.f - depthRatio, 0.f };
		default: return { alongRatio, 0.f, depthRatio, 0.f };
	}
}

// 沿frontage轴(alongRatio增大方向)的朝向角度；towardIncreasing=false时转180度。
// 推导见station_common.md。
float AlongAxisHeadingDegrees(int direction, bool towardIncreasing) {
	bool alongIsYAxis = (direction == FACE_WEST || direction == FACE_EAST);
	float baseDegrees = alongIsYAxis ? 90.f : 0.f;
	return towardIncreasing ? baseDegrees : baseDegrees + 180.f;
}

StationCurveSpec StraightCurve(const PointParams& from, const PointParams& to, float z, const string& mesh, float meshUnit) {
	StationCurveSpec curve;
	curve.p1 = from;
	curve.p2 = to;
	for (int i = 0; i < 4; i++) {
		curve.c1[i] = from[i] + (to[i] - from[i]) / 3.f;
		curve.c2[i] = from[i] + (to[i] - from[i]) * 2.f / 3.f;
	}
	curve.z1 = curve.c1z = curve.c2z = curve.z2 = z;
	curve.mesh = mesh;
	curve.meshUnit = meshUnit;
	return curve;
}

StationConnectionSpec StraightConnection(PortKind fromKind, int fromIndex, const PointParams& from,
	PortKind toKind, int toIndex, const PointParams& to, float z, const string& mesh, float meshUnit) {
	StationConnectionSpec connection;
	connection.fromKind = fromKind; connection.fromIndex = fromIndex;
	connection.toKind = toKind; connection.toIndex = toIndex;
	connection.curves.push_back(StraightCurve(from, to, z, mesh, meshUnit));
	return connection;
}
