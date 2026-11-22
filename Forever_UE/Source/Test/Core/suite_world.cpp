#include "suites.h"

#include "test_framework.h"

#include "map/map.h"
#include "map/building.h"
#include "map/room.h"
#include "map/roadnet.h"
#include "populace/populace.h"
#include "populace/citizen.h"
#include "populace/household.h"
#include "society/society.h"
#include "society/organization.h"
#include "society/job.h"
#include "society/ledger.h"
#include "traffic/traffic.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <vector>


using namespace std;

namespace {
	using Graph = NavGraph;

	unordered_set<int> Bfs(const Graph& graph, const unordered_map<int, vector<int>>& reverse, int start, bool forward) {
		unordered_set<int> seen = { start };
		vector<int> stack = { start };
		while (!stack.empty()) {
			const int current = stack.back();
			stack.pop_back();
			if (forward) {
				auto it = graph.find(current);
				if (it == graph.end()) continue;
				for (const auto& [to, conn] : it->second) if (seen.insert(to).second) stack.push_back(to);
			}
			else {
				auto it = reverse.find(current);
				if (it == reverse.end()) continue;
				for (int from : it->second) if (seen.insert(from).second) stack.push_back(from);
			}
		}
		return seen;
	}

	// 机场停机坪：layout的4个parkings矩形各只切一个机位(AssignParking spacing=-1)，每个机位停一架飞机；
	// 恰好2个机位=2个矩形各一份，没有被切成多份。
	void CheckAirportBays(TestWorld& world) {
		TestRun& run = TestRun::Get();
		int airports = 0, badCount = 0, unparked = 0;
		for (const auto& [name, building] : world.map->GetBuildings()) {
			if (string(building->GetType()) != "building_airport") continue;
			airports++;
			int planeSlots = 0;
			for (Room* room : building->GetRooms()) {
				for (ParkingSlot* slot : room->GetParkingSlots()) {
					if (!slot->Accepts("plane")) continue;
					planeSlots++;
					if (!slot->parkedVehicle) unparked++;
				}
			}
			if (planeSlots != 2) badCount++;
		}
		run.Metric("airports", airports, "");
		if (airports == 0) return;
		run.Check(badCount == 0, "every airport has exactly 2 plane bays (one slot per parking rect)", to_string(badCount) + " of " + to_string(airports) + " airports differ");
		run.Check(unparked == 0, "every plane bay has a parked plane", to_string(unparked) + " empty");
	}

	// 港口：layout的1个parkings矩形只切一个船位(AssignStockedParking spacing=-1)，开局停一艘船。
	// 另外通用检查：所有stocked车位(机场飞机/港口船)开局都停了车(Traffic::InitStockedVehicles)，没有stocked的车位不会被它抢先占用
	// ——由人/组织拥有的车才走AssignVehicles，所以这里只验stocked那一侧。
	void CheckPortBoats(TestWorld& world) {
		TestRun& run = TestRun::Get();
		int ports = 0, badCount = 0, unparked = 0, stockedSlots = 0, stockedUnparked = 0;
		for (const auto& [name, building] : world.map->GetBuildings()) {
			const bool isPort = string(building->GetType()) == "building_port";
			if (isPort) ports++;
			int boatSlots = 0;
			for (Room* room : building->GetRooms()) {
				for (ParkingSlot* slot : room->GetParkingSlots()) {
					if (slot->stocked) {
						stockedSlots++;
						if (!slot->parkedVehicle) stockedUnparked++;
					}
					if (isPort && slot->Accepts("boat")) {
						boatSlots++;
						if (!slot->parkedVehicle) unparked++;
					}
				}
			}
			if (isPort && boatSlots != 8) badCount++;
		}
		run.Metric("ports", ports, "");
		run.Metric("stocked slots", stockedSlots, "");
		run.Check(stockedUnparked == 0, "every stocked parking slot has a parked vehicle", to_string(stockedUnparked) + " empty");
		if (ports == 0) return;
		run.Check(badCount == 0, "every port has exactly 8 boat slots (one slot per parking rect)", to_string(badCount) + " of " + to_string(ports) + " ports differ");
		run.Check(unparked == 0, "every boat slot has a parked boat", to_string(unparked) + " empty");
	}

