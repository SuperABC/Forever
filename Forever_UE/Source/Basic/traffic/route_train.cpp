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

// ---- TrainRoute::BuildTrack：过山判定和分段常量照抄roadnet_jing.cpp(JingRoadnet::buildArm)，
// 两边数值需要保持一致，改动时一起看。 ----
#define MOUNTAIN_MAX_SLOPE 0.5f
#define TERRAIN_FIT_FALLOFF 1.5f
#define TUNNEL_HEIGHT -1.f
#define TUNNEL_HATCH_LENGTH 8.f
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
	constexpr float kBridgeHeight = 1.5f; // 出站后全程架桥的高度(地图单位，1单位=10m)
	constexpr float kRiseDistance = 4.f; // 站内出站口到桥高节点的水平距离
	constexpr float kRiseArm = 0.5f;     // 上升/下降S形曲线的控制臂长
	constexpr int kVehiclesPerTrainLine = 4; // 每条火车线路上的车辆数

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

	// 一条线路的构建状态：已经排好的必经点，加上最后一个/第一个站的轨道(用来在相邻站之间连link)。
	struct LineBuilder {
		vector<RouteLineEntry> entries;
		const TrainStationLayout* first = nullptr;
		int firstTrack = 0;
		const TrainStationLayout* last = nullptr;
		int lastTrack = 0;
		char pendingLabel = 0; // 下一次连接要绕地图边界label回转
		char startLabel = 0;   // 首尾相接(finish)时要绕地图边界label回转
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
	if (stations.empty()) return;

	// link：两个端点之间的完整几何由这里算好，Core只按它建曲线。同一条link被多条线路共用只加一次；每段都铺轨道mesh。
	set<pair<string, string>> linkKeys;
	auto addLink = [&](const RouteEndpoint& from, const RouteEndpoint& to, vector<TrackSegment> segments) {
		if (!linkKeys.insert({ EndpointKey(from), EndpointKey(to) }).second) return;
		RouteLinkSpec link;
		link.from = from;
		link.to = to;
		for (TrackSegment& seg : segments) { seg.mesh = kTrainTrackMesh; seg.meshUnit = kTrainTrackUnit; }
		link.segments = std::move(segments);
		links.push_back(std::move(link));
	};
	// 出站端：出站口沿航向kRiseDistance、抬升到桥高；进站端反过来。(桥高节点位置)
	auto riseOutPoint = [&](const TrainStationLayout& station, int t) {
		return make_pair(station.Out(t).x + station.hx[t] * kRiseDistance, station.Out(t).y + station.hy[t] * kRiseDistance);
	};
	auto riseInPoint = [&](const TrainStationLayout& station, int t) {
		return make_pair(station.In(t).x - station.hx[t] * kRiseDistance, station.In(t).y - station.hy[t] * kRiseDistance);
	};
	auto riseSegment = [&](const TrainStationLayout& station, int t) {
		auto [rx, ry] = riseOutPoint(station, t);
		return MakeBezierSegment(station.Out(t).x, station.Out(t).y, station.Out(t).z, station.hx[t], station.hy[t],
			rx, ry, kBridgeHeight, station.hx[t], station.hy[t], kRiseArm);
	};
	auto descendSegment = [&](const TrainStationLayout& station, int t) {
		auto [rx, ry] = riseInPoint(station, t);
		return MakeBezierSegment(rx, ry, kBridgeHeight, station.hx[t], station.hy[t],
			station.In(t).x, station.In(t).y, station.In(t).z, station.hx[t], station.hy[t], kRiseArm);
	};

	// 车站之间：A的出站端(轨道tA)->B的进站端(轨道tB)。抬升->沿地形的桥(控制臂=两点水平距离1/3)->下降。
	auto connectSegments = [&](const TrainStationLayout& a, int ta, const TrainStationLayout& b, int tb) {
		auto [ax, ay] = riseOutPoint(a, ta);
		auto [bx, by] = riseInPoint(b, tb);
		float arm = sqrtf((bx - ax) * (bx - ax) + (by - ay) * (by - ay)) / 3.f;
		return vector<TrackSegment>{ riseSegment(a, ta),
			MakeBezierSegment(ax, ay, kBridgeHeight, a.hx[ta], a.hy[ta], bx, by, kBridgeHeight, b.hx[tb], b.hy[tb], arm, true),
			descendSegment(b, tb) };
	};

	// 沿station的端e(轨道e的出站端)向label边界('N'/'S'/'W'/'E')伸出再回来：出图点=向边界做垂线的垂足(边界在侧面时先沿航向前进D)，
	// 进图点同理(另一条轨道1-e的riseIn)，两点之间沿边界直线回转，从另一条轨道1-e回到车站。
	// 出图点放一个extern node：出站口->extern(抬升+桥)，extern->另一条轨道的进站口(沿边界回转+回程+下降)。返回extern名字。
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
		int enter = 1 - e;
		const RoutePort& out = station.Out(e);
		const RoutePort& in = station.In(enter);
		auto [rox, roy] = riseOutPoint(station, e);
		auto [rix, riy] = riseInPoint(station, enter);
		// 车站到边界的距离D(从桥高节点量)。边界正对着航向(n·h≈1)时出图点就是正前方的垂足；边界在侧面时，出图点沿航向再前进D再取垂足，
		// 让轨道从航向平滑拐向边界(约1/4弧)，不在站旁直角拐弯。
		auto boundaryDistance = [&](float px, float py) {
			auto [fx, fy] = foot(px, py);
			return sqrtf((fx - px) * (fx - px) + (fy - py) * (fy - py));
		};
		float outDistance = boundaryDistance(rox, roy);
		float inDistance = boundaryDistance(rix, riy);
		float outAlong = fabsf(station.hx[e] * nx + station.hy[e] * ny) > 0.5f ? 0.f : outDistance;
		float inAlong = fabsf(station.hx[enter] * nx + station.hy[enter] * ny) > 0.5f ? 0.f : inDistance;
		auto [ox, oy] = foot(rox + station.hx[e] * outAlong, roy + station.hy[e] * outAlong);
		auto [ix, iy] = foot(rix - station.hx[enter] * inAlong, riy - station.hy[enter] * inAlong);
		float sideX = ix - ox, sideY = iy - oy;
		float sideLength = sqrtf(sideX * sideX + sideY * sideY);
		if (sideLength > 1e-6f) { sideX /= sideLength; sideY /= sideLength; }
		else { sideX = -ny; sideY = nx; } // 两点重合(理论不会)，任取一个垂直方向

		string name = "bd_" + station.info.name + "_" + to_string(e) + "_" + label;
		if (linkKeys.insert({ "extern", name }).second) {
			RouteExternNode node;
			node.name = name;
			node.x = ox; node.y = oy; node.z = kBridgeHeight; node.dirX = nx; node.dirY = ny;
			externs.push_back(node);
		}
		// 从车站出来/回到车站的控制臂=车站到边界距离的1/3。
		float outArm = outDistance / 3.f;
		float inArm = inDistance / 3.f;
		addLink(OutletOf(station, e), ExternOf(name), { riseSegment(station, e),
			MakeBezierSegment(rox, roy, kBridgeHeight, station.hx[e], station.hy[e], ox, oy, kBridgeHeight, nx, ny, outArm, true) });
		addLink(ExternOf(name), InletOf(station, enter), {
			MakeBezierSegment(ox, oy, kBridgeHeight, sideX, sideY, ix, iy, kBridgeHeight, sideX, sideY, 0.f, true),
			MakeBezierSegment(ix, iy, kBridgeHeight, -nx, -ny, rix, riy, kBridgeHeight, station.hx[enter], station.hy[enter], inArm, true),
			descendSegment(station, enter) });
		return name;
	};
	// 航向主轴方向的边界。
	auto dominantLabel = [](float dx, float dy) {
		if (fabsf(dx) >= fabsf(dy)) return dx >= 0.f ? 'E' : 'W';
		return dy >= 0.f ? 'S' : 'N';
	};

	// 把(from站轨道ta的出站端)连到(to站轨道tb的进站端)；label非0表示绕地图边界回转(from/to是同一座站的两条轨道)，此时在entries里串入extern点。
	auto connect = [&](LineBuilder& line, const TrainStationLayout& a, int ta, const TrainStationLayout& b, int tb, char label) {
		if (label) {
			line.entries.push_back({ boundaryExtern(a, ta, label), -1 });
		}
		else {
			addLink(OutletOf(a, ta), InletOf(b, tb), connectSegments(a, ta, b, tb));
		}
	};
	auto appendPass = [&](LineBuilder& line, const TrainStationLayout& station, int t) {
		if (line.last) {
			connect(line, *line.last, line.lastTrack, station, t, line.pendingLabel);
			line.pendingLabel = 0;
		}
		else {
			line.first = &station;
			line.firstTrack = t;
		}
		line.entries.push_back({ station.info.name, t });
		line.last = &station;
		line.lastTrack = t;
	};
	// 在当前位置(上一站的出站端)绕地图边界label回转：线路还没开始(空)时记成首尾相接处的回转。
	auto appendBoundary = [&](LineBuilder& line, char label) {
		if (line.last) line.pendingLabel = label;
		else line.startLabel = label;
	};
	auto finish = [&](LineBuilder& line) {
		if (!line.first || !line.last) return;
		connect(line, *line.last, line.lastTrack, *line.first, line.firstTrack, line.startLabel);
		lines.push_back(std::move(line.entries));
		lineVehicleCounts.push_back(kVehiclesPerTrainLine);
	};
	auto unit = [](float dx, float dy) {
		float length = sqrtf(dx * dx + dy * dy);
		return length > 1e-6f ? make_pair(dx / length, dy / length) : make_pair(1.f, 0.f);
	};

	size_t count = stations.size();
	if (count == 1) {
		// 单站：两端各伸到边界回转，往返各用一条轨道。
		const TrainStationLayout& s = stations[0];
		LineBuilder line;
		appendBoundary(line, dominantLabel(s.hx[0], s.hy[0]));
		appendPass(line, s, 1);
		appendBoundary(line, dominantLabel(s.hx[1], s.hy[1]));
		appendPass(line, s, 0);
		finish(line);
		return;
	}
	if (count == 2) {
		// 两站：A<->B直接相连，两端各自往外伸到边界回转(外端=航向背离对方的那一端)。一条线。
		const TrainStationLayout& a = stations[0];
		const TrainStationLayout& b = stations[1];
		auto [abx, aby] = unit(b.info.x - a.info.x, b.info.y - a.info.y);
		int ea = a.BestTrack(-abx, -aby), eb = b.BestTrack(abx, aby);
		LineBuilder line;
		appendBoundary(line, dominantLabel(a.hx[ea], a.hy[ea]));
		appendPass(line, a, 1 - ea);
		appendPass(line, b, eb);
		appendBoundary(line, dominantLabel(b.hx[eb], b.hy[eb]));
		appendPass(line, b, 1 - eb);
		appendPass(line, a, ea);
		finish(line);
		return;
	}

	// >=3站：绕地图中心排序(+Y向南，atan2升序=屏幕上的顺时针)。
	float centerX = static_cast<float>(sizeX) * 0.5f, centerY = static_cast<float>(sizeY) * 0.5f;
	vector<int> order(count);
	for (size_t i = 0; i < count; i++) order[i] = static_cast<int>(i);
	sort(order.begin(), order.end(), [&](int a, int b) {
		return atan2f(stations[a].info.y - centerY, stations[a].info.x - centerX) < atan2f(stations[b].info.y - centerY, stations[b].info.x - centerX);
	});
	vector<int> position(count);
	for (size_t i = 0; i < count; i++) position[order[i]] = static_cast<int>(i);
	// 顺时针环上每站走的轨道：航向最接近环切线(前一站->后一站)的那条；逆时针取另一条(航向正好相反)。
	vector<int> clockwiseTrack(count);
	for (size_t i = 0; i < count; i++) {
		const TrainStationLayout& prev = stations[order[(i + count - 1) % count]];
		const TrainStationLayout& next = stations[order[(i + 1) % count]];
		auto [tx, ty] = unit(next.info.x - prev.info.x, next.info.y - prev.info.y);
		clockwiseTrack[order[i]] = stations[order[i]].BestTrack(tx, ty);
	}

	{
		LineBuilder line;
		for (size_t i = 0; i < count; i++) appendPass(line, stations[order[i]], clockwiseTrack[order[i]]);
		finish(line);
	}
	{
		LineBuilder line;
		for (size_t i = count; i-- > 0; ) appendPass(line, stations[order[i]], 1 - clockwiseTrack[order[i]]);
		finish(line);
	}

	// 南北线/东西线：去程沿顺时针弧、回程沿逆时针弧，每个站(含端点站)的轨道和环线一致(靠右行驶，顺时针靠内侧、逆时针靠外侧)：
	// 起点站去程走顺时针轨道，所以extern接在它的进站端(1-clockwiseTrack)；终点站去程走顺时针轨道出站，extern接在它的出站端(clockwiseTrack)。
	// extern位置仍是该站向label边界做垂线的垂足，端的朝向不一定正对边界(控制点沿航向4单位后转弯)。
	auto express = [&](int from, char fromLabel, int to, char toLabel) {
		if (from == to) return;
		const TrainStationLayout& a = stations[from];
		const TrainStationLayout& b = stations[to];
		int ea = 1 - clockwiseTrack[from], eb = clockwiseTrack[to];
		vector<int> arc; // 顺时针弧上的途经站
		for (int k = (position[from] + 1) % static_cast<int>(count); k != position[to]; k = (k + 1) % static_cast<int>(count)) {
			arc.push_back(order[k]);
		}
		LineBuilder line;
		appendBoundary(line, fromLabel);
		appendPass(line, a, 1 - ea);
		for (int s : arc) appendPass(line, stations[s], clockwiseTrack[s]);
		appendPass(line, b, eb);
		appendBoundary(line, toLabel);
		appendPass(line, b, 1 - eb);
		for (size_t k = arc.size(); k-- > 0; ) appendPass(line, stations[arc[k]], 1 - clockwiseTrack[arc[k]]);
		appendPass(line, a, ea);
		finish(line);
	};
	int north = 0, south = 0, west = 0, east = 0;
	for (size_t i = 1; i < count; i++) {
		if (stations[i].info.y < stations[north].info.y) north = static_cast<int>(i);
		if (stations[i].info.y > stations[south].info.y) south = static_cast<int>(i);
		if (stations[i].info.x < stations[west].info.x) west = static_cast<int>(i);
		if (stations[i].info.x > stations[east].info.x) east = static_cast<int>(i);
	}
	express(north, 'N', south, 'S');
	express(west, 'W', east, 'E');
}

