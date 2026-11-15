#include "traffic/route.h"

#include "traffic/vehicle.h"
#include "map/map.h"
#include "map/building.h"
#include "common/utility.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <functional>
#include <queue>
#include <unordered_map>

using namespace std;

namespace {
	constexpr float PI = 3.14159265358979323846f;
	// 游戏内秒/真实秒：默认timeFlowRatio=2.0对应的系数，easeSeconds是真实秒，运动学全部按游戏秒算。
	constexpr float kGameSecondsPerRealSecond = 120.f;

	float ToDegrees(float radians) { return radians * (180.f / PI); }
}

TrackPath::~TrackPath() {
	if (!owned) return;
	for (Connection* connection : connections) delete connection;
}

shared_ptr<TrackPath> TrackPath::FromSegments(const vector<TrackSegment>& segments) {
	if (segments.empty()) return nullptr;
	auto path = make_shared<TrackPath>();
	path->segments = segments;
	path->owned = true;
	for (const TrackSegment& seg : segments) {
		Connection* conn = new Connection(Node("route", seg.x1, seg.y1, seg.z1), Node("route", seg.x2, seg.y2, seg.z2));
		conn->AddControls({ { Node("route_ctrl", seg.c1x, seg.c1y, seg.c1z), 1.f },
			{ Node("route_ctrl", seg.c2x, seg.c2y, seg.c2z), 1.f } });
		path->connections.push_back(conn);
		float segLength = conn->CalcDistance();
		path->lengths.push_back(segLength);
		path->length += segLength;
	}
	return path;
}

void TrackPath::Evaluate(float arcLength, float& outX, float& outY, float& outZ,
	float& outYawDegrees, float& outPitchDegrees) const {
	float remaining = arcLength;
	for (size_t i = 0; i < connections.size(); i++) {
		Connection* seg = connections[i];
		float segLength = lengths[i];
		bool isLast = (i + 1 == connections.size());
		if (remaining <= segLength || isLast) {
			float f = segLength > 0.f ? max(0.f, min(1.f, remaining / segLength)) : 0.f;
			Node point = seg->GetPoint(f);
			float dx, dy, dz;
			seg->GetTangent(f, dx, dy, dz);
			outX = point.GetX(); outY = point.GetY(); outZ = point.GetZ();
			outYawDegrees = ToDegrees(atan2f(dy, dx));
			outPitchDegrees = ToDegrees(atan2f(dz, sqrtf(dx * dx + dy * dy)));
			return;
		}
		remaining -= segLength;
	}
}

Route::Route(RouteFactory* factory, const string& modId, const string& name) :
	factory(factory), name(name) {
	mod = factory->CreateRoute(modId);
	if (mod) mod->SetProperty();
}

