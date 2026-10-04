#include "traffic.h"

#include "common/registry.h"
#include "common/utility.h"
#include "map/map.h"
#include "map/building.h"
#include "map/room.h"
#include "map/geometry.h"
#include "story/change.h"

#include <algorithm>
#include <cmath>

using namespace std;

namespace {
	struct RoadStationCollector {
		vector<RoadStationRequest> requests;
	};

	void CollectRoadStationRequest(void* context, const RoadStationRequest& request) {
		static_cast<RoadStationCollector*>(context)->requests.push_back(request);
	}
}

Traffic::Traffic() :
	vehicleFactory(Registry::Get().GetVehicleFactory()),
	stationFactory(Registry::Get().GetStationFactory()),
	routeFactory(Registry::Get().GetRouteFactory()),
	scriptFactory(Registry::Get().GetScriptFactory()) {
}

Traffic::~Traffic() {
	for (auto& [name, route] : routes) delete route;
	for (auto& [name, station] : stations) delete station;
	for (auto& [name, vehicle] : vehicles) delete vehicle;
}

void Traffic::Init(Map* map) {
	if (!map) return;

	InitStations(map);
	InitRoadsideStations(map);
	InitRoutes(map);
	InitParkedVehicles(map);
	InitTransitVehicles();
}

void Traffic::InitStations(Map* map) {
	int createdCount = 0;
	for (auto& [buildingName, building] : map->GetBuildings()) {
		if (!building || !building->GetMod()) continue;
		const string& stationModId = building->GetMod()->stationMod;
		if (stationModId.empty()) continue;

		string name = "Station" + to_string(stationCounter++);
		Station* station = new Station(&stationFactory, stationModId, name, building);
		if (!station->IsValid()) {
			delete station;
			continue;
		}

		stations.insert_or_assign(name, station);
		createdCount++;
	}

	debugf("Traffic::InitStations: created %d station(s).\n", createdCount);
}

void Traffic::InitRoadsideStations(Map* map) {
	vector<string> ids = stationFactory.GetRegisteredIds();
	int createdCount = 0;

	for (const string& modId : ids) {
		if (modId == "empty") continue;

		// 先探测这个mod类型想贴哪些路——AssignRoads()是static方法(见station_mod.h
		// StationMod::AssignRoads的说明)，不需要创建/销毁任何StationMod实例，直接转发
		// 到注册时提供的函数指针，和BuildingFactory::Assign同一个模式。
		RoadStationCollector collector;
		stationFactory.AssignRoads(modId, map->GetRoads(), &CollectRoadStationRequest, &collector);

		for (const RoadStationRequest& request : collector.requests) {
			if (!request.road) continue;

			Node centerPoint = request.road->GetPoint(request.t);
			float dx, dy, dz;
			request.road->GetTangent(request.t, dx, dy, dz);
			float len = sqrtf(dx * dx + dy * dy);
			if (len < 1e-6f) len = 1.f;
			dx /= len; dy /= len;

			// rightSide对应side0(沿Road Start->End方向的右手边)——之前这里自己手搓了一个
			// "perp=(-dy,dx)+半路宽偏移"的公式，且headingDegrees一直用正向切线没有按rightSide
			// 翻转，两个bug叠加导致车辆被指挥着往错误的方向开，PIE实测反馈"公交车的行驶方向
			// 有点问题"。排查过程中还顺带发现Map::ComputeLaneAnchorPosition等9处"perp0=
			// (tdy,-tdx)"公式本身也标反了(这个项目+Y=南，真正右手边是(-tdy,tdx))，已经在
			// map.cpp/roadnet.cpp统一修正，见那边的说明——这里不用再操心具体用哪个符号，直接
			// 问GetLaneSegmentAt要car真实走的那条车道的世界坐标就行(见下)。
			//
			// 这次改成直接问GetLaneSegmentAt要这条车道的真实世界坐标(和Station::EnsureRoadLink
			// 后续给这同一个站点接旁路时用的是完全同一次查询逻辑)，而不是自己算一个大概的偏移——
			// 车站因此精确落在车道中线上，不再是停靠时瞬移到车站独立坐标、起步后又瞬移回车道
			// 坐标(实测反馈"公交车到站了直接移动到车站的位置...车辆会从车道瞬移到旁边")。
			// travelDirX/Y是这条车道实际的车流方向(side0=正向，side1=反向)，查不到对应车道
			// (比如单行道只有一侧)时放弃这个请求。
			float travelDirX = request.rightSide ? dx : -dx;
			float travelDirY = request.rightSide ? dy : -dy;

			Map::LaneSegment segment;
			if (!map->GetLaneSegmentAt(request.road, centerPoint.GetX(), centerPoint.GetY(), travelDirX, travelDirY, segment)) {
				continue;
			}

			float headingDegrees = atan2f(travelDirY, travelDirX) * (180.f / 3.14159265358979323846f);

			string name = "Station" + to_string(stationCounter++);
			Station* station = new Station(&stationFactory, modId, name,
				segment.worldX, segment.worldY, segment.worldZ, headingDegrees, request.road);
			if (!station->IsValid()) {
				delete station;
				continue;
			}

			stations.insert_or_assign(name, station);
			createdCount++;
		}
	}

	debugf("Traffic::InitRoadsideStations: created %d roadside station(s).\n", createdCount);
}

