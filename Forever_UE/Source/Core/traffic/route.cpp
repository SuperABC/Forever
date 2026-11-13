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
	if (mod) mod->SetProperty();
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
bool Route::ShouldInstantiate() const { return mod && mod->instantiate; }

const string& Route::GetTrackMesh() const {
	static const string empty;
	return mod ? mod->trackMesh : empty;
}

const string& Route::GetTrackTunnelMesh() const {
	static const string empty;
	return mod ? mod->trackTunnelMesh : empty;
}

float Route::GetTrackWidth() const { return mod ? mod->trackWidth : 0.f; }
float Route::GetTrackUnit() const { return mod ? mod->trackUnit : 0.f; }
int Route::GetVehiclesPerLine() const { return mod ? mod->vehiclesPerLine : 0; }

int Route::PackStop(int station, int interfaceIndex) { return station * 100 + interfaceIndex; }

Route::RouteEdge Route::BuildEdgeGeometry(Map* map, Station* from, int fromInterfaceIndex,
	Station* to, int toInterfaceIndex, const vector<TrackSegment>* track) const {
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
		if (track && !track->empty()) {
			// instantiate线路：按mod给的分段值建Connection链(过山抬升/隧道的高度变化都在里面)。
			for (const TrackSegment& seg : *track) {
				Connection* conn = new Connection(Node("route", seg.x1, seg.y1, seg.z1), Node("route", seg.x2, seg.y2, seg.z2));
				conn->AddControls({ { Node("route_ctrl", seg.c1x, seg.c1y, seg.c1z), 1.f },
					{ Node("route_ctrl", seg.c2x, seg.c2y, seg.c2z), 1.f } });
				edge.segments.push_back(conn);
			}
			edge.owned = true;
			edge.track = *track;
			edge.length = 0.f;
			for (Connection* seg : edge.segments) edge.length += seg->CalcDistance();
			return edge;
		}

		// 非路网边：三次贝塞尔，端点切线和接口方向一致，相邻边在站点处切线连续，见
		// route.md"接口与平滑曲线"一节。控制点z分别取A.z/B.z，支持飞机这类有高度变化的线路。
		float controlLength = mod->controlLength;
		auto tagMatch = [](const Station* station, const string& tag) {
			if (tag.empty()) return true;
			if (tag == "@real") return !station->IsEdge();
			return station->GetName().find(tag) != string::npos;
		};
		// 边缘站点名=<线路名>_Edge<i>_<mod站名>，组=mod站名里第一个"_"之前的部分；真实站点没有组。
		auto groupOf = [](const Station* station) {
			if (!station->IsEdge()) return string();
			const string& stationName = station->GetName();
			size_t at = stationName.find("_Edge");
			if (at == string::npos) return string();
			size_t modName = stationName.find('_', at + 1);
			if (modName == string::npos) return string();
			modName++;
			size_t end = stationName.find('_', modName);
			return stationName.substr(modName, end == string::npos ? string::npos : end - modName);
		};
		for (const RouteMod::ControlRule& rule : mod->controlRules) {
			if (rule.sameGroup) {
				string groupFrom = groupOf(from);
				if (groupFrom.empty() || groupFrom != groupOf(to)) continue;
			}
			if (tagMatch(from, rule.fromTag) && tagMatch(to, rule.toTag)) {
				controlLength = rule.arm;
				if (rule.ratio) controlLength *= sqrtf((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y));
				break;
			}
		}
		Node start("route", a.x, a.y, a.z);
		Node end("route", b.x, b.y, b.z);
		Connection* conn = new Connection(start, end);
		if (controlLength > 0.f) { // <=0：不加控制点，直线
			Node control1("route_ctrl", a.x + a.departDirX * controlLength, a.y + a.departDirY * controlLength, a.z);
			Node control2("route_ctrl", b.x - b.arriveDirX * controlLength, b.y - b.arriveDirY * controlLength, b.z);
			conn->AddControls({ { control1, 1.f }, { control2, 1.f } });
		}
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
		string edgeName = name + "_Edge" + to_string(i) + (info.name.empty() ? string() : "_" + info.name);
		Station* edgeStation = new Station(edgeName, info.x, info.y, info.z,
			info.arriveDirX, info.arriveDirY, info.departDirX, info.departDirY);
		resolvedStations.push_back(edgeStation);
		outCreatedEdgeStations.push_back(edgeStation);
	}

	// instantiate线路：先让mod沿每条link铺轨(过山抬升/挖隧道)，几何交给BuildEdgeGeometry、请求转发给
	// Map——和Map::InitRoadnet对路网的做法一致，所以Traffic必须在地形网格生成之前构建。读档不走这里
	// (地形已经烘焙，几何从存档读)，见traffic_save.md。
	if (mod->instantiate && !mod->useRoadnet) {
		auto interfaceInfo = [&](const RouteStop& stop, RouteStationInfo& out) {
			if (stop.station < 0 || stop.station >= static_cast<int>(resolvedStations.size())) return false;
			const vector<StationInterface>& interfaces = resolvedStations[stop.station]->GetInterfaces();
			if (stop.interfaceIndex < 0 || stop.interfaceIndex >= static_cast<int>(interfaces.size())) return false;
			const StationInterface& iface = interfaces[stop.interfaceIndex];
			out.x = iface.x; out.y = iface.y; out.z = iface.z;
			out.arriveDirX = iface.arriveDirX; out.arriveDirY = iface.arriveDirY;
			out.departDirX = iface.departDirX; out.departDirY = iface.departDirY;
			out.stationIndex = stop.station; out.interfaceIndex = stop.interfaceIndex;
			return true;
			};
		vector<RouteTrackLinkEnds> linkEnds(mod->links.size());
		for (size_t i = 0; i < mod->links.size(); i++) {
			if (!interfaceInfo(mod->links[i].from, linkEnds[i].from) || !interfaceInfo(mod->links[i].to, linkEnds[i].to)) {
				linkEnds[i].from.stationIndex = -1; // 越界的link，下面建边时同样会被跳过
			}
		}
		auto getTerrain = [map](int x, int y) -> const string& { return map->GetTerrainRef(x, y); };
		auto getWater = [map](int x, int y) -> pair<bool, float> { return map->GetWater(x, y); };
		auto getHeight = [map](float x, float y) -> float { return map->SampleHeight(x, y); };
		mod->BuildTrack(linkEnds, getTerrain, getWater, getHeight);
		for (const TerrainHatch& hatch : mod->hatches) map->AddHatch(hatch);
		map->ApplyTerrainFits(mod->terrainFits);
	}

	for (size_t linkIndex = 0; linkIndex < mod->links.size(); linkIndex++) {
		const RouteLink& link = mod->links[linkIndex];
		if (link.from.station < 0 || link.from.station >= static_cast<int>(resolvedStations.size()) ||
			link.to.station < 0 || link.to.station >= static_cast<int>(resolvedStations.size())) {
			debugf("Route::Build: 线路%s的link引用了越界的站点下标，跳过。\n", name.c_str());
			continue;
		}

		Station* fromStation = resolvedStations[link.from.station];
		Station* toStation = resolvedStations[link.to.station];
		const vector<TrackSegment>* track = (linkIndex < mod->trackEdges.size() && !mod->trackEdges[linkIndex].segments.empty())
			? &mod->trackEdges[linkIndex].segments : nullptr;
		RouteEdge edge = BuildEdgeGeometry(map, fromStation, link.from.interfaceIndex, toStation, link.to.interfaceIndex, track);
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
			leg.silentDwell = toStop.dwell && toStop.silent;
			legs.push_back(leg);
		}

		if (!legs.empty()) {
			FinalizeLegTimes(legs);
			lines.push_back(legs);
		}
	}

	stopStations = resolvedStations;
}