void TrainRoute::BuildTrack(const function<const string&(int, int)>& getTerrain,
	const function<pair<bool, float>(int, int)>& getWater,
	const function<float(float, float)>& getHeight) {
	// 把一段mod自己算的基础曲线(平面三次贝塞尔+端点高度)按地形采样成最终几何：过山抬升/挖隧道。
	auto refine = [&](const TrackSegment& base, size_t linkIndex, vector<TrackSegment>& out) {
		struct Point { float x, y, z; };
		Point a{ base.x1, base.y1, base.z1 }, b{ base.x2, base.y2, base.z2 };
		float c1x = base.c1x, c1y = base.c1y, c2x = base.c2x, c2y = base.c2y;

		auto bezier = [&](float t, float& x, float& y) {
			float m = 1.f - t;
			x = m * m * m * a.x + 3.f * m * m * t * c1x + 3.f * m * t * t * c2x + t * t * t * b.x;
			y = m * m * m * a.y + 3.f * m * m * t * c1y + 3.f * m * t * t * c2y + t * t * t * b.y;
		};

		float chord = sqrtf((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y));
		int fine = max(16, static_cast<int>(ceilf(chord * 8.f)));
		vector<float> fineT(fine + 1), fineArc(fine + 1);
		float px, py;
		bezier(0.f, px, py);
		fineT[0] = 0.f; fineArc[0] = 0.f;
		for (int i = 1; i <= fine; i++) {
			float x, y;
			float t = static_cast<float>(i) / static_cast<float>(fine);
			bezier(t, x, y);
			fineT[i] = t;
			fineArc[i] = fineArc[i - 1] + sqrtf((x - px) * (x - px) + (y - py) * (y - py));
			px = x; py = y;
		}
		float totalLength = fineArc[fine];
		if (totalLength < 1e-3f) { out.push_back(base); return; }

		int count = max(1, static_cast<int>(ceilf(totalLength / TRACK_SAMPLE_STEP)));
		vector<float> sx(count + 1), sy(count + 1), arc(count + 1);
		{
			int cursor = 0;
			for (int i = 0; i <= count; i++) {
				float target = totalLength * static_cast<float>(i) / static_cast<float>(count);
				while (cursor + 1 < fine && fineArc[cursor + 1] < target) cursor++;
				float span = fineArc[cursor + 1] - fineArc[cursor];
				float f = span > 1e-9f ? (target - fineArc[cursor]) / span : 0.f;
				bezier(fineT[cursor] + (fineT[cursor + 1] - fineT[cursor]) * f, sx[i], sy[i]);
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

		// ---- 第一遍：每个采样点的地形类型和高度。getTerrain返回""(出图)按平地处理——地图边缘站点正好
		// 落在边界上。 ----
		vector<string> terr(count + 1);
		vector<float> heights(count + 1);
		for (int i = 0; i <= count; i++) {
			terr[i] = getTerrain(static_cast<int>(floorf(sx[i])), static_cast<int>(floorf(sy[i])));
			heights[i] = getHeight(sx[i], sy[i]);
		}
		auto isGood = [&](int k) { return terr[k] == "" || terr[k] == "plain" || terr[k] == "construction"; };
		auto allGood = [&](int from, int to) {
			for (int i = from; i <= to; i++) if (!isGood(i)) return false;
			return true;
		};

		// ---- 切出每座山并决定做法(抬升/隧道)，规则同buildArm。 ----
		struct TunnelSpan { int p; int tIn; int tOut; int q; };
		vector<TunnelSpan> tunnels;
		vector<bool> raised(count + 1, false);
		const int rampSamples = static_cast<int>(ceilf(TUNNEL_HATCH_LENGTH / TRACK_SAMPLE_STEP));
		const int lastMountainSample = count - TRACK_STATION_FLAT_SAMPLES;
		int lastQ = TRACK_STATION_FLAT_SAMPLES;
		for (int k = TRACK_STATION_FLAT_SAMPLES; k <= lastMountainSample; ) {
			if (terr[k] != "terrain_mountain") { k++; continue; }
			int first = k;
			while (k + 1 <= lastMountainSample && terr[k + 1] == "terrain_mountain") k++;
			int last = k;
			k++;

			float maxSlope = 0.f;
			int from = max(0, first - 1), to = min(count, last + 1);
			for (int i = from; i < to; i++) {
				maxSlope = max(maxSlope, fabsf(heights[i + 1] - heights[i]) / max(1e-3f, arc[i + 1] - arc[i]));
			}
			bool seaBehind = last + 1 <= count && terr[last + 1] == "terrain_ocean";
			bool raise = seaBehind || maxSlope <= MOUNTAIN_MAX_SLOPE;
			if (!raise) {
				TunnelSpan span{ first - 1 - rampSamples, first - 1, last + 1, last + 1 + rampSamples };
				bool roomBefore = span.p >= lastQ && allGood(span.p, span.tIn);
				bool roomAfter = span.q <= lastMountainSample && allGood(span.tOut, span.q);
				if (roomBefore && roomAfter) {
					tunnels.push_back(span);
					lastQ = span.q;
				}
				else {
					raise = true;
				}
			}
			if (raise) {
				for (int i = first; i <= last; i++) raised[i] = true;
			}
			debugf("Log: TrainRoute link %d mountain samples [%d,%d] max slope %.3f sea behind %d -> %s.\n",
				static_cast<int>(linkIndex), first, last, maxSlope, seaBehind ? 1 : 0, raise ? "raise" : "tunnel");
		}

		// ---- 每个采样点的高度、坡度(Z每水平单位的变化量)和归类。基准高度在两端站点z之间线性过渡。 ----
		vector<float> z(count + 1), slope(count + 1, 0.f);
		vector<char> inTunnelLevel(count + 1, 0), inRamp(count + 1, 0);
		for (int i = 0; i <= count; i++) {
			float base = a.z + (b.z - a.z) * (arc[i] / totalLength);
			z[i] = raised[i] ? max(heights[i], base) : base;
		}
		for (const TunnelSpan& t : tunnels) {
			for (int i = t.p; i <= t.q; i++) {
				float base = a.z + (b.z - a.z) * (arc[i] / totalLength);
				float depth;
				float dDepth = 0.f; // 深度对弧长的导数
				if (i <= t.tIn) {
					float u = static_cast<float>(i - t.p) / static_cast<float>(t.tIn - t.p);
					depth = SmoothStep(u);
					dDepth = 6.f * u * (1.f - u) / (arc[t.tIn] - arc[t.p]);
				}
				else if (i < t.tOut) {
					depth = 1.f;
				}
				else {
					float u = static_cast<float>(i - t.tOut) / static_cast<float>(t.q - t.tOut);
					depth = 1.f - SmoothStep(u);
					dDepth = -6.f * u * (1.f - u) / (arc[t.q] - arc[t.tOut]);
				}
				z[i] = base + TUNNEL_HEIGHT * depth;
				slope[i] = TUNNEL_HEIGHT * dDepth;
				if (i >= t.tIn && i <= t.tOut) inTunnelLevel[i] = 1;
				else inRamp[i] = 1;
			}
		}
		// 抬升段的坡度取前后相邻点连线的坡度(Catmull-Rom)；山脚/平地/两端取0，不带着惯性冲到基准以下。
		for (int i = 1; i < count; i++) {
			if (!raised[i]) continue;
			float run = arc[i + 1] - arc[i - 1];
			slope[i] = run > 1e-6f ? (z[i + 1] - z[i - 1]) / run : 0.f;
		}

		// ---- 第二遍：每相邻两点一段三次贝塞尔(平面上是弦，Z用端点坡度做Hermite)，坡度夹到单调范围
		// (与这段高度差同号、不超过3倍平均坡度)，三次曲线不会冲出两端高度。完全水平且共线的连续
		// 采样点合并成一段，不然火车几百单位的线路会有几百个无谓的小段。 ----
		auto segmentTag = [&](int i) -> int { // 相邻两点(i,i+1)这一段的归类，合并只能在同类里进行
			if (inTunnelLevel[i] && inTunnelLevel[i + 1]) return 3;
			if (raised[i] || raised[i + 1]) return 1;
			if (inRamp[i] || inRamp[i + 1] || inTunnelLevel[i] || inTunnelLevel[i + 1]) return 2;
			return 0;
		};
		auto isFlat = [&](int i) {
			return fabsf(z[i] - z[i + 1]) < 1e-4f && fabsf(slope[i]) < 1e-6f && fabsf(slope[i + 1]) < 1e-6f;
		};

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

			float dx = sx[j] - sx[i], dy = sy[j] - sy[i];
			float run = sqrtf(dx * dx + dy * dy);
			float delta = run > 1e-6f ? (z[j] - z[i]) / run : 0.f;
			auto monotone = [delta](float s) {
				if (delta == 0.f) return 0.f;
				float aligned = s * (delta > 0.f ? 1.f : -1.f);
				aligned = max(0.f, min(aligned, 3.f * fabsf(delta)));
				return aligned * (delta > 0.f ? 1.f : -1.f);
			};
			float startSlope = monotone(slope[i]), endSlope = monotone(slope[j]);

			TrackSegment seg;
			seg.x1 = sx[i]; seg.y1 = sy[i]; seg.z1 = z[i];
			seg.c1x = sx[i] + dx / 3.f; seg.c1y = sy[i] + dy / 3.f; seg.c1z = z[i] + startSlope * run / 3.f;
			seg.c2x = sx[i] + dx * 2.f / 3.f; seg.c2y = sy[i] + dy * 2.f / 3.f; seg.c2z = z[j] - endSlope * run / 3.f;
			seg.x2 = sx[j]; seg.y2 = sy[j]; seg.z2 = z[j];
			seg.mesh = base.mesh;
			seg.meshUnit = base.meshUnit;
			out.push_back(seg);

			// 请求：抬升段贴地；隧道口(坡道，tag 2)挖带侧壁的洞。水平隧道段在山体里面不挖。
			int tag = segmentTag(i);
			if (tag == 1 || tag == 2) {
				Connection chordConnection(Node("route", seg.x1, seg.y1, seg.z1), Node("route", seg.x2, seg.y2, seg.z2));
				chordConnection.AddControls({ { Node("route_ctrl", seg.c1x, seg.c1y, seg.c1z), 1.f }, { Node("route_ctrl", seg.c2x, seg.c2y, seg.c2z), 1.f } });
				if (tag == 1) AddTerrainFit(&chordConnection, 0.f, 1.f, trackWidth, TERRAIN_FIT_FALLOFF);
				else AddTunnelHatch(&chordConnection, 0.f, 1.f, trackWidth);
			}
			i = j;
		}
	};

	for (size_t linkIndex = 0; linkIndex < links.size(); linkIndex++) {
		vector<TrackSegment> refined;
		for (const TrackSegment& base : links[linkIndex].segments) {
			if (!base.terrainFollow) { refined.push_back(base); continue; }
			refine(base, linkIndex, refined);
		}
		links[linkIndex].segments = std::move(refined);
	}
}
