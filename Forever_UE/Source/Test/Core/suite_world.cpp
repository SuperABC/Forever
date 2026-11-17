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
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <vector>


using namespace std;

namespace {
	using Graph = unordered_map<int, vector<pair<int, Connection*>>>;

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
	CheckBuildings(world);
	CheckVehicleConnectivity(world);
	CheckPedestrianConnectivity(world);
	CheckPopulaceAndSociety(world);
}

void RunWorldSuite(const TestOptions& options) {
	TestRun::Get().Begin("world", 0, options.selfCommand + " --suite world");
	RunSuiteInChildren(options, "world", options.EffectiveMapSize());
}
