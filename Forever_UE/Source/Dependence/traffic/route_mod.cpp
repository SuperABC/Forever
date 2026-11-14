#include "route_mod.h"

void RouteMod::AddHatch(const Connection* connection, float t1, float t2, float width) {
	BuildHatch(connection, t1, t2, width, false, hatches);
}

void RouteMod::AddTunnelHatch(const Connection* connection, float t1, float t2, float width) {
	BuildHatch(connection, t1, t2, width, true, hatches);
}

void RouteMod::AddDemolition(const std::vector<std::pair<float, float>>& centerline, float width) {
	DemolishRequest request;
	request.centerline = centerline;
	request.width = width;
	demolitions.push_back(std::move(request));
}

void RouteMod::AddPier(const std::string& mesh, std::vector<std::vector<Connection>> chains, float defaultSpacing, float minSpacing, float radius) {
	PierRequest request;
	request.mesh = mesh;
	request.chains = std::move(chains);
	request.defaultSpacing = defaultSpacing;
	request.minSpacing = minSpacing;
	request.radius = radius;
	piers.push_back(std::move(request));
}

void RouteMod::AddTerrainFit(const Connection* connection, float t1, float t2, float width, float falloff) {
	BuildTerrainFit(connection, t1, t2, width, falloff, terrainFits);
}
