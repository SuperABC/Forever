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
