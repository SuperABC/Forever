#include "route_air.h"

#include "route_common.h"
#include "common/mod_args.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cfloat>
#include <functional>
#include <map>
#include <set>

using namespace std;

int AirRoute::count = 0;

AirRoute::AirRoute(const string& args) : id(count++) {
	skipAuto = ModArgs(args).GetBool("skip");
}

const char* AirRoute::GetName() {
	name = "AirRoute" + to_string(id);
	return name.data();
}

void AirRoute::SetProperty() {
	stationType = "plane";
	vehicleType = "vehicle_plane";
	useRoadnet = false;
	instantiate = false;
	tiltAlongPath = true; // 爬升/下降时机头跟路径切线俯仰(Core Route::Update 只在这个开关或 instantiate 下写 pitch)

	// 校准依据见route_air.md：跑一圈约100真实秒、停约6真实秒。三个速度都是绝对值(地图单位/游戏秒)。
	normalSpeed = 0.175f;
	sameStationSpeed = 0.0175f; // 降落跑道滑行到起飞跑道，不能按起飞速度滑
	passSpeed = 0.0875f;        // 飞机目前不会穿过不停的机场，值只是占位
	dwellSeconds = 720.f;

	// 飞机比火车更大更重，启动/刹车更缓：从0加速到normalSpeed用这么久(真实秒)，见route_air.md。
	easeSeconds = 6.f;
}

namespace {
	constexpr float kCruiseHeight = 100.f; // 飞机巡航高度(地图单位)
	// 起飞/降落爬升斜率tan=0.5：水平每出去20抬升10，所以天空点离跑道端头的水平距离=巡航高度/0.5。
	constexpr float kClimbSlope = 0.5f;
	constexpr float kSkyLead = kCruiseHeight / kClimbSlope;
	constexpr float kClimbArm = 16.f;  // 跑道端头<->天空点的控制臂长(160m)
	constexpr float kTaxiArm = 1.f;    // 同一机场两条跑道之间滑行的控制臂长
	constexpr float kExternOffset = 0.01f; // 天空点已在图外：extern在原位稍微沿飞行方向错开，避免零长度的段

	// 一座机场：两条跑道r=0/1，每条跑道的进站口/出站口/停靠位下标都等于r(`station_air`保证)。
	struct Airport {
		RouteStationInfo info;
		string externName(const char* kind, int runway) const { return "air_" + info.name + "_" + kind + to_string(runway); }

		// 跑道朝向(单位向量)：进站口的行驶方向。
		pair<float, float> Heading(int runway) const {
			const RoutePort& in = info.inlets[runway];
			float length = sqrtf(in.dirX * in.dirX + in.dirY * in.dirY);
			if (length < 1e-6f) return { 1.f, 0.f };
			return { in.dirX / length, in.dirY / length };
		}
		// 朝向最接近(dirX,dirY)的跑道。
		int BestRunway(float dirX, float dirY) const {
			int best = 0;
			float bestDot = -FLT_MAX;
			for (int runway = 0; runway < 2; runway++) {
				auto [hx, hy] = Heading(runway);
				float dot = hx * dirX + hy * dirY;
				if (dot > bestDot) { bestDot = dot; best = runway; }
			}
			return best;
		}
		const RoutePort& In(int r) const { return info.inlets[r]; }
		const RoutePort& Out(int r) const { return info.outlets[r]; }
	};

	RouteEndpoint OutletOf(const Airport& airport, int r) {
		RouteEndpoint endpoint; endpoint.kind = RouteEndpoint::Outlet; endpoint.station = airport.info.stationIndex; endpoint.portIndex = r;
		return endpoint;
	}
	RouteEndpoint InletOf(const Airport& airport, int r) {
		RouteEndpoint endpoint; endpoint.kind = RouteEndpoint::Inlet; endpoint.station = airport.info.stationIndex; endpoint.portIndex = r;
		return endpoint;
	}
	RouteEndpoint ExternOf(const string& name) {
		RouteEndpoint endpoint; endpoint.kind = RouteEndpoint::Extern; endpoint.externName = name;
		return endpoint;
	}
	string EndpointKey(const RouteEndpoint& endpoint) {
		return to_string(static_cast<int>(endpoint.kind)) + ":" + to_string(endpoint.station) + ":" + to_string(endpoint.portIndex) + ":" + endpoint.externName;
	}
}

