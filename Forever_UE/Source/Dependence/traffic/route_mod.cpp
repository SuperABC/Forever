#include "route_mod.h"

void RouteMod::AddHatch(const Connection* connection, float t1, float t2, float width) {
	BuildHatch(connection, t1, t2, width, false, hatches);
}

void RouteMod::AddTunnelHatch(const Connection* connection, float t1, float t2, float width) {
	BuildHatch(connection, t1, t2, width, true, hatches);
}

void RouteMod::AddTerrainFit(const Connection* connection, float t1, float t2, float width, float falloff) {
	BuildTerrainFit(connection, t1, t2, width, falloff, terrainFits);
}
