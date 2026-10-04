#include "traffic/route.h"

#include "traffic/vehicle.h"
#include "map/map.h"
#include "common/utility.h"

#include <algorithm>
#include <cmath>

using namespace std;

namespace {
	constexpr float PI = 3.14159265358979323846f;

	float ToDegrees(float radians) { return radians * (180.f / PI); }
}

Route::Route(RouteFactory* factory, const string& modId, const string& name) :
	factory(factory), name(name) {
	mod = factory->CreateRoute(modId);
}

Route::~Route() {
	for (auto& [fromKey, inner] : graph) {
		for (auto& [toKey, edge] : inner) {
			if (edge.owned) {
				for (Connection* seg : edge.segments) delete seg;
			}
		}
	}
	if (mod) factory->DestroyRoute(mod);
}

bool Route::IsValid() const { return mod != nullptr; }

const string& Route::GetName() const { return name; }

const string& Route::GetStationType() const {
	static const string empty;
	return mod ? mod->stationType : empty;
}

const string& Route::GetVehicleType() const {
	static const string empty;
	return mod ? mod->vehicleType : empty;
}

bool Route::UsesRoadnet() const { return mod && mod->useRoadnet; }
bool Route::ShouldDrawPath() const { return mod && mod->drawPath; }

const string& Route::GetTrackMesh() const {
	static const string empty;
	return mod ? mod->trackMesh : empty;
}

float Route::GetTrackUnit() const { return mod ? mod->trackUnit : 0.f; }
int Route::GetVehiclesPerLine() const { return mod ? mod->vehiclesPerLine : 0; }

int Route::PackStop(int station, int interfaceIndex) { return station * 100 + interfaceIndex; }

Route::RouteEdge Route::BuildEdgeGeometry(Map* map, Station* from, int fromInterfaceIndex,
	Station* to, int toInterfaceIndex) const {
	RouteEdge edge;
	if (!mod || !from || !to) return edge;
	if (fromInterfaceIndex < 0 || fromInterfaceIndex >= static_cast<int>(from->GetInterfaces().size())) return edge;
	if (toInterfaceIndex < 0 || toInterfaceIndex >= static_cast<int>(to->GetInterfaces().size())) return edge;

	const StationInterface& a = from->GetInterfaces()[fromInterfaceIndex];
	const StationInterface& b = to->GetInterfaces()[toInterfaceIndex];

	if (mod->useRoadnet) {
		const StationRoadLink* linkA = from->EnsureRoadLink(map, fromInterfaceIndex);
		const StationRoadLink* linkB = to->EnsureRoadLink(map, toInterfaceIndex);
		if (!linkA || !linkB) return edge; // segments留空，调用方据此判定失败

		if (linkA->sourceEdge == linkB->sourceEdge && linkA->sourceEdge && linkA->t < linkB->t) {
			// 同车道特例：A、B在同一段原车道上且A在上游，直接用两个stationNode间的直线弦，
			// 不绕一圈FindVehiclePath，见route.md"同车道特例"一节。
			edge.segments.push_back(new Connection(*linkA->stationNode, *linkB->stationNode));
			edge.owned = true;
		} else {
			vector<Connection*> path = map->FindVehiclePath(linkA->laneTo->GetId(), linkB->laneFrom->GetId());
			edge.segments.push_back(linkA->outEdge);
			for (Connection* seg : path) edge.segments.push_back(seg);
			edge.segments.push_back(linkB->inEdge);
			edge.owned = false; // 全部借用Station/Map已有的Connection，不持有
		}
	} else {
		// 非路网边：三次贝塞尔，端点切线和接口方向一致，相邻边在站点处切线连续，见
		// route.md"接口与平滑曲线"一节。控制点z分别取A.z/B.z，支持飞机这类有高度变化的线路。
		float controlLength = mod->controlLength;
		Node start("route", a.x, a.y, a.z);
		Node end("route", b.x, b.y, b.z);
		Connection* conn = new Connection(start, end);
		Node control1("route_ctrl", a.x + a.departDirX * controlLength, a.y + a.departDirY * controlLength, a.z);
		Node control2("route_ctrl", b.x - b.arriveDirX * controlLength, b.y - b.arriveDirY * controlLength, b.z);
		conn->AddControls({ { control1, 1.f }, { control2, 1.f } });
		edge.segments.push_back(conn);
		edge.owned = true;
	}

	edge.length = 0.f;
	for (Connection* seg : edge.segments) edge.length += seg->CalcDistance();
	return edge;
}

