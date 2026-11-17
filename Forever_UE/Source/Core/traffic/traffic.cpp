#include "traffic.h"
#include "common/random.h"

#include "common/registry.h"
#include "common/utility.h"
#include "map/map.h"
#include "map/building.h"
#include "map/room.h"
#include "map/general_geometry.h"
#include "story/change.h"
#include "story/script.h"
#include "populace/populace.h"
#include "populace/citizen.h"
#include "society/society.h"
#include "society/organization.h"
#include "society/job.h"
#include "map/component.h"
#include "player/vehicle_asset.h"
#include "player/asset.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <unordered_map>

using namespace std;

namespace {
	// 每个成年市民开局尝试分配一辆car的概率(百分比)。
	constexpr int kCitizenVehicleChance = 20;
	// 每个组织开局尝试分配truck的次数范围[1, 5]。
	constexpr int kOrganizationMaxVehicleAttempts = 5;
	constexpr int kAdultAge = 18;

	// 在这些房间里挑一个空闲且接受category的车位(随机)，没有返回false。
	bool PickFreeSlot(const vector<Room*>& rooms, const string& category, Room*& outRoom, int& outIndex) {
		// BeginCommute/任务分配由UE层在运行时直接调用，没有外层作用域，这里自己包上(嵌套同一个域的流是安全的)。
		CORE_RANDOM_SCOPE("core.traffic");
		vector<pair<Room*, int>> candidates;
		for (Room* room : rooms) {
			if (!room) continue;
			const vector<ParkingSlot*>& slots = room->GetParkingSlots();
			for (size_t i = 0; i < slots.size(); i++) {
				if (slots[i]->IsFree() && slots[i]->navigationNode && slots[i]->Accepts(category)) candidates.emplace_back(room, static_cast<int>(i));
			}
		}
		if (candidates.empty()) return false;
		const auto& picked = candidates[GetRandom(static_cast<int>(candidates.size()))];
		outRoom = picked.first;
		outIndex = picked.second;
		return true;
	}

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

Traffic::CommuteTrip* Traffic::BeginCommute(Citizen* citizen, Room* finalDest) {
	if (!citizen || !finalDest || !citizen->IsActive() || trips.count(citizen)) return nullptr;
	Room* current = citizen->GetCurrentRoom();
	if (!current) return nullptr;
	Building* currentBuilding = current->GetParentBuilding();
	Building* destBuilding = finalDest->GetParentBuilding();
	if (!currentBuilding || !destBuilding) return nullptr;
	if (currentBuilding == destBuilding) { commuteStats.walkSameBuilding++; return nullptr; } // 同楼直接走

	bool hasCar = false, carHere = false;
	for (Asset* asset : citizen->GetAssets()) {
		VehicleAsset* vehicleAsset = dynamic_cast<VehicleAsset*>(asset);
		if (!vehicleAsset) continue;
		Vehicle* vehicle = vehicleAsset->GetVehicle();
		if (!vehicle) continue;
		hasCar = true;
		if (vehicle->GetParkState() != Vehicle::ParkState::Parked || vehicle->IsDriving()) continue;
		if (vehicle->GetRoute() || vehicle->GetReservedRoom()) continue;
		Room* parkOrigin = vehicle->GetRoom();
		if (!parkOrigin || parkOrigin->GetParentBuilding() != currentBuilding || !parkOrigin->GetNavigationNode()) continue;
		carHere = true;

		Room* parkDest = nullptr;
		int destSlot = -1;
		if (!PickFreeSlot(destBuilding->GetRooms(), vehicle->GetCategory(), parkDest, destSlot)) continue;
		if (!vehicle->ReserveSlot(parkDest, destSlot)) continue;

		CommuteTrip& trip = trips[citizen];
		trip.citizen = citizen;
		trip.vehicle = vehicle;
		trip.finalDest = finalDest;
		trip.parkOrigin = parkOrigin;
		trip.parkDest = parkDest;
		trip.destSlot = destSlot;
		trip.phase = CommutePhase::WalkToCar;
		commuteStats.started++;
		return &trip;
	}
	if (!hasCar) commuteStats.walkNoCar++;
	else if (!carHere) commuteStats.walkCarElsewhere++;
	else commuteStats.walkNoSlot++;
	return nullptr;
}

vector<string> Traffic::CommuteReport(const Populace& populace) const {
	int currentYear = populace.GetCurrentYear();
	auto hasCar = [](const Citizen* citizen) {
		for (Asset* asset : citizen->GetAssets()) {
			if (dynamic_cast<VehicleAsset*>(asset)) return true;
		}
		return false;
	};
	int adults = 0, adultsWithCar = 0;
	map<string, pair<int, int>> byJob; // 职位类型->{人数, 有车人数}，空串=无业
	for (Citizen* citizen : populace.GetCitizens()) {
		if (!citizen || !citizen->IsActive() || citizen->GetAge(currentYear) < kAdultAge) continue;
		bool car = hasCar(citizen);
		adults++;
		if (car) adultsWithCar++;
		string jobType = citizen->GetJob() ? citizen->GetJob()->GetType() : string();
		byJob[jobType].first++;
		if (car) byJob[jobType].second++;
	}
	vector<string> lines;
	lines.push_back("Commute census: adults=" + to_string(adults) + ", withCar=" + to_string(adultsWithCar));
	for (const auto& [type, counts] : byJob) {
		lines.push_back("Commute census: job " + (type.empty() ? string("(none)") : type) + ": people=" + to_string(counts.first) + ", withCar=" + to_string(counts.second));
	}
	lines.push_back("Commute census: drive pathfinding total=" + to_string(commuteStats.started) + ", arrived=" + to_string(commuteStats.arrived) + ", failed=" + to_string(commuteStats.failed) +
		", inProgress=" + to_string((int)trips.size()) + " | walked: sameBuilding=" + to_string(commuteStats.walkSameBuilding) + " noCar=" + to_string(commuteStats.walkNoCar) +
		" carElsewhere=" + to_string(commuteStats.walkCarElsewhere) + " noSlot=" + to_string(commuteStats.walkNoSlot));
	return lines;
}

Traffic::CommuteTrip* Traffic::FindTrip(const Citizen* citizen) {
	auto it = trips.find(citizen);
	return it != trips.end() ? &it->second : nullptr;
}

Traffic::CommuteTrip* Traffic::FindTripByVehicle(const Vehicle* vehicle) {
	for (auto& [citizen, trip] : trips) {
		if (trip.vehicle == vehicle) return &trip;
	}
	return nullptr;
}

void Traffic::EndTrip(const Citizen* citizen) { trips.erase(citizen); }

void Traffic::CancelTrip(const Citizen* citizen) {
	auto it = trips.find(citizen);
	if (it == trips.end()) return;
	// 车还没出发(WalkToCar)才需要释放预订；Driving阶段的预订随车的导航结果处理。
	if (it->second.phase == CommutePhase::WalkToCar && it->second.vehicle) it->second.vehicle->ClearReservation();
	trips.erase(it);
}

namespace {
	void WriteTaskVariable(Citizen* worker, const string& task) {
		if (!worker || !worker->GetJob() || !worker->GetJob()->GetScript()) return;
		worker->GetJob()->GetScript()->SetValue("task", ValueType(task));
	}