	// 车行导航图：每个停车位的导航节点必须和参照点双向连通(隧道出口方向、停车位单向的历史坑)。
	void CheckVehicleConnectivity(TestWorld& world) {
		TestRun& run = TestRun::Get();
		const Graph& graph = world.map->GetVehicleNavGraph();
		unordered_map<int, vector<int>> reverse;
		for (const auto& [from, edges] : graph) for (const auto& [to, conn] : edges) reverse[to].push_back(from);

		int reference = -1;
		int slotCount = 0, withoutNode = 0;
		vector<int> slotNodes;
		for (const auto& [name, building] : world.map->GetBuildings()) {
			for (Room* room : building->GetRooms()) {
				for (ParkingSlot* slot : room->GetParkingSlots()) {
					slotCount++;
					if (!slot->navigationNode) { withoutNode++; continue; }
					if (!slot->Accepts("car")) continue; // 机场的飞机位不接路网
					slotNodes.push_back(slot->navigationNode->GetId());
					if (reference < 0) reference = slot->navigationNode->GetId();
				}
			}
		}
		run.Metric("parking slots", slotCount, "");
		run.Check(withoutNode == 0, "every parking slot has a navigation node", to_string(withoutNode) + " of " + to_string(slotCount) + " without");
		if (reference < 0) {
			run.Check(slotCount == 0, "world has parking slots to test connectivity", "no slot with a navigation node");
			return;
		}
		const auto forward = Bfs(graph, reverse, reference, true);
		const auto backward = Bfs(graph, reverse, reference, false);
		int unreachable = 0, oneWay = 0;
		for (int node : slotNodes) {
			const bool f = forward.count(node) > 0, b = backward.count(node) > 0;
			if (!f || !b) unreachable++;
			if (f != b) oneWay++;
		}
		// 诊断：列出前若干个不连通车位的建筑类型/地址/出入边数，方便定位是哪类建筑的接路出了问题。
		int listed = 0;
		for (const auto& [name, building] : world.map->GetBuildings()) {
			for (Room* room : building->GetRooms()) {
				for (ParkingSlot* slot : room->GetParkingSlots()) {
					if (!slot->navigationNode || !slot->Accepts("car") || listed >= 10) continue;
					const int node = slot->navigationNode->GetId();
					const bool f = forward.count(node) > 0, b = backward.count(node) > 0;
					if (f && b) continue;
					listed++;
					const auto out = graph.find(node);
					const auto in = reverse.find(node);
					run.Note("disconnected slot: " + string(building->GetType()) + " " + name + " @ " + room->GetAddress() + " node " + to_string(node)
						+ " out=" + to_string(out == graph.end() ? 0 : out->second.size()) + " in=" + to_string(in == reverse.end() ? 0 : in->second.size())
						+ (f ? " reachable-from-ref" : " not-reachable-from-ref") + (b ? " can-reach-ref" : " cannot-reach-ref"));
				}
			}
		}
		run.Metric("vehicle nav nodes", static_cast<double>(graph.size()), "");
		run.Metric("parking slots not bidirectionally connected", unreachable, "");
		run.Check(unreachable == 0, "every parking slot is reachable from and back to the reference slot",
			to_string(unreachable) + " of " + to_string(slotNodes.size()) + " slots (" + to_string(oneWay) + " one-way)");
	}