void Route::Build(Map* map, const vector<Station*>& stations, vector<Station*>& outCreatedEdgeStations) {
	if (!mod || !map) return;

	vector<RouteStationInfo> candidateInterfaces;
	vector<Station*> resolvedStations; // 下标就是RouteStop::station

	for (Station* station : stations) {
		if (!station || station->IsEdge() || station->GetStationType() != mod->stationType) continue;

		int stationIndex = static_cast<int>(resolvedStations.size());
		resolvedStations.push_back(station);

		const vector<StationInterface>& interfaces = station->GetInterfaces();
		for (int i = 0; i < static_cast<int>(interfaces.size()); i++) {
			const StationInterface& iface = interfaces[i];
			RouteStationInfo info;
			info.name = station->GetName();
			info.x = iface.x; info.y = iface.y; info.z = iface.z;
			info.arriveDirX = iface.arriveDirX; info.arriveDirY = iface.arriveDirY;
			info.departDirX = iface.departDirX; info.departDirY = iface.departDirY;
			info.isEdge = false;
			info.stationIndex = stationIndex;
			info.interfaceIndex = i;
			candidateInterfaces.push_back(info);
		}
	}

	auto [mapSizeX, mapSizeY] = map->GetSize();
	mod->LayoutRoute(candidateInterfaces, mapSizeX, mapSizeY);

	for (size_t i = 0; i < mod->edgeStations.size(); i++) {
		const RouteStationInfo& info = mod->edgeStations[i];
		string edgeName = name + "_Edge" + to_string(i);
		Station* edgeStation = new Station(edgeName, info.x, info.y, info.z,
			info.arriveDirX, info.arriveDirY, info.departDirX, info.departDirY);
		resolvedStations.push_back(edgeStation);
		outCreatedEdgeStations.push_back(edgeStation);
	}

	for (const RouteLink& link : mod->links) {
		if (link.from.station < 0 || link.from.station >= static_cast<int>(resolvedStations.size()) ||
			link.to.station < 0 || link.to.station >= static_cast<int>(resolvedStations.size())) {
			debugf("Route::Build: 线路%s的link引用了越界的站点下标，跳过。\n", name.c_str());
			continue;
		}

		Station* fromStation = resolvedStations[link.from.station];
		Station* toStation = resolvedStations[link.to.station];
		RouteEdge edge = BuildEdgeGeometry(map, fromStation, link.from.interfaceIndex, toStation, link.to.interfaceIndex);
		if (edge.segments.empty()) {
			debugf("Route::Build: 线路%s的一条link几何构建失败(%s->%s)，跳过。\n",
				name.c_str(), fromStation->GetName().c_str(), toStation->GetName().c_str());
			continue;
		}

		int fromKey = PackStop(link.from.station, link.from.interfaceIndex);
		int toKey = PackStop(link.to.station, link.to.interfaceIndex);
		graph[fromKey][toKey] = edge;

		if (link.bidirectional) {
			RouteEdge reverseEdge = edge;
			reverseEdge.owned = false; // 共用同一份几何，只有正向entry负责delete
			reverseEdge.reversed = true;
			graph[toKey][fromKey] = reverseEdge;
		}
	}

	for (const vector<RouteStop>& stopSeq : mod->lines) {
		if (stopSeq.empty()) continue;

		vector<RouteLeg> legs;
		for (size_t i = 0; i < stopSeq.size(); i++) {
			const RouteStop& fromStop = stopSeq[i];
			const RouteStop& toStop = stopSeq[(i + 1) % stopSeq.size()];
			if (fromStop.station < 0 || fromStop.station >= static_cast<int>(resolvedStations.size()) ||
				toStop.station < 0 || toStop.station >= static_cast<int>(resolvedStations.size())) {
				debugf("Route::Build: 线路%s的lines引用了越界的站点下标，跳过这一腿。\n", name.c_str());
				continue;
			}

			int fromKey = PackStop(fromStop.station, fromStop.interfaceIndex);
			int toKey = PackStop(toStop.station, toStop.interfaceIndex);
			auto graphIt = graph.find(fromKey);
			if (graphIt == graph.end() || !graphIt->second.count(toKey)) {
				debugf("Route::Build: 线路%s里%s->%s没有可走的link，跳过这一腿(线路仍然按剩余站点成环)。\n",
					name.c_str(), resolvedStations[fromStop.station]->GetName().c_str(),
					resolvedStations[toStop.station]->GetName().c_str());
				continue;
			}

			RouteLeg leg;
			leg.fromStation = resolvedStations[fromStop.station];
			leg.fromInterfaceIndex = fromStop.interfaceIndex;
			leg.toStation = resolvedStations[toStop.station];
			leg.toInterfaceIndex = toStop.interfaceIndex;
			leg.edge = &graphIt->second.at(toKey);
			leg.dwellAtDestination = toStop.dwell;
			legs.push_back(leg);
		}

		if (!legs.empty()) lines.push_back(legs);
	}
}