void Route::FinalizeLegTimes(vector<RouteLeg>& legs) const {
	float scale = (mod && mod->sameStationSpeedScale > 0.f) ? mod->sameStationSpeedScale : 1.f;
	size_t n = legs.size();
	for (RouteLeg& leg : legs) leg.timeLength = leg.edge ? leg.edge->length : 0.f;
	if (scale == 1.f || n == 0) return;

	// 行驶区段=两条真正停靠的leg之间；环线上从某条停靠leg之后开始逐段扫，没有停靠的线路整条当一个区段。
	size_t start = 0;
	for (size_t i = 0; i < n; i++) {
		if (legs[i].dwellAtDestination) { start = (i + 1) % n; break; }
	}
	size_t i = 0;
	while (i < n) {
		size_t first = i;
		bool transfer = false;
		while (i < n) {
			const RouteLeg& leg = legs[(start + i) % n];
			if (leg.fromStation && leg.fromStation == leg.toStation) transfer = true;
			i++;
			if (leg.dwellAtDestination) break;
		}
		if (!transfer) continue;
		for (size_t k = first; k < i; k++) {
			RouteLeg& leg = legs[(start + k) % n];
			leg.timeLength = (leg.edge ? leg.edge->length : 0.f) / scale;
		}
	}
}

const vector<vector<Route::RouteLeg>>& Route::GetLines() const { return lines; }