void AirRoute::LayoutRoute(const vector<RouteStationInfo>& stations, int sizeX, int sizeY) {
	if (skipAuto) { LayoutManual(stations); return; }
	vector<Airport> airports;
	for (const RouteStationInfo& info : stations) {
		if (info.inlets.size() < 2 || info.outlets.size() < 2 || info.stops.size() < 2) continue;
		Airport airport;
		airport.info = info;
		airports.push_back(std::move(airport));
	}

	set<pair<string, string>> linkKeys; // 同一条link被多条线路共用只加一次
	auto addLink = [&](const RouteEndpoint& from, const RouteEndpoint& to, vector<TrackSegment> segments, bool withinStation = false) {
		if (!linkKeys.insert({ EndpointKey(from), EndpointKey(to) }).second) return;
		RouteLinkSpec link;
		link.from = from;
		link.to = to;
		link.withinStation = withinStation;
		link.segments = std::move(segments);
		links.push_back(std::move(link));
	};
	auto insideMap = [&](float x, float y) { return x >= 0.f && x <= static_cast<float>(sizeX) && y >= 0.f && y <= static_cast<float>(sizeY); };

	// 天空点：出站口沿跑道方向前kSkyLead、进站口沿跑道方向后kSkyLead，高度=巡航高度。
	auto skyOutPoint = [&](const Airport& airport, int r) {
		auto [hx, hy] = airport.Heading(r);
		return make_pair(airport.Out(r).x + hx * kSkyLead, airport.Out(r).y + hy * kSkyLead);
	};
	auto skyInPoint = [&](const Airport& airport, int r) {
		auto [hx, hy] = airport.Heading(r);
		return make_pair(airport.In(r).x - hx * kSkyLead, airport.In(r).y - hy * kSkyLead);
	};
	// 起飞：出站口爬升到天空点。
	auto climbSegment = [&](const Airport& airport, int r) {
		auto [hx, hy] = airport.Heading(r);
		auto [sx, sy] = skyOutPoint(airport, r);
		return MakeBezierSegment(airport.Out(r).x, airport.Out(r).y, airport.Out(r).z, hx, hy, sx, sy, kCruiseHeight, hx, hy, kClimbArm);
	};
	// 降落：天空点下降到进站口。
	auto descendSegment = [&](const Airport& airport, int r) {
		auto [hx, hy] = airport.Heading(r);
		auto [sx, sy] = skyInPoint(airport, r);
		return MakeBezierSegment(sx, sy, kCruiseHeight, hx, hy, airport.In(r).x, airport.In(r).y, airport.In(r).z, hx, hy, kClimbArm);
	};

	// 连外界：天空点本身不等于extern(它可能只是机场互联线路的点，互联不算出城)——天空点在地图范围内就沿跑道方向水平延长到地图边界建一个extern；
	// 已在图外就在天空点原位(沿飞行方向错开kExternOffset，避免零长度的段)。
	// out=true：extern在飞出方向；out=false：在飞入方向(沿跑道反方向回溯)。
	auto externPoint = [&](const Airport& airport, int r, bool out) {
		auto [hx, hy] = airport.Heading(r);
		auto [sx, sy] = out ? skyOutPoint(airport, r) : skyInPoint(airport, r);
		float dirX = out ? hx : -hx, dirY = out ? hy : -hy;
		return insideMap(sx, sy) ? RayBoxIntersection(sx, sy, dirX, dirY, sizeX, sizeY)
			: make_pair(sx + dirX * kExternOffset, sy + dirY * kExternOffset);
	};

	// 每座机场：每条跑道各有一个"飞出"extern和一个"飞入"extern，起飞->飞出extern、飞出extern->飞入extern(出城再回来的直线)、
	// 飞入extern->降落，自成一圈环线：
	// 跑道0停靠 -> 飞出0 -> 飞入1 -> 跑道1停靠 -> 飞出1 -> 飞入0 -> 回跑道0停靠。飞机在extern处直接转向，没有停靠。
	for (const Airport& airport : airports) {
		for (int r = 0; r < 2; r++) {
			auto [hx, hy] = airport.Heading(r);
			auto [sox, soy] = skyOutPoint(airport, r);
			auto [six, siy] = skyInPoint(airport, r);
			auto [eox, eoy] = externPoint(airport, r, true);
			auto [eix, eiy] = externPoint(airport, r, false);

			RouteExternNode out;
			out.name = airport.externName("out", r);
			out.x = eox; out.y = eoy; out.z = kCruiseHeight; out.dirX = hx; out.dirY = hy;
			externs.push_back(out);
			RouteExternNode in;
			in.name = airport.externName("in", r);
			in.x = eix; in.y = eiy; in.z = kCruiseHeight; in.dirX = hx; in.dirY = hy;
			externs.push_back(in);

			addLink(OutletOf(airport, r), ExternOf(out.name), { climbSegment(airport, r),
				MakeBezierSegment(sox, soy, kCruiseHeight, hx, hy, eox, eoy, kCruiseHeight, hx, hy, 0.f) });
			addLink(ExternOf(in.name), InletOf(airport, r), { MakeBezierSegment(eix, eiy, kCruiseHeight, hx, hy, six, siy, kCruiseHeight, hx, hy, 0.f),
				descendSegment(airport, r) });
		}
		for (int r = 0; r < 2; r++) {
			int other = 1 - r;
			auto [hx, hy] = airport.Heading(r);
			auto [ix, iy] = airport.Heading(other);
			auto [eox, eoy] = externPoint(airport, r, true);
			auto [eix, eiy] = externPoint(airport, other, false);
			addLink(ExternOf(airport.externName("out", r)), ExternOf(airport.externName("in", other)),
				{ MakeBezierSegment(eox, eoy, kCruiseHeight, hx, hy, eix, eiy, kCruiseHeight, ix, iy, 0.f) });
		}
		const string& name = airport.info.name;
		lines.push_back({ { name, 0 }, { airport.externName("out", 0), -1 }, { airport.externName("in", 1), -1 },
			{ name, 1 }, { airport.externName("out", 1), -1 }, { airport.externName("in", 0), -1 } });
	}

	// 机场两两互联：X用朝向最接近目标方向的跑道起飞，Y用朝向最接近来向的跑道降落，
	// X的跑道出站口 -> Y的跑道进站口 一条完整的飞行曲线(起飞爬升 -> 天空点之间贝塞尔，控制臂=两点水平距离1/3 -> 降落)。一对机场一架互联飞机，不考虑调度。
	for (size_t a = 0; a < airports.size(); a++)
	for (size_t b = a + 1; b < airports.size(); b++) {
		const Airport& A = airports[a];
		const Airport& B = airports[b];
		float dx = B.info.x - A.info.x, dy = B.info.y - A.info.y;
		float length = sqrtf(dx * dx + dy * dy);
		if (length < 1e-3f) continue;
		dx /= length; dy /= length;

		int takeoffA = A.BestRunway(dx, dy), landingA = A.BestRunway(-dx, -dy);
		int takeoffB = B.BestRunway(-dx, -dy), landingB = B.BestRunway(dx, dy);
		auto flight = [&](const Airport& from, int takeoff, const Airport& to, int landing) {
			auto [hx, hy] = from.Heading(takeoff);
			auto [tx, ty] = to.Heading(landing);
			auto [sox, soy] = skyOutPoint(from, takeoff);
			auto [six, siy] = skyInPoint(to, landing);
			float arm = sqrtf((six - sox) * (six - sox) + (siy - soy) * (siy - soy)) / 3.f;
			addLink(OutletOf(from, takeoff), InletOf(to, landing), { climbSegment(from, takeoff),
				MakeBezierSegment(sox, soy, kCruiseHeight, hx, hy, six, siy, kCruiseHeight, tx, ty, arm),
				descendSegment(to, landing) });
		};
		flight(A, takeoffA, B, landingB);
		flight(B, takeoffB, A, landingA);

		// 一座机场的停靠：降落跑道(停) -> (换跑道则滑行转场，同站转场按同站速度) -> 起飞跑道(停)。
		auto stay = [&](const Airport& X, int landing, int takeoff, vector<RouteLineEntry>& line) {
			line.push_back({ X.info.name, landing });
			if (landing != takeoff) {
				auto [hx, hy] = X.Heading(landing);
				auto [tx, ty] = X.Heading(takeoff);
				addLink(OutletOf(X, landing), InletOf(X, takeoff), { MakeBezierSegment(X.Out(landing).x, X.Out(landing).y, X.Out(landing).z, hx, hy,
					X.In(takeoff).x, X.In(takeoff).y, X.In(takeoff).z, tx, ty, kTaxiArm) }, true);
				line.push_back({ X.info.name, takeoff });
			}
		};
		vector<RouteLineEntry> line;
		stay(A, landingA, takeoffA, line);
		stay(B, landingB, takeoffB, line);
		lines.push_back(std::move(line));
	}
}


