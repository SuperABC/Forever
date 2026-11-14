#include "traffic_save.h"

#include "traffic/traffic.h"
#include "story/script.h"

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

	void WriteSegments(const vector<TrackSegment>& segments, ByteWriter& w) {
		w.U32(static_cast<uint32_t>(segments.size()));
		for (const TrackSegment& seg : segments) {
			w.F32(seg.x1); w.F32(seg.y1); w.F32(seg.z1);
			w.F32(seg.c1x); w.F32(seg.c1y); w.F32(seg.c1z);
			w.F32(seg.c2x); w.F32(seg.c2y); w.F32(seg.c2z);
			w.F32(seg.x2); w.F32(seg.y2); w.F32(seg.z2);
			w.Str(seg.mesh); w.F32(seg.meshUnit);
		}
	}
	vector<TrackSegment> ReadSegments(ByteReader& r) {
		vector<TrackSegment> segments(r.U32());
		for (TrackSegment& seg : segments) {
			seg.x1 = r.F32(); seg.y1 = r.F32(); seg.z1 = r.F32();
			seg.c1x = r.F32(); seg.c1y = r.F32(); seg.c1z = r.F32();
			seg.c2x = r.F32(); seg.c2y = r.F32(); seg.c2z = r.F32();
			seg.x2 = r.F32(); seg.y2 = r.F32(); seg.z2 = r.F32();
			seg.mesh = r.Str(); seg.meshUnit = r.F32();
		}
		return segments;
	}
	void WritePorts(const vector<StationPort>& ports, ByteWriter& w) {
		w.U32(static_cast<uint32_t>(ports.size()));
		for (const StationPort& p : ports) { w.F32(p.x); w.F32(p.y); w.F32(p.z); w.F32(p.dirX); w.F32(p.dirY); }
	}
	vector<StationPort> ReadPorts(ByteReader& r) {
		vector<StationPort> ports(r.U32());
		for (StationPort& p : ports) { p.x = r.F32(); p.y = r.F32(); p.z = r.F32(); p.dirX = r.F32(); p.dirY = r.F32(); }
		return ports;
	}
	void WriteEndpoint(const RouteEndpoint& e, ByteWriter& w) {
		w.U8(static_cast<uint8_t>(e.kind)); w.I32(e.station); w.I32(e.portIndex); w.Str(e.externName);
	}
	RouteEndpoint ReadEndpoint(ByteReader& r) {
		RouteEndpoint e;
		e.kind = static_cast<RouteEndpoint::Kind>(r.U8()); e.station = r.I32(); e.portIndex = r.I32(); e.externName = r.Str();
		return e;
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
		uint8_t kind = s->building ? STATION_BUILDING : STATION_ROADSIDE;
		w.Str(name);
		w.U8(kind);
		w.Str(SaveRefs::LookupId(stationIds, static_cast<const StationMod*>(s->mod)));
		SaveRefs::WriteModState(s->mod, w);
		w.Str(s->building ? s->building->GetName() : string());
		w.I32(RoadIndex(map, s->attachedRoad));
		WritePorts(s->inlets, w);
		WritePorts(s->outlets, w);
		WritePorts(s->stops, w);
		w.U32(static_cast<uint32_t>(s->connections.size()));
		for (const StationConnection& connection : s->connections) {
			w.U8(static_cast<uint8_t>(connection.fromKind)); w.I32(connection.fromIndex);
			w.U8(static_cast<uint8_t>(connection.toKind)); w.I32(connection.toIndex);
			WriteSegments(connection.segments, w);
		}
	}

	// 线路：mod状态 + 站点表 + extern点/站间连线(最终几何值)/停靠顺序 + 每条线路上的车；寻路图和运动学读档后重建。
	vector<string> routeNames = SortedKeys(traffic.routes);
	w.U32(static_cast<uint32_t>(routeNames.size()));
	for (const string& name : routeNames) {
		const Route* route = traffic.routes.at(name);
		w.Str(name);
		w.Str(SaveRefs::LookupId(routeIds, static_cast<const RouteMod*>(route->mod)));
		SaveRefs::WriteModState(route->mod, w);

		w.U32(static_cast<uint32_t>(route->stopStations.size()));
		for (const Station* station : route->stopStations) w.Str(station ? station->GetName() : string());

		w.U32(static_cast<uint32_t>(route->mod->externs.size()));
		for (const RouteExternNode& node : route->mod->externs) {
			w.Str(node.name);
			w.F32(node.x); w.F32(node.y); w.F32(node.z); w.F32(node.dirX); w.F32(node.dirY);
		}
		w.U32(static_cast<uint32_t>(route->mod->links.size()));
		for (const RouteLinkSpec& link : route->mod->links) {
			WriteEndpoint(link.from, w);
			WriteEndpoint(link.to, w);
			w.U8(link.withinStation ? 1 : 0);
			// instantiate线路的分段几何：地形已经按它抬升/挖洞，读档不能重新规划，直接存值。
			WriteSegments(link.segments, w);
		}
		w.U32(static_cast<uint32_t>(route->mod->lines.size()));
		for (const vector<RouteLineEntry>& entries : route->mod->lines) {
			w.U32(static_cast<uint32_t>(entries.size()));
			for (const RouteLineEntry& entry : entries) { w.Str(entry.name); w.I32(entry.stopIndex); }
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

Traffic* TrafficSaveCodec::Load(const SaveArchive& archive, Map& map, string& error) {
	const SaveSection* section = archive.Find(SAVE_SECTION_TRAFFIC);
	if (!section) {
		error = "the save has no section '" SAVE_SECTION_TRAFFIC "'";
		return nullptr;
	}

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
				vector<StationPort> inlets = ReadPorts(r), outlets = ReadPorts(r), stops = ReadPorts(r);
				vector<StationConnection> connections(r.U32());
			for (StationConnection& connection : connections) {
				connection.fromKind = static_cast<PortKind>(r.U8()); connection.fromIndex = r.I32();
				connection.toKind = static_cast<PortKind>(r.U8()); connection.toIndex = r.I32();
				connection.segments = ReadSegments(r);
			}
			if (r.Failed()) break;

			// 用原来的构造函数建(mod创建和SetProperty同新开局)，接口直接用存档里的值覆盖。
			Station* station = nullptr;
			if (kind == STATION_BUILDING) {
				station = new Station(&traffic->stationFactory, modId, name, map.GetBuilding(buildingName));
			}
			else {
				Road* road = (roadIndex >= 0 && roadIndex < static_cast<int>(map.GetRoads().size())) ? map.GetRoads()[roadIndex] : nullptr;
				station = new Station(&traffic->stationFactory, modId, name, 0.f, 0.f, 0.f, 0.f, road);
			}
			station->inlets = inlets; station->outlets = outlets; station->stops = stops;
			station->connections = connections;
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
			route->SilenceInitialStopEvents(); // 读档后第一次Update只记录停靠状态，不补发enter_stop
			if (!SaveRefs::ReadModState(route->mod, r)) debugf("Warning: failed to parse saved state of route mod %s.\n", modId.c_str());

			uint32_t stopCount = r.U32();
			for (uint32_t k = 0; k < stopCount && !r.Failed(); k++) route->stopStations.push_back(stationByName(r.Str()));

			uint32_t externCount = r.U32();
			for (uint32_t k = 0; k < externCount && !r.Failed(); k++) {
				RouteExternNode node;
				node.name = r.Str();
				node.x = r.F32(); node.y = r.F32(); node.z = r.F32(); node.dirX = r.F32(); node.dirY = r.F32();
				route->mod->externs.push_back(std::move(node));
			}
			uint32_t linkCount = r.U32();
			for (uint32_t k = 0; k < linkCount && !r.Failed(); k++) {
				RouteLinkSpec link;
				link.from = ReadEndpoint(r);
				link.to = ReadEndpoint(r);
				link.withinStation = r.U8() != 0;
				link.segments = ReadSegments(r);
				route->mod->links.push_back(std::move(link));
			}
			uint32_t lineCount = r.U32();
			for (uint32_t k = 0; k < lineCount && !r.Failed(); k++) {
				vector<RouteLineEntry> entries(r.U32());
				for (RouteLineEntry& entry : entries) { entry.name = r.Str(); entry.stopIndex = r.I32(); }
				route->mod->lines.push_back(std::move(entries));
			}
			if (r.Failed()) break;
			route->Assemble(&map); // 同新开局：建寻路图+规划每条线路，几何都来自存档

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
			vehicle->options = options;
			vehicle->room = room;
			vehicle->parkingLocalX = localX;
			vehicle->parkingLocalY = localY;
			vehicle->parkingRotationDegrees = rotation;
			auto routeIt = traffic->routes.find(routeName);
			vehicle->route = routeIt != traffic->routes.end() ? routeIt->second : nullptr;
			vehicle->routeLine = routeLine;
			if (vehicle->script && vehicle->route) vehicle->script->SetValue("on_route", ValueType(1));
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