void Route::AddVehicleToLine(size_t lineIndex, Vehicle* vehicle) {
	if (lineIndex >= lineVehicles.size()) lineVehicles.resize(lineIndex + 1);
	lineVehicles[lineIndex].push_back(vehicle);
}

void Route::EvaluateEdge(const RouteEdge& edge, float arcLength,
	float& outX, float& outY, float& outZ, float& outYawDegrees, float& outPitchDegrees) const {
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
			outPitchDegrees = ToDegrees(atan2f(dz, sqrtf(dx * dx + dy * dy)));
			return;
		}
		remaining -= segLength;
	}
}

void Route::DriveVehicle(Vehicle* vehicle, const vector<RouteLeg>& legs, float localTime,
	Station** outDwellStation) const {
	if (outDwellStation) *outDwellStation = nullptr;
	if (!vehicle || !mod || legs.empty()) return;

	// 缓动按"行驶区段"(两次真正停靠之间的若干条leg)整体套梯形速度曲线，不按单leg，原因见route.md。
	// easeSeconds是真实秒，下面都是游戏内秒，系数120对应默认timeFlowRatio=2.0。
	constexpr float kGameSecondsPerRealSecond = 120.f;
	float kEaseSeconds = mod->easeSeconds * kGameSecondsPerRealSecond;

	float t = localTime;
	size_t legIndex = legs.size();
	bool inDwell = false;
	for (size_t i = 0; i < legs.size(); i++) {
		const RouteLeg& leg = legs[i];
		if (!leg.edge) continue;
		float travelTime = (mod->speed > 0.f) ? leg.timeLength / mod->speed : 0.f;
		if (t < travelTime) { legIndex = i; break; }
		t -= travelTime;

		float dwellSeconds = leg.dwellAtDestination ? mod->dwellSeconds : 0.f;
		if (t < dwellSeconds) { legIndex = i; inDwell = true; break; }
		t -= dwellSeconds;
	}
	if (legIndex >= legs.size()) return;

	if (inDwell) {
		const RouteLeg& leg = legs[legIndex];
		if (outDwellStation && !leg.silentDwell) *outDwellStation = leg.toStation;
		const vector<StationInterface>& interfaces = leg.toStation->GetInterfaces();
		if (leg.toInterfaceIndex >= 0 && leg.toInterfaceIndex < static_cast<int>(interfaces.size())) {
			const StationInterface& iface = interfaces[leg.toInterfaceIndex];
			float yawDeg = ToDegrees(atan2f(iface.arriveDirY, iface.arriveDirX));
			vehicle->SetTransform(iface.x, iface.y, iface.z, yawDeg);
			vehicle->SetPitch(0.f);
		}
		return;
	}

	// t现在是legIndex这条leg"行驶"阶段已经过去的时间。往回找这条leg所在"行驶区段"的起点——
	// 上一条真正停靠(dwellAtDestination)的leg之后那一条就是区段起点；绕一整圈都没找到就说明
	// 这条线路从不停靠(比如还没配置dwell的线路)，把整条线路当一个区段处理。
	size_t segStart = legIndex;
	while (true) {
		size_t prevIdx = (segStart == 0) ? legs.size() - 1 : segStart - 1;
		if (legs[prevIdx].dwellAtDestination) break;
		if (prevIdx == legIndex) break;
		segStart = prevIdx;
	}

	// 时长累计只用来定位缓动曲线上的点；定位落在哪条leg必须按累计长度重走(见route.md)，否则接口处卡顿/瞬移。
	float segTotalTime = 0.f, segTotalLength = 0.f;
	float segTimeBeforeCurrentLeg = 0.f;
	{
		size_t j = segStart;
		while (true) {
			const RouteLeg& segLeg = legs[j];
			float segLegLength = segLeg.timeLength;
			float segLegTravelTime = (mod->speed > 0.f) ? segLegLength / mod->speed : 0.f;
			if (j == legIndex) {
				segTimeBeforeCurrentLeg = segTotalTime;
			}
			segTotalTime += segLegTravelTime;
			segTotalLength += segLegLength;
			if (segLeg.dwellAtDestination) break; // 这条leg自己就是区段终点(真正停靠)
			size_t nextIdx = (j + 1) % legs.size();
			if (nextIdx == segStart) break; // 保护性退出：绕完一圈都没碰到停靠
			j = nextIdx;
		}
	}

	float segElapsed = segTimeBeforeCurrentLeg + t;

	// 梯形速度曲线：ta/td钳在区段总时长一半以内，避免区段比2*kEaseSeconds还短时两头的
	// 加减速阶段重叠打架。cruiseSpeed比配置的mod->speed略高一点，补偿两头比匀速慢那部分
	// 距离，确保segTotalTime(=Σlength/speed，和原来完全一样)时间内还是正好走完
	// segTotalLength，不影响Update()按这个值算的时刻表/period。
	float ta = min(kEaseSeconds, segTotalTime * 0.5f);
	float td = min(kEaseSeconds, segTotalTime * 0.5f);
	float cruiseSpan = segTotalTime - ta * 0.5f - td * 0.5f;
	float cruiseSpeed = (cruiseSpan > 0.f) ? segTotalLength / cruiseSpan : mod->speed;

	float segArcLength;
	if (ta > 0.f && segElapsed < ta) {
		segArcLength = cruiseSpeed * segElapsed * segElapsed / (2.f * ta);
	} else if (segElapsed < segTotalTime - td) {
		segArcLength = cruiseSpeed * (ta * 0.5f + max(0.f, segElapsed - ta));
	} else if (td > 0.f) {
		float remain = max(0.f, segTotalTime - segElapsed);
		segArcLength = segTotalLength - cruiseSpeed * remain * remain / (2.f * td);
	} else {
		segArcLength = segTotalLength;
	}

	// 按累计长度(不是累计时间)重新从segStart走一遍，找出segArcLength真正落在哪条leg上——
	// 和EvaluateEdge内部按弧长在多段segments里定位同一个道理，只是这里定位的是"哪条leg"，
	// 不是"哪个segment"。
	size_t resultLegIndex = segStart;
	float lengthBeforeResultLeg = 0.f;
	{
		size_t j = segStart;
		while (true) {
			const RouteLeg& segLeg = legs[j];
			float segLegLength = segLeg.timeLength;
			bool isLastInSeg = segLeg.dwellAtDestination;
			if (segArcLength <= lengthBeforeResultLeg + segLegLength || isLastInSeg) {
				resultLegIndex = j;
				break;
			}
			lengthBeforeResultLeg += segLegLength;
			size_t nextIdx = (j + 1) % legs.size();
			if (nextIdx == segStart) { resultLegIndex = j; break; } // 保护性退出
			j = nextIdx;
		}
	}

	const RouteLeg& resultLeg = legs[resultLegIndex];
	if (!resultLeg.edge) return;
	float arcLength = segArcLength - lengthBeforeResultLeg;
	arcLength = max(0.f, min(arcLength, resultLeg.timeLength));
	// 时间等效长度换回几何弧长(降速区段里timeLength>length)。
	if (resultLeg.timeLength > 1e-6f) arcLength *= resultLeg.edge->length / resultLeg.timeLength;

	float x, y, z, yaw, pitch = 0.f;
	EvaluateEdge(*resultLeg.edge, arcLength, x, y, z, yaw, pitch);
	vehicle->SetTransform(x, y, z, yaw);
	vehicle->SetPitch((mod->instantiate || mod->tiltAlongPath) ? pitch : 0.f);
}