// ================= 手动线路(--skip)：编辑器output.transit的航线，设计见route_air.md"手动线路" =================
// 节点z=飞行高度。每个节点一个extern(飞机在extern处直接转向，同自动布局)，每条连线两个方向各一条link(无偏移、一段三次Bezier)。
// 站和节点之间的缺口：一段Bezier(沿跑道朝向起降)；两座机场之间没有航线的缺口：起飞爬升->天空点->降落，同自动互联。
void AirRoute::LayoutManual(const vector<RouteStationInfo>& stationInfos) {
	if (!manual || manual->Empty()) {
		debugf(DebugLevel::Warning, "AirRoute --skip but the roadnet has no manual transit data, no air lines.\n");
		return;
	}
	const TransitData& data = *manual;
	const string routeId = GetType();
	ManualPlan plan = PlanManualRoute(data, routeId, stationInfos, true);
	for (const string& warning : plan.warnings) debugf(DebugLevel::Warning, "AirRoute manual: %s\n", warning.c_str());
	if (plan.lines.empty()) {
		debugf(DebugLevel::Warning, "AirRoute manual: no usable air line in the roadnet loops of %s.\n", routeId.c_str());
		return;
	}

	set<pair<string, string>> linkKeys;
	auto addLink = [&](const RouteEndpoint& from, const RouteEndpoint& to, vector<TrackSegment> segments, bool withinStation = false) {
		if (!linkKeys.insert({ EndpointKey(from), EndpointKey(to) }).second) return;
		RouteLinkSpec link;
		link.from = from;
		link.to = to;
		link.withinStation = withinStation;
		link.segments = std::move(segments);
		links.push_back(std::move(link));
	};
	set<int> madeNodes;
	auto nodeExtern = [&](int node) {
		string name = "air_n" + to_string(node);
		if (madeNodes.insert(node).second) {
			const TransitData::Node& n = data.nodes[node];
			RouteExternNode extern_;
			extern_.name = name; extern_.x = n.x; extern_.y = n.y; extern_.z = n.z; extern_.dirX = 1.f; extern_.dirY = 0.f;
			externs.push_back(extern_);
		}
		return name;
	};
	auto heading = [&](const RouteStationInfo& info, int t) {
		const RoutePort& in = info.inlets[t];
		float length = sqrtf(in.dirX * in.dirX + in.dirY * in.dirY);
		return length > 1e-6f ? make_pair(in.dirX / length, in.dirY / length) : make_pair(1.f, 0.f);
	};

	// 实体航线：每条用到的连线两个方向各一条link。
	for (int c : plan.conns) {
		const TransitData::Conn& conn = data.conns[c];
		TrackSegment axis = ManualAxis(data, c);
		TrackSegment back = axis;
		back.x1 = axis.x2; back.y1 = axis.y2; back.z1 = axis.z2;
		back.c1x = axis.c2x; back.c1y = axis.c2y; back.c1z = axis.c2z;
		back.c2x = axis.c1x; back.c2y = axis.c1y; back.c2z = axis.c1z;
		back.x2 = axis.x1; back.y2 = axis.y1; back.z2 = axis.z1;
		addLink(ExternOf(nodeExtern(conn.from)), ExternOf(nodeExtern(conn.to)), { axis });
		addLink(ExternOf(nodeExtern(conn.to)), ExternOf(nodeExtern(conn.from)), { back });
	}

	// 缺口。
	for (const ManualGap& gap : plan.gaps) {
		const ManualPort& pf = gap.from;
		const ManualPort& pt = gap.to;
		bool fromStation = pf.kind == ManualPort::StationOut, toStation = pt.kind == ManualPort::StationIn;
		if (fromStation && toStation && gap.withinStation) {
			const RouteStationInfo& info = stationInfos[pf.station];
			auto [hx, hy] = heading(info, pf.track);
			auto [tx, ty] = heading(info, pt.track);
			const RoutePort& out = info.outlets[pf.track];
			const RoutePort& in = info.inlets[pt.track];
			RouteEndpoint from; from.kind = RouteEndpoint::Outlet; from.station = info.stationIndex; from.portIndex = pf.track;
			RouteEndpoint to; to.kind = RouteEndpoint::Inlet; to.station = info.stationIndex; to.portIndex = pt.track;
			addLink(from, to, { MakeBezierSegment(out.x, out.y, out.z, hx, hy, in.x, in.y, in.z, tx, ty, kTaxiArm) }, true);
		}
		else if (fromStation && toStation) {
			const RouteStationInfo& A = stationInfos[pf.station];
			const RouteStationInfo& B = stationInfos[pt.station];
			auto [hx, hy] = heading(A, pf.track);
			auto [tx, ty] = heading(B, pt.track);
			const RoutePort& out = A.outlets[pf.track];
			const RoutePort& in = B.inlets[pt.track];
			float sox = out.x + hx * kSkyLead, soy = out.y + hy * kSkyLead;
			float six = in.x - tx * kSkyLead, siy = in.y - ty * kSkyLead;
			float arm = hypotf(six - sox, siy - soy) / 3.f;
			RouteEndpoint from; from.kind = RouteEndpoint::Outlet; from.station = A.stationIndex; from.portIndex = pf.track;
			RouteEndpoint to; to.kind = RouteEndpoint::Inlet; to.station = B.stationIndex; to.portIndex = pt.track;
			addLink(from, to, { MakeBezierSegment(out.x, out.y, out.z, hx, hy, sox, soy, kCruiseHeight, hx, hy, kClimbArm),
				MakeBezierSegment(sox, soy, kCruiseHeight, hx, hy, six, siy, kCruiseHeight, tx, ty, arm),
				MakeBezierSegment(six, siy, kCruiseHeight, tx, ty, in.x, in.y, in.z, tx, ty, kClimbArm) });
		}
		else if (fromStation || toStation) {
			// 站<->节点：一段Bezier，站端沿跑道朝向，节点端沿弦的方向。
			const RouteStationInfo& info = stationInfos[fromStation ? pf.station : pt.station];
			int track = fromStation ? pf.track : pt.track;
			const ManualPort& nodePort = fromStation ? pt : pf;
			const TransitData::Node& node = data.nodes[nodePort.node];
			auto [hx, hy] = heading(info, track);
			const RoutePort& port = fromStation ? info.outlets[track] : info.inlets[track];
			float dx = node.x - port.x, dy = node.y - port.y, chord = hypotf(dx, dy);
			if (chord < 1e-3f) continue;
			float gx = fromStation ? dx / chord : -dx / chord, gy = fromStation ? dy / chord : -dy / chord; // 行驶方向的弦向
			float alongStation = (fromStation ? 1.f : -1.f) * (hx * dx + hy * dy) / chord;
			if (alongStation < 0.5f) {
				debugf(DebugLevel::Warning, "AirRoute manual: gap between station %s runway %d and transit node %d bends more than 60 deg; move the node in line with the runway.\n",
					info.name.c_str(), track, nodePort.node);
			}
			if (fabsf(node.z - port.z) / chord > 0.6f) {
				debugf(DebugLevel::Warning, "AirRoute manual: gap between station %s runway %d and transit node %d climbs steeper than 0.6; raise the node farther away or lower its height.\n",
					info.name.c_str(), track, nodePort.node);
			}
			string externName = nodeExtern(nodePort.node);
			if (fromStation) {
				RouteEndpoint from; from.kind = RouteEndpoint::Outlet; from.station = info.stationIndex; from.portIndex = track;
				addLink(from, ExternOf(externName), { MakeBezierSegment(port.x, port.y, port.z, hx, hy, node.x, node.y, node.z, gx, gy, chord / 3.f) });
			}
			else {
				RouteEndpoint to; to.kind = RouteEndpoint::Inlet; to.station = info.stationIndex; to.portIndex = track;
				addLink(ExternOf(externName), to, { MakeBezierSegment(node.x, node.y, node.z, gx, gy, port.x, port.y, port.z, hx, hy, chord / 3.f) });
			}
		}
		else if (pf.node >= 0 && pt.node >= 0 && pf.node != pt.node) {
			// 节点<->节点没有实体航线：直连。
			const TransitData::Node& a = data.nodes[pf.node];
			const TransitData::Node& b = data.nodes[pt.node];
			float dx = b.x - a.x, dy = b.y - a.y, chord = hypotf(dx, dy);
			if (chord < 1e-3f) continue;
			addLink(ExternOf(nodeExtern(pf.node)), ExternOf(nodeExtern(pt.node)),
				{ MakeBezierSegment(a.x, a.y, a.z, dx / chord, dy / chord, b.x, b.y, b.z, dx / chord, dy / chord, 0.f) });
		}
	}

	// 线路
	for (const vector<ManualLineEntry>& line : plan.lines) {
		vector<RouteLineEntry> entries;
		for (const ManualLineEntry& entry : line) {
			RouteLineEntry out;
			if (entry.station >= 0) { out.name = stationInfos[entry.station].name; out.stopIndex = entry.track; }
			else { out.name = nodeExtern(entry.via.node); out.stopIndex = -1; }
			entries.push_back(out);
		}
		if (entries.size() < 2) { debugf(DebugLevel::Warning, "AirRoute manual: a line has fewer than 2 waypoints, dropped.\n"); continue; }
		lines.push_back(std::move(entries));
	}
	debugf(DebugLevel::Log, "AirRoute manual: %d links, %d lines.\n", static_cast<int>(links.size()), static_cast<int>(lines.size()));
}
