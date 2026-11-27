#include "suites.h"

#include "test_framework.h"

#include "common/registry.h"
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
#include "traffic/vehicle.h"
#include "player/vehicle_asset.h"
#include "player/asset.h"
#include "traffic/route.h"
#include "traffic/junction.h"
#include "player/player.h"
#include "story/event.h"

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

	// 港口：layout的1个parkings矩形只切一个船位(spacing=-1)，开局停一艘船。
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
	// 游戏时钟按整毫秒走，但不足1毫秒的零头要累加：每帧只有0.4毫秒时时钟也得前进，总量不随帧数变。
	// 会把时钟往前拨1秒，放在world套件靠后跑。
	void CheckClockRemainder(TestWorld& world) {
		TestRun& run = TestRun::Get();
		if (!world.player || !world.player->GetTime()) return;
		const auto millisOfDay = [](const Time& t) {
			return ((static_cast<long long>(t.GetHour()) * 60 + t.GetMinute()) * 60 + t.GetSecond()) * 1000 + t.GetMillisecond();
		};
		const double ratio = world.player->GetTimeFlowRatio();
		world.player->SetTimeFlowRatio(1.0);
		const long long before = millisOfDay(*world.player->GetTime());
		// 倍率1时1现实秒=60000游戏毫秒；每帧1/144000秒=0.4167毫秒，2400帧合计正好1000毫秒。
		for (int i = 0; i < 2400; i++) world.player->Tick(1.0f / 144000.0f);
		const long long day = 24LL * 60 * 60 * 1000;
		const long long elapsed = ((millisOfDay(*world.player->GetTime()) - before) % day + day) % day;
		world.player->SetTimeFlowRatio(ratio);
		run.Check(elapsed >= 999 && elapsed <= 1001, "clock keeps sub-millisecond remainders across ticks (2400 ticks of 0.4167ms = 1000ms)", to_string(elapsed) + " ms");
	}

	// 销毁车辆：它占着的车位、预订的车位、通勤行程、资产都要一起解开，不能留指向它的指针。
	// 会改动世界(少一辆车)，所以放在world套件最后跑。
	void CheckDestroyVehicle(TestWorld& world) {
		TestRun& run = TestRun::Get();
		if (!world.traffic) return;
		// 先尽量造一条真实的通勤行程(车停在车主所在的楼里、别的楼有空车位)，这样一次销毁能同时覆盖车位、预订和行程。
		vector<VehicleAsset*> assets = world.traffic->GetVehicleAssets();
		sort(assets.begin(), assets.end(), [](const VehicleAsset* a, const VehicleAsset* b) { return a->GetVehicle()->GetName() < b->GetVehicle()->GetName(); });
		vector<string> buildingNames;
		for (const auto& [name, building] : world.map->GetBuildings()) if (building && !building->GetRooms().empty()) buildingNames.push_back(name);
		sort(buildingNames.begin(), buildingNames.end());
		Vehicle* victim = nullptr;
		Citizen* driver = nullptr;
		int tried = 0;
		for (VehicleAsset* asset : assets) {
			if (victim || tried >= 40) break;
			Citizen* owner = asset->GetOwnerCitizen();
			if (!owner || !asset->GetVehicle() || !asset->GetVehicle()->GetParkingSlot()) continue;
			tried++;
			for (const string& name : buildingNames) {
				Traffic::CommuteTrip* trip = world.traffic->BeginCommute(owner, world.map->GetBuildings().at(name)->GetRooms()[0]);
				if (!trip) continue;
				victim = trip->vehicle;
				driver = owner;
				break;
			}
		}
		// 造不出行程(这个种子里没有合适的车)就退一步：随便找一辆停着的车，手动给它预订一个空车位。
		if (!victim) {
			run.Note("destroy-vehicle check: no commute trip could be started, trip cleanup not exercised in this seed");
			vector<string> names;
			for (const auto& [name, vehicle] : world.traffic->GetVehicles()) if (vehicle->GetParkingSlot() && !vehicle->GetReservedRoom() && !vehicle->GetRoute()) names.push_back(name);
			sort(names.begin(), names.end());
			if (!names.empty()) victim = world.traffic->FindVehicleByName(names[0]);
			if (victim) {
				bool reserved = false;
				for (const string& name : buildingNames) {
					for (Room* room : world.map->GetBuildings().at(name)->GetRooms()) {
						const vector<ParkingSlot*>& slots = room->GetParkingSlots();
						for (size_t i = 0; i < slots.size() && !reserved; i++) {
							if (slots[i]->IsFree()) reserved = victim->ReserveSlot(room, static_cast<int>(i));
						}
						if (reserved) break;
					}
					if (reserved) break;
				}
			}
		}
		run.Check(victim != nullptr, "world has a parked vehicle to destroy");
		if (!victim) return;
		const string victimName = victim->GetName();
		ParkingSlot* parkedSlot = victim->GetParkingSlot();
		ParkingSlot* reservedSlot = victim->GetReservedSlot();
		run.Check(parkedSlot && parkedSlot->parkedVehicle == victim, "vehicle to destroy is parked in a slot");
		run.Check(reservedSlot && reservedSlot->reservedBy == victim, "vehicle to destroy holds a reservation on another slot");
		const size_t vehiclesBefore = world.traffic->GetVehicles().size();

		world.traffic->DestroyVehicle(victimName);

		// victim之后只当作一个地址来比较，不再解引用。
		run.Check(world.traffic->FindVehicleByName(victimName) == nullptr && world.traffic->GetVehicles().size() + 1 == vehiclesBefore, "destroyed vehicle is gone from the vehicle table");
		if (parkedSlot) run.Check(parkedSlot->parkedVehicle == nullptr && parkedSlot->IsFree(), "destroying a vehicle frees the slot it was parked in");
		if (reservedSlot) run.Check(reservedSlot->reservedBy == nullptr && reservedSlot->IsFree(), "destroying a vehicle releases the slot it had reserved");
		if (driver) run.Check(world.traffic->FindTrip(driver) == nullptr, "destroying a vehicle cancels its commute trip");
		bool tripLeft = false, taskLeft = false, assetLeft = false;
		for (const auto& [citizen, trip] : world.traffic->GetTrips()) if (trip.vehicle == victim) tripLeft = true;
		for (const Traffic::TaskTrip& task : world.traffic->GetTasks()) if (task.vehicle == victim) taskLeft = true;
		for (const VehicleAsset* asset : world.traffic->GetVehicleAssets()) if (asset->GetVehicle() == victim) assetLeft = true;
		run.Check(!tripLeft && !taskLeft, "no commute or task trip still points at the destroyed vehicle");
		run.Check(!assetLeft, "no vehicle asset still points at the destroyed vehicle");
	}
	// 房间里已经有同名物品时，第二件放不进去(不覆盖、不泄漏)，第一件原样留着。
	void CheckRoomAssetDuplicate(TestWorld& world) {
		TestRun& run = TestRun::Get();
		Room* room = nullptr;
		for (const auto& [name, building] : world.map->GetBuildings()) {
			if (building && !building->GetRooms().empty()) { room = building->GetRooms()[0]; break; }
		}
		if (!room) return;
		const string name = "coretest duplicate asset";
		// 用mod创建的物品名字一定不同，造不出重名；借"内置物品"的构造函数(给名字就行，protected)造两件同名的。
		struct NamedAsset : Asset {
			explicit NamedAsset(const string& assetName) : Asset(AssetMobility::Object, assetName, "coretest") {}
		};
		Asset* first = new NamedAsset(name);
		Asset* second = new NamedAsset(name);
		const bool firstAdded = room->AddAsset(first);
		const bool secondAdded = room->AddAsset(second);
		auto held = room->GetAssets().find(name);
		run.Check(firstAdded && !secondAdded && held != room->GetAssets().end() && held->second == first,
			"a room rejects a second asset with the same name and keeps the first");
		delete second;                      // 没放进去，还是我们的
		delete room->RemoveAsset(name);     // 第一件拿回来销毁，不留在世界里
	}

	// 房间mod在自己房间里放的物品(RoomMod::PlaceAsset/PlaceFixture)：新开局时Core按清单建好、放进Room::assets、带地面摆位。
	//   - 固定物品(没有AssetMod)：有脚本、名字全局唯一且以房间地址开头、不能进背包/进容器/记成室外物品。
	//   - 仓库里的汉堡：每个仓库10个各自独立的(物品mod启用时)。
	//   - 床改成固定物品之后不再出现在家具列表里(不会画两遍)。
	void CheckPlacedAssets(TestWorld& world) {
		TestRun& run = TestRun::Get();
		const bool burgerEnabled = Registry::Get().GetAssetFactory().CheckRegistered("asset_burger");
		size_t fixtures = 0, beds = 0, placed = 0, warehouses = 0;
		size_t badFixtures = 0, badNames = 0, badWarehouses = 0, bedsAsFurniture = 0, withoutGround = 0;
		unordered_set<string> names;
		Asset* sampleFixture = nullptr;
		string firstBad;
		for (const auto& [buildingName, building] : world.map->GetBuildings()) {
			if (!building) continue;
			for (Room* room : building->GetRooms()) {
				if (!room) continue;
				size_t burgersHere = 0;
				bool bedHere = false;
				for (const auto& [assetName, asset] : room->GetAssets()) {
					if (!names.insert(assetName).second) { badNames++; if (firstBad.empty()) firstBad = "duplicate name " + assetName; }
					const AssetGround& ground = asset->GetGround();
					if (ground.x == 0.f && ground.y == 0.f) withoutGround++;
					if (asset->IsFixture()) {
						fixtures++;
						if (asset->GetType() == "bed") { beds++; bedHere = true; }
						if (!sampleFixture) sampleFixture = asset;
						const bool ok = asset->IsValid() && asset->GetMobility() == AssetMobility::Fixed && asset->GetFixtureDesc() &&
							!asset->GetMeshPath().empty() && asset->GetScript() != nullptr && !ground.restOnBottom &&
							assetName.rfind(room->GetAddress(), 0) == 0;
						if (!ok) { badFixtures++; if (firstBad.empty()) firstBad = "fixture " + assetName; }
					}
					else {
						placed++;
						if (asset->GetType() == "asset_burger" && asset->GetCount() == 1 && ground.restOnBottom) burgersHere++;
					}
				}
				if (bedHere) {
					for (const DecorationInstance& item : room->GetFurniture()) {
						if (item.meshPath.find("/Bed_") != string::npos) bedsAsFurniture++;
					}
				}
				if (room->GetType() == "room_warehouse") {
					warehouses++;
					if (burgersHere != (burgerEnabled ? 10u : 0u)) { badWarehouses++; if (firstBad.empty()) firstBad = "warehouse " + room->GetAddress() + " has " + to_string(burgersHere) + " burgers"; }
				}
			}
		}
		run.Metric("room fixtures", static_cast<double>(fixtures), "");
		run.Metric("room fixtures of type bed", static_cast<double>(beds), "");
		run.Metric("room placed assets", static_cast<double>(placed), "");
		run.Metric("warehouse rooms", static_cast<double>(warehouses), "");
		run.Check(badNames == 0, "every asset placed in a room has a globally unique name", firstBad);
		run.Check(badFixtures == 0, "room fixtures are valid mod-less assets with a mesh, a script and a name that starts with the room address", firstBad);
		run.Check(withoutGround == 0, "every asset placed in a room has a ground placement", to_string(withoutGround));
		run.Check(badWarehouses == 0, burgerEnabled ? "every warehouse room holds 10 separate burgers" : "warehouses hold no burgers when the burger mod is disabled", firstBad);
		run.Check(bedsAsFurniture == 0, "a bed placed as a fixture is not also listed as furniture", to_string(bedsAsFurniture));
		if (beds == 0) run.Note("no bed fixture in this seed's world (small map without residences?), fixture rules only partly exercised");
		if (!sampleFixture) return;

		// 固定物品挪不走：进不了背包/手、装不进容器、不能记成室外物品。
		run.Check(world.player->StoreAsset(sampleFixture) == Player::StoreResult::Failed, "a fixture cannot be stored on the player");
		run.Check(!world.player->AddByPath("left", sampleFixture) && !world.player->AddByPath("back", sampleFixture), "a fixture cannot be put in a hand or on the back");
		run.Check(!world.player->AddWorldAsset(sampleFixture, AssetGround()), "a fixture cannot be registered as an outdoor asset");
		if (Asset* container = world.player->CreateAsset("asset_container")) {
			run.Check(!container->AddContent(sampleFixture), "a fixture cannot be put inside a container");
			world.player->DestroyAsset(container);
		}
	}

	// 加载界面预热用的Asset::CollectRegisteredMeshes：每个启用的物品mod都列出来，汉堡带网格。
	void CheckRegisteredMeshes() {
		TestRun& run = TestRun::Get();
		AssetFactory& factory = Registry::Get().GetAssetFactory();
		size_t enabled = 0;
		for (const string& id : factory.GetRegisteredIds()) if (factory.CheckRegistered(id)) enabled++;
		const vector<Asset::RegisteredMesh> meshes = Asset::CollectRegisteredMeshes(factory);
		bool burgerHasMesh = false;
		for (const Asset::RegisteredMesh& mesh : meshes) if (mesh.id == "asset_burger") burgerHasMesh = !mesh.meshPath.empty();
		run.Metric("registered asset meshes", static_cast<double>(meshes.size()), "");
		run.Check(meshes.size() == enabled, "CollectRegisteredMeshes lists every enabled asset mod", to_string(meshes.size()) + " of " + to_string(enabled));
		if (factory.CheckRegistered("asset_burger")) run.Check(burgerHasMesh, "the burger asset mod reports its mesh path", "");
	}

	// 死亡复活点(Map::PickJunctionRespawn)：有路口时总能挑到，且挑到的一定是某个路口的行人锚点坐标。
	// 顺带验player_decease/player_revive事件：name留空=不限，填了只匹配同名，local字段读得到。
	void CheckJunctionRespawn(TestWorld& world) {
		TestRun& run = TestRun::Get();
		vector<const Node*> anchors;
		for (const RoadJunction* junction : world.map->GetJunctions()) {
			if (!junction) continue;
			for (const RoadJunctionApproach& approach : junction->GetApproaches()) {
				for (const Node* anchor : approach.pedestrianSide) if (anchor) anchors.push_back(anchor);
			}
		}
		float x = 0.f, y = 0.f, z = 0.f;
		const bool picked = world.map->PickJunctionRespawn(x, y, z);
		run.Check(picked == !anchors.empty(), "PickJunctionRespawn succeeds exactly when some junction has a pedestrian anchor",
			to_string(anchors.size()) + " anchors");
		if (picked) {
			size_t misses = 0;
			for (int i = 0; i < 20; i++) {
				if (!world.map->PickJunctionRespawn(x, y, z)) { misses++; continue; }
				bool onAnchor = false;
				for (const Node* anchor : anchors) {
					if (anchor->GetX() == x && anchor->GetY() == y && anchor->GetZ() == z) { onAnchor = true; break; }
				}
				if (!onAnchor) misses++;
			}
			run.Check(misses == 0, "every respawn point is a junction pedestrian anchor", to_string(misses) + " of 20 off-anchor");
		}

		ScriptContext context;
		PlayerDeceaseEvent decease("someone", "killer", "pistol", "head", "here");
		PlayerDeceaseEvent anyDecease;
		PlayerDeceaseEvent namedDecease("\"someone\"");
		PlayerDeceaseEvent otherDecease("\"nobody\"");
		run.Check(anyDecease.Match(&decease, context) && namedDecease.Match(&decease, context) && !otherDecease.Match(&decease, context),
			"player_decease matches any name when empty and only the same name otherwise", "");
		string killer;
		decease.GetLocalValue("killer", [&](const ValueType& value) { killer = ToString(value); });
		run.Check(killer == "killer", "player_decease exposes $$local.killer", killer);
		PlayerReviveEvent revive("someone", "street");
		PlayerReviveEvent anyRevive;
		string location;
		revive.GetLocalValue("location", [&](const ValueType& value) { location = ToString(value); });
		run.Check(anyRevive.Match(&revive, context) && location == "street", "player_revive matches and exposes $$local.location", location);
	}

	// 红绿灯分组(JunctionTraffic::BuildFromMap)：多于两条路的路口都有信号表、其余没有；每条有驶入车道的路恰好在一组里，
	// 只出不进的路不分组；每组1~2条、单独一组的最多一个且只在路数为奇数时出现；全路口最接近180°的那两条一定配成一对(贪心第一步)。
	// 激活前当作没有灯，激活后正好有一组是绿灯；横道只在横穿的那条路不是绿灯时放行人。
	void CheckSignalGroups(TestWorld& world) {
		TestRun& run = TestRun::Get();
		JunctionTraffic traffic;
		traffic.BuildFromMap(*world.map);
		const float pi = 3.14159265f;
		auto deviation = [pi](float a, float b) {
			float d = std::fabs(std::remainder(a - b, 2.f * pi));
			return pi - d;
		};
		size_t signalJunctions = 0, crosswalkChecks = 0, crosswalkCollisions = 0, bad = 0;
		string firstBad, firstCollision;
		auto fail = [&](const string& why) { bad++; if (firstBad.empty()) firstBad = why; };
		for (const RoadJunction* junction : world.map->GetJunctions()) {
			if (!junction || !junction->GetNode()) continue;
			const int id = junction->GetNode()->GetId();
			const auto& approaches = junction->GetApproaches();
			const string where = "junction " + to_string(id);
			if (approaches.size() <= 2) {
				if (traffic.HasSignal(id)) fail(where + " has a signal with only " + to_string(approaches.size()) + " roads");
				continue;
			}
			const vector<int>* groups = traffic.GetSignalGroups(id);
			if (!groups || groups->size() != approaches.size()) { fail(where + " has no signal table"); continue; }
			signalJunctions++;
			vector<int> inbound;
			map<int, vector<int>> members;
			for (size_t i = 0; i < approaches.size(); i++) {
				const bool hasInbound = !approaches[i].vehicleInbound.empty();
				const int g = (*groups)[i];
				if (hasInbound != (g >= 0)) fail(where + " approach " + to_string(i) + (hasInbound ? " has no group" : " is outbound-only but grouped"));
				if (hasInbound) inbound.push_back(static_cast<int>(i));
				if (g >= 0) members[g].push_back(static_cast<int>(i));
			}
			size_t singles = 0;
			for (const auto& [g, list] : members) {
				if (list.empty() || list.size() > 2) fail(where + " group " + to_string(g) + " has " + to_string(list.size()) + " roads");
				if (list.size() == 1) singles++;
			}
			if (singles != inbound.size() % 2) fail(where + " has " + to_string(singles) + " single-road groups for " + to_string(inbound.size()) + " inbound roads");
			if (inbound.size() >= 2) {
				float best = 1e9f;
				int bestA = -1, bestB = -1;
				for (size_t a = 0; a < inbound.size(); a++) for (size_t b = a + 1; b < inbound.size(); b++) {
					float dev = deviation(approaches[inbound[a]].angle, approaches[inbound[b]].angle);
					if (dev < best - 1e-5f) { best = dev; bestA = inbound[a]; bestB = inbound[b]; }
				}
				if ((*groups)[bestA] != (*groups)[bestB]) {
					// 并列最直的情况允许任选其一：只要bestA的搭档同样直就行
					bool tie = false;
					for (int other : members[(*groups)[bestA]]) {
						if (other != bestA && deviation(approaches[bestA].angle, approaches[other].angle) <= best + 1e-4f) tie = true;
					}
					for (int other : members[(*groups)[bestB]]) {
						if (other != bestB && deviation(approaches[bestB].angle, approaches[other].angle) <= best + 1e-4f) tie = true;
					}
					if (!tie) fail(where + " does not pair its straightest two roads " + to_string(bestA) + "," + to_string(bestB));
				}
			}
			if (!inbound.empty()) {
				if (traffic.GetSignal(id, inbound[0], 1.0) != JunctionTraffic::Signal::None) fail(where + " shows a light before activation");
				traffic.SetSignalActive(id, true, 0.0);
				size_t green = 0;
				for (int i : inbound) if (traffic.GetSignal(id, i, 1.0) == JunctionTraffic::Signal::Green) green++;
				const size_t firstGroupSize = members.empty() ? 0 : members.begin()->second.size();
				if (green == 0 || green != firstGroupSize) fail(where + " has " + to_string(green) + " green roads right after activation");
				// 行人横道跟着横穿的那条路的灯：那条路绿灯时不放，红灯(或那条路不分组)时放。
				for (size_t i = 0; i < approaches.size(); i++) {
					const Node* p0 = approaches[i].pedestrianSide[0];
					const Node* p1 = approaches[i].pedestrianSide[1];
					if (!p0 || !p1) continue;
					const Connection* crosswalk = traffic.FindCrosswalk(p0->GetX(), p0->GetY(), p1->GetX(), p1->GetY());
					if (!crosswalk) continue;
					// FindCrosswalk按位置找：别的路口如果有一条两端恰好在同一位置的横道，会先找到那条。只检查本路口这条。
					const bool same = (crosswalk->GetStart().GetId() == p0->GetId() && crosswalk->GetEnd().GetId() == p1->GetId())
						|| (crosswalk->GetStart().GetId() == p1->GetId() && crosswalk->GetEnd().GetId() == p0->GetId());
					if (!same) {
						crosswalkCollisions++;
						string owner = "?";
						for (const RoadJunction* other : world.map->GetJunctions()) {
							if (!other || !other->GetNode()) continue;
							for (const RoadJunctionApproach& a : other->GetApproaches()) {
								if (a.pedestrianSide[0] && a.pedestrianSide[0]->GetId() == crosswalk->GetStart().GetId()) owner = to_string(other->GetNode()->GetId())
									+ "@(" + to_string(other->GetNode()->GetX()) + "," + to_string(other->GetNode()->GetY()) + ")";
								if (a.pedestrianSide[1] && a.pedestrianSide[1]->GetId() == crosswalk->GetStart().GetId()) owner = to_string(other->GetNode()->GetId())
									+ "@(" + to_string(other->GetNode()->GetX()) + "," + to_string(other->GetNode()->GetY()) + ")";
							}
						}
						if (firstCollision.empty()) firstCollision = where + "@(" + to_string(junction->GetNode()->GetX()) + "," + to_string(junction->GetNode()->GetY())
							+ ") road " + to_string(i) + " crosswalk position also matches junction " + owner;
						continue;
					}
					crosswalkChecks++;
					const JunctionTraffic::Signal signal = traffic.GetSignal(id, static_cast<int>(i), 1.0);
					const bool expected = signal != JunctionTraffic::Signal::Green && signal != JunctionTraffic::Signal::Yellow;
					const int64_t ped = JunctionTraffic::MakeEntity(JunctionTraffic::EntityKind::Pedestrian, static_cast<int>(crosswalkChecks));
					if (traffic.Request(crosswalk, ped, 1.0, 0.f, 1e9f) != expected) {
						fail(where + " crosswalk over road " + to_string(i) + (expected ? " refused on red" : " granted on green"));
					}
					traffic.Leave(crosswalk, ped);
				}
				traffic.SetSignalActive(id, false, 2.0);
			}
		}
		run.Metric("signal junctions", static_cast<double>(signalJunctions), "");
		run.Metric("signal crosswalks", static_cast<double>(crosswalkChecks), "");
		run.Metric("crosswalk position collisions", static_cast<double>(crosswalkCollisions), "");
		if (!firstCollision.empty()) run.Note(firstCollision);
		if (signalJunctions > 0 && crosswalkChecks == 0) fail("no crosswalk found at any signal junction");
		run.Check(bad == 0, "junctions with more than two roads get signal groups that pair the straightest roads and cover every inbound road", firstBad);
		if (signalJunctions == 0) run.Note("no junction with more than two roads in this seed's world, signal grouping not exercised");
	}

	// 销毁线路上的车(公交/火车/飞机)：线路不再认识它，之后交通照常能走一步(线路里不留它的指针)。必须在最后跑。
	void CheckDestroyLineVehicle(TestWorld& world) {
		TestRun& run = TestRun::Get();
		if (!world.traffic || !world.player) return;
		vector<string> names;
		for (const auto& [name, vehicle] : world.traffic->GetVehicles()) if (vehicle->GetRoute()) names.push_back(name);
		sort(names.begin(), names.end());
		run.Metric("line vehicles", static_cast<double>(names.size()), "");
		if (names.empty()) {
			run.Note("destroy-line-vehicle check: this world has no vehicle on a route, not exercised");
			return;
		}
		Vehicle* victim = world.traffic->FindVehicleByName(names[0]);
		Route* route = victim->GetRoute();
		run.Check(route->QueryVehicleState(victim, 60.0).valid, "the line vehicle is known to its route before it is destroyed");
		world.traffic->DestroyVehicle(names[0]);
		// victim之后只当作一个地址来比较，不再解引用。
		run.Check(!route->QueryVehicleState(victim, 60.0).valid, "a destroyed line vehicle is no longer on its route");
		Time next = *world.player->GetTime();
		next.AddSeconds(60);
		world.traffic->Tick(next);
		run.Check(world.traffic->FindVehicleByName(names[0]) == nullptr, "traffic keeps ticking after a line vehicle was destroyed");
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
	CheckRoomAssetDuplicate(world);
	CheckPlacedAssets(world);
	CheckRegisteredMeshes();
	CheckSignalGroups(world);
	CheckJunctionRespawn(world);
	CheckClockRemainder(world);
	CheckDestroyVehicle(world); // 会销毁一辆车，必须最后跑
	CheckDestroyLineVehicle(world);
}

void RunWorldSuite(const TestOptions& options) {
	TestRun::Get().Begin("world", 0, options.selfCommand + " --suite world");
	RunSuiteInChildren(options, "world", options.EffectiveMapSize());
}