void Route::Update(float elapsedSeconds) {
	if (!mod) return;
	lastElapsedSeconds = elapsedSeconds;

	for (size_t lineIdx = 0; lineIdx < lines.size(); lineIdx++) {
		const vector<RouteLeg>& legs = lines[lineIdx];
		if (legs.empty() || lineIdx >= lineVehicles.size()) continue;

		float period = 0.f;
		for (const RouteLeg& leg : legs) {
			if (!leg.edge) continue;
			float travelTime = (mod->speed > 0.f) ? leg.timeLength / mod->speed : 0.f;
			period += travelTime + (leg.dwellAtDestination ? mod->dwellSeconds : 0.f);
		}
		if (period <= 0.f) continue;

		const vector<Vehicle*>& vehicles = lineVehicles[lineIdx];
		int vehicleCount = static_cast<int>(vehicles.size());
		for (int k = 0; k < vehicleCount; k++) {
			float phase = period * static_cast<float>(k) / static_cast<float>(vehicleCount);
			Vehicle* vehicle = vehicles[k];
			float localTime = fmodf(elapsedSeconds + phase + GetPhaseAdjust(vehicle), period);
			if (localTime < 0.f) localTime += period;
			// 物理段（UE层的物理公交+Controller在开）：位姿和停站事件都由UE层负责，这里不碰，
			// 否则时刻表会和实车打架、重复发进站/离站事件。交还时刻表时ResetStopState重新观察。
			if (vehicle->IsPhysicsDriven()) continue;
			Station* dwellStation = nullptr;
			DriveVehicle(vehicle, legs, localTime, &dwellStation);

			// 停站/离站边沿检测，见route.h的StopEvent说明。
			auto last = lastStopStation.find(vehicle);
			if (last == lastStopStation.end()) {
				if (!silentInitialStopState && dwellStation) pendingStopEvents.push_back({ vehicle, dwellStation, true });
				lastStopStation[vehicle] = dwellStation;
			}
			else if (last->second != dwellStation) {
				if (last->second) pendingStopEvents.push_back({ vehicle, last->second, false });
				if (dwellStation) pendingStopEvents.push_back({ vehicle, dwellStation, true });
				last->second = dwellStation;
			}
		}
	}
	silentInitialStopState = false;
}

