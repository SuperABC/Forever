#include "route_train.h"

#include "route_common.h"
#include "station_train.h"
#include "common/mod_args.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cfloat>
#include <map>
#include <set>

using namespace std;

// ---- TrainRoute::BuildTrack：隧道只按高度判定(与地形类型无关)，见route_train.md。1单位=10m：
// 铁轨平时在z=1.5(15m)，地形高度>=该阈值的位置是"山"，轨道在山前TUNNEL_RAMP_LENGTH单位内从1.5降到-1.5(15m)，
// 其中靠山的TUNNEL_HATCH_LENGTH单位向地形请求开口；间隙<TUNNEL_MIN_OPEN_LENGTH的两座山合并成一座(一直在隧道里)。 ----
#define TERRAIN_FIT_FALLOFF 1.5f
#define TUNNEL_THRESHOLD_Z 1.5f
#define TUNNEL_DROP -3.f
#define TUNNEL_RAMP_LENGTH 16.f
#define TUNNEL_HATCH_LENGTH 8.f
// 隧道资产房顶比路面高多少(地图单位，1=10m)：端墙从隧道端头的地形最高处封到"该端路面高度+这个值"(也就是资产房顶)，房顶以下留着洞口。
#define TUNNEL_ROOF_CLEARANCE 1.f
#define TUNNEL_MIN_OPEN_LENGTH 32.f
// 桥墩(AddPier，Core选位)：默认墩距40m、最短20m、半径1.5m，估值待PIE调。资产留空=Core/UE用默认圆柱。
#define PIER_DEFAULT_SPACING 4.f
#define PIER_MIN_SPACING 2.f
#define PIER_RADIUS 0.15f
#define TRACK_SAMPLE_STEP 1.f

// 车站两端各留这么多个采样点保持水平，不在车站范围内爬坡/下坡。
#define TRACK_STATION_FLAT_SAMPLES 2

namespace {
	float SmoothStep(float u) { return u * u * (3.f - 2.f * u); }
}

int TrainRoute::count = 0;

TrainRoute::TrainRoute(const string& args) : id(count++) {
	skipAuto = ModArgs(args).GetBool("skip");
}

const char* TrainRoute::GetName() {
	name = "TrainRoute" + to_string(id);
	return name.data();
}

void TrainRoute::SetProperty() {
	stationType = "train";
	vehicleType = "vehicle_train";
	useRoadnet = false;
	instantiate = true;
	trackWidth = 0.8f; // 比轨道本身宽，给列车和隧道口留余量；轨道mesh见station_train.h(站内连线和这里的站间连线共用)

	// 校准依据见route_train.md：跑一圈约170真实秒、进站停约5真实秒。三个速度都是绝对值(地图单位/游戏秒)。
	normalSpeed = 0.1f;
	sameStationSpeed = 0.1f; // 火车不会同站转场，值只是占位
	passSpeed = 0.05f;       // 穿过不停的车站时减到这个速度
	dwellSeconds = 600.f;

	// 火车体型大，启动/刹车比普通载具更缓：从0加速到normalSpeed用这么久(真实秒)，见route_train.md。
	easeSeconds = 10.f;
}

namespace {
	// 车站外(站间铁轨、站端连接、边界半圆)铺的轨道资产(带底座)：枢轴在中心、沿局部+X延伸，meshUnit必须等于模型沿X的实际长度(地图单位，1单位=10m)。
	// 车站里的轨道用station_train.h的kTrainTrackMesh/kTrainTrackUnit。
	constexpr const char* kTrainBridgeMesh = "/Game/3rdParty/SM_ufekaevdw_tier_3/StaticMeshes/TrackBridge.TrackBridge";
	constexpr float kTrainBridgeUnit = 0.25f;
	constexpr int kVehiclesPerTrainLine = 4; // 每条火车线路上的车辆数
	constexpr float kTrackGauge = 0.5f;    // 中轴线两侧两条铁轨之间的距离(地图单位，1单位=10m)
	constexpr float kStationExtend = 2.f;  // 车站端口向外直线延伸到轴点的距离
	constexpr float kOffsetStep = 4.f;     // 偏移铁轨按这个弧长采样中轴线，相邻采样点之间一段Hermite贝塞尔

	// 一座火车站：两条轨道t=0/1(航向相反)，每条轨道的进站口/出站口/停靠位下标都等于t(`station_train`保证)。
	struct TrainStationLayout {
		RouteStationInfo info;
		float hx[2] = { 1.f, -1.f }, hy[2] = { 0.f, 0.f }; // 轨道航向(单位向量，进站口的行驶方向)

		const RoutePort& In(int t) const { return info.inlets[t]; }
		const RoutePort& Out(int t) const { return info.outlets[t]; }
		// 航向最接近(dx,dy)的轨道；轨道t的出站端也叫"端t"，所以同时也是朝向(dx,dy)的端。
		int BestTrack(float dx, float dy) const {
			return (hx[0] * dx + hy[0] * dy) >= (hx[1] * dx + hy[1] * dy) ? 0 : 1;
		}
	};