Route::~Route() {
	// 先放掉所有路径(借用的Connection不delete，自己建的由TrackPath析构delete)，再销毁mod。
	lines.clear();
	graphEdges.clear();
	trackPaths.clear();
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
float Route::GetTrackWidth() const { return mod ? mod->trackWidth : 0.f; }
int Route::GetLineVehicleCount(size_t lineIndex) const {
	if (lineIndex < lines.size() && lines[lineIndex].vehicleCount > 0) return lines[lineIndex].vehicleCount;
	return 1;
}

void Route::Build(Map* map, const vector<Station*>& stations) {
	if (!mod || !map) return;

	vector<RouteStationInfo> infos;
	stopStations.clear();
	for (Station* station : stations) {
		if (!station || station->GetStationType() != mod->stationType) continue;

		RouteStationInfo info;
		info.name = station->GetName();
		if (station->GetAttachedRoad()) info.roadName = station->GetAttachedRoad()->GetName();
		info.stationIndex = static_cast<int>(stopStations.size());
		stopStations.push_back(station);

		auto convert = [](const vector<StationPort>& ports) {
			vector<RoutePort> result;
			for (const StationPort& port : ports) {
				RoutePort out;
				out.x = port.x; out.y = port.y; out.z = port.z; out.dirX = port.dirX; out.dirY = port.dirY;
				result.push_back(out);
			}
			return result;
		};
		info.inlets = convert(station->GetInlets());
		info.outlets = convert(station->GetOutlets());
		info.stops = convert(station->GetStops());
		for (const RoutePort& port : info.stops) { info.x += port.x; info.y += port.y; }
		if (!info.stops.empty()) { info.x /= info.stops.size(); info.y /= info.stops.size(); }
		infos.push_back(std::move(info));
	}

	auto [mapSizeX, mapSizeY] = map->GetSize();
	mod->LayoutRoute(infos, mapSizeX, mapSizeY);

	// instantiate线路：让mod按地形把每条link的几何改成最终形状(过山抬升/挖隧道)，请求转发给
	// Map——和Map::InitRoadnet对路网的做法一致，所以Traffic必须在地形网格生成之前构建。读档不走这里
	// (地形已经烘焙，几何从存档读)，见traffic_save.md。
	if (mod->instantiate && !mod->useRoadnet) {
		auto getTerrain = [map](int x, int y) -> const string& { return map->GetTerrainRef(x, y); };
		auto getWater = [map](int x, int y) -> pair<bool, float> { return map->GetWater(x, y); };
		auto getHeight = [map](float x, float y) -> float { return map->SampleHeight(x, y); };
		mod->BuildTrack(getTerrain, getWater, getHeight);
		for (const TerrainHatch& hatch : mod->hatches) map->AddRouteHatch(hatch);
		map->ApplyTerrainFits(mod->terrainFits);
		// 拆掉轨道扫过的楼体/园区；带stationMod的建筑不拆(它的Station已经建好，轨道从上方过)。
		int demolished = map->DemolishAlongStrips(mod->demolitions, [](const Building* building) {
			return !building->GetStationMod().empty();
		});
		debugf("Route::Build: 线路%s拆除了%d座建筑。\n", name.c_str(), demolished);
	}

	Assemble(map);
}

const vector<Route::Pier>& Route::GetPiers() const { return piers; }

namespace {
	// 桥墩选位参数(地图单位，1=10m)。
	constexpr float PIER_SPACING_STEP = 0.5f;   // 墩距一档=5m
	constexpr float PIER_MIN_HEIGHT = 0.05f;    // 墩顶离地不足0.5m(贴地/隧道内/过山抬升段)不放墩
	constexpr float PIER_SAMPLE_STEP = 0.25f;   // 障碍线按这个间距采样成折线

	// 一条障碍折线：采样点+AABB(预筛)+半宽。
	struct PierObstacle {
		vector<pair<float, float>> points;
		float minX = FLT_MAX, minY = FLT_MAX, maxX = -FLT_MAX, maxY = -FLT_MAX;
		float halfWidth = 0.f;
	};

	PierObstacle SampleObstacle(const Connection& connection, float halfWidth) {
		PierObstacle obstacle;
		obstacle.halfWidth = halfWidth;
		float length = connection.CalcDistance();
		int count = max(1, static_cast<int>(ceilf(length / PIER_SAMPLE_STEP)));
		for (int i = 0; i <= count; i++) {
			Node point = connection.GetPoint(static_cast<float>(i) / static_cast<float>(count));
			float x = point.GetX(), y = point.GetY();
			obstacle.points.emplace_back(x, y);
			obstacle.minX = min(obstacle.minX, x); obstacle.maxX = max(obstacle.maxX, x);
			obstacle.minY = min(obstacle.minY, y); obstacle.maxY = max(obstacle.maxY, y);
		}
		return obstacle;
	}

	float PointSegmentDistSq(float px, float py, float ax, float ay, float bx, float by) {
		float dx = bx - ax, dy = by - ay;
		float lengthSq = dx * dx + dy * dy;
		float t = lengthSq > 1e-12f ? max(0.f, min(1.f, ((px - ax) * dx + (py - ay) * dy) / lengthSq)) : 0.f;
		float cx = ax + dx * t - px, cy = ay + dy * t - py;
		return cx * cx + cy * cy;
	}

	// (x,y)处半径radius的墩是否碰到这条障碍(到折线距离<radius+halfWidth)。
	bool PierHitsObstacle(const PierObstacle& obstacle, float x, float y, float radius) {
		float reach = radius + obstacle.halfWidth;
		if (x < obstacle.minX - reach || x > obstacle.maxX + reach || y < obstacle.minY - reach || y > obstacle.maxY + reach) return false;
		float reachSq = reach * reach;
		if (obstacle.points.size() == 1) {
			float dx = obstacle.points[0].first - x, dy = obstacle.points[0].second - y;
			return dx * dx + dy * dy < reachSq;
		}
		for (size_t i = 0; i + 1 < obstacle.points.size(); i++) {
			if (PointSegmentDistSq(x, y, obstacle.points[i].first, obstacle.points[i].second,
				obstacle.points[i + 1].first, obstacle.points[i + 1].second) < reachSq) return true;
		}
		return false;
	}
}

void Route::PlacePiers(Map* map, const vector<const Route*>& others) {
	piers.clear();
	if (!mod || !map || mod->piers.empty()) return;

	// 障碍：路网每条路(半宽=路总宽/2)，加所有instantiate的route轨道(半宽=轨道占地/2，含本route里不属于请求链的部分)。
	vector<PierObstacle> roadObstacles;
	for (const Road* road : map->GetRoads()) {
		if (road) roadObstacles.push_back(SampleObstacle(*road, road->GetTotalWidth() * 0.5f));
	}
	auto addRouteObstacles = [](const Route* route, vector<PierObstacle>& out, const vector<PierObstacle>* ownChains) {
		if (!route || !route->ShouldInstantiate() || route->UsesRoadnet()) return;
		for (const shared_ptr<TrackPath>& path : route->GetTrackPaths()) {
			if (!path) continue;
			for (const Connection* connection : path->connections) {
				PierObstacle obstacle = SampleObstacle(*connection, route->GetTrackWidth() * 0.5f);
				if (ownChains) {
					// 本请求自己那根轨道不算障碍：中点离请求链(中轴线)不到轨道半宽就是它(双轨各在中轴线两侧，不排除会挡住每一根墩)。
					const pair<float, float>& middle = obstacle.points[obstacle.points.size() / 2];
					bool own = false;
					for (const PierObstacle& chain : *ownChains) {
						if (PierHitsObstacle(chain, middle.first, middle.second, route->GetTrackWidth() * 0.5f)) { own = true; break; }
					}
					if (own) continue;
				}
				out.push_back(std::move(obstacle));
			}
		}
	};
	vector<PierObstacle> otherObstacles;
	for (const Route* other : others) if (other != this) addRouteObstacles(other, otherObstacles, nullptr);

	for (const PierRequest& request : mod->piers) {
		// 链是mod DLL分配的：Connection的弧长缓存是mutable、首次CalcDistance/GetPoint时在本模块(Core)里惰性resize，
		// 直接在mod的链上算会把Core分配的缓冲挂到mod堆的对象上，销毁时跨堆释放(STATUS_HEAP_CORRUPTION)。
		// 所以先在本模块深拷贝一份再算，见Dependence/map/general_geometry.md。
		vector<vector<Connection>> chains;
		for (const vector<Connection>& sourceChain : request.chains) {
			chains.emplace_back();
			for (const Connection& connection : sourceChain) chains.back().push_back(connection);
		}
		// 本请求所有链的折线(半宽0)，用来把本route里自己这条轨道(中轴线两侧的两根铁轨)从障碍里剔掉。
		vector<PierObstacle> chainObstacles;
		for (const vector<Connection>& chain : chains)
			for (const Connection& connection : chain) chainObstacles.push_back(SampleObstacle(connection, 0.f));
		vector<PierObstacle> ownRouteObstacles;
		addRouteObstacles(this, ownRouteObstacles, &chainObstacles);

		auto blocked = [&](float x, float y) {
			for (const PierObstacle& obstacle : roadObstacles) if (PierHitsObstacle(obstacle, x, y, request.radius)) return true;
			for (const PierObstacle& obstacle : otherObstacles) if (PierHitsObstacle(obstacle, x, y, request.radius)) return true;
			for (const PierObstacle& obstacle : ownRouteObstacles) if (PierHitsObstacle(obstacle, x, y, request.radius)) return true;
			return false;
		};

		float minSpacing = max(request.minSpacing, PIER_SPACING_STEP);
		float defaultSpacing = max(request.defaultSpacing, minSpacing);
		for (const vector<Connection>& chain : chains) {
			if (chain.empty()) continue;
			vector<float> cumulative(chain.size() + 1, 0.f);
			for (size_t i = 0; i < chain.size(); i++) cumulative[i + 1] = cumulative[i] + chain[i].CalcDistance();
			float total = cumulative.back();
			auto pointAt = [&](float s, float& x, float& y, float& z) {
				size_t i = 0;
				while (i + 1 < chain.size() && s > cumulative[i + 1]) i++;
				float length = cumulative[i + 1] - cumulative[i];
				Node point = chain[i].GetPoint(length > 1e-6f ? max(0.f, min(1.f, (s - cumulative[i]) / length)) : 0.f);
				x = point.GetX(); y = point.GetY(); z = point.GetZ();
			};

			// 链首是车站，视为已有支撑：从那里往后一根根放，剩余长度不足默认墩距时收尾(尾端也是车站/落地)。
			float s = 0.f;
			while (total - s > defaultSpacing) {
				// 候选墩距：默认→每次缩短一档到最短；都放不下再从默认每次加长一档，直到超出链尾。
				vector<float> candidates;
				for (float d = defaultSpacing; d >= minSpacing - 1e-4f; d -= PIER_SPACING_STEP) candidates.push_back(d);
				for (float d = defaultSpacing + PIER_SPACING_STEP; s + d <= total; d += PIER_SPACING_STEP) candidates.push_back(d);

				bool found = false;
				for (float d : candidates) {
					if (s + d > total) continue;
					float x, y, z;
					pointAt(s + d, x, y, z);
					bool needPier = z - map->SampleHeight(x, y) >= PIER_MIN_HEIGHT;
					if (needPier && blocked(x, y)) continue;
					if (needPier) {
						Pier pier;
						pier.x = x; pier.y = y; pier.topZ = z; pier.radius = request.radius; pier.mesh = request.mesh;
						piers.push_back(std::move(pier));
					}
					s += d;
					found = true;
					break;
				}
				if (!found) break;
			}
		}
	}
	debugf("Route::PlacePiers: 线路%s放了%d根桥墩。\n", name.c_str(), static_cast<int>(piers.size()));
}

int Route::GraphNodeIndex(const GraphNode& node) {
	string key = node.isExtern ? "e:" + to_string(node.index)
		: to_string(node.station) + ":" + to_string(static_cast<int>(node.kind)) + ":" + to_string(node.index);
	auto it = graphNodeIndexByKey.find(key);
	if (it != graphNodeIndexByKey.end()) return it->second;
	int index = static_cast<int>(graphNodes.size());
	graphNodes.push_back(node);
	graphEdges.emplace_back();
	graphNodeIndexByKey[key] = index;
	return index;
}

void Route::AddGraphEdge(int from, int to, shared_ptr<TrackPath> path, EdgeClass edgeClass, bool withinStation) {
	GraphEdge edge;
	edge.to = to;
	edge.path = path;
	edge.edgeClass = edgeClass;
	edge.withinStation = withinStation;
	graphEdges[from].push_back(std::move(edge));
	trackPaths.push_back(std::move(path));
}

bool Route::FindGraphPath(int src, int dst, vector<const GraphEdge*>& outEdges) const {
	outEdges.clear();
	int count = static_cast<int>(graphNodes.size());
	vector<float> dist(count, FLT_MAX);
	vector<const GraphEdge*> via(count, nullptr);
	vector<int> prev(count, -1);

	using Item = pair<float, int>;
	priority_queue<Item, vector<Item>, greater<Item>> heap;
	auto relax = [&](int from, float base) {
		for (const GraphEdge& edge : graphEdges[from]) {
			const GraphNode& target = graphNodes[edge.to];
			// 过站不停只能走站内直通连线：停靠位节点除了终点谁都不能经过。
			if (!target.isExtern && target.kind == PortKind::Stop && edge.to != dst) continue;
			float cost = base + edge.path->length;
			if (cost < dist[edge.to]) {
				dist[edge.to] = cost; via[edge.to] = &edge; prev[edge.to] = from;
				heap.push({ cost, edge.to });
			}
		}
	};
	// src==dst时(整圈只有一个停靠位)必须真的离开再回来，所以不预设dist[src]=0，只从src的出边开始松弛。
	relax(src, 0.f);
	while (!heap.empty()) {
		auto [cost, node] = heap.top();
		heap.pop();
		if (cost > dist[node]) continue;
		if (node == dst) break;
		relax(node, cost);
	}
	if (dist[dst] == FLT_MAX) return false;

	int cursor = dst;
	do {
		outEdges.push_back(via[cursor]);
		cursor = prev[cursor];
		if (outEdges.size() > graphNodes.size()) { outEdges.clear(); return false; } // 保护性退出
	} while (cursor != src);
	reverse(outEdges.begin(), outEdges.end());
	return true;
}

void Route::ComputeRunPhases(RouteRun& run) const {
	run.phases.clear();
	run.length = 0.f;
	run.travelTime = 0.f;
	size_t n = run.pieces.size();
	if (n == 0) return;

	float ease = max(mod->easeSeconds, 0.01f) * kGameSecondsPerRealSecond;
	float acc = max(mod->normalSpeed, 1e-6f) / ease;

	// 边界速度：两端为0，其余取相邻两块上限的较小者，再按加速度前向/后向收紧。
	vector<float> boundary(n + 1, 0.f);
	for (size_t i = 1; i < n; i++) boundary[i] = min(run.pieces[i - 1].speedCap, run.pieces[i].speedCap);
	for (size_t i = 0; i < n; i++) {
		boundary[i + 1] = min(boundary[i + 1], sqrtf(boundary[i] * boundary[i] + 2.f * acc * run.pieces[i].length));
	}
	for (size_t i = n; i-- > 0;) {
		boundary[i] = min(boundary[i], sqrtf(boundary[i + 1] * boundary[i + 1] + 2.f * acc * run.pieces[i].length));
	}

	float t = 0.f, s = 0.f;
	auto push = [&](float v0, float a, float duration) {
		if (duration <= 1e-6f) return;
		RunPhase phase;
		phase.t0 = t; phase.s0 = s; phase.v0 = v0; phase.acc = a; phase.duration = duration;
		run.phases.push_back(phase);
		t += duration;
		s += v0 * duration + 0.5f * a * duration * duration;
	};
	for (size_t i = 0; i < n; i++) {
		float length = run.pieces[i].length;
		float cap = max(run.pieces[i].speedCap, 1e-3f);
		float vin = boundary[i], vout = boundary[i + 1];
		float peak = min(cap, sqrtf(max(0.f, (2.f * acc * length + vin * vin + vout * vout) * 0.5f)));
		peak = max(peak, max(vin, vout));
		float accelDistance = (peak * peak - vin * vin) / (2.f * acc);
		float decelDistance = (peak * peak - vout * vout) / (2.f * acc);
		float cruiseDistance = max(0.f, length - accelDistance - decelDistance);
		push(vin, acc, (peak - vin) / acc);
		if (peak > 1e-6f) push(peak, 0.f, cruiseDistance / peak);
		push(peak, -acc, (peak - vout) / acc);
	}
	run.travelTime = t;
	run.length = 0.f;
	for (const RoutePiece& piece : run.pieces) run.length += piece.length;
}

float Route::RunArcAtTime(const RouteRun& run, float t) {
	if (run.phases.empty()) return 0.f;
	if (t <= 0.f) return 0.f;
	if (t >= run.travelTime) return run.length;
	size_t lo = 0, hi = run.phases.size() - 1;
	while (lo < hi) {
		size_t mid = (lo + hi + 1) / 2;
		if (run.phases[mid].t0 <= t) lo = mid; else hi = mid - 1;
	}
	const RunPhase& phase = run.phases[lo];
	float dt = t - phase.t0;
	return min(run.length, phase.s0 + phase.v0 * dt + 0.5f * phase.acc * dt * dt);
}

float Route::RunTimeAtArc(const RouteRun& run, float s) {
	if (run.phases.empty() || s <= 0.f) return 0.f;
	if (s >= run.length) return run.travelTime;
	for (const RunPhase& phase : run.phases) {
		float end = phase.s0 + phase.v0 * phase.duration + 0.5f * phase.acc * phase.duration * phase.duration;
		if (s > end) continue;
		float ds = s - phase.s0;
		float dt;
		if (fabsf(phase.acc) < 1e-9f) dt = phase.v0 > 1e-9f ? ds / phase.v0 : 0.f;
		else dt = (-phase.v0 + sqrtf(max(0.f, phase.v0 * phase.v0 + 2.f * phase.acc * ds))) / phase.acc;
		return phase.t0 + max(0.f, min(dt, phase.duration));
	}
	return run.travelTime;
}

void Route::EvaluateRun(const RouteRun& run, float t, float& x, float& y, float& z, float& yaw, float& pitch) const {
	float s = RunArcAtTime(run, t);
	float before = 0.f;
	for (size_t i = 0; i < run.pieces.size(); i++) {
		const RoutePiece& piece = run.pieces[i];
		if (s <= before + piece.length || i + 1 == run.pieces.size()) {
			piece.path->Evaluate(max(0.f, min(s - before, piece.path->length)), x, y, z, yaw, pitch);
			return;
		}
		before += piece.length;
	}
}

void Route::FinalizeLine(RouteLine& line) const {
	line.period = 0.f;
	for (const RouteRun& run : line.runs) line.period += run.travelTime + mod->dwellSeconds;
}

bool Route::PlanGraphLine(const vector<int>& waypoints, RouteLine& outLine) const {
	auto isStop = [&](int node) { return !graphNodes[node].isExtern && graphNodes[node].kind == PortKind::Stop; };

	size_t n = waypoints.size();
	size_t first = n;
	for (size_t i = 0; i < n; i++) if (isStop(waypoints[i])) { first = i; break; }
	if (first == n) return false;

	vector<int> rotated;
	for (size_t i = 0; i < n; i++) rotated.push_back(waypoints[(first + i) % n]);
	vector<size_t> stopPositions;
	for (size_t i = 0; i < n; i++) if (isStop(rotated[i])) stopPositions.push_back(i);

	for (size_t k = 0; k < stopPositions.size(); k++) {
		size_t fromPos = stopPositions[k];
		size_t toPos = stopPositions[(k + 1) % stopPositions.size()];
		size_t steps = (toPos + n - fromPos) % n;
		if (steps == 0) steps = n;

		vector<const GraphEdge*> edges;
		for (size_t j = 0; j < steps; j++) {
			vector<const GraphEdge*> hop;
			int a = rotated[(fromPos + j) % n], b = rotated[(fromPos + j + 1) % n];
			if (!FindGraphPath(a, b, hop)) return false;
			edges.insert(edges.end(), hop.begin(), hop.end());
		}

		RouteRun run;
		const GraphNode& startNode = graphNodes[rotated[fromPos]];
		const GraphNode& endNode = graphNodes[rotated[toPos]];
		run.fromStation = stopStations[startNode.station]; run.fromStop = startNode.index;
		run.toStation = stopStations[endNode.station]; run.toStop = endNode.index;

		// 速度分类：起止同站且途经的站间连线全是同站转场 -> 整段按同站速度；否则按正常速度，穿过中间站的直通连线降到通过速度。
		bool sameStation = startNode.station == endNode.station;
		for (const GraphEdge* edge : edges) {
			if (edge->edgeClass == EDGE_LINK && !edge->withinStation) sameStation = false;
		}
		float base = sameStation ? mod->sameStationSpeed : mod->normalSpeed;
		for (const GraphEdge* edge : edges) {
			RoutePiece piece;
			piece.path = edge->path;
			piece.length = edge->path->length;
			piece.speedCap = edge->edgeClass == EDGE_THROUGH ? min(base, mod->passSpeed) : base;
			if (piece.length > 1e-4f) run.pieces.push_back(std::move(piece));
		}
		ComputeRunPhases(run);
		outLine.runs.push_back(std::move(run));
	}
	FinalizeLine(outLine);
	return !outLine.runs.empty();
}

bool Route::BuildRoadRun(Map* map, Station* from, int fromStop, Station* to, int toStop, RouteRun& outRun) const {
	if (!from || !to) return false;
	const StationRoadLink* linkA = from->EnsureRoadLink(map, fromStop);
	const StationRoadLink* linkB = to->EnsureRoadLink(map, toStop);
	if (!linkA || !linkB) return false;

	auto path = make_shared<TrackPath>();
	if (linkA->sourceEdge == linkB->sourceEdge && linkA->sourceEdge && linkA->t < linkB->t) {
		// 同车道特例：A、B在同一段原车道上且A在上游，直接用两个stationNode间的直线弦，
		// 不绕一圈FindVehiclePath，见route.md"同车道特例"一节。
		path->connections.push_back(new Connection(*linkA->stationNode, *linkB->stationNode));
		path->owned = true;
	} else {
		vector<Connection*> found = map->FindVehiclePath(linkA->laneTo->GetId(), linkB->laneFrom->GetId());
		path->connections.push_back(linkA->outEdge);
		for (Connection* seg : found) path->connections.push_back(seg);
		path->connections.push_back(linkB->inEdge);
		path->owned = false; // 全部借用Station/Map已有的Connection，不持有
	}
	for (Connection* conn : path->connections) {
		float segLength = conn->CalcDistance();
		path->lengths.push_back(segLength);
		path->length += segLength;
	}

	outRun.fromStation = from; outRun.fromStop = fromStop;
	outRun.toStation = to; outRun.toStop = toStop;
	RoutePiece piece;
	piece.path = path;
	piece.length = path->length;
	piece.speedCap = mod->normalSpeed;
	outRun.pieces.push_back(std::move(piece));
	ComputeRunPhases(outRun);
	return true;
}

void Route::Assemble(Map* map) {
	lines.clear();
	graphNodes.clear();
	graphEdges.clear();
	graphNodeIndexByKey.clear();
	trackPaths.clear();
	if (!mod) return;

	unordered_map<string, int> stationIndexByName;
	for (size_t i = 0; i < stopStations.size(); i++) {
		if (stopStations[i]) stationIndexByName[stopStations[i]->GetName()] = static_cast<int>(i);
	}

	if (mod->useRoadnet) {
		// 路网线路(公交)：只有停靠顺序，相邻两站之间的路径由Core用路网车辆寻路图拼。
		for (size_t modLineIdx = 0; modLineIdx < mod->lines.size(); modLineIdx++) {
		const vector<RouteLineEntry>& entries = mod->lines[modLineIdx];
			vector<pair<Station*, int>> stops;
			bool valid = !entries.empty();
			for (const RouteLineEntry& entry : entries) {
				auto it = stationIndexByName.find(entry.name);
				Station* station = it != stationIndexByName.end() ? stopStations[it->second] : nullptr;
				if (!station || entry.stopIndex < 0 || entry.stopIndex >= static_cast<int>(station->GetStops().size())) {
					debugf("Route::Assemble: 线路%s的一条线路引用了不存在的站点/停靠位(%s,%d)，整条线路丢弃。\n",
						name.c_str(), entry.name.c_str(), entry.stopIndex);
					valid = false;
					break;
				}
				stops.emplace_back(station, entry.stopIndex);
			}
			if (!valid) continue;

			RouteLine line;
			for (size_t i = 0; i < stops.size() && valid; i++) {
				RouteRun run;
				const auto& a = stops[i];
				const auto& b = stops[(i + 1) % stops.size()];
				if (!BuildRoadRun(map, a.first, a.second, b.first, b.second, run)) {
					debugf("Route::Assemble: 线路%s里%s->%s在路网上不可达，整条线路丢弃。\n",
						name.c_str(), a.first->GetName().c_str(), b.first->GetName().c_str());
					valid = false;
					break;
				}
				line.runs.push_back(std::move(run));
			}
			if (!valid) continue;
			FinalizeLine(line);
			line.vehicleCount = modLineIdx < mod->lineVehicleCounts.size() ? mod->lineVehicleCounts[modLineIdx] : 0;
			lines.push_back(std::move(line));
		}
		return;
	}

	// ---- 非路网线路：建寻路图 ----
	for (size_t i = 0; i < mod->externs.size(); i++) {
		GraphNode node;
		node.isExtern = true;
		node.index = static_cast<int>(i);
		GraphNodeIndex(node);
	}
	unordered_map<string, int> externIndexByName;
	for (size_t i = 0; i < mod->externs.size(); i++) externIndexByName[mod->externs[i].name] = static_cast<int>(i);

	for (size_t stationIndex = 0; stationIndex < stopStations.size(); stationIndex++) {
		Station* station = stopStations[stationIndex];
		if (!station) continue;
		for (const StationConnection& connection : station->GetConnections()) {
			shared_ptr<TrackPath> path = TrackPath::FromSegments(connection.segments);
			if (!path) {
				debugf("Route::Assemble: 站点%s有一条站内连线没有几何，跳过。\n", station->GetName().c_str());
				continue;
			}
			GraphNode from, to;
			from.station = to.station = static_cast<int>(stationIndex);
			from.kind = connection.fromKind; from.index = connection.fromIndex;
			to.kind = connection.toKind; to.index = connection.toIndex;
			EdgeClass edgeClass = connection.toKind == PortKind::Stop ? EDGE_STATION_IN
				: connection.fromKind == PortKind::Stop ? EDGE_STATION_OUT : EDGE_THROUGH;
			int fromIndex = GraphNodeIndex(from);
			int toIndex = GraphNodeIndex(to);
			AddGraphEdge(fromIndex, toIndex, std::move(path), edgeClass, false);
		}
	}

	auto endpointNode = [&](const RouteEndpoint& endpoint) -> int {
		GraphNode node;
		if (endpoint.kind == RouteEndpoint::Extern) {
			auto it = externIndexByName.find(endpoint.externName);
			if (it == externIndexByName.end()) return -1;
			node.isExtern = true;
			node.index = it->second;
			return GraphNodeIndex(node);
		}
		if (endpoint.station < 0 || endpoint.station >= static_cast<int>(stopStations.size()) || !stopStations[endpoint.station]) return -1;
		const Station* station = stopStations[endpoint.station];
		const vector<StationPort>& ports = endpoint.kind == RouteEndpoint::Outlet ? station->GetOutlets() : station->GetInlets();
		if (endpoint.portIndex < 0 || endpoint.portIndex >= static_cast<int>(ports.size())) return -1;
		node.station = endpoint.station;
		node.kind = endpoint.kind == RouteEndpoint::Outlet ? PortKind::Outlet : PortKind::Inlet;
		node.index = endpoint.portIndex;
		return GraphNodeIndex(node);
	};
	for (const RouteLinkSpec& link : mod->links) {
		int from = endpointNode(link.from), to = endpointNode(link.to);
		shared_ptr<TrackPath> path = TrackPath::FromSegments(link.segments);
		if (from < 0 || to < 0 || !path) {
			debugf("Route::Assemble: 线路%s的一条站间连线端点无效或没有几何，跳过。\n", name.c_str());
			continue;
		}
		AddGraphEdge(from, to, std::move(path), EDGE_LINK, link.withinStation);
	}

	// ---- 规划每条线路 ----
	for (size_t modLineIdx = 0; modLineIdx < mod->lines.size(); modLineIdx++) {
		const vector<RouteLineEntry>& entries = mod->lines[modLineIdx];
		vector<int> waypoints;
		bool valid = !entries.empty();
		for (const RouteLineEntry& entry : entries) {
			GraphNode node;
			if (entry.stopIndex >= 0) {
				auto it = stationIndexByName.find(entry.name);
				if (it == stationIndexByName.end() || entry.stopIndex >= static_cast<int>(stopStations[it->second]->GetStops().size())) {
					debugf("Route::Assemble: 线路%s的一条线路引用了不存在的站点/停靠位(%s,%d)，整条线路丢弃。\n",
						name.c_str(), entry.name.c_str(), entry.stopIndex);
					valid = false;
					break;
				}
				node.station = it->second;
				node.kind = PortKind::Stop;
				node.index = entry.stopIndex;
			} else {
				auto it = externIndexByName.find(entry.name);
				if (it == externIndexByName.end()) {
					debugf("Route::Assemble: 线路%s的一条线路引用了不存在的extern点(%s)，整条线路丢弃。\n", name.c_str(), entry.name.c_str());
					valid = false;
					break;
				}
				node.isExtern = true;
				node.index = it->second;
			}
			waypoints.push_back(GraphNodeIndex(node));
		}
		if (!valid) continue;

		RouteLine line;
		if (!PlanGraphLine(waypoints, line)) {
			debugf("Route::Assemble: 线路%s的一条线路在寻路图里不可达(没有停靠位或站间连线不通)，整条线路丢弃。\n", name.c_str());
			continue;
		}
		line.vehicleCount = modLineIdx < mod->lineVehicleCounts.size() ? mod->lineVehicleCounts[modLineIdx] : 0;
		lines.push_back(std::move(line));
	}
}

const vector<Route::RouteLine>& Route::GetLines() const { return lines; }

const vector<shared_ptr<TrackPath>>& Route::GetTrackPaths() const { return trackPaths; }

void Route::AddVehicleToLine(size_t lineIndex, Vehicle* vehicle) {
	if (lineIndex >= lineVehicles.size()) lineVehicles.resize(lineIndex + 1);
	lineVehicles[lineIndex].push_back(vehicle);
}

void Route::DriveVehicle(Vehicle* vehicle, const RouteLine& line, float localTime, Station** outDwellStation) const {
	if (outDwellStation) *outDwellStation = nullptr;
	if (!vehicle || !mod || line.runs.empty()) return;

	float t = localTime;
	for (const RouteRun& run : line.runs) {
		bool driving = t < run.travelTime;
		bool dwelling = !driving && t - run.travelTime < mod->dwellSeconds;
		if (!driving && !dwelling) { t -= run.travelTime + mod->dwellSeconds; continue; }

		float x = 0.f, y = 0.f, z = 0.f, yaw = 0.f, pitch = 0.f;
		EvaluateRun(run, driving ? t : run.travelTime, x, y, z, yaw, pitch);
		vehicle->SetTransform(x, y, z, yaw);
		// 停靠时车身放平；行驶时instantiate/tiltAlongPath的线路跟着路径俯仰。
		vehicle->SetPitch((driving && (mod->instantiate || mod->tiltAlongPath)) ? pitch : 0.f);
		if (dwelling && outDwellStation) *outDwellStation = run.toStation;
		return;
	}
}

void Route::Update(float elapsedSeconds) {
	if (!mod) return;
	lastElapsedSeconds = elapsedSeconds;

	for (size_t lineIdx = 0; lineIdx < lines.size(); lineIdx++) {
		const RouteLine& line = lines[lineIdx];
		if (lineIdx >= lineVehicles.size() || line.period <= 0.f) continue;

		const vector<Vehicle*>& vehicles = lineVehicles[lineIdx];
		int vehicleCount = static_cast<int>(vehicles.size());
		for (int k = 0; k < vehicleCount; k++) {
			float phase = line.period * static_cast<float>(k) / static_cast<float>(vehicleCount);
			Vehicle* vehicle = vehicles[k];
			float localTime = fmodf(elapsedSeconds + phase + GetPhaseAdjust(vehicle), line.period);
			if (localTime < 0.f) localTime += line.period;
			// 物理段（UE层的物理公交+Controller在开）：位姿和停站事件都由UE层负责，这里不碰，
			// 否则时刻表会和实车打架、重复发进站/离站事件。交还时刻表时ResetStopState重新观察。
			if (vehicle->IsPhysicsDriven()) continue;
			Station* dwellStation = nullptr;
			DriveVehicle(vehicle, line, localTime, &dwellStation);

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

	for (size_t lineIdx = 0; lineIdx < lines.size() && lineIdx < lineVehicles.size(); lineIdx++) {
		const RouteLine& line = lines[lineIdx];
		const vector<Vehicle*>& vehicles = lineVehicles[lineIdx];
		int vehicleCount = static_cast<int>(vehicles.size());
		for (int k = 0; k < vehicleCount; k++) {
			if (vehicles[k] != vehicle) continue;
			if (line.period <= 0.f) return state;

			float phase = line.period * static_cast<float>(k) / static_cast<float>(vehicleCount);
			float t = fmodf(lastElapsedSeconds + phase + GetPhaseAdjust(vehicle), line.period);
			if (t < 0.f) t += line.period;

			state.line = static_cast<int>(lineIdx);
			state.speedMapPerRealSecond = mod->normalSpeed * kGameSecondsPerRealSecond;
			for (size_t i = 0; i < line.runs.size(); i++) {
				const RouteRun& run = line.runs[i];
				if (t < run.travelTime) {
					state.valid = true;
					state.legIndex = static_cast<int>(i);
					return state;
				}
				t -= run.travelTime;
				if (t < mod->dwellSeconds) {
					state.valid = true;
					state.legIndex = static_cast<int>(i);
					state.dwelling = true;
					state.dwellRemainingSeconds = (mod->dwellSeconds - t) / kGameSecondsPerRealSecond;
					return state;
				}
				t -= mod->dwellSeconds;
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
		const RouteLine& line = lines[lineIdx];
		const vector<Vehicle*>& vehicles = lineVehicles[lineIdx];
		int vehicleCount = static_cast<int>(vehicles.size());
		for (int k = 0; k < vehicleCount; k++) {
			if (vehicles[k] != vehicle) continue;
			if (legIndex < 0 || legIndex >= static_cast<int>(line.runs.size())) return false;
			if (line.period <= 0.f) return false;

			float target = 0.f; // 目标进度对应的时刻表时间
			for (int i = 0; i < legIndex; i++) target += line.runs[i].travelTime + mod->dwellSeconds;
			const RouteRun& run = line.runs[legIndex];
			if (dwelling) target += run.travelTime + mod->dwellSeconds * max(0.f, min(1.f, dwellFraction));
			else target += RunTimeAtArc(run, run.length * max(0.f, min(1.f, legFraction)));

			float phase = line.period * static_cast<float>(k) / static_cast<float>(vehicleCount);
			float adjust = fmodf(target - (lastElapsedSeconds + phase), line.period);
			phaseAdjust[vehicle] = adjust;
			return true;
		}
	}
	return false;
}

vector<int> Route::GetLineStopNodeIds(size_t lineIndex) const {
	vector<int> ids;
	if (lineIndex >= lines.size()) return ids;
	for (const RouteRun& run : lines[lineIndex].runs) {
		const StationRoadLink* link = run.fromStation ? run.fromStation->GetRoadLink(run.fromStop) : nullptr;
		if (!link || !link->stationNode) return {};
		ids.push_back(link->stationNode->GetId());
	}
	return ids;
}