	// 曲线路上的车道导航线要跟着路的弯走(不是两个锚点之间的弦)：沿每条有控制点的路取若干t，
	// 车道中心线上的点(GetLaneSegmentAt给出)到所属车道边的最近距离要很小。
	void CheckCurvedLaneEdges(TestWorld& world) {
		TestRun& run = TestRun::Get();
		int roads = 0, samples = 0, bad = 0, curvedEdges = 0;
		float worst = 0.f;
		unordered_set<int> externIds;
		for (const Node* e : world.map->GetExterns()) externIds.insert(e->GetId());
		for (Road* road : world.map->GetRoads()) {
			if (road->GetControls().empty() || road->GetVehicleLanes(0).empty()) continue;
			// 接在城外连接点上的路：端点处所有车道收成同一个点，那一头的车道边不在车道中心线上。
			if (externIds.count(road->GetStart().GetId()) || externIds.count(road->GetEnd().GetId())) continue;
			const float len = road->CalcDistance();
			if (len < 3.f) continue; // 极短的路两端锚点几乎贴在一起，没有"弯"可言
			roads++;
			const float tMin = road->GetSetback(true) / len + 0.02f, tMax = 1.f - road->GetSetback(false) / len - 0.02f; // 路口收缩区里没有车道边(路口连线接管)
			for (int i = 1; i < 20; i++) {
				const float t = i / 20.f;
				// 只看路中间：个别路口端的锚点不在路端点上(沿路方向错开)，端部附近的偏差来自锚点位置，不是曲线拟合。
				if (t < tMin || t > tMax || t < 0.15f || t > 0.75f) continue;
				Node p = road->GetPoint(t);
				float dx, dy, dz;
				road->GetTangent(t, dx, dy, dz);
				Map::LaneSegment seg;
				if (!world.map->GetLaneSegmentAt(road, p.GetX(), p.GetY(), dx, dy, seg) || !seg.edge) continue;
				samples++;
				float best = 1e9f;
				for (int k = 0; k <= 64; k++) {
					Node q = seg.edge->GetPoint(k / 64.f);
					best = std::min(best, std::hypot(q.GetX() - seg.worldX, q.GetY() - seg.worldY));
				}
				worst = std::max(worst, best);
				if (best > 0.15f) { bad++; if (bad <= 8) run.Note("off-lane sample road " + road->GetName() + " len=" + to_string(road->CalcDistance()) + " t=" + to_string(t) + " dev=" + to_string(best)); }
				if (!seg.edge->GetControls().empty()) curvedEdges++;
			}
		}
		run.Metric("curved roads", roads, "");
		run.Metric("curved lane edge worst deviation", worst, "");
		run.Check(bad == 0, "vehicle lane edges follow curved roads", to_string(bad) + " of " + to_string(samples) + " samples off the lane centerline (worst " + to_string(worst) + ")");
		if (roads > 0) run.Check(curvedEdges > 0, "curved roads have curved lane edges");
		// 行人车道边同样带控制点(行人规划PlanPedestrianRoute沿边采样，不再只连节点)。
		int pedCurved = 0;
		for (const auto& [from, outs] : world.map->GetPedestrianNavGraph()) for (const auto& [to, conn] : outs) if (conn && !conn->GetControls().empty()) pedCurved++;
		run.Metric("pedestrian nav edges with controls", pedCurved, "");
		if (roads > 0) run.Check(pedCurved > 0, "curved roads have curved pedestrian lane edges");
	}

