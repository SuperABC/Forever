#include "roadnet_mod.h"

#include <cmath>
#include <algorithm>

using namespace std;

void RoadnetMod::AddHatch(const Connection* connection, float t1, float t2, float width) {
	BuildHatch(connection, t1, t2, width, false, hatches);
}

void RoadnetMod::AddTunnelHatch(const Connection* connection, float t1, float t2, float width) {
	BuildHatch(connection, t1, t2, width, true, hatches);
}

void RoadnetMod::AddTerrainFit(const Connection* connection, float t1, float t2, float width, float falloff) {
	BuildTerrainFit(connection, t1, t2, width, falloff, terrainFits);
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

void RoadnetMod::AddLampSingle(const PointParams& position, float rotationDegrees, const string& blueprintPath) {
	lamps.AddSingle(position, rotationDegrees, blueprintPath);
}

void RoadnetMod::AddLampLine(const PointParams& from, const PointParams& to, int num, float rotationDegrees,
	const string& blueprintPath) {
	lamps.AddLine(from, to, num, rotationDegrees, blueprintPath);
}

void RoadnetMod::AddLampArea(const PointParams& from, const PointParams& to, int num, float rotationDegrees,
	const string& blueprintPath) {
	lamps.AddArea(from, to, num, rotationDegrees, blueprintPath);
}