const vector<vector<Route::RouteLeg>>& Route::GetLines() const { return lines; }

void Route::AddVehicleToLine(size_t lineIndex, Vehicle* vehicle) {
	if (lineIndex >= lineVehicles.size()) lineVehicles.resize(lineIndex + 1);
	lineVehicles[lineIndex].push_back(vehicle);
}

void Route::EvaluateEdge(const RouteEdge& edge, float arcLength,
	float& outX, float& outY, float& outZ, float& outYawDegrees) const {
	vector<Connection*> ordered = edge.segments;
	if (edge.reversed) reverse(ordered.begin(), ordered.end());

	float remaining = arcLength;
	for (size_t i = 0; i < ordered.size(); i++) {
		Connection* seg = ordered[i];
		float segLength = seg->CalcDistance();
		bool isLast = (i + 1 == ordered.size());
		if (remaining <= segLength || isLast) {
			float f = segLength > 0.f ? max(0.f, min(1.f, remaining / segLength)) : 0.f;
			float effectiveF = edge.reversed ? 1.f - f : f;

			Node point = seg->GetPoint(effectiveF);
			float dx, dy, dz;
			seg->GetTangent(effectiveF, dx, dy, dz);
			if (edge.reversed) { dx = -dx; dy = -dy; dz = -dz; }

			outX = point.GetX(); outY = point.GetY(); outZ = point.GetZ();
			outYawDegrees = ToDegrees(atan2f(dy, dx));
			return;
		}
		remaining -= segLength;
	}
}

void Route::DriveVehicle(Vehicle* vehicle, const vector<RouteLeg>& legs, float localTime) const {
	if (!vehicle || !mod) return;

	float t = localTime;
	for (const RouteLeg& leg : legs) {
		if (!leg.edge) continue;
		float travelTime = (mod->speed > 0.f) ? leg.edge->length / mod->speed : 0.f;
		if (t < travelTime) {
			float arcLength = (travelTime > 0.f) ? (t / travelTime) * leg.edge->length : 0.f;
			float x, y, z, yaw;
			EvaluateEdge(*leg.edge, arcLength, x, y, z, yaw);
			vehicle->SetTransform(x, y, z, yaw);
			return;
		}
		t -= travelTime;

		float dwellSeconds = leg.dwellAtDestination ? mod->dwellSeconds : 0.f;
		if (t < dwellSeconds) {
			const vector<StationInterface>& interfaces = leg.toStation->GetInterfaces();
			if (leg.toInterfaceIndex >= 0 && leg.toInterfaceIndex < static_cast<int>(interfaces.size())) {
				const StationInterface& iface = interfaces[leg.toInterfaceIndex];
				float yawDeg = ToDegrees(atan2f(iface.arriveDirY, iface.arriveDirX));
				vehicle->SetTransform(iface.x, iface.y, iface.z, yawDeg);
			}
			return;
		}
		t -= dwellSeconds;
	}
}

void Route::Update(float elapsedSeconds) {
	if (!mod) return;

	for (size_t lineIdx = 0; lineIdx < lines.size(); lineIdx++) {
		const vector<RouteLeg>& legs = lines[lineIdx];
		if (legs.empty() || lineIdx >= lineVehicles.size()) continue;

		float period = 0.f;
		for (const RouteLeg& leg : legs) {
			if (!leg.edge) continue;
			float travelTime = (mod->speed > 0.f) ? leg.edge->length / mod->speed : 0.f;
			period += travelTime + (leg.dwellAtDestination ? mod->dwellSeconds : 0.f);
		}
		if (period <= 0.f) continue;

		const vector<Vehicle*>& vehicles = lineVehicles[lineIdx];
		int vehicleCount = static_cast<int>(vehicles.size());
		for (int k = 0; k < vehicleCount; k++) {
			float phase = period * static_cast<float>(k) / static_cast<float>(vehicleCount);
			float localTime = fmodf(elapsedSeconds + phase, period);
			if (localTime < 0.f) localTime += period;
			DriveVehicle(vehicles[k], legs, localTime);
		}
	}
}