	// 车行导航图每条边的采样点必须是有限数且落在地图范围内附近：Bezier权重和接近0时GetPoint会除出天文数字，
	// 公交沿这种边铺路径时 BuildPath 会细分上亿段而卡死/内存暴涨(见 Controller/VehicleCommandController.cpp 的诊断日志)。
	void CheckVehicleEdgeGeometry(TestWorld& world) {
		TestRun& run = TestRun::Get();
		const Graph& graph = world.map->GetVehicleNavGraph();
		const float limit = 1.0e6f; // 地图单位；1024元素图的边长远小于这个
		int edges = 0, bad = 0, listed = 0;
		for (const auto& [from, outs] : graph) {
			for (const auto& [to, conn] : outs) {
				if (!conn) continue;
				edges++;
				bool ok = true;
				float worst = 0.f;
				const int steps = 16;
				for (int i = 0; i <= steps; i++) {
					Node p = conn->GetPoint(static_cast<float>(i) / steps);
					const float x = p.GetX(), y = p.GetY(), z = p.GetZ();
					if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) || std::fabs(x) > limit || std::fabs(y) > limit || std::fabs(z) > limit) {
						ok = false;
						worst = std::max(worst, std::max(std::fabs(x), std::fabs(y)));
					}
				}
				if (!ok) {
					bad++;
					if (listed++ < 10) {
						const Node s = conn->GetStart(), e = conn->GetEnd();
						run.Note("bad vehicle edge " + to_string(from) + "->" + to_string(to) + " start(" + to_string(s.GetX()) + "," + to_string(s.GetY()) + ") end(" + to_string(e.GetX()) + "," + to_string(e.GetY())
							+ ") controls=" + to_string(conn->GetControls().size()) + " worst=" + to_string(worst));
					}
				}
			}
		}
		run.Metric("vehicle nav edges", edges, "");
		run.Check(bad == 0, "every vehicle nav edge samples to finite, in-range points", to_string(bad) + " of " + to_string(edges) + " edges");
	}


	// 公交/线路站间的规划路径(含站点辅助边)每个点都要有限且在地图范围内、相邻点不能离谱地远：
	// 控制器沿坏点细分会卡死，见 CheckVehicleEdgeGeometry 的说明。
	void CheckRouteLegGeometry(TestWorld& world) {
		TestRun& run = TestRun::Get();
		int legs = 0, bad = 0, failedPlans = 0, listed = 0;
		for (const auto& [routeName, route] : world.traffic->GetRoutes()) {
			if (!route) continue;
			for (size_t line = 0; line < route->GetLines().size(); line++) {
				const vector<int> ids = route->GetLineStopNodeIds(line);
				if (ids.size() < 2) continue;
				for (size_t i = 0; i < ids.size(); i++) {
					const int from = ids[i], to = ids[(i + 1) % ids.size()];
					if (from == to) continue;
					legs++;
					const Map::VehicleRoutePlan plan = world.map->PlanVehicleLeg(from, to);
					if (plan.status != Map::VehicleRoutePlan::OK) { failedPlans++; continue; }
					bool ok = true;
					float worst = 0.f;
					for (size_t k = 0; k < plan.points.size(); k++) {
						const auto& p = plan.points[k];
						if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) || std::fabs(p.x) > 1.0e6f || std::fabs(p.y) > 1.0e6f) { ok = false; worst = std::max(worst, std::max(std::fabs(p.x), std::fabs(p.y))); }
						if (k > 0) {
							const auto& q = plan.points[k - 1];
							const float d = std::hypot(p.x - q.x, p.y - q.y);
							if (!(d < 5000.f)) { ok = false; worst = std::max(worst, d); }
						}
					}
					if (!ok) {
						bad++;
						if (listed++ < 10) run.Note("bad route leg " + routeName + " line " + to_string(line) + " " + to_string(from) + "->" + to_string(to) + " points=" + to_string(plan.points.size()) + " worst=" + to_string(worst));
					}
				}
			}
		}
		run.Metric("route legs", legs, "");
		run.Metric("route legs without a plan", failedPlans, "");
		run.Check(bad == 0, "every route leg plans to finite, contiguous points", to_string(bad) + " of " + to_string(legs) + " legs");
	}

	// 步行导航图：以最大连通块为准，孤立节点比例要低。
	void CheckPedestrianConnectivity(TestWorld& world) {
		TestRun& run = TestRun::Get();
		const Graph& graph = world.map->GetPedestrianNavGraph();
		unordered_map<int, vector<int>> undirected;
		for (const auto& [from, edges] : graph) {
			undirected[from];
			for (const auto& [to, conn] : edges) { undirected[from].push_back(to); undirected[to].push_back(from); }
		}
		unordered_set<int> visited;
		size_t largest = 0;
		int components = 0;
		for (const auto& [node, neighbours] : undirected) {
			if (visited.count(node)) continue;
			components++;
			size_t size = 0;
			vector<int> stack = { node };
			visited.insert(node);
			while (!stack.empty()) {
				const int current = stack.back();
				stack.pop_back();
				size++;
				for (int next : undirected[current]) if (visited.insert(next).second) stack.push_back(next);
			}
			largest = max(largest, size);
		}
		run.Metric("pedestrian nav nodes", static_cast<double>(undirected.size()), "");
		run.Metric("pedestrian nav components", components, "");
		const double share = undirected.empty() ? 1.0 : static_cast<double>(largest) / static_cast<double>(undirected.size());
		run.Metric("pedestrian largest component share", share, "");
		run.Check(share >= 0.9, "pedestrian navigation is one connected network", "largest component " + to_string(share * 100.0) + "%");
	}

	// 粗高度采样：格点上等于GetHeight，格子里不过冲(落在4个角的最小/最大之间)，跨格子边连续，4角相等时是平的。
	// 对角线自适应(UsesAntiDiagonal)改过采样方式，这里守住这些不变量。
	void CheckTerrainSampling(TestWorld& world) {
		TestRun& run = TestRun::Get();
		const auto size = world.map->GetSize();
		int checked = 0, badLattice = 0, badRange = 0, badEdge = 0, badFlat = 0;
		for (int y = 0; y + 1 < size.second; y++) {
			for (int x = 0; x + 1 < size.first; x++) {
				if (world.map->GetFineHeights(x, y)) continue; // 有细化数组的格子走另一条路径(1/16网格双线性)
				const float h00 = world.map->GetHeight(x, y), h10 = world.map->GetHeight(x + 1, y);
				const float h01 = world.map->GetHeight(x, y + 1), h11 = world.map->GetHeight(x + 1, y + 1);
				const float lo = min(min(h00, h10), min(h01, h11)), hi = max(max(h00, h10), max(h01, h11));
				checked++;
				if (world.map->SampleHeight(static_cast<float>(x), static_cast<float>(y)) != h00) badLattice++;
				bool flat = h00 == h10 && h00 == h01 && h00 == h11;
				for (int i = 0; i <= 4; i++) for (int j = 0; j <= 4; j++) {
					const float v = world.map->SampleHeight(x + i / 4.f, y + j / 4.f);
					if (v < lo - 1e-4f || v > hi + 1e-4f) badRange++;
					if (flat && v != h00) badFlat++;
				}
				// 右边：从本格内侧和邻格外侧各取一点，差只能是很小的坡度量(两格各自的最大落差*2e-3级)。
				if (x + 2 < size.first && !world.map->GetFineHeights(x + 1, y)) {
					const float n10 = world.map->GetHeight(x + 2, y), n11 = world.map->GetHeight(x + 2, y + 1);
					const float span = max(hi - lo, max(fabsf(n10 - h10), fabsf(n11 - h11)));
					const float a = world.map->SampleHeight(x + 1 - 1e-3f, y + 0.37f), b = world.map->SampleHeight(x + 1 + 1e-3f, y + 0.37f);
					if (fabsf(a - b) > span * 0.01f + 1e-3f) badEdge++;
				}
			}
		}
		run.Metric("terrain cells sampled", checked, "");
		run.Check(badLattice == 0, "SampleHeight on a lattice point equals GetHeight", to_string(badLattice) + " cells");
		run.Check(badRange == 0, "SampleHeight stays inside the corner heights (no overshoot)", to_string(badRange) + " samples");
		run.Check(badFlat == 0, "SampleHeight is flat where all four corners are equal", to_string(badFlat) + " samples");
		run.Check(badEdge == 0, "SampleHeight is continuous across element edges", to_string(badEdge) + " cells");
	}

	void CheckBuildings(TestWorld& world) {
		TestRun& run = TestRun::Get();
		int badHeights = 0, roomsWithoutBuilding = 0, buildings = 0;
		for (const auto& [name, building] : world.map->GetBuildings()) {
			buildings++;
			if (static_cast<int>(building->GetFloorHeights().size()) != building->GetBasementCount() + building->GetLayerCount()) badHeights++;
			for (Room* room : building->GetRooms()) if (room->GetParentBuilding() != building) roomsWithoutBuilding++;
		}
		run.Metric("buildings", buildings, "");
		run.Check(buildings > 0, "world has buildings");
		run.Check(badHeights == 0, "floorHeights.size() == basements + layers for every building", to_string(badHeights) + " buildings");
		run.Check(roomsWithoutBuilding == 0, "every room points back at its building", to_string(roomsWithoutBuilding) + " rooms");
		run.Check(!world.map->GetRoads().empty(), "world has roads");
		run.Check(!world.map->GetLots().empty(), "world has lots");
	}

	void CheckPopulaceAndSociety(TestWorld& world) {
		TestRun& run = TestRun::Get();
		const vector<Citizen*>& citizens = world.populace->GetCitizens();
		run.Metric("citizens", static_cast<double>(citizens.size()), "");
		run.Check(!citizens.empty(), "world has citizens");

		int withoutHousehold = 0, jobMismatch = 0, employed = 0;
		for (Citizen* citizen : citizens) {
			if (citizen->GetRoom() && !citizen->GetHousehold()) withoutHousehold++;
			if (citizen->GetJob()) {
				employed++;
				if (citizen->GetJob()->GetOccupant() != citizen) jobMismatch++;
			}
		}
		run.Metric("employed citizens", employed, "");
		run.Check(withoutHousehold == 0, "a citizen with a room always belongs to a household", to_string(withoutHousehold) + " citizens");
		run.Check(jobMismatch == 0, "citizen->job->occupant points back at the citizen", to_string(jobMismatch) + " citizens");

		int badShares = 0, filledJobs = 0;
		for (Organization* organization : world.society->GetOrganizations()) {
			int64_t shares = 0;
			for (const auto& holder : organization->GetShareholders()) shares += holder.shares;
			if (!organization->GetShareholders().empty() && shares != organization->GetTotalShares()) badShares++;
			for (Job* job : organization->GetJobs()) if (job->GetOccupant()) filledJobs++;
		}
		run.Check(badShares == 0, "shareholder shares add up to the total shares", to_string(badShares) + " organizations");
		run.Check(filledJobs == employed, "filled jobs equal employed citizens", to_string(filledJobs) + " vs " + to_string(employed));
		run.Check(world.society->GetLedger().Total() == 0, "ledger total is zero", to_string(static_cast<long long>(world.society->GetLedger().Total())));
	}
}

void WorldSuiteBody(TestWorld& world, const TestOptions& options) {
	(void)options;
	RecordMemory("world: before checks");
	CheckBuildings(world);
	CheckTerrainSampling(world);
	CheckVehicleConnectivity(world);
	CheckAirportBays(world);
	CheckPortBoats(world);
	CheckVehicleEdgeGeometry(world);
	RecordMemory("world: after connectivity checks"); // 寻路校验会填满寻路缓存和线程局部缓冲，这里能看到它们的内存
	// 行道树/路灯总数(信息性，看roadnet mod有没有放；不同路网差异很大，不做通过条件)。
	TestRun::Get().Metric("trees", static_cast<double>(world.map->GetVegetation().GetTrees().size()), "");
	TestRun::Get().Metric("lamps", static_cast<double>(world.map->GetLamps().GetLamps().size()), "");
	CheckCurvedLaneEdges(world);
	CheckRouteLegGeometry(world);
	CheckPedestrianConnectivity(world);
	CheckPopulaceAndSociety(world);
}

void RunWorldSuite(const TestOptions& options) {
	TestRun::Get().Begin("world", 0, options.selfCommand + " --suite world");
	RunSuiteInChildren(options, "world", options.EffectiveMapSize());
}