void Traffic::InitRoutes(Map* map) {
	vector<Station*> allStations;
	for (auto& [name, station] : stations) allStations.push_back(station);

	vector<string> routeIds = routeFactory.GetRegisteredIds();
	routeIds.erase(remove(routeIds.begin(), routeIds.end(), "empty"), routeIds.end());

	for (const string& modId : routeIds) {
		string name = "Route_" + modId;
		Route* route = new Route(&routeFactory, modId, name);
		if (!route->IsValid()) {
			delete route;
			continue;
		}

		vector<Station*> createdEdgeStations;
		route->Build(map, allStations, createdEdgeStations);
		for (Station* edgeStation : createdEdgeStations) {
			stations.insert_or_assign(edgeStation->GetName(), edgeStation);
		}

		debugf("Traffic::InitRoutes: route %s built %d line(s), %d edge station(s).\n",
			name.c_str(), static_cast<int>(route->GetLines().size()), static_cast<int>(createdEdgeStations.size()));

		routes.insert_or_assign(name, route);
	}
}

void Traffic::InitParkedVehicles(Map* map) {
	// 先探测哪些已注册车型属于category=="car"——这几个mod实例是临时的，探测完立刻销毁，不
	// 留在vehicles里，见traffic.md"停车位车辆"一节。
	vector<string> allIds = vehicleFactory.GetRegisteredIds();
	vector<string> carIds;
	for (const string& id : allIds) {
		if (id == "empty") continue;
		VehicleMod* probe = vehicleFactory.CreateVehicle(id);
		if (!probe) continue;
		if (probe->category == "car") carIds.push_back(id);
		vehicleFactory.DestroyVehicle(probe);
	}

	int parkingRoomCount = 0;

	for (auto& [buildingName, building] : map->GetBuildings()) {
		if (!building) continue;
		for (Room* room : building->GetRooms()) {
			if (!room || !room->IsParking()) continue;
			parkingRoomCount++;

			for (const ParkingSpot& spot : room->GetParkingSpots()) {
				if (carIds.empty()) continue; // 没有真正可用的车型，跳过这个车位

				string modId = carIds[GetRandom(static_cast<int>(carIds.size()))];
				string name = "Vehicle" + to_string(vehicleCounter++);

				Vehicle* vehicle = CreateVehicle(modId, name);
				if (!vehicle) continue;

				// room->GetPosX()/GetPosY()已经是房间自己的中心点(Quad的约定，不是角点)，
				// 这里的ratio要按"相对房间中心的偏移"解释——ratio=0.5意味着偏移0(正中心)，
				// ratio=0/1分别是房间两侧边缘(-/+半边长)。
				float localX = (spot.position[0] - 0.5f) * room->GetSizeX() + spot.position[1];
				float localY = (spot.position[2] - 0.5f) * room->GetSizeY() + spot.position[3];
				vehicle->SetParking(room, localX, localY, spot.rotationDegrees);

				debugf("Vehicle %s parked at %s.\n", name.c_str(), room->GetAddress().c_str());
			}
		}
	}

	debugf("Traffic::InitParkedVehicles: found %d parking room(s).\n", parkingRoomCount);
}

void Traffic::InitTransitVehicles() {
	for (auto& [routeName, route] : routes) {
		const vector<vector<Route::RouteLeg>>& lines = route->GetLines();
		for (size_t lineIdx = 0; lineIdx < lines.size(); lineIdx++) {
			for (int k = 0; k < route->GetVehiclesPerLine(); k++) {
				string name = "Transit" + to_string(vehicleCounter++);
				Vehicle* vehicle = CreateVehicle(route->GetVehicleType(), name);
				if (!vehicle) continue;

				vehicle->SetRouteBinding(route, static_cast<int>(lineIdx));
				route->AddVehicleToLine(lineIdx, vehicle);
			}
		}
	}
}

Vehicle* Traffic::CreateVehicle(const string& modId, const string& name) {
	auto it = vehicles.find(name);
	if (it != vehicles.end()) {
		delete it->second;
		vehicles.erase(it);
	}

	Vehicle* vehicle = new Vehicle(&vehicleFactory, &scriptFactory, modId, name);
	if (!vehicle->IsValid()) {
		delete vehicle;
		return nullptr;
	}

	vehicles.insert_or_assign(name, vehicle);
	return vehicle;
}

void Traffic::DestroyVehicle(const string& name) {
	auto it = vehicles.find(name);
	if (it == vehicles.end()) return;
	delete it->second;
	vehicles.erase(it);
}

Vehicle* Traffic::FindVehicleByName(const string& name) const {
	auto it = vehicles.find(name);
	return it != vehicles.end() ? it->second : nullptr;
}

const unordered_map<string, Vehicle*>& Traffic::GetVehicles() const { return vehicles; }

Station* Traffic::FindStationByName(const string& name) const {
	auto it = stations.find(name);
	return it != stations.end() ? it->second : nullptr;
}

const unordered_map<string, Station*>& Traffic::GetStations() const { return stations; }

const unordered_map<string, Route*>& Traffic::GetRoutes() const { return routes; }

void Traffic::Tick(const Time& currentTime, bool crossedDay, PostHandle* post) {
	if (hasLastTickTime) {
		double delta = lastTickTime.DifferenceInSeconds(currentTime);
		if (delta > 0.0) elapsedSeconds += static_cast<float>(delta);
	}
	lastTickTime = currentTime;
	hasLastTickTime = true;

	for (auto& [name, route] : routes) {
		route->Update(elapsedSeconds);
	}
}

void Traffic::ApplyChange(const Change* change, const ScriptContext& context) {
	if (auto* addOption = dynamic_cast<const AddOptionChange*>(change)) {
		Vehicle* target = FindVehicleByName(ToString(EvaluateExpression(addOption->GetName(), context)));
		if (target) target->AddOption(ToString(EvaluateExpression(addOption->GetOption(), context)));
		return;
	}
}
