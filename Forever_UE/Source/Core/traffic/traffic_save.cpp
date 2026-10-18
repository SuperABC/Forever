#include "traffic_save.h"

#include "traffic/traffic.h"

#include "common/save.h"
#include "common/save_refs.h"
#include "common/utility.h"

#include <algorithm>
#include <unordered_map>


using namespace std;

namespace {
	enum StationKind : uint8_t {
		STATION_BUILDING = 0, // 建在建筑里(Traffic::InitStations)
		STATION_ROADSIDE = 1, // 贴路站点(Traffic::InitRoadsideStations)
		STATION_EDGE = 2,     // 线路创建的地图边缘站点(Route::Build)
	};

	int VersionOf(const char* name) {
		for (const SaveSectionVersion& v : GetSaveSectionVersions()) {
			if (string(v.name) == name) return v.current;
		}
		return 1;
	}

	template <typename K, typename V>
	vector<K> SortedKeys(const unordered_map<K, V>& m) {
		vector<K> keys;
		keys.reserve(m.size());
		for (auto& [k, _] : m) keys.push_back(k);
		sort(keys.begin(), keys.end());
		return keys;
	}

	void WriteStrings(const vector<string>& v, ByteWriter& w) {
		w.U32(static_cast<uint32_t>(v.size()));
		for (const string& s : v) w.Str(s);
	}
	vector<string> ReadStrings(ByteReader& r) {
		vector<string> v(r.U32());
		for (string& s : v) s = r.Str();
		return v;
	}

	// 贴路站点挂在哪条路上：按map->GetRoads()的下标(InitRoadsideStations只在这些路上建站)。
	int RoadIndex(const Map& map, const Road* road) {
		return road ? SaveRefs::IndexOf(map.GetRoads(), road) : -1;
	}
}