	RouteEndpoint OutletOf(const TrainStationLayout& station, int t) {
		RouteEndpoint endpoint; endpoint.kind = RouteEndpoint::Outlet; endpoint.station = station.info.stationIndex; endpoint.portIndex = t;
		return endpoint;
	}
	RouteEndpoint InletOf(const TrainStationLayout& station, int t) {
		RouteEndpoint endpoint; endpoint.kind = RouteEndpoint::Inlet; endpoint.station = station.info.stationIndex; endpoint.portIndex = t;
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

void TrainRoute::LayoutRoute(const vector<RouteStationInfo>& stationInfos, int sizeX, int sizeY) {
	vector<TrainStationLayout> stations;
	for (const RouteStationInfo& info : stationInfos) {
		if (info.inlets.size() < 2 || info.outlets.size() < 2 || info.stops.size() < 2) continue;
		TrainStationLayout station;
		station.info = info;
		for (int t = 0; t < 2; t++) {
			const RoutePort& in = info.inlets[t];
			float length = sqrtf(in.dirX * in.dirX + in.dirY * in.dirY);
			if (length > 1e-6f) { station.hx[t] = in.dirX / length; station.hy[t] = in.dirY / length; }
		}
		stations.push_back(std::move(station));
	}
	railPairs.clear();
	manualPairs.clear();
	manualGapLinks.clear();
	if (skipAuto) { LayoutManual(stationInfos); return; }
	if (stations.empty()) return;

	// link：两个端点之间的完整几何由这里算好，Core只按它建曲线。同一条link被多条线路共用只加一次；每段都铺轨道mesh。
	set<pair<string, string>> linkKeys;
	auto addLink = [&](const RouteEndpoint& from, const RouteEndpoint& to, vector<TrackSegment> segments) {
		if (!linkKeys.insert({ EndpointKey(from), EndpointKey(to) }).second) return;
		RouteLinkSpec link;
		link.from = from;
		link.to = to;
		for (TrackSegment& seg : segments) { seg.mesh = kTrainBridgeMesh; seg.meshUnit = kTrainBridgeUnit; }
		link.segments = std::move(segments);
		links.push_back(std::move(link));
	};
	// ---- 站间铁轨 = 中轴线 + 两侧偏移 ----
	// 车站的"端e"=轨道e的出站口Out(e)和轨道1-e的进站口In(1-e)(同一侧，相距随楼体进深变化，不是定值)。
	// 每端在车站外沿航向直线延伸kStationExtend的位置取一个轴点P，两条铁轨的连接点是P两侧各偏半个轨距：出站轨在行驶方向右手边(O')，进站轨在左手边(I')。
	// 车站端口到连接点的短连接(扁S形)每个车站每端只做一份，后面连到哪条轨道都共用；中轴线两侧的偏移铁轨共用同一组采样点，所以永远平行。
	constexpr float kHalfGauge = kTrackGauge * 0.5f;
	struct EndGeometry { float px, py, pz, hx, hy; };
	set<string> endKeys, pairKeys;
	auto endName = [](const TrainStationLayout& station, int e, const char* suffix) {
		return station.info.name + "_e" + to_string(e) + "_" + suffix;
	};
	auto ensureEnd = [&](const TrainStationLayout& station, int e) {
		const RoutePort& out = station.Out(e);
		const RoutePort& in = station.In(1 - e);
		float hx = station.hx[e], hy = station.hy[e];
		EndGeometry end;
		end.hx = hx; end.hy = hy;
		end.px = (out.x + in.x) * 0.5f + hx * kStationExtend;
		end.py = (out.y + in.y) * 0.5f + hy * kStationExtend;
		end.pz = out.z;
		if (endKeys.insert(station.info.name + to_string(e)).second) {
			float ux = -hy, uy = hx; // 右手侧
			float ox = end.px + ux * kHalfGauge, oy = end.py + uy * kHalfGauge;
			float ix = end.px - ux * kHalfGauge, iy = end.py - uy * kHalfGauge;
			for (int side = 0; side < 2; side++) {
				RouteExternNode node;
				node.name = endName(station, e, side == 0 ? "out" : "in");
				node.x = side == 0 ? ox : ix; node.y = side == 0 ? oy : iy; node.z = end.pz;
				node.dirX = side == 0 ? hx : -hx; node.dirY = side == 0 ? hy : -hy;
				externs.push_back(node);
			}
			// 两端臂长=弦长/3，方向都沿航向：出站口->O'、I'->进站口，扁S形。
			float outArm = sqrtf((ox - out.x) * (ox - out.x) + (oy - out.y) * (oy - out.y)) / 3.f;
			float inArm = sqrtf((in.x - ix) * (in.x - ix) + (in.y - iy) * (in.y - iy)) / 3.f;
			addLink(OutletOf(station, e), ExternOf(endName(station, e, "out")), {
				MakeBezierSegment(out.x, out.y, out.z, hx, hy, ox, oy, end.pz, hx, hy, outArm) });
			addLink(ExternOf(endName(station, e, "in")), InletOf(station, 1 - e), {
				MakeBezierSegment(ix, iy, end.pz, -hx, -hy, in.x, in.y, in.z, -hx, -hy, inArm) });
		}
		return end;
	};

	// 车站A的端ea和车站B的端eb之间：一条中轴线，两侧各偏半个轨距，A->B 和 B->A 两条铁轨一起生成。
	auto connectEnds = [&](const TrainStationLayout& a, int ea, const TrainStationLayout& b, int eb) {
		EndGeometry ga = ensureEnd(a, ea), gb = ensureEnd(b, eb);
		string keyA = a.info.name + to_string(ea), keyB = b.info.name + to_string(eb);
		if (!pairKeys.insert(keyA < keyB ? keyA + "|" + keyB : keyB + "|" + keyA).second) return;
		float arm = sqrtf((gb.px - ga.px) * (gb.px - ga.px) + (gb.py - ga.py) * (gb.py - ga.py)) / 3.f;
		TrackSegment axis = MakeBezierSegment(ga.px, ga.py, ga.pz, ga.hx, ga.hy, gb.px, gb.py, gb.pz, -gb.hx, -gb.hy, arm);
		vector<CurveSample> samples = SampleBezier(axis, kOffsetStep, ga.hx, ga.hy);
		size_t before = links.size();
		addLink(ExternOf(endName(a, ea, "out")), ExternOf(endName(b, eb, "in")), OffsetRail(samples, kHalfGauge, false, ga.pz, gb.pz, true));
		addLink(ExternOf(endName(b, eb, "out")), ExternOf(endName(a, ea, "in")), OffsetRail(samples, -kHalfGauge, true, ga.pz, gb.pz, true));
		if (links.size() == before + 2) railPairs.push_back({ static_cast<int>(before), static_cast<int>(before) + 1, SampleBezier(axis, TRACK_SAMPLE_STEP, ga.hx, ga.hy) });
	};

	// 沿station的端e向label边界('N'/'S'/'W'/'E')伸出再回来：中轴线从轴点P到边界垂足(从航向前进D/3的控制点到边界的垂足)，两条铁轨是它两侧的偏移，
	// 在边界用半径=半轨距的半圆掉头；半圆顶点(正好在边界上)就是extern node，返回它的名字。
	auto boundaryExtern = [&](const TrainStationLayout& station, int e, char label) {
		float nx = 0.f, ny = 0.f;
		switch (label) {
		case 'N': ny = -1.f; break;
		case 'S': ny = 1.f; break;
		case 'W': nx = -1.f; break;
		default: nx = 1.f; break;
		}
		// 沿边界方向的坐标夹在地图范围内(沿航向多前进D之后可能越出地图)。
		auto foot = [&](float cx, float cy) {
			float clampedX = max(0.f, min(static_cast<float>(sizeX), cx));
			float clampedY = max(0.f, min(static_cast<float>(sizeY), cy));
			switch (label) {
			case 'N': return make_pair(clampedX, 0.f);
			case 'S': return make_pair(clampedX, static_cast<float>(sizeY));
			case 'W': return make_pair(0.f, clampedY);
			default: return make_pair(static_cast<float>(sizeX), clampedY);
			}
		};
		EndGeometry end = ensureEnd(station, e);
		// 轴点到边界的距离D；extern的位置=从航向前进D/3的控制点到边界的垂足，轨道从航向平滑拐向边界，不在站旁直角拐弯。
		// 从实际连出来的方向(端的航向)向前延伸 D/3 得到控制点C(D=轴点P到该边界的垂直距离)，C到边界的垂足就是extern的位置。
		auto [footX, footY] = foot(end.px, end.py);
		float distance = sqrtf((footX - end.px) * (footX - end.px) + (footY - end.py) * (footY - end.py));
		auto [qx, qy] = foot(end.px + end.hx * distance / 3.f, end.py + end.hy * distance / 3.f);
		// 中轴线终点往里缩半个轨距，半圆顶点才正好落在边界上。
		float ax = qx - nx * kHalfGauge, ay = qy - ny * kHalfGauge;

		string name = "bd_" + station.info.name + "_" + to_string(e) + "_" + label;
		if (!linkKeys.insert({ "extern", name }).second) return name;
		RouteExternNode node;
		node.name = name;
		node.x = qx; node.y = qy; node.z = end.pz; node.dirX = ny; node.dirY = -nx; // 顶点处的行进方向(右手侧->左手侧)
		externs.push_back(node);

		// 控制臂=车站到边界距离的1/3。
		TrackSegment axis = MakeBezierSegment(end.px, end.py, end.pz, end.hx, end.hy, ax, ay, end.pz, nx, ny, distance / 3.f);
		vector<CurveSample> samples = SampleBezier(axis, kOffsetStep, end.hx, end.hy);
		// 半圆：去程铁轨在中轴线右手侧(r=(-ny,nx))走到A+r*g，经顶点掉头到A-r*g，回程铁轨在左手侧。每个1/4圆弧的控制臂=0.5523*半径。
		float rx = -ny, ry = nx;
		float arcArm = 0.5523f * kHalfGauge;
		TrackSegment firstArc = MakeBezierSegment(ax + rx * kHalfGauge, ay + ry * kHalfGauge, end.pz, nx, ny, qx, qy, end.pz, -rx, -ry, arcArm);
		TrackSegment secondArc = MakeBezierSegment(qx, qy, end.pz, -rx, -ry, ax - rx * kHalfGauge, ay - ry * kHalfGauge, end.pz, -nx, -ny, arcArm);
		vector<TrackSegment> outbound = OffsetRail(samples, kHalfGauge, false, end.pz, end.pz, true);
		outbound.push_back(firstArc);
		vector<TrackSegment> inbound = OffsetRail(samples, -kHalfGauge, true, end.pz, end.pz, true);
		inbound.insert(inbound.begin(), secondArc);
		size_t before = links.size();
		addLink(ExternOf(endName(station, e, "out")), ExternOf(name), std::move(outbound));
		addLink(ExternOf(name), ExternOf(endName(station, e, "in")), std::move(inbound));
		if (links.size() == before + 2) railPairs.push_back({ static_cast<int>(before), static_cast<int>(before) + 1, SampleBezier(axis, TRACK_SAMPLE_STEP, end.hx, end.hy) });
		return name;
	};
	// 航向主轴方向的边界。
	auto dominantLabel = [](float dx, float dy) {
		if (fabsf(dx) >= fabsf(dy)) return dx >= 0.f ? 'E' : 'W';
		return dy >= 0.f ? 'S' : 'N';
	};

	// ---- 线路拓扑：一个中央车站 + 东南西北最多各一座(`building_train_station`的Assign放的，没位置的方向就没有) ----
	// 中央车站=到其它站距离之和最小的站；其它站按相对中央的主方向分到东/南/西/北，每个方向一座。
	// 没有车站的方向不连地图边界，按外站数量n分情形，见route_train.md。
	size_t count = stations.size();
	size_t central = 0;
	if (count > 1) {
		float best = FLT_MAX;
		for (size_t i = 0; i < count; i++) {
			float sum = 0.f;
			for (size_t j = 0; j < count; j++) sum += hypotf(stations[i].info.x - stations[j].info.x, stations[i].info.y - stations[j].info.y);
			if (sum < best) { best = sum; central = i; }
		}
	}
	const TrainStationLayout& hub = stations[central];
	auto addLine = [&](vector<RouteLineEntry> entries) {
		lines.push_back(std::move(entries));
		lineVehicleCounts.push_back(kVehiclesPerTrainLine);
	};

	const TrainStationLayout* outer[4] = { nullptr, nullptr, nullptr, nullptr }; // 0东 1南 2西 3北
	const char labels[4] = { 'E', 'S', 'W', 'N' };
	for (size_t i = 0; i < count; i++) {
		if (i == central) continue;
		float dx = stations[i].info.x - hub.info.x, dy = stations[i].info.y - hub.info.y;
		int d = fabsf(dx) >= fabsf(dy) ? (dx >= 0.f ? 0 : 2) : (dy >= 0.f ? 1 : 3);
		if (!outer[d] || hypotf(dx, dy) < hypotf(outer[d]->info.x - hub.info.x, outer[d]->info.y - hub.info.y)) outer[d] = &stations[i];
	}
	auto unitToward = [](float fromX, float fromY, float toX, float toY) {
		float dx = toX - fromX, dy = toY - fromY, length = sqrtf(dx * dx + dy * dy);
		return length > 1e-6f ? make_pair(dx / length, dy / length) : make_pair(1.f, 0.f);
	};
	// 中央车站朝向外站s的那一端(两端航向相反，取航向和到s方向点积大的)。
	auto hubEndFacing = [&](const TrainStationLayout* s) {
		auto [ux, uy] = unitToward(hub.info.x, hub.info.y, s->info.x, s->info.y);
		return (hub.hx[0] * ux + hub.hy[0] * uy) >= (hub.hx[1] * ux + hub.hy[1] * uy) ? 0 : 1;
	};
	// 同一条线上的两座外站a、b必须接在中央车站相反的两端(穿过中央是X端进、1-X端出)：返回朝a的那一端X，按航向和方向的点积之和最大选。
	auto pickHubEnd = [&](const TrainStationLayout* a, const TrainStationLayout* b) {
		auto [uax, uay] = unitToward(hub.info.x, hub.info.y, a->info.x, a->info.y);
		auto [ubx, uby] = unitToward(hub.info.x, hub.info.y, b->info.x, b->info.y);
		float score0 = hub.hx[0] * uax + hub.hy[0] * uay + hub.hx[1] * ubx + hub.hy[1] * uby;
		float score1 = hub.hx[1] * uax + hub.hy[1] * uay + hub.hx[0] * ubx + hub.hy[0] * uby;
		return score0 >= score1 ? 0 : 1;
	};

	// 一条经过中央车站的线：A外站 -> 中央 -> B外站 -> B外边界回转 -> B外站 -> 中央 -> A外站 -> A外边界回转，首尾相接。
	// x=中央车站朝A的那一端，B接在1-x端。a/b为空表示那一侧没有外站，此时中央车站对应的那一端直接连到labelA/labelB方向的地图边界；
	// a/b非空时labelA/labelB是外站远端要连的边界。
	auto throughHub = [&](const TrainStationLayout* a, char labelA, const TrainStationLayout* b, char labelB, int x) {
		const string& hubName = hub.info.name;
		vector<RouteLineEntry> entries;
		int ea = 0, eb = 0;
		string bdA, bdB;
		if (a) {
			ea = a->BestTrack(hub.info.x - a->info.x, hub.info.y - a->info.y); // a朝向中央的那一端
			connectEnds(*a, ea, hub, x);
			bdA = boundaryExtern(*a, 1 - ea, labelA);
		}
		else {
			bdA = boundaryExtern(hub, x, labelA);
		}
		if (b) {
			eb = b->BestTrack(hub.info.x - b->info.x, hub.info.y - b->info.y);
			connectEnds(*b, eb, hub, 1 - x);
			bdB = boundaryExtern(*b, 1 - eb, labelB);
		}
		else {
			bdB = boundaryExtern(hub, 1 - x, labelB);
		}
		// 绕一圈：... -> A方向回转点 -> a(进站轨ea) -> 中央(1-x) -> b(1-eb) -> B方向回转点 -> b(eb) -> 中央(x) -> a(1-ea) -> A方向回转点
		entries.push_back({ bdA, -1 });
		if (a) entries.push_back({ a->info.name, ea });
		entries.push_back({ hubName, 1 - x });
		if (b) entries.push_back({ b->info.name, 1 - eb });
		entries.push_back({ bdB, -1 });
		if (b) entries.push_back({ b->info.name, eb });
		entries.push_back({ hubName, x });
		if (a) entries.push_back({ a->info.name, 1 - ea });
		addLine(std::move(entries));
	};

	int present[4], n = 0;
	for (int d = 0; d < 4; d++) if (outer[d]) present[n++] = d;

	if (n == 0) {
		// 只有中央车站：按它的中轴线偏南北还是偏东西，两端各连到对向的两条边界，往返各用一条轨道。
		string bd1 = boundaryExtern(hub, 1, dominantLabel(hub.hx[1], hub.hy[1]));
		string bd0 = boundaryExtern(hub, 0, dominantLabel(hub.hx[0], hub.hy[0]));
		addLine({ { hub.info.name, 1 }, { bd1, -1 }, { hub.info.name, 0 }, { bd0, -1 } });
	}
	else if (n == 1) {
		// 一座外站：外站-中央-中央空闲端连到外站方向的对向边界。
		int d = present[0];
		throughHub(outer[d], labels[d], nullptr, labels[(d + 2) % 4], hubEndFacing(outer[d]));
	}
	else if (n == 2) {
		int i = present[0], j = present[1];
		if (j - i == 2) {
			// 对向两座(只有南北或只有东西)：只建这一条线，另一条轴线不建。
			throughHub(outer[i], labels[i], outer[j], labels[j], pickHubEnd(outer[i], outer[j]));
		}
		else {
			// 东西轴一座 + 南北轴一座：在中央车站两端 -> 一条线连三站和两侧边界；同一侧 -> 看中轴线偏南北还是偏东西，
			// 中央车站空闲端连到对应的边界(dominantLabel)，两座外站各一条线。
			int xi = hubEndFacing(outer[i]), xj = hubEndFacing(outer[j]);
			if (xi != xj) {
				throughHub(outer[i], labels[i], outer[j], labels[j], xi);
			}
			else {
				char freeLabel = dominantLabel(hub.hx[1 - xi], hub.hy[1 - xi]);
				throughHub(outer[i], labels[i], nullptr, freeLabel, xi);
				throughHub(outer[j], labels[j], nullptr, freeLabel, xi);
			}
		}
	}
	else if (n == 3) {
		// 缺方向m：m的对向站o独占一条轴，另一轴上的p、q照常成线；原来经过缺失方向的那条线改成 o-中央-X，
		// X取p、q中接在中央另一端(不是o所在端)的那一座，该座远端的边界回转点和p-q线共用。
		int m = 0;
		while (outer[m]) m++;
		int o = (m + 2) % 4, p = (m + 1) % 4, q = (m + 3) % 4;
		int xp = pickHubEnd(outer[p], outer[q]);
		throughHub(outer[p], labels[p], outer[q], labels[q], xp);
		int xo = hubEndFacing(outer[o]);
		int other = xo != xp ? p : q;
		throughHub(outer[o], labels[o], outer[other], labels[other], xo);
	}
	else {
		throughHub(outer[3], 'N', outer[1], 'S', pickHubEnd(outer[3], outer[1]));
		throughHub(outer[0], 'E', outer[2], 'W', pickHubEnd(outer[0], outer[2]));
	}
}

void TrainRoute::BuildTrack(const function<const string&(int, int)>& getTerrain,
	const function<pair<bool, float>(int, int)>& getWater,
	const function<float(float, float)>& getHeight) {
	if (skipAuto) { BuildManualTrack(getHeight); return; }
	// 隧道区：入口阈值点sIn、出口阈值点sOut(中轴线弧长，A->B方向)，tunnel=false表示这座山不钻隧道，沿山体爬升。
	struct Zone { float sIn, sOut; bool tunnel; };

	for (const RailPair& railPair : railPairs) {
		const vector<CurveSample>& mid = railPair.midline;
		size_t midCount = mid.size();
		if (midCount < 2) continue;

		// ---- 中轴线：按弧长累计，沿线取高度。只看高度，与地形类型无关。 ----
		vector<float> midArc(midCount, 0.f), midHeight(midCount);
		for (size_t i = 0; i < midCount; i++) {
			if (i > 0) midArc[i] = midArc[i - 1] + sqrtf((mid[i].x - mid[i - 1].x) * (mid[i].x - mid[i - 1].x) + (mid[i].y - mid[i - 1].y) * (mid[i].y - mid[i - 1].y));
			midHeight[i] = getHeight(mid[i].x, mid[i].y);
		}
		const float midLength = midArc.back();
		if (midLength < 1e-3f) continue;

		// ---- 切出每座山：连续高度>=阈值的采样点。间隙<TUNNEL_MIN_OPEN_LENGTH的两座山并成一座(两座山之间，不出地面)。 ----
		struct Span { int first, last; };
		vector<Span> spans;
		for (int i = 0; i < static_cast<int>(midCount); i++) {
			if (midHeight[i] < TUNNEL_THRESHOLD_Z) continue;
			int first = i;
			while (i + 1 < static_cast<int>(midCount) && midHeight[i + 1] >= TUNNEL_THRESHOLD_Z) i++;
			if (!spans.empty() && midArc[first] - midArc[spans.back().last] < TUNNEL_MIN_OPEN_LENGTH) spans.back().last = i;
			else spans.push_back({ first, i });
		}
		// 阈值点距链首/链尾(车站端中轴线起点)不足坡道长度，坡道放不下：这座山沿山体爬升，地形贴合轨道。
		vector<Zone> zones;
		for (const Span& span : spans) {
			Zone zone{ midArc[span.first], midArc[span.last], true };
			if (zone.sIn < TUNNEL_RAMP_LENGTH || midLength - zone.sOut < TUNNEL_RAMP_LENGTH) zone.tunnel = false;
			zones.push_back(zone);
			debugf(DebugLevel::Log, "TrainRoute pair (%d,%d) mountain arc [%.1f,%.1f] of %.1f -> %s.\n",
				railPair.right, railPair.left, zone.sIn, zone.sOut, midLength, zone.tunnel ? "tunnel" : "raise");
		}

		// 中轴线弧长s处的下沉比例(0=地面，1=隧道底)和它对s的导数。
		auto depthAt = [&](float s, float& dDepth) {
			dDepth = 0.f;
			for (const Zone& zone : zones) {
				if (!zone.tunnel) continue;
				if (s > zone.sIn - TUNNEL_RAMP_LENGTH && s < zone.sIn) {
					float u = (s - (zone.sIn - TUNNEL_RAMP_LENGTH)) / TUNNEL_RAMP_LENGTH;
					dDepth = 6.f * u * (1.f - u) / TUNNEL_RAMP_LENGTH;
					return SmoothStep(u);
				}
				if (s >= zone.sIn && s <= zone.sOut) return 1.f;
				if (s > zone.sOut && s < zone.sOut + TUNNEL_RAMP_LENGTH) {
					float u = (s - zone.sOut) / TUNNEL_RAMP_LENGTH;
					dDepth = -6.f * u * (1.f - u) / TUNNEL_RAMP_LENGTH;
					return 1.f - SmoothStep(u);
				}
			}
			return 0.f;
		};
		auto inRaise = [&](float s) {
			for (const Zone& zone : zones) if (!zone.tunnel && s >= zone.sIn && s <= zone.sOut) return true;
			return false;
		};

		// 相邻两点一段三次贝塞尔：平面用给定切线做Hermite，Z用端点坡度做Hermite，坡度夹到单调范围
		// (与这段高度差同号、不超过3倍平均坡度)，三次曲线不会冲出两端高度。
		auto makeSegment = [&](float x1, float y1, float z1, float t1x, float t1y, float slope1,
			float x2, float y2, float z2, float t2x, float t2y, float slope2) {
			float dx = x2 - x1, dy = y2 - y1;
			float run = sqrtf(dx * dx + dy * dy);
			float delta = run > 1e-6f ? (z2 - z1) / run : 0.f;
			auto monotone = [delta](float s) {
				if (delta == 0.f) return 0.f;
				float aligned = s * (delta > 0.f ? 1.f : -1.f);
				aligned = max(0.f, min(aligned, 3.f * fabsf(delta)));
				return aligned * (delta > 0.f ? 1.f : -1.f);
			};
			TrackSegment seg;
			seg.x1 = x1; seg.y1 = y1; seg.z1 = z1;
			seg.c1x = x1 + t1x * run / 3.f; seg.c1y = y1 + t1y * run / 3.f; seg.c1z = z1 + monotone(slope1) * run / 3.f;
			seg.c2x = x2 - t2x * run / 3.f; seg.c2y = y2 - t2y * run / 3.f; seg.c2z = z2 - monotone(slope2) * run / 3.f;
			seg.x2 = x2; seg.y2 = y2; seg.z2 = z2;
			return seg;
		};

		// ---- 一条铁轨：reversed=false沿中轴线A->B走，true沿B->A走。把整条链(同一条link里连续的terrainFollow段)采样成最终几何：
		// 隧道下沉剖面直接由中轴线上的zone按弧长比例换算，两条铁轨同进同出。 ----
		auto refine = [&](const vector<TrackSegment>& chain, bool reversed, vector<TrackSegment>& out) {
			const TrackSegment& base = chain.front();
			struct Point { float x, y, z; };
			Point a{ base.x1, base.y1, base.z1 }, b{ chain.back().x2, chain.back().y2, chain.back().z2 };
			float c1x = base.c1x, c1y = base.c1y, c2x = chain.back().c2x, c2y = chain.back().c2y;

			// 逐段细采样，累计弧长。
			vector<float> fineX, fineY, fineArc;
			for (size_t k = 0; k < chain.size(); k++) {
				const TrackSegment& seg = chain[k];
				float chord = sqrtf((seg.x2 - seg.x1) * (seg.x2 - seg.x1) + (seg.y2 - seg.y1) * (seg.y2 - seg.y1));
				int fine = max(16, static_cast<int>(ceilf(chord * 8.f)));
				for (int i = (k == 0 ? 0 : 1); i <= fine; i++) {
					float t = static_cast<float>(i) / static_cast<float>(fine), m = 1.f - t;
					float x = m * m * m * seg.x1 + 3.f * m * m * t * seg.c1x + 3.f * m * t * t * seg.c2x + t * t * t * seg.x2;
					float y = m * m * m * seg.y1 + 3.f * m * m * t * seg.c1y + 3.f * m * t * t * seg.c2y + t * t * t * seg.y2;
					float length = fineArc.empty() ? 0.f : fineArc.back() + sqrtf((x - fineX.back()) * (x - fineX.back()) + (y - fineY.back()) * (y - fineY.back()));
					fineX.push_back(x); fineY.push_back(y); fineArc.push_back(length);
				}
			}
			int fine = static_cast<int>(fineArc.size()) - 1;
			float totalLength = fineArc[fine];
			if (totalLength < 1e-3f) { for (const TrackSegment& seg : chain) out.push_back(seg); return; }

			int count = max(1, static_cast<int>(ceilf(totalLength / TRACK_SAMPLE_STEP)));
			vector<float> sx(count + 1), sy(count + 1), arc(count + 1);
			{
				int cursor = 0;
				for (int i = 0; i <= count; i++) {
					float target = totalLength * static_cast<float>(i) / static_cast<float>(count);
					while (cursor + 1 < fine && fineArc[cursor + 1] < target) cursor++;
					float span = fineArc[cursor + 1] - fineArc[cursor];
					float f = span > 1e-9f ? (target - fineArc[cursor]) / span : 0.f;
					sx[i] = fineX[cursor] + (fineX[cursor + 1] - fineX[cursor]) * f;
					sy[i] = fineY[cursor] + (fineY[cursor + 1] - fineY[cursor]) * f;
					arc[i] = target;
				}
				sx[0] = a.x; sy[0] = a.y; sx[count] = b.x; sy[count] = b.y;
			}

			// 拆掉沿线相交的楼体/园区(Core按请求拆，带stationMod的建筑除外)，条带宽=轨道宽。
			{
				vector<pair<float, float>> centerline(count + 1);
				for (int i = 0; i <= count; i++) centerline[i] = { sx[i], sy[i] };
				AddDemolition(centerline, trackWidth);
			}

			// ---- 每个采样点的高度、坡度(Z每水平单位的变化量)和归类。基准高度在两端站点z之间线性过渡。 ----
			// 采样点对应的中轴线弧长sMid；铁轨弧长比中轴线略长/短(弯道内外侧)，按比例换算。反向铁轨沿中轴线倒着走，深度导数反号。
			const float dirSign = reversed ? -1.f : 1.f;
			vector<float> z(count + 1), slope(count + 1, 0.f);
			vector<char> raised(count + 1, 0), inTunnelLevel(count + 1, 0), inRamp(count + 1, 0);
			for (int i = 0; i <= count; i++) {
				float fraction = arc[i] / totalLength;
				float sMid = (reversed ? 1.f - fraction : fraction) * midLength;
				float base = a.z + (b.z - a.z) * fraction;
				float dDepth;
				float depth = depthAt(sMid, dDepth);
				// 链首/链尾各TRACK_STATION_FLAT_SAMPLES个采样点保持水平，不在车站范围内爬坡。
				bool flatEnd = i < TRACK_STATION_FLAT_SAMPLES || i > count - TRACK_STATION_FLAT_SAMPLES;
				if (!flatEnd && inRaise(sMid)) {
					raised[i] = 1;
					z[i] = max(getHeight(sx[i], sy[i]), base);
				}
				else {
					z[i] = base + TUNNEL_DROP * depth;
					slope[i] = TUNNEL_DROP * dDepth * dirSign * midLength / totalLength;
					if (depth >= 1.f) inTunnelLevel[i] = 1;
					else if (depth > 0.f) inRamp[i] = 1;
				}
			}
			// 抬升段的坡度取前后相邻点连线的坡度(Catmull-Rom)；山脚/平地/两端取0，不带着惯性冲到基准以下。
			for (int i = 1; i < count; i++) {
				if (!raised[i]) continue;
				float run = arc[i + 1] - arc[i - 1];
				slope[i] = run > 1e-6f ? (z[i + 1] - z[i - 1]) / run : 0.f;
			}

			// 完全水平且共线的连续采样点合并成一段，不然火车几百单位的线路会有几百个无谓的小段。
			auto segmentTag = [&](int i) -> int { // 相邻两点(i,i+1)这一段的归类，合并只能在同类里进行
				if (inTunnelLevel[i] && inTunnelLevel[i + 1]) return 3;
				if (raised[i] || raised[i + 1]) return 1;
				if (inRamp[i] || inRamp[i + 1] || inTunnelLevel[i] || inTunnelLevel[i + 1]) return 2;
				return 0;
			};
			auto isFlat = [&](int i) {
				return fabsf(z[i] - z[i + 1]) < 1e-4f && fabsf(slope[i]) < 1e-6f && fabsf(slope[i + 1]) < 1e-6f;
			};

			// 每个采样点的平面切线(Catmull-Rom：前后相邻采样点连线的方向)，两端取基础曲线自己的出发/到达方向。
			// 相邻两段共用同一条切线，接缝处航向连续，火车过接缝时不会突然转几度。
			vector<float> tanX(count + 1), tanY(count + 1);
			for (int k = 0; k <= count; k++) {
				float dirX, dirY;
				if (k == 0) { dirX = c1x - a.x; dirY = c1y - a.y; }
				else if (k == count) { dirX = b.x - c2x; dirY = b.y - c2y; }
				else { dirX = sx[k + 1] - sx[k - 1]; dirY = sy[k + 1] - sy[k - 1]; }
				float dirLength = sqrtf(dirX * dirX + dirY * dirY);
				if (dirLength < 1e-6f) { dirX = sx[min(k + 1, count)] - sx[max(k - 1, 0)]; dirY = sy[min(k + 1, count)] - sy[max(k - 1, 0)]; dirLength = sqrtf(dirX * dirX + dirY * dirY); }
				tanX[k] = dirLength > 1e-6f ? dirX / dirLength : 0.f;
				tanY[k] = dirLength > 1e-6f ? dirY / dirLength : 0.f;
			}

			int i = 0;
			while (i < count) {
				int j = i + 1;
				if (isFlat(i)) {
					int tag = segmentTag(i);
					while (j < count && isFlat(j) && segmentTag(j) == tag && fabsf(z[j + 1] - z[i]) < 1e-4f) {
						// 候选终点j+1：中间点都要贴着弦(偏差<0.01单位)
						float dx = sx[j + 1] - sx[i], dy = sy[j + 1] - sy[i];
						float len = sqrtf(dx * dx + dy * dy);
						bool straight = len > 1e-6f;
						for (int m = i + 1; m <= j && straight; m++) {
							float dev = fabsf((sx[m] - sx[i]) * dy - (sy[m] - sy[i]) * dx) / len;
							if (dev > 0.01f) straight = false;
						}
						if (!straight) break;
						j++;
					}
				}

				TrackSegment seg = makeSegment(sx[i], sy[i], z[i], tanX[i], tanY[i], slope[i], sx[j], sy[j], z[j], tanX[j], tanY[j], slope[j]);
				seg.mesh = base.mesh;
				seg.meshUnit = base.meshUnit;
				out.push_back(seg);

				// 抬升段(tag 1)贴地；隧道口的洞由中轴线统一挖(见下)，水平隧道段在山体里面不挖。
				if (segmentTag(i) == 1) {
					Connection chordConnection(Node("route", seg.x1, seg.y1, seg.z1), Node("route", seg.x2, seg.y2, seg.z2));
					chordConnection.AddControls({ { Node("route_ctrl", seg.c1x, seg.c1y, seg.c1z), 1.f }, { Node("route_ctrl", seg.c2x, seg.c2y, seg.c2z), 1.f } });
					AddTerrainFit(&chordConnection, 0.f, 1.f, trackWidth, TERRAIN_FIT_FALLOFF);
				}
				i = j;
			}
		};

		// 两条铁轨：右轨沿A->B(中轴线方向)，左轨沿B->A(倒着走)。
		for (int side = 0; side < 2; side++) {
			size_t linkIndex = static_cast<size_t>(side == 0 ? railPair.right : railPair.left);
			vector<TrackSegment> refined;
			const vector<TrackSegment>& segments = links[linkIndex].segments;
			for (size_t k = 0; k < segments.size(); ) {
				if (!segments[k].terrainFollow) { refined.push_back(segments[k]); k++; continue; }
				size_t end = k;
				while (end < segments.size() && segments[end].terrainFollow) end++;
				refine(vector<TrackSegment>(segments.begin() + k, segments.begin() + end), side == 1, refined);
				k = end;
			}
			links[linkIndex].segments = std::move(refined);
		}

		// ---- 隧道口开洞：由中轴线统一请求(两条铁轨合起来的宽度)，每个隧道口坡道靠山的TUNNEL_HATCH_LENGTH单位。
		// 沿中轴线每1单位一段，z取下沉剖面。 ----
		float zStart = 0.f, zEnd = 0.f;
		{
			const vector<TrackSegment>& rightSegments = links[static_cast<size_t>(railPair.right)].segments;
			zStart = rightSegments.front().z1;
			for (auto it = rightSegments.rbegin(); it != rightSegments.rend(); ++it) {
				if (it->terrainFollow || it == rightSegments.rbegin()) { zEnd = it->z2; break; }
			}
		}
		auto midAt = [&](float s, float& x, float& y, float& tx, float& ty) {
			float u = max(0.f, min(1.f, s / midLength)) * static_cast<float>(midCount - 1);
			size_t i = min(static_cast<size_t>(u), midCount - 2);
			float f = u - static_cast<float>(i);
			x = mid[i].x + (mid[i + 1].x - mid[i].x) * f; y = mid[i].y + (mid[i + 1].y - mid[i].y) * f;
			tx = mid[i].tx + (mid[i + 1].tx - mid[i].tx) * f; ty = mid[i].ty + (mid[i + 1].ty - mid[i].ty) * f;
			float length = sqrtf(tx * tx + ty * ty);
			if (length > 1e-6f) { tx /= length; ty /= length; }
		};
		// tunnelAtEnd：隧道在[s1,s2]的s2端(入口侧)还是s1端(出口侧)；只有最靠隧道的那一片的隧道端加端墙。
		auto hatchRange = [&](float s1, float s2, bool tunnelAtEnd) {
			int pieces = max(1, static_cast<int>(ceilf((s2 - s1) / TRACK_SAMPLE_STEP)));
			for (int p = 0; p < pieces; p++) {
				float sa = s1 + (s2 - s1) * static_cast<float>(p) / static_cast<float>(pieces);
				float sb = s1 + (s2 - s1) * static_cast<float>(p + 1) / static_cast<float>(pieces);
				float xa, ya, txa, tya, xb, yb, txb, tyb, dA, dB;
				midAt(sa, xa, ya, txa, tya); midAt(sb, xb, yb, txb, tyb);
				float depthA = depthAt(sa, dA), depthB = depthAt(sb, dB);
				float baseA = zStart + (zEnd - zStart) * sa / midLength, baseB = zStart + (zEnd - zStart) * sb / midLength;
				TrackSegment seg = makeSegment(xa, ya, baseA + TUNNEL_DROP * depthA, txa, tya, TUNNEL_DROP * dA,
					xb, yb, baseB + TUNNEL_DROP * depthB, txb, tyb, TUNNEL_DROP * dB);
				Connection chordConnection(Node("route", seg.x1, seg.y1, seg.z1), Node("route", seg.x2, seg.y2, seg.z2));
				chordConnection.AddControls({ { Node("route_ctrl", seg.c1x, seg.c1y, seg.c1z), 1.f }, { Node("route_ctrl", seg.c2x, seg.c2y, seg.c2z), 1.f } });
				bool wallEnd = tunnelAtEnd && p == pieces - 1, wallStart = !tunnelAtEnd && p == 0;
				AddTunnelHatch(&chordConnection, 0.f, 1.f, 2.f * kTrackGauge,
					wallStart ? seg.z1 + TUNNEL_ROOF_CLEARANCE : kNoEndWall, wallEnd ? seg.z2 + TUNNEL_ROOF_CLEARANCE : kNoEndWall);
			}
		};
		for (const Zone& zone : zones) {
			if (!zone.tunnel) continue;
			hatchRange(zone.sIn - TUNNEL_HATCH_LENGTH, zone.sIn, true);
			hatchRange(zone.sOut, zone.sOut + TUNNEL_HATCH_LENGTH, false);
		}

		// ---- 桥墩：按中轴线，两条铁轨共用一排墩(不是每根铁轨各一排)。沿中轴线每TRACK_SAMPLE_STEP一段Connection，
		// z=桥面高度(和铁轨同一套剖面：基准线性过渡、隧道下沉、抬升段贴地形、链首尾各TRACK_STATION_FLAT_SAMPLES个采样点水平)；
		// 隧道内/贴地的位置Core自己不放墩。 ----
		auto deckZ = [&](float s) {
			float dDepth;
			float base = zStart + (zEnd - zStart) * s / midLength;
			if (s < TRACK_STATION_FLAT_SAMPLES * TRACK_SAMPLE_STEP || s > midLength - TRACK_STATION_FLAT_SAMPLES * TRACK_SAMPLE_STEP) return base;
			if (inRaise(s)) {
				float x, y, tx, ty;
				midAt(s, x, y, tx, ty);
				return max(getHeight(x, y), base);
			}
			return base + TUNNEL_DROP * depthAt(s, dDepth);
		};
		int pieces = max(1, static_cast<int>(ceilf(midLength / TRACK_SAMPLE_STEP)));
		vector<Connection> midChain;
		for (int p = 0; p < pieces; p++) {
			float sa = midLength * static_cast<float>(p) / static_cast<float>(pieces);
			float sb = midLength * static_cast<float>(p + 1) / static_cast<float>(pieces);
			float xa, ya, txa, tya, xb, yb, txb, tyb;
			midAt(sa, xa, ya, txa, tya); midAt(sb, xb, yb, txb, tyb);
			float za = deckZ(sa), zb = deckZ(sb);
			float run = sqrtf((xb - xa) * (xb - xa) + (yb - ya) * (yb - ya));
			Connection connection(Node("route", xa, ya, za), Node("route", xb, yb, zb));
			connection.AddControls({ { Node("route_ctrl", xa + txa * run / 3.f, ya + tya * run / 3.f, za + (zb - za) / 3.f), 1.f },
				{ Node("route_ctrl", xb - txb * run / 3.f, yb - tyb * run / 3.f, zb - (zb - za) / 3.f), 1.f } });
			midChain.push_back(std::move(connection));
		}
		vector<vector<Connection>> pierChains;
		pierChains.push_back(std::move(midChain));
		AddPier("", std::move(pierChains), PIER_DEFAULT_SPACING, PIER_MIN_SPACING, PIER_RADIUS);
	}
}


// ================= 手动线路(--skip)：编辑器output.transit的火车线路，设计见route_train.md"手动线路" =================
namespace {
	pair<float, float> SegmentStartHeading(const TrackSegment& s) {
		float dx = s.c1x - s.x1, dy = s.c1y - s.y1;
		if (dx * dx + dy * dy < 1e-10f) { dx = s.c2x - s.x1; dy = s.c2y - s.y1; }
		if (dx * dx + dy * dy < 1e-10f) { dx = s.x2 - s.x1; dy = s.y2 - s.y1; }
		float length = sqrtf(dx * dx + dy * dy);
		return length > 1e-6f ? make_pair(dx / length, dy / length) : make_pair(1.f, 0.f);
	}
	pair<float, float> SegmentEndHeading(const TrackSegment& s) {
		float dx = s.x2 - s.c2x, dy = s.y2 - s.c2y;
		if (dx * dx + dy * dy < 1e-10f) { dx = s.x2 - s.c1x; dy = s.y2 - s.c1y; }
		if (dx * dx + dy * dy < 1e-10f) { dx = s.x2 - s.x1; dy = s.y2 - s.y1; }
		float length = sqrtf(dx * dx + dy * dy);
		return length > 1e-6f ? make_pair(dx / length, dy / length) : make_pair(1.f, 0.f);
	}
	// 沿采样点偏移的铁轨：z按序号比例走扁S(两端节点高度)，和编辑器里中轴线的高度剖面一致。
	vector<TrackSegment> ManualRail(const vector<CurveSample>& samples, float offset, bool reverse, float zStart, float zEnd) {
		vector<TrackSegment> segments = OffsetRail(samples, offset, reverse, zStart, zEnd, false);
		const float m = static_cast<float>(segments.size());
		for (size_t k = 0; k < segments.size(); k++) {
			float z1 = zStart + (zEnd - zStart) * SmoothStep(static_cast<float>(k) / m);
			float z2 = zStart + (zEnd - zStart) * SmoothStep(static_cast<float>(k + 1) / m);
			segments[k].z1 = segments[k].c1z = z1;
			segments[k].z2 = segments[k].c2z = z2;
		}
		return segments;
	}
	// 直线弦(三等分控制点)，给AddTerrainFit/AddTunnelHatch/AddPier用。
	Connection ChordConnection(float xa, float ya, float za, float xb, float yb, float zb) {
		Connection chord(Node("route", xa, ya, za), Node("route", xb, yb, zb));
		chord.AddControls({ { Node("route_ctrl", xa + (xb - xa) / 3.f, ya + (yb - ya) / 3.f, za + (zb - za) / 3.f), 1.f },
			{ Node("route_ctrl", xb - (xb - xa) / 3.f, yb - (yb - ya) / 3.f, zb - (zb - za) / 3.f), 1.f } });
		return chord;
	}
}

void TrainRoute::LayoutManual(const vector<RouteStationInfo>& stationInfos) {
	if (!manual || manual->Empty()) {
		debugf(DebugLevel::Warning, "TrainRoute --skip but the roadnet has no manual transit data, no train lines.\n");
		return;
	}
	const TransitData& data = *manual;
	const string routeId = GetType();
	ManualPlan plan = PlanManualRoute(data, routeId, stationInfos, false);
	for (const string& warning : plan.warnings) debugf(DebugLevel::Warning, "TrainRoute manual: %s\n", warning.c_str());
	if (plan.lines.empty()) {
		debugf(DebugLevel::Warning, "TrainRoute manual: no usable train line in the roadnet loops of %s.\n", routeId.c_str());
		return;
	}

	constexpr float kHalfGauge = kTrackGauge * 0.5f;
	set<pair<string, string>> linkKeys;
	auto addLink = [&](const RouteEndpoint& from, const RouteEndpoint& to, vector<TrackSegment> segments) -> int {
		if (!linkKeys.insert({ EndpointKey(from), EndpointKey(to) }).second) return -1;
		RouteLinkSpec link;
		link.from = from;
		link.to = to;
		for (TrackSegment& seg : segments) { seg.mesh = kTrainBridgeMesh; seg.meshUnit = kTrainBridgeUnit; }
		link.segments = std::move(segments);
		links.push_back(std::move(link));
		return static_cast<int>(links.size()) - 1;
	};

	// extern按位置去重：同一节点上位置相差<0.02的端点是同一个extern(直线通过的节点，到达端和离开端正好重合，不用转向link)。
	struct ExternRec { float x, y; string name; };
	map<int, vector<ExternRec>> externsAtNode;
	auto ensureExtern = [&](int node, float x, float y, float z, float hx, float hy) -> string {
		vector<ExternRec>& list = externsAtNode[node];
		for (const ExternRec& rec : list) if (hypotf(rec.x - x, rec.y - y) < 0.02f) return rec.name;
		RouteExternNode extern_;
		extern_.name = "mt" + to_string(node) + "_" + to_string(list.size());
		extern_.x = x; extern_.y = y; extern_.z = z; extern_.dirX = hx; extern_.dirY = hy;
		externs.push_back(extern_);
		list.push_back({ x, y, extern_.name });
		return extern_.name;
	};

	// 端口几何：(node,conn,in?) -> extern名+位置+行驶方向。
	struct PortGeo { string name; float x, y, z, hx, hy; };
	map<array<int, 3>, PortGeo> portGeo;
	auto registerPort = [&](int node, int conn, int in, const TrackSegment& seg, bool atEnd) {
		float x = atEnd ? seg.x2 : seg.x1, y = atEnd ? seg.y2 : seg.y1, z = atEnd ? seg.z2 : seg.z1;
		auto heading = atEnd ? SegmentEndHeading(seg) : SegmentStartHeading(seg);
		PortGeo geo;
		geo.name = ensureExtern(node, x, y, z, heading.first, heading.second);
		geo.x = x; geo.y = y; geo.z = z; geo.hx = heading.first; geo.hy = heading.second;
		portGeo[{ node, conn, in }] = geo;
	};

	// 掉头：以两端中点为圆心的半圆，顶点沿到达方向往前凸出，两个1/4圆弧。
	auto addUTurn = [&](const PortGeo& in, const PortGeo& out) {
		float cx = (in.x + out.x) * 0.5f, cy = (in.y + out.y) * 0.5f;
		float radius = hypotf(out.x - in.x, out.y - in.y) * 0.5f;
		float gx = out.x - in.x, gy = out.y - in.y, gl = hypotf(gx, gy);
		if (gl < 1e-6f) return;
		gx /= gl; gy /= gl;
		float ax = cx + in.hx * radius, ay = cy + in.hy * radius;
		float arm = 0.5523f * radius;
		addLink(ExternOf(in.name), ExternOf(out.name), {
			MakeBezierSegment(in.x, in.y, in.z, in.hx, in.hy, ax, ay, in.z, gx, gy, arm),
			MakeBezierSegment(ax, ay, in.z, gx, gy, out.x, out.y, out.z, out.hx, out.hy, arm) });
	};

	// ---- 实体线路：每条用到的连线 = 中轴线 + 两侧各偏半个轨距的两条铁轨 ----
	for (int c : plan.conns) {
		const TransitData::Conn& conn = data.conns[c];
		TrackSegment axis = ManualAxis(data, c);
		vector<CurveSample> samples = SampleBezier(axis, kOffsetStep, axis.x2 - axis.x1, axis.y2 - axis.y1);
		vector<TrackSegment> rightRail = ManualRail(samples, kHalfGauge, false, axis.z1, axis.z2);
		vector<TrackSegment> leftRail = ManualRail(samples, -kHalfGauge, true, axis.z2, axis.z1);
		if (rightRail.empty() || leftRail.empty()) continue;
		registerPort(conn.from, c, 0, rightRail.front(), false); // 右轨从from出发
		registerPort(conn.to, c, 1, rightRail.back(), true);     // 右轨到达to
		registerPort(conn.to, c, 0, leftRail.front(), false);    // 左轨从to出发
		registerPort(conn.from, c, 1, leftRail.back(), true);    // 左轨到达from
		int right = addLink(ExternOf(portGeo[{ conn.from, c, 0 }].name), ExternOf(portGeo[{ conn.to, c, 1 }].name), std::move(rightRail));
		int left = addLink(ExternOf(portGeo[{ conn.to, c, 0 }].name), ExternOf(portGeo[{ conn.from, c, 1 }].name), std::move(leftRail));
		if (right >= 0 && left >= 0) manualPairs.push_back({ c, right, left, axis });
	}

	// ---- 节点转向：到达连线的到达端 -> 离开连线的离开端；位置重合(直线通过)的已经是同一个extern，不用加 ----
	for (const array<int, 3>& turn : plan.turns) {
		auto inIt = portGeo.find({ turn[0], turn[1], 1 }), outIt = portGeo.find({ turn[0], turn[2], 0 });
		if (inIt == portGeo.end() || outIt == portGeo.end()) continue;
		const PortGeo& in = inIt->second;
		const PortGeo& out = outIt->second;
		if (in.name == out.name) continue;
		if (turn[1] == turn[2]) { addUTurn(in, out); continue; }
		if (in.hx * out.hx + in.hy * out.hy < 0.5f) {
			debugf(DebugLevel::Warning, "TrainRoute manual: sharp turn (>60 deg) at transit node %d between connections %d and %d.\n", turn[0], turn[1], turn[2]);
		}
		float chord = hypotf(out.x - in.x, out.y - in.y);
		addLink(ExternOf(in.name), ExternOf(out.name), { MakeBezierSegment(in.x, in.y, in.z, in.hx, in.hy, out.x, out.y, out.z, out.hx, out.hy, chord / 3.f) });
	}

	// ---- 缺口(编辑器里的虚线)：站端口/节点端口之间用扁S补 ----
	// 节点中心点(Hub)：前后都是缺口。掉头的节点(同时有HubOut)到达端/离开端各一个extern，靠右错开半个轨距，中间一个半圆。
	map<int, pair<float, float>> hubHeading;       // Hub：到达(或单一)朝向
	set<int> hubTurnNodes;
	for (const ManualPort& hub : plan.hubs) {
		if (hub.kind == ManualPort::HubOut) hubTurnNodes.insert(hub.node);
		else hubHeading[hub.node] = { hub.hx, hub.hy };
	}
	for (int nodeIndex : hubTurnNodes) {
		auto headingIt = hubHeading.find(nodeIndex);
		if (headingIt == hubHeading.end()) continue;
		const TransitData::Node& node = data.nodes[nodeIndex];
		float hx = headingIt->second.first, hy = headingIt->second.second;
		float rx = -hy, ry = hx; // 到达方向的右手侧
		PortGeo in, out;
		in.x = node.x + rx * kHalfGauge; in.y = node.y + ry * kHalfGauge; in.z = node.z; in.hx = hx; in.hy = hy;
		out.x = node.x - rx * kHalfGauge; out.y = node.y - ry * kHalfGauge; out.z = node.z; out.hx = -hx; out.hy = -hy;
		in.name = ensureExtern(nodeIndex, in.x, in.y, in.z, in.hx, in.hy);
		out.name = ensureExtern(nodeIndex, out.x, out.y, out.z, out.hx, out.hy);
		portGeo[{ nodeIndex, -1, 1 }] = in;
		portGeo[{ nodeIndex, -1, 0 }] = out;
		addUTurn(in, out);
	}
	struct Resolved { RouteEndpoint endpoint; float x = 0.f, y = 0.f, z = 0.f, hx = 1.f, hy = 0.f; bool ok = false; };
	auto resolve = [&](const ManualPort& port) {
		Resolved r;
		if (port.kind == ManualPort::StationIn || port.kind == ManualPort::StationOut) {
			const RouteStationInfo& info = stationInfos[port.station];
			bool isIn = port.kind == ManualPort::StationIn;
			const RoutePort& p = isIn ? info.inlets[port.track] : info.outlets[port.track];
			const RoutePort& heading = info.inlets[port.track];
			float length = hypotf(heading.dirX, heading.dirY);
			r.x = p.x; r.y = p.y; r.z = p.z;
			if (length > 1e-6f) { r.hx = heading.dirX / length; r.hy = heading.dirY / length; }
			r.endpoint.kind = isIn ? RouteEndpoint::Inlet : RouteEndpoint::Outlet;
			r.endpoint.station = info.stationIndex; r.endpoint.portIndex = port.track;
			r.ok = true;
		}
		else if ((port.kind == ManualPort::Hub || port.kind == ManualPort::HubOut) && hubTurnNodes.count(port.node)) {
			auto it = portGeo.find({ port.node, -1, port.kind == ManualPort::Hub ? 1 : 0 });
			if (it != portGeo.end()) {
				r.x = it->second.x; r.y = it->second.y; r.z = it->second.z; r.hx = it->second.hx; r.hy = it->second.hy;
				r.endpoint = ExternOf(it->second.name);
				r.ok = true;
			}
		}
		else if (port.kind == ManualPort::Hub) {
			const TransitData::Node& node = data.nodes[port.node];
			auto heading = hubHeading.count(port.node) ? hubHeading[port.node] : make_pair(port.hx, port.hy);
			r.x = node.x; r.y = node.y; r.z = node.z; r.hx = heading.first; r.hy = heading.second;
			r.endpoint = ExternOf(ensureExtern(port.node, node.x, node.y, node.z, r.hx, r.hy));
			r.ok = true;
		}
		else {
			auto it = portGeo.find({ port.node, port.conn, port.kind == ManualPort::NodeIn ? 1 : 0 });
			if (it != portGeo.end()) {
				r.x = it->second.x; r.y = it->second.y; r.z = it->second.z; r.hx = it->second.hx; r.hy = it->second.hy;
				r.endpoint = ExternOf(it->second.name);
				r.ok = true;
			}
		}
		return r;
	};
	for (const ManualGap& gap : plan.gaps) {
		Resolved from = resolve(gap.from), to = resolve(gap.to);
		if (!from.ok || !to.ok) continue;
		float chord = hypotf(to.x - from.x, to.y - from.y);
		if (chord < 1e-3f) continue;
		float cx = (to.x - from.x) / chord, cy = (to.y - from.y) / chord;
		if (from.hx * cx + from.hy * cy < 0.5f || to.hx * cx + to.hy * cy < 0.5f) {
			debugf(DebugLevel::Warning, "TrainRoute manual: gap link (%.1f,%.1f)->(%.1f,%.1f) bends more than 60 deg at an end; move the nearest transit node in line with the station track.\n",
				from.x, from.y, to.x, to.y);
		}
		int index = addLink(from.endpoint, to.endpoint, { MakeBezierSegment(from.x, from.y, from.z, from.hx, from.hy, to.x, to.y, to.z, to.hx, to.hy, chord / 3.f) });
		if (index >= 0) manualGapLinks.push_back(index);
	}

	// ---- 线路 ----
	for (const vector<ManualLineEntry>& line : plan.lines) {
		vector<RouteLineEntry> entries;
		for (const ManualLineEntry& entry : line) {
			RouteLineEntry out;
			if (entry.station >= 0) { out.name = stationInfos[entry.station].name; out.stopIndex = entry.track; }
			else {
				Resolved r = resolve(entry.via);
				if (!r.ok) continue;
				out.name = r.endpoint.externName; out.stopIndex = -1;
			}
			entries.push_back(out);
		}
		if (entries.size() < 2) { debugf(DebugLevel::Warning, "TrainRoute manual: a line has fewer than 2 waypoints, dropped.\n"); continue; }
		lines.push_back(std::move(entries));
		lineVehicleCounts.push_back(kVehiclesPerTrainLine);
	}
	debugf(DebugLevel::Log, "TrainRoute manual: %d rail pairs, %d turn/gap links, %d lines.\n",
		static_cast<int>(manualPairs.size()), static_cast<int>(links.size()) - 2 * static_cast<int>(manualPairs.size()), static_cast<int>(lines.size()));
}

void TrainRoute::BuildManualTrack(const function<float(float, float)>& getHeight) {
	if (!manual) return;
	const TransitData& data = *manual;
	constexpr float kHalfGauge = kTrackGauge * 0.5f;

	for (const ManualPair& mp : manualPairs) {
		const TransitData::Conn& conn = data.conns[mp.conn];
		const string& kind = conn.kind;
		if (kind == "tunnel") continue; // 隧道段：在山体里，不动地形、不拆迁

		vector<CurveSample> mid = SampleBezier(mp.axis, TRACK_SAMPLE_STEP, mp.axis.x2 - mp.axis.x1, mp.axis.y2 - mp.axis.y1);
		const size_t n = mid.size();
		if (n < 2) continue;
		vector<float> arc(n, 0.f);
		for (size_t i = 1; i < n; i++) arc[i] = arc[i - 1] + hypotf(mid[i].x - mid[i - 1].x, mid[i].y - mid[i - 1].y);
		const float total = max(arc.back(), 1e-6f);
		// 中轴线每个采样点的高度：默认=两端节点高度的扁S；hug=地形高度+两端偏差的线性过渡。
		vector<float> z(n);
		for (size_t i = 0; i < n; i++) z[i] = mp.axis.z1 + (mp.axis.z2 - mp.axis.z1) * SmoothStep(arc[i] / total);
		if (kind == "hug") {
			float offA = mp.axis.z1 - getHeight(mid.front().x, mid.front().y);
			float offB = mp.axis.z2 - getHeight(mid.back().x, mid.back().y);
			for (size_t i = 0; i < n; i++) {
				float u = arc[i] / total;
				z[i] = getHeight(mid[i].x, mid[i].y) + offA + (offB - offA) * u;
			}
			z.front() = mp.axis.z1; z.back() = mp.axis.z2;
			// 铁轨按这条z剖面重铺(位置和端点不变，接缝仍然连续)。
			vector<TrackSegment> right = OffsetRail(mid, kHalfGauge, false, 0.f, 0.f, false);
			vector<TrackSegment> left = OffsetRail(mid, -kHalfGauge, true, 0.f, 0.f, false);
			for (size_t k = 0; k < right.size(); k++) {
				right[k].z1 = right[k].c1z = z[k];
				right[k].z2 = right[k].c2z = z[k + 1];
				right[k].mesh = kTrainBridgeMesh; right[k].meshUnit = kTrainBridgeUnit;
			}
			for (size_t k = 0; k < left.size(); k++) {
				left[k].z1 = left[k].c1z = z[n - 1 - k];
				left[k].z2 = left[k].c2z = z[n - 2 - k];
				left[k].mesh = kTrainBridgeMesh; left[k].meshUnit = kTrainBridgeUnit;
			}
			links[static_cast<size_t>(mp.right)].segments = std::move(right);
			links[static_cast<size_t>(mp.left)].segments = std::move(left);
		}

		vector<pair<float, float>> centerline(n);
		for (size_t i = 0; i < n; i++) centerline[i] = { mid[i].x, mid[i].y };
		AddDemolition(centerline, trackWidth);

		if (kind == "mouth") {
			auto adjoinsTunnel = [&](int node) {
				for (size_t k = 0; k < data.conns.size(); k++) {
					if (static_cast<int>(k) == mp.conn) continue;
					const TransitData::Conn& other = data.conns[k];
					if (other.route == conn.route && other.kind == "tunnel" && (other.from == node || other.to == node)) return true;
				}
				return false;
			};
			bool wallStart = adjoinsTunnel(conn.from), wallEnd = adjoinsTunnel(conn.to);
			for (size_t i = 0; i + 1 < n; i++) {
				Connection chord = ChordConnection(mid[i].x, mid[i].y, z[i], mid[i + 1].x, mid[i + 1].y, z[i + 1]);
				AddTunnelHatch(&chord, 0.f, 1.f, 2.f * kTrackGauge,
					(wallStart && i == 0) ? z[i] + TUNNEL_ROOF_CLEARANCE : kNoEndWall,
					(wallEnd && i + 2 == n) ? z[i + 1] + TUNNEL_ROOF_CLEARANCE : kNoEndWall);
			}
		}
		else if (kind == "bridge") {
			vector<Connection> chain;
			for (size_t i = 0; i + 1 < n; i++) chain.push_back(ChordConnection(mid[i].x, mid[i].y, z[i], mid[i + 1].x, mid[i + 1].y, z[i + 1]));
			vector<vector<Connection>> chains;
			chains.push_back(std::move(chain));
			AddPier("", std::move(chains), PIER_DEFAULT_SPACING, PIER_MIN_SPACING, PIER_RADIUS);
		}
		else if (kind == "hug") { // 贴合地形：地形贴合轨道；ground(平地)只拆迁、不改地形，和路网一致
			for (size_t i = 0; i + 1 < n; i++) {
				Connection chord = ChordConnection(mid[i].x, mid[i].y, z[i], mid[i + 1].x, mid[i + 1].y, z[i + 1]);
				AddTerrainFit(&chord, 0.f, 1.f, trackWidth, TERRAIN_FIT_FALLOFF);
			}
		}
	}

	// 缺口连线(站端到节点)：火车route一律按桥处理——拆迁 + 沿线放桥墩(编辑器里没有这段线，没有类型可选)。
	for (int index : manualGapLinks) {
		for (const TrackSegment& seg : links[static_cast<size_t>(index)].segments) {
			vector<CurveSample> mid = SampleBezier(seg, TRACK_SAMPLE_STEP, seg.x2 - seg.x1, seg.y2 - seg.y1);
			const size_t n = mid.size();
			if (n < 2) continue;
			vector<float> arc(n, 0.f);
			for (size_t i = 1; i < n; i++) arc[i] = arc[i - 1] + hypotf(mid[i].x - mid[i - 1].x, mid[i].y - mid[i - 1].y);
			const float total = max(arc.back(), 1e-6f);
			vector<pair<float, float>> centerline(n);
			for (size_t i = 0; i < n; i++) centerline[i] = { mid[i].x, mid[i].y };
			AddDemolition(centerline, trackWidth);
			vector<Connection> chain;
			for (size_t i = 0; i + 1 < n; i++) {
				float za = seg.z1 + (seg.z2 - seg.z1) * SmoothStep(arc[i] / total);
				float zb = seg.z1 + (seg.z2 - seg.z1) * SmoothStep(arc[i + 1] / total);
				chain.push_back(ChordConnection(mid[i].x, mid[i].y, za, mid[i + 1].x, mid[i + 1].y, zb));
			}
			vector<vector<Connection>> chains;
			chains.push_back(std::move(chain));
			AddPier("", std::move(chains), PIER_DEFAULT_SPACING, PIER_MIN_SPACING, PIER_RADIUS);
		}
	}
}