vector<Route::StopEvent> Route::ConsumeStopEvents() {
	vector<StopEvent> result;
	result.swap(pendingStopEvents);
	return result;
}

void Route::SilenceInitialStopEvents() {
	silentInitialStopState = true;
}

void Route::ResetStopState(Vehicle* vehicle) {
	lastStopStation.erase(vehicle);
}

Route::ScheduleState Route::QueryVehicleState(const Vehicle* vehicle) const {
	ScheduleState state;
	if (!mod || !vehicle) return state;
	constexpr float kGameSecondsPerRealSecond = 120.f; // 和DriveVehicle的缓动换算同一个假设

	for (size_t lineIdx = 0; lineIdx < lines.size() && lineIdx < lineVehicles.size(); lineIdx++) {
		const vector<RouteLeg>& legs = lines[lineIdx];
		const vector<Vehicle*>& vehicles = lineVehicles[lineIdx];
		int vehicleCount = static_cast<int>(vehicles.size());
		for (int k = 0; k < vehicleCount; k++) {
			if (vehicles[k] != vehicle) continue;

			float period = 0.f;
			for (const RouteLeg& leg : legs) {
				if (!leg.edge) continue;
				float travelTime = (mod->speed > 0.f) ? leg.timeLength / mod->speed : 0.f;
				period += travelTime + (leg.dwellAtDestination ? mod->dwellSeconds : 0.f);
			}
			if (period <= 0.f) return state;

			float phase = period * static_cast<float>(k) / static_cast<float>(vehicleCount);
			float t = fmodf(lastElapsedSeconds + phase + GetPhaseAdjust(vehicle), period);
			if (t < 0.f) t += period;

			state.line = static_cast<int>(lineIdx);
			state.speedMapPerRealSecond = mod->speed * kGameSecondsPerRealSecond;
			for (size_t i = 0; i < legs.size(); i++) {
				const RouteLeg& leg = legs[i];
				if (!leg.edge) continue;
				float travelTime = (mod->speed > 0.f) ? leg.timeLength / mod->speed : 0.f;
				if (t < travelTime) {
					state.valid = true;
					state.legIndex = static_cast<int>(i);
					return state;
				}
				t -= travelTime;
				float dwell = leg.dwellAtDestination ? mod->dwellSeconds : 0.f;
				if (t < dwell) {
					state.valid = true;
					state.legIndex = static_cast<int>(i);
					state.dwelling = true;
					state.dwellRemainingSeconds = (dwell - t) / kGameSecondsPerRealSecond;
					return state;
				}
				t -= dwell;
			}
			return state;
		}
	}
	return state;
}