	// 车开走后原车位靠reservedBy=车占住(返程ReserveSlot允许reservedBy==自己)。
	void HoldOriginSlot(Vehicle* vehicle, Room* room, int index) {
		if (!vehicle || !room || index < 0 || index >= static_cast<int>(room->GetParkingSlots().size())) return;
		room->GetParkingSlots()[index]->reservedBy = vehicle;
	}

	void ReleaseOriginSlot(Vehicle* vehicle, Room* room, int index) {
		if (!vehicle || !room || index < 0 || index >= static_cast<int>(room->GetParkingSlots().size())) return;
		ParkingSlot* slot = room->GetParkingSlots()[index];
		// 车正预订着它(返程中)就保留，由车自己的到达/清预订处理。
		if (slot->reservedBy == vehicle && vehicle->GetReservedRoom() != room) slot->reservedBy = nullptr;
	}
}

Traffic::TaskTrip* Traffic::BeginTask(Organization* organization, const string& task, Room* destRoom, const string& category,
	const string& workerName, const function<bool(const Citizen*)>& workerAvailable) {
	if (!organization || !destRoom) return nullptr;
	Building* destBuilding = destRoom->GetParentBuilding();
	if (!destBuilding) return nullptr;

	for (Job* job : organization->GetJobs()) {
		if (!job || !job->IsOpen() || !job->GetPosition()) continue;
		Citizen* worker = job->GetOccupant();
		if (!worker || !worker->IsActive()) continue;
		if (!workerName.empty() && worker->GetName() != workerName) continue;
		Room* workRoom = job->GetPosition();
		if (worker->GetCurrentRoom() != workRoom || worker->IsWalking() || trips.count(worker) || FindTaskByWorker(worker)) continue;
		if (workerAvailable && !workerAvailable(worker)) continue;

		if (category.empty()) {
			// 走路任务：不用车，目的房间不是工位本身就行。
			if (destRoom == workRoom) continue;
			taskTrips.emplace_back();
			TaskTrip& trip = taskTrips.back();
			trip.task = task;
			trip.worker = worker;
			trip.destRoom = destRoom;
			trip.workRoom = workRoom;
			trip.phase = TaskPhase::WalkOut;
			WriteTaskVariable(worker, task);
			return &trip;
		}

		Building* workBuilding = workRoom->GetParentBuilding();
		if (!workBuilding || workBuilding == destBuilding) continue;
		for (Asset* asset : organization->GetAssets()) {
			VehicleAsset* vehicleAsset = dynamic_cast<VehicleAsset*>(asset);
			Vehicle* vehicle = vehicleAsset ? vehicleAsset->GetVehicle() : nullptr;
			if (!vehicle || vehicle->GetCategory() != category) continue;
			if (vehicle->GetParkState() != Vehicle::ParkState::Parked || vehicle->IsDriving()) continue;
			if (vehicle->GetRoute() || vehicle->GetReservedRoom() || FindTaskByVehicle(vehicle)) continue;
			Room* parkOrigin = vehicle->GetRoom();
			if (!parkOrigin || parkOrigin->GetParentBuilding() != workBuilding || !parkOrigin->GetNavigationNode()) continue;
			int originSlot = vehicle->GetParkingSlotIndex();

			Room* parkDest = nullptr;
			int destSlot = -1;
			if (!PickFreeSlot(destBuilding->GetRooms(), category, parkDest, destSlot)) return nullptr; // 目的地没车位：别的车也一样
			if (!vehicle->ReserveSlot(parkDest, destSlot)) continue;
			HoldOriginSlot(vehicle, parkOrigin, originSlot);

			taskTrips.emplace_back();
			TaskTrip& trip = taskTrips.back();
			trip.task = task;
			trip.worker = worker;
			trip.vehicle = vehicle;
			trip.parkOrigin = parkOrigin;
			trip.originSlot = originSlot;
			trip.parkDest = parkDest;
			trip.destSlot = destSlot;
			trip.destRoom = destRoom;
			trip.workRoom = workRoom;
			trip.phase = TaskPhase::WalkToVehicle;
			WriteTaskVariable(worker, task);
			return &trip;
		}
	}
	return nullptr;
}

Traffic::TaskTrip* Traffic::FindTaskByVehicle(const Vehicle* vehicle) {
	if (!vehicle) return nullptr;
	for (TaskTrip& trip : taskTrips) {
		if (trip.vehicle == vehicle) return &trip;
	}
	return nullptr;
}

Traffic::TaskTrip* Traffic::FindTaskByWorker(const Citizen* worker) {
	if (!worker) return nullptr;
	for (TaskTrip& trip : taskTrips) {
		if (trip.worker == worker) return &trip;
	}
	return nullptr;
}

void Traffic::EndTask(TaskTrip* trip) {
	if (!trip) return;
	ReleaseOriginSlot(trip->vehicle, trip->parkOrigin, trip->originSlot);
	WriteTaskVariable(trip->worker, string());
	for (auto it = taskTrips.begin(); it != taskTrips.end(); ++it) {
		if (&*it == trip) { taskTrips.erase(it); return; }
	}
}

void Traffic::CancelTask(TaskTrip* trip) {
	if (!trip) return;
	if (trip->phase == TaskPhase::WalkToVehicle && trip->vehicle) trip->vehicle->ClearReservation();
	EndTask(trip);
}

void Traffic::ResolveVehicleAssets(const Populace& populace, const Society& society) {
	const vector<Citizen*>& citizens = populace.GetCitizens();
	const vector<Organization*>& organizations = society.GetOrganizations();
	for (const PendingVehicleAsset& pending : pendingVehicleAssets) {
		auto it = vehicles.find(pending.vehicleName);
		if (it == vehicles.end()) continue;
		Citizen* ownerCitizen = nullptr;
		Organization* ownerOrganization = nullptr;
		if (pending.ownerKind == 0 && pending.ownerIndex >= 0 && pending.ownerIndex < static_cast<int>(citizens.size())) {
			ownerCitizen = citizens[pending.ownerIndex];
		} else if (pending.ownerKind == 1 && pending.ownerIndex >= 0 && pending.ownerIndex < static_cast<int>(organizations.size())) {
			ownerOrganization = organizations[pending.ownerIndex];
		}
		if (!ownerCitizen && !ownerOrganization) continue;
		VehicleAsset* asset = new VehicleAsset(it->second, ownerCitizen, ownerOrganization);
		vehicleAssets.push_back(asset);
		if (ownerCitizen) ownerCitizen->AddAsset(asset);
		if (ownerOrganization) ownerOrganization->AddAsset(asset);
	}
	pendingVehicleAssets.clear();

	// 通勤行程：市民下标到这里才能解析。找不到市民/车的行程丢弃(并释放车上的预订)。
	for (const PendingTrip& pending : pendingTrips) {
		auto vehicleIt = vehicles.find(pending.vehicleName);
		Citizen* citizen = (pending.citizenIndex >= 0 && pending.citizenIndex < static_cast<int>(citizens.size())) ? citizens[pending.citizenIndex] : nullptr;
		if (!citizen || vehicleIt == vehicles.end()) {
			if (vehicleIt != vehicles.end()) vehicleIt->second->ClearReservation();
			continue;
		}
		CommuteTrip& trip = trips[citizen];
		trip.citizen = citizen;
		trip.vehicle = vehicleIt->second;
		trip.finalDest = pending.finalDest;
		trip.parkOrigin = pending.parkOrigin;
		trip.parkDest = pending.parkDest;
		trip.destSlot = pending.destSlot;
		trip.phase = pending.phase == 1 ? CommutePhase::Driving : CommutePhase::WalkToCar;
	}
	pendingTrips.clear();

	// 组织任务行程：车必须在，员工可以没了(车仍要开回)；走路任务没员工就丢弃。
	for (const PendingTask& pending : pendingTasks) {
		auto vehicleIt = pending.vehicleName.empty() ? vehicles.end() : vehicles.find(pending.vehicleName);
		Citizen* worker = (pending.workerIndex >= 0 && pending.workerIndex < static_cast<int>(citizens.size())) ? citizens[pending.workerIndex] : nullptr;
		if (pending.vehicleName.empty() ? !worker : vehicleIt == vehicles.end()) {
			if (vehicleIt != vehicles.end()) vehicleIt->second->ClearReservation();
			continue;
		}
		taskTrips.emplace_back();
		TaskTrip& trip = taskTrips.back();
		trip.task = pending.task;
		trip.worker = worker;
		trip.vehicle = vehicleIt != vehicles.end() ? vehicleIt->second : nullptr;
		trip.parkOrigin = pending.parkOrigin;
		trip.originSlot = pending.originSlot;
		trip.parkDest = pending.parkDest;
		trip.destSlot = pending.destSlot;
		trip.destRoom = pending.destRoom;
		trip.workRoom = pending.workRoom;
		trip.phase = static_cast<TaskPhase>(pending.phase);
		if (trip.vehicle) HoldOriginSlot(trip.vehicle, trip.parkOrigin, trip.originSlot);
	}
	pendingTasks.clear();
}

Traffic::~Traffic() {
	for (VehicleAsset* asset : vehicleAssets) delete asset;
	for (auto& [name, route] : routes) delete route;
	for (auto& [name, station] : stations) delete station;
	for (auto& [name, vehicle] : vehicles) delete vehicle;
}

void Traffic::Init(Map* map) {
	if (!map) return;

	InitStations(map);
	InitRoadsideStations(map);
	InitRoutes(map);
	InitAirportVehicles(map);
	InitTransitVehicles();
}

void Traffic::InitStations(Map* map) {
	int createdCount = 0;
	for (auto& [buildingName, building] : map->GetBuildings()) {
		if (!building) continue;
		const string& stationModId = building->GetStationMod();
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
		if (modId == "station_empty") continue;

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

			// rightSide对应side0；+Y=南，右手边是(-tdy,tdx)，和直觉相反(见Map::ComputeLaneAnchorPosition)。
			// 站点坐标必须取GetLaneSegmentAt的真实车道坐标，见station.md；查不到车道(单行道只有一侧)放弃该请求。
			float travelDirX = request.rightSide ? dx : -dx;
			float travelDirY = request.rightSide ? dy : -dy;

			Map::LaneSegment segment;
			if (!map->GetLaneSegmentAt(request.road, centerPoint.GetX(), centerPoint.GetY(), travelDirX, travelDirY, segment)) {
				continue;
			}

			float headingDegrees = atan2f(travelDirY, travelDirX) * (180.f / 3.14159265358979323846f);

			string name = request.name.empty() ? "Station" + to_string(stationCounter++) : request.name;
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
	routeIds.erase(remove(routeIds.begin(), routeIds.end(), "route_empty"), routeIds.end());

	for (const string& modId : routeIds) {
		string name = "Route_" + modId;
		Route* route = new Route(&routeFactory, modId, name);
		if (!route->IsValid()) {
			delete route;
			continue;
		}

		route->Build(map, allStations);
		// 桥墩要避开路网和先建好的route轨道，所以Build之后、按已建好的route表选位。
		vector<const Route*> built;
		for (auto& [builtName, builtRoute] : routes) built.push_back(builtRoute);
		route->PlacePiers(map, built);

		debugf("Traffic::InitRoutes: route %s built %d line(s).\n", name.c_str(), static_cast<int>(route->GetLines().size()));

		routes.insert_or_assign(name, route);
	}
}

void Traffic::InitAirportVehicles(Map* map) {
	CORE_RANDOM_SCOPE("core.traffic");
	// 机场的飞机还没有对应的组织，先每个机位无条件停一架(不挂资产)；普通停车位的车由AssignVehicles按人分配。
	// 车型按种类查(VehicleFactory::GetIdsByCategory)。
	const vector<string> ids = vehicleFactory.GetIdsByCategory("plane");
	int parkedCount = 0;
	if (!ids.empty()) {
		for (auto& [buildingName, building] : map->GetBuildings()) {
			if (!building) continue;
			for (Room* room : building->GetRooms()) {
				if (!room) continue;
				const vector<ParkingSlot*>& slots = room->GetParkingSlots();
				for (size_t slotIndex = 0; slotIndex < slots.size(); slotIndex++) {
					if (!slots[slotIndex]->Accepts("plane")) continue;
					string modId = ids[GetRandom(static_cast<int>(ids.size()))];
					string name = "Vehicle" + to_string(vehicleCounter++);
					Vehicle* vehicle = CreateVehicle(modId, name);
					if (!vehicle) continue;
					vehicle->BindSlot(room, static_cast<int>(slotIndex));
					parkedCount++;
					debugf("Vehicle %s (%s) parked at %s.\n", name.c_str(), modId.c_str(), room->GetAddress().c_str());
				}
			}
		}
	}
	debugf("Traffic::InitAirportVehicles: %d plane(s).\n", parkedCount);
}

VehicleAsset* Traffic::CreateOwnedVehicle(const string& category, Room* room, int slotIndex,
	Citizen* ownerCitizen, Organization* ownerOrganization) {
	CORE_RANDOM_SCOPE("core.traffic");
	const vector<string> ids = vehicleFactory.GetIdsByCategory(category);
	if (ids.empty()) return nullptr; // 这个种类没有任何已启用车型
	string modId = ids[GetRandom(static_cast<int>(ids.size()))];
	string name = "Vehicle" + to_string(vehicleCounter++);
	Vehicle* vehicle = CreateVehicle(modId, name);
	if (!vehicle) return nullptr;
	vehicle->BindSlot(room, slotIndex);

	VehicleAsset* asset = new VehicleAsset(vehicle, ownerCitizen, ownerOrganization);
	vehicleAssets.push_back(asset);
	if (ownerCitizen) ownerCitizen->AddAsset(asset);
	if (ownerOrganization) ownerOrganization->AddAsset(asset);
	debugf("Vehicle %s (%s) parked at %s.\n", name.c_str(), modId.c_str(), room->GetAddress().c_str());
	return asset;
}

void Traffic::AssignVehicles(const Populace& populace, const Society& society) {
	CORE_RANDOM_SCOPE("core.traffic");
	// 市民：每个成年人20%概率尝试在自己家所在的建筑里找一个空闲car车位(建筑里任何带车位的房间，和是不是
	// room_parking无关)，没有就不分配。
	int citizenVehicles = 0;
	int currentYear = populace.GetCurrentYear();
	for (Citizen* citizen : populace.GetCitizens()) {
		if (!citizen || citizen->GetAge(currentYear) < kAdultAge) continue;
		if (GetRandom(100) >= kCitizenVehicleChance) continue;
		Building* building = citizen->GetBuilding();
		if (!building) continue;
		Room* room = nullptr;
		int slotIndex = -1;
		if (!PickFreeSlot(building->GetRooms(), "car", room, slotIndex)) continue;
		if (CreateOwnedVehicle("car", room, slotIndex, citizen, nullptr)) citizenVehicles++;
	}

	// 组织：在它所有组合(Component)的房间里找带车位的房间，随机1-5次尝试分配truck，找不到空位就停。
	int organizationVehicles = 0;
	for (Organization* organization : society.GetOrganizations()) {
		if (!organization) continue;
		vector<Room*> parkingRooms;
		for (Component* component : organization->GetComponents()) {
			if (!component) continue;
			for (Room* room : component->GetRooms()) {
				if (room && !room->GetParkingSlots().empty()) parkingRooms.push_back(room);
			}
		}
		if (parkingRooms.empty()) continue;
		int attempts = 1 + GetRandom(kOrganizationMaxVehicleAttempts);
		for (int i = 0; i < attempts; i++) {
			Room* room = nullptr;
			int slotIndex = -1;
			if (!PickFreeSlot(parkingRooms, "truck", room, slotIndex)) break;
			if (CreateOwnedVehicle("truck", room, slotIndex, nullptr, organization)) organizationVehicles++;
		}
	}

	debugf("Traffic::AssignVehicles: %d citizen vehicle(s), %d organization vehicle(s).\n", citizenVehicles, organizationVehicles);
}

void Traffic::InitTransitVehicles() {
	// routes是unordered_map，先按线路名排序再遍历，车辆编号(Transit<N>)才与哈希顺序无关。
	vector<string> routeNames;
	routeNames.reserve(routes.size());
	for (const auto& [routeName, route] : routes) routeNames.push_back(routeName);
	sort(routeNames.begin(), routeNames.end());
	for (const string& routeName : routeNames) {
		Route* route = routes.at(routeName);
		const vector<Route::RouteLine>& lines = route->GetLines();
		for (size_t lineIdx = 0; lineIdx < lines.size(); lineIdx++) {
			for (int k = 0; k < route->GetLineVehicleCount(lineIdx); k++) {
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
		RemoveVehicleAsset(it->second);
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
	RemoveVehicleAsset(it->second);
	delete it->second;
	vehicles.erase(it);
}

void Traffic::RemoveVehicleAsset(const Vehicle* vehicle) {
	for (auto it = vehicleAssets.begin(); it != vehicleAssets.end(); ++it) {
		if ((*it)->GetVehicle() != vehicle) continue;
		if ((*it)->GetOwnerCitizen()) (*it)->GetOwnerCitizen()->RemoveAsset(*it);
		if ((*it)->GetOwnerOrganization()) (*it)->GetOwnerOrganization()->RemoveAsset(*it);
		delete *it;
		vehicleAssets.erase(it);
		return;
	}
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

vector<Route::StopEvent> Traffic::ConsumeStopEvents() {
	vector<Route::StopEvent> result;
	for (auto& [name, route] : routes) {
		vector<Route::StopEvent> events = route->ConsumeStopEvents();
		result.insert(result.end(), events.begin(), events.end());
	}
	return result;
}

void Traffic::SilenceInitialStopEvents() {
	for (auto& [name, route] : routes) route->SilenceInitialStopEvents();
}

bool Traffic::ApplyChange(const Change* change, const ScriptContext& context) {
	if (auto* addOption = dynamic_cast<const AddOptionChange*>(change)) {
		Vehicle* target = FindVehicleByName(ToString(EvaluateExpression(addOption->GetName(), context)));
		if (target) target->AddOption(ToString(EvaluateExpression(addOption->GetOption(), context)));
		return true;
	}
	if (auto* removeOption = dynamic_cast<const RemoveOptionChange*>(change)) {
		Vehicle* target = FindVehicleByName(ToString(EvaluateExpression(removeOption->GetName(), context)));
		if (target) target->RemoveOption(ToString(EvaluateExpression(removeOption->GetOption(), context)));
		return true;
	}
	return false;
}