void TrafficSaveCodec::Save(const Traffic& traffic, const Map& map, SaveArchive& archive) {
	ByteWriter w;

	w.I32(traffic.vehicleCounter);
	w.I32(traffic.stationCounter);
	w.F32(traffic.elapsedSeconds);

	auto stationIds = SaveRefs::LiveIds<StationFactory, StationMod>(traffic.stationFactory);
	auto routeIds = SaveRefs::LiveIds<RouteFactory, RouteMod>(traffic.routeFactory);
	auto vehicleIds = SaveRefs::LiveIds<VehicleFactory, VehicleMod>(traffic.vehicleFactory);

	// 站点按名字排序。
	vector<string> stationNames = SortedKeys(traffic.stations);
	w.U32(static_cast<uint32_t>(stationNames.size()));
	for (const string& name : stationNames) {
		const Station* s = traffic.stations.at(name);
		uint8_t kind = s->edge ? STATION_EDGE : (s->building ? STATION_BUILDING : STATION_ROADSIDE);
		w.Str(name);
		w.U8(kind);
		w.Str(SaveRefs::LookupId(stationIds, static_cast<const StationMod*>(s->mod)));
		SaveRefs::WriteModState(s->mod, w);
		w.Str(s->building ? s->building->GetName() : string());
		w.I32(RoadIndex(map, s->attachedRoad));
		w.U32(static_cast<uint32_t>(s->interfaces.size()));
		for (const StationInterface& i : s->interfaces) {
			w.F32(i.x); w.F32(i.y); w.F32(i.z);
			w.F32(i.arriveDirX); w.F32(i.arriveDirY);
			w.F32(i.departDirX); w.F32(i.departDirY);
		}
	}

	// 线路：站点表 + graph里的每条连线(正向/反向) + 各条线路的停靠顺序 + 每条线路上的车。
	vector<string> routeNames = SortedKeys(traffic.routes);
	w.U32(static_cast<uint32_t>(routeNames.size()));
	for (const string& name : routeNames) {
		const Route* route = traffic.routes.at(name);
		w.Str(name);
		w.Str(SaveRefs::LookupId(routeIds, static_cast<const RouteMod*>(route->mod)));
		SaveRefs::WriteModState(route->mod, w);

		unordered_map<const Station*, int> stopIndex;
		w.U32(static_cast<uint32_t>(route->stopStations.size()));
		for (size_t i = 0; i < route->stopStations.size(); i++) {
			stopIndex[route->stopStations[i]] = static_cast<int>(i);
			w.Str(route->stopStations[i] ? route->stopStations[i]->GetName() : string());
		}

		vector<tuple<int, int, uint8_t>> edges;
		for (auto& [fromKey, inner] : route->graph) {
			for (auto& [toKey, edge] : inner) edges.emplace_back(fromKey, toKey, edge.reversed ? 1 : 0);
		}
		sort(edges.begin(), edges.end());
		w.U32(static_cast<uint32_t>(edges.size()));
		for (auto& [fromKey, toKey, reversed] : edges) { w.I32(fromKey); w.I32(toKey); w.U8(reversed); }

		auto stopOf = [&](const Station* station) {
			auto it = stopIndex.find(station);
			return it != stopIndex.end() ? it->second : -1;
			};
		w.U32(static_cast<uint32_t>(route->lines.size()));
		for (const vector<Route::RouteLeg>& legs : route->lines) {
			w.U32(static_cast<uint32_t>(legs.size()));
			for (const Route::RouteLeg& leg : legs) {
				w.I32(stopOf(leg.fromStation)); w.I32(leg.fromInterfaceIndex);
				w.I32(stopOf(leg.toStation)); w.I32(leg.toInterfaceIndex);
				w.U8(leg.dwellAtDestination ? 1 : 0);
			}
		}

		w.U32(static_cast<uint32_t>(route->lineVehicles.size()));
		for (const vector<Vehicle*>& vehicles : route->lineVehicles) {
			w.U32(static_cast<uint32_t>(vehicles.size()));
			for (const Vehicle* vehicle : vehicles) w.Str(vehicle ? vehicle->GetName() : string());
		}
	}

	vector<string> vehicleNames = SortedKeys(traffic.vehicles);
	w.U32(static_cast<uint32_t>(vehicleNames.size()));
	for (const string& name : vehicleNames) {
		const Vehicle* v = traffic.vehicles.at(name);
		w.Str(name);
		w.Str(SaveRefs::LookupId(vehicleIds, static_cast<const VehicleMod*>(v->mod)));
		SaveRefs::WriteModState(v->mod, w);
		WriteStrings(v->options, w);
		SaveRefs::WriteRoom(v->room, w);
		w.F32(v->parkingLocalX); w.F32(v->parkingLocalY); w.F32(v->parkingRotationDegrees);
		w.Str(v->route ? v->route->GetName() : string());
		w.I32(v->routeLine);
		w.F32(v->x); w.F32(v->y); w.F32(v->z); w.F32(v->yaw);
	}

	archive.SetBinary(SAVE_SECTION_TRAFFIC, VersionOf(SAVE_SECTION_TRAFFIC), w.Take());
}