float Route::GetPhaseAdjust(const Vehicle* vehicle) const {
	auto it = phaseAdjust.find(vehicle);
	return it == phaseAdjust.end() ? 0.f : it->second;
}

bool Route::SyncVehicleProgress(const Vehicle* vehicle, int legIndex, float legFraction, bool dwelling, float dwellFraction) {
	if (!mod || !vehicle) return false;
	for (size_t lineIdx = 0; lineIdx < lines.size() && lineIdx < lineVehicles.size(); lineIdx++) {
		const vector<RouteLeg>& legs = lines[lineIdx];
		const vector<Vehicle*>& vehicles = lineVehicles[lineIdx];
		int vehicleCount = static_cast<int>(vehicles.size());
		for (int k = 0; k < vehicleCount; k++) {
			if (vehicles[k] != vehicle) continue;
			if (legIndex < 0 || legIndex >= static_cast<int>(legs.size())) return false;

			float period = 0.f;
			float target = 0.f; // 目标进度对应的时刻表时间
			for (size_t i = 0; i < legs.size(); i++) {
				const RouteLeg& leg = legs[i];
				float travelTime = (leg.edge && mod->speed > 0.f) ? leg.timeLength / mod->speed : 0.f;
				float dwell = leg.dwellAtDestination ? mod->dwellSeconds : 0.f;
				if (static_cast<int>(i) < legIndex) target += travelTime + dwell;
				else if (static_cast<int>(i) == legIndex) {
					target += dwelling ? travelTime + dwell * max(0.f, min(1.f, dwellFraction))
						: travelTime * max(0.f, min(1.f, legFraction));
				}
				period += travelTime + dwell;
			}
			if (period <= 0.f) return false;

			float phase = period * static_cast<float>(k) / static_cast<float>(vehicleCount);
			float adjust = fmodf(target - (lastElapsedSeconds + phase), period);
			phaseAdjust[vehicle] = adjust;
			return true;
		}
	}
	return false;
}

vector<int> Route::GetLineStopNodeIds(size_t lineIndex) const {
	vector<int> ids;
	if (lineIndex >= lines.size()) return ids;
	for (const RouteLeg& leg : lines[lineIndex]) {
		const StationRoadLink* link = leg.fromStation ? leg.fromStation->GetRoadLink(leg.fromInterfaceIndex) : nullptr;
		if (!link || !link->stationNode) return {};
		ids.push_back(link->stationNode->GetId());
	}
	return ids;
}
