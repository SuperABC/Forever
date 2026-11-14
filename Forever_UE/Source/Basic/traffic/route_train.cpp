#include "route_train.h"

#include "route_common.h"
#include "station_train.h"

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
#define TUNNEL_MIN_OPEN_LENGTH 32.f
#define TRACK_SAMPLE_STEP 1.f

// 车站两端各留这么多个采样点保持水平，不在车站范围内爬坡/下坡。
#define TRACK_STATION_FLAT_SAMPLES 2

namespace {
	float SmoothStep(float u) { return u * u * (3.f - 2.f * u); }
}

int TrainRoute::count = 0;

TrainRoute::TrainRoute() : id(count++) {
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
	auto boundaryExtern = [&](const TrainStationLayout& station, int e, char label, bool straight) {
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
		// straight=true：不挑边界，沿端的航向直线射到地图边界(label只用来给extern起名)，边界法向取航向。
		float qx, qy, distance;
		if (straight) {
			nx = end.hx; ny = end.hy;
			auto [hitX, hitY] = RayBoxIntersection(end.px, end.py, end.hx, end.hy, sizeX, sizeY);
			qx = hitX; qy = hitY;
			distance = sqrtf((qx - end.px) * (qx - end.px) + (qy - end.py) * (qy - end.py));
		}
		else {
			// 从实际连出来的方向(端的航向)向前延伸 D/3 得到控制点C(D=轴点P到该边界的垂直距离)，C到边界的垂足就是extern的位置。
			auto [footX, footY] = foot(end.px, end.py);
			distance = sqrtf((footX - end.px) * (footX - end.px) + (footY - end.py) * (footY - end.py));
			auto [sideX, sideY] = foot(end.px + end.hx * distance / 3.f, end.py + end.hy * distance / 3.f);
			qx = sideX; qy = sideY;
		}
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

	// ---- 线路拓扑：一个中央车站 + 东南西北各一座(`building_train_station`的Assign放的) ----
	// 中央车站=到其它站距离之和最小的站；其它站按相对中央的主方向分到东/南/西/北，每个方向一座。
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

	if (count == 1) {
		// 单站：两端沿航向直线伸到地图外回转，往返各用一条轨道。
		string bd1 = boundaryExtern(hub, 1, '*', true);
		string bd0 = boundaryExtern(hub, 0, '*', true);
		addLine({ { hub.info.name, 1 }, { bd1, -1 }, { hub.info.name, 0 }, { bd0, -1 } });
		return;
	}

	const TrainStationLayout* outer[4] = { nullptr, nullptr, nullptr, nullptr }; // 0东 1南 2西 3北
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

	// 一条轴线(北-南、东-西)：A外站 -> 中央 -> B外站 -> B外边界回转 -> B外站 -> 中央 -> A外站 -> A外边界回转，首尾相接。
	// 中央车站的两端：X端朝A、1-X端朝B(两者都有时按航向和方向的点积之和最大选X)，穿过中央是X端进、1-X端出，所以A/B必须接在相反的两端；
	// 某个方向没有外站时，中央车站对应的那一端直接连到该方向的城外。
	auto axisLine = [&](const TrainStationLayout* a, char labelA, const TrainStationLayout* b, char labelB) {
		// 方向向量：有外站取到外站的方向，没有就取该方向的地图边界方向(北-y、南+y、东+x、西-x)。
		auto directionOf = [&](const TrainStationLayout* s, char label) {
			if (s) return unitToward(hub.info.x, hub.info.y, s->info.x, s->info.y);
			return label == 'N' ? make_pair(0.f, -1.f) : label == 'S' ? make_pair(0.f, 1.f) : label == 'E' ? make_pair(1.f, 0.f) : make_pair(-1.f, 0.f);
		};
		auto [uax, uay] = directionOf(a, labelA);
		auto [ubx, uby] = directionOf(b, labelB);
		float score0 = hub.hx[0] * uax + hub.hy[0] * uay + hub.hx[1] * ubx + hub.hy[1] * uby;
		float score1 = hub.hx[1] * uax + hub.hy[1] * uay + hub.hx[0] * ubx + hub.hy[0] * uby;
		int x = score0 >= score1 ? 0 : 1;
		const string& hubName = hub.info.name;
		vector<RouteLineEntry> entries;
		int ea = 0, eb = 0;
		string bdA, bdB;
		if (a) {
			ea = a->BestTrack(hub.info.x - a->info.x, hub.info.y - a->info.y); // a朝向中央的那一端
			connectEnds(*a, ea, hub, x);
			bdA = boundaryExtern(*a, 1 - ea, labelA, false);
		}
		else {
			bdA = boundaryExtern(hub, x, labelA, false); // 没有A方向的外站：中央车站直接连到A方向的城外
		}
		if (b) {
			eb = b->BestTrack(hub.info.x - b->info.x, hub.info.y - b->info.y);
			connectEnds(*b, eb, hub, 1 - x);
			bdB = boundaryExtern(*b, 1 - eb, labelB, false);
		}
		else {
			bdB = boundaryExtern(hub, 1 - x, labelB, false); // 同理，连到B方向的城外
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
	axisLine(outer[3], 'N', outer[1], 'S');
	axisLine(outer[0], 'E', outer[2], 'W');
}

void TrainRoute::BuildTrack(const function<const string&(int, int)>& getTerrain,
	const function<pair<bool, float>(int, int)>& getWater,
	const function<float(float, float)>& getHeight) {
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
			debugf("Log: TrainRoute pair (%d,%d) mountain arc [%.1f,%.1f] of %.1f -> %s.\n",
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
		auto hatchRange = [&](float s1, float s2) {
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
				AddTunnelHatch(&chordConnection, 0.f, 1.f, 2.f * kTrackGauge);
			}
		};
		for (const Zone& zone : zones) {
			if (!zone.tunnel) continue;
			hatchRange(zone.sIn - TUNNEL_HATCH_LENGTH, zone.sIn);
			hatchRange(zone.sOut, zone.sOut + TUNNEL_HATCH_LENGTH);
		}
	}
}