Traffic* TrafficSaveCodec::Load(const SaveArchive& archive, Map& map, bool restoreOptions, string& error) {
	const SaveSection* section = archive.Find(SAVE_SECTION_TRAFFIC);
	if (!section) return nullptr;

	try {
		ByteReader r(section->data);
		Traffic* traffic = new Traffic();
		traffic->vehicleCounter = r.I32();
		traffic->stationCounter = r.I32();
		traffic->elapsedSeconds = r.F32();
		// 公交位置由elapsedSeconds推算；上一帧的游戏时间不恢复，下一次Tick重新定锚(读档后游戏
		// 时钟可能和存档时不一致，用旧的lastTickTime会算出负的时间差，公交停住不动)。
		traffic->hasLastTickTime = false;

		uint32_t stationCount = r.U32();
		for (uint32_t i = 0; i < stationCount && !r.Failed(); i++) {
			string name = r.Str();
			uint8_t kind = r.U8();
			string modId = r.Str();
			int stateVersion = r.I32();
			string stateText = r.Str();
			string buildingName = r.Str();
			int roadIndex = r.I32();
			vector<StationInterface> interfaces(r.U32());
			for (StationInterface& iface : interfaces) {
				iface.x = r.F32(); iface.y = r.F32(); iface.z = r.F32();
				iface.arriveDirX = r.F32(); iface.arriveDirY = r.F32();
				iface.departDirX = r.F32(); iface.departDirY = r.F32();
			}
			if (r.Failed()) break;

			// 用原来的构造函数建(mod创建和SetProperty同新开局)，接口直接用存档里的值覆盖。
			Station* station = nullptr;
			if (kind == STATION_EDGE) {
				station = new Station(name, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f);
			}
			else if (kind == STATION_BUILDING) {
				station = new Station(&traffic->stationFactory, modId, name, map.GetBuilding(buildingName));
			}
			else {
				Road* road = (roadIndex >= 0 && roadIndex < static_cast<int>(map.GetRoads().size())) ? map.GetRoads()[roadIndex] : nullptr;
				station = new Station(&traffic->stationFactory, modId, name, 0.f, 0.f, 0.f, 0.f, road);
			}
			station->interfaces = interfaces;
			if (station->mod && !stateText.empty()) {
				JsonValue root;
				JsonReader reader;
				if (reader.Parse(stateText, root, false)) {
					JsonStateReader stateReader(root, stateVersion);
					station->mod->LoadState(stateReader);
				}
			}
			traffic->stations[name] = station;
		}
		auto stationByName = [&](const string& name) -> Station* {
			auto it = traffic->stations.find(name);
			return it != traffic->stations.end() ? it->second : nullptr;
			};

		// 线路：车辆还没建，先记下每条线路上车辆的名字，建完车辆再挂上去。
		vector<pair<Route*, vector<vector<string>>>> pendingLineVehicles;
		uint32_t routeCount = r.U32();
		for (uint32_t i = 0; i < routeCount && !r.Failed(); i++) {
			string name = r.Str();
			string modId = r.Str();
			Route* route = new Route(&traffic->routeFactory, modId, name);
			traffic->routes[name] = route;
			if (!SaveRefs::ReadModState(route->mod, r)) debugf("Warning: failed to parse saved state of route mod %s.\n", modId.c_str());

			uint32_t stopCount = r.U32();
			for (uint32_t k = 0; k < stopCount && !r.Failed(); k++) route->stopStations.push_back(stationByName(r.Str()));
			auto stopAt = [&](int index) -> Station* {
				return (index >= 0 && index < static_cast<int>(route->stopStations.size())) ? route->stopStations[index] : nullptr;
				};

			// 连线：正向的按两端站点接口重新算几何；反向的共用正向那一份(owned=false)。
			vector<tuple<int, int, uint8_t>> edges(r.U32());
			for (auto& [fromKey, toKey, reversed] : edges) { fromKey = r.I32(); toKey = r.I32(); reversed = r.U8(); }
			for (auto& [fromKey, toKey, reversed] : edges) {
				if (reversed) continue;
				Route::RouteEdge edge = route->BuildEdgeGeometry(&map, stopAt(fromKey / 100), fromKey % 100,
					stopAt(toKey / 100), toKey % 100);
				route->graph[fromKey][toKey] = edge;
			}
			for (auto& [fromKey, toKey, reversed] : edges) {
				if (!reversed) continue;
				auto forward = route->graph.find(toKey);
				if (forward == route->graph.end() || !forward->second.count(fromKey)) continue;
				Route::RouteEdge edge = forward->second.at(fromKey);
				edge.owned = false;
				edge.reversed = true;
				route->graph[fromKey][toKey] = edge;
			}

			uint32_t lineCount = r.U32();
			for (uint32_t k = 0; k < lineCount && !r.Failed(); k++) {
				vector<Route::RouteLeg> legs;
				uint32_t legCount = r.U32();
				for (uint32_t m = 0; m < legCount && !r.Failed(); m++) {
					int fromStop = r.I32(), fromInterface = r.I32();
					int toStop = r.I32(), toInterface = r.I32();
					bool dwell = r.U8() != 0;
					Route::RouteLeg leg;
					leg.fromStation = stopAt(fromStop);
					leg.fromInterfaceIndex = fromInterface;
					leg.toStation = stopAt(toStop);
					leg.toInterfaceIndex = toInterface;
					leg.dwellAtDestination = dwell;
					auto graphIt = route->graph.find(Route::PackStop(fromStop, fromInterface));
					if (graphIt != route->graph.end()) {
						auto edgeIt = graphIt->second.find(Route::PackStop(toStop, toInterface));
						if (edgeIt != graphIt->second.end()) leg.edge = &edgeIt->second;
					}
					if (leg.edge) legs.push_back(leg);
				}
				route->lines.push_back(move(legs));
			}

			vector<vector<string>> lineVehicles(r.U32());
			for (vector<string>& names : lineVehicles) names = ReadStrings(r);
			pendingLineVehicles.emplace_back(route, move(lineVehicles));
		}

		uint32_t vehicleCount = r.U32();
		for (uint32_t i = 0; i < vehicleCount && !r.Failed(); i++) {
			string name = r.Str();
			string modId = r.Str();
			int stateVersion = r.I32();
			string stateText = r.Str();
			vector<string> options = ReadStrings(r);
			Room* room = SaveRefs::ReadRoom(map, r);
			float localX = r.F32(), localY = r.F32(), rotation = r.F32();
			string routeName = r.Str();
			int routeLine = r.I32();
			float x = r.F32(), y = r.F32(), z = r.F32(), yaw = r.F32();
			if (r.Failed()) break;

			Vehicle* vehicle = new Vehicle(&traffic->vehicleFactory, &traffic->scriptFactory, modId, name);
			if (!vehicle->IsValid()) {
				debugf("Warning: vehicle mod %s is not available, vehicle %s dropped.\n", modId.c_str(), name.c_str());
				delete vehicle;
				continue;
			}
			traffic->vehicles[name] = vehicle;
			if (vehicle->mod && !stateText.empty()) {
				JsonValue root;
				JsonReader reader;
				if (reader.Parse(stateText, root, false)) {
					JsonStateReader stateReader(root, stateVersion);
					vehicle->mod->LoadState(stateReader);
				}
			}
			if (restoreOptions) vehicle->options = options;
			vehicle->room = room;
			vehicle->parkingLocalX = localX;
			vehicle->parkingLocalY = localY;
			vehicle->parkingRotationDegrees = rotation;
			auto routeIt = traffic->routes.find(routeName);
			vehicle->route = routeIt != traffic->routes.end() ? routeIt->second : nullptr;
			vehicle->routeLine = routeLine;
			vehicle->x = x; vehicle->y = y; vehicle->z = z; vehicle->yaw = yaw;
		}

		for (auto& [route, lineVehicles] : pendingLineVehicles) {
			for (size_t lineIndex = 0; lineIndex < lineVehicles.size(); lineIndex++) {
				for (const string& vehicleName : lineVehicles[lineIndex]) {
					auto it = traffic->vehicles.find(vehicleName);
					if (it != traffic->vehicles.end()) route->AddVehicleToLine(lineIndex, it->second);
				}
			}
			if (route->lineVehicles.size() < lineVehicles.size()) route->lineVehicles.resize(lineVehicles.size());
		}

		if (r.Failed()) {
			error = "traffic section is truncated";
			return nullptr;
		}
		return traffic;
	}
	catch (const exception& e) {
		error = string("traffic load failed: ") + e.what();
		return nullptr;
	}
	catch (...) {
		error = "traffic load failed with an unknown error";
		return nullptr;
	}
}
