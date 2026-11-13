#include "route_air.h"

#include "route_common.h"

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <map>

using namespace std;

int AirRoute::count = 0;

AirRoute::AirRoute() : id(count++) {
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

	// 校准依据见route_air.md：跑一圈约100真实秒、停约6真实秒。
	speed = 0.175f;
	dwellSeconds = 720.f;

	// 飞机比火车更大更重，启动/刹车更缓。
	easeSeconds = 50.f;

	// 降落跑道滑行到起飞跑道(同一机场两个接口之间的link)的速度倍率，不能按起飞速度滑。读档时重建边几何也要用，所以放这里。
	sameStationSpeedScale = 0.1f;

	// 贝塞尔两端水平切线，沿切线放控制点，按端点类型(站名里的_sky/_mid见LayoutRoute)：
	//   天空节点<->天空节点两点水平距离的1/3；in/out<->中点不加控制点(直线)；in/out U形转场(两端都是真实站点)1单位；
	//   天空节点<->in/out(及连外界的extern)16单位(160m)。
	controlLength = 16.f;
	controlRules = {
		// 同一座机场的出/进extern之间、天空节点<->自己的extern：直线
		{ "_ext", "_ext", 0.f, false, true },
		{ "_sky", "_ext", 0.f, false, true },
		{ "_ext", "_sky", 0.f, false, true },
		{ "_sky", "_sky", 1.f / 3.f, true },
		{ "_mid", "", 0.f },
		{ "", "_mid", 0.f },
		{ "@real", "@real", 1.f },
		{ "_sky", "", 16.f },
		{ "", "_sky", 16.f },
	};
	tiltAlongPath = true; // 爬升/下降时机头跟着路径抬低
}

namespace {
	// 一座机场的接口，按[in0,out0,in1,out1,...]成对排列，一对=一条跑道；目前只用前两条(环线/互联都只做两条)。
	struct Airport {
		vector<RouteStationInfo> interfaces;
		int stationIndex = 0;
		float x = 0.f, y = 0.f;
		// 每条跑道的中点停靠点、in/out接口后方/前方的天空节点(RouteStop)，LayoutRoute里填。
		RouteStop mid[2], skyIn[2], skyOut[2];

		int RunwayCount() const { return min(2, static_cast<int>(interfaces.size()) / 2); }
		const RouteStationInfo& In(int runway) const { return interfaces[runway * 2]; }
		const RouteStationInfo& Out(int runway) const { return interfaces[runway * 2 + 1]; }
		// 跑道朝向(单位向量)：in接口的出发方向。
		pair<float, float> Heading(int runway) const {
			const RouteStationInfo& in = In(runway);
			float length = sqrtf(in.departDirX * in.departDirX + in.departDirY * in.departDirY);
			if (length < 1e-6f) return { 1.f, 0.f };
			return { in.departDirX / length, in.departDirY / length };
		}
		// 朝向最接近(dirX,dirY)的跑道。
		int BestRunway(float dirX, float dirY) const {
			int best = 0;
			float bestDot = -FLT_MAX;
			for (int runway = 0; runway < RunwayCount(); runway++) {
				auto [hx, hy] = Heading(runway);
				float dot = hx * dirX + hy * dirY;
				if (dot > bestDot) { bestDot = dot; best = runway; }
			}
			return best;
		}
		RouteStop InStop(int runway) const { return RouteStop{ stationIndex, In(runway).interfaceIndex, false }; }
		RouteStop OutStop(int runway) const { return RouteStop{ stationIndex, Out(runway).interfaceIndex, false }; }
	};
}

void AirRoute::LayoutRoute(const vector<RouteStationInfo>& interfaces, int sizeX, int sizeY) {
	constexpr float kCruiseHeight = 100.f; // 飞机巡航高度(地图单位)
	// 起飞/降落爬升斜率tan=0.5：水平每出去20抬升10，所以天空节点离跑道端头的水平距离=巡航高度/0.5。
	constexpr float kClimbSlope = 0.5f;
	constexpr float kSkyLead = kCruiseHeight / kClimbSlope;

	// 按站点分组(Core按站点顺序给接口)。
	map<int, Airport> groups;
	int realStations = 0;
	for (const RouteStationInfo& info : interfaces) {
		if (info.isEdge) continue;
		groups[info.stationIndex].interfaces.push_back(info);
		groups[info.stationIndex].stationIndex = info.stationIndex;
		realStations = max(realStations, info.stationIndex + 1);
	}
	vector<Airport> airports;
	for (auto& [index, airport] : groups) {
		if (airport.interfaces.size() < 4) continue;
		sort(airport.interfaces.begin(), airport.interfaces.end(),
			[](const RouteStationInfo& a, const RouteStationInfo& b) { return a.interfaceIndex < b.interfaceIndex; });
		for (const RouteStationInfo& info : airport.interfaces) { airport.x += info.x; airport.y += info.y; }
		airport.x /= static_cast<float>(airport.interfaces.size());
		airport.y /= static_cast<float>(airport.interfaces.size());
		airports.push_back(std::move(airport));
	}

	auto addLink = [&](const RouteStop& from, const RouteStop& to) { links.push_back({ from, to, false }); };
	// 追加一个边缘站点(合成站点，接在所有真实站点之后)，返回它的stop。
	auto addStation = [&](const string& name, float x, float y, float z, float arriveX, float arriveY,
		float departX, float departY, bool dwell = false, bool silent = false) {
		RouteStationInfo info;
		info.name = name;
		info.x = x; info.y = y; info.z = z;
		info.arriveDirX = arriveX; info.arriveDirY = arriveY;
		info.departDirX = departX; info.departDirY = departY;
		info.isEdge = true;
		edgeStations.push_back(info);
		return RouteStop{ realStations + static_cast<int>(edgeStations.size()) - 1, 0, dwell, silent };
	};
	auto infoOf = [&](const RouteStop& stop) -> RouteStationInfo& { return edgeStations[stop.station - realStations]; };
	auto insideMap = [&](float x, float y) { return x >= 0.f && x <= static_cast<float>(sizeX) && y >= 0.f && y <= static_cast<float>(sizeY); };

	// 每座机场：先给每条跑道的in/out接口算好天空节点(接口沿跑道方向后/前kSkyLead，高度=巡航高度)和跑道中点停靠点，
	// 接口<->天空节点用一条贝塞尔(两端水平切线，控制臂controlLength)连接；再各自连外界、自成一圈环线。
	for (Airport& airport : airports) {
		string tag = "air" + to_string(airport.stationIndex);
		int runways = airport.RunwayCount();
		for (int r = 0; r < runways; r++) {
			const RouteStationInfo& in = airport.In(r);
			const RouteStationInfo& out = airport.Out(r);
			auto [hx, hy] = airport.Heading(r);
			string runway = tag + "_r" + to_string(r);
			float outX = out.x + hx * kSkyLead, outY = out.y + hy * kSkyLead;
			float inX = in.x - hx * kSkyLead, inY = in.y - hy * kSkyLead;
			airport.skyOut[r] = addStation(runway + "_skyOut", outX, outY, kCruiseHeight, hx, hy, hx, hy);
			airport.skyIn[r] = addStation(runway + "_skyIn", inX, inY, kCruiseHeight, hx, hy, hx, hy);
			// 跑道中点：两端共线同向，位置/高度取平均，贝塞尔自然退化成直线；真正的停靠只发生在这里。
			airport.mid[r] = addStation(runway + "_mid", (in.x + out.x) * 0.5f, (in.y + out.y) * 0.5f, (in.z + out.z) * 0.5f,
				hx, hy, hx, hy, true, false);
			addLink(airport.InStop(r), airport.mid[r]);
			addLink(airport.mid[r], airport.OutStop(r));
			addLink(airport.OutStop(r), airport.skyOut[r]);
			addLink(airport.skyIn[r], airport.InStop(r));
		}
		if (runways < 2) continue;

		// 连外界：每个天空节点都另建一个extern(站名带"_ext")：天空节点在地图范围内就沿跑道方向水平延长到地图边界；
		// 已在图外就在天空节点原位(沿飞行方向错开kExternOffset，避免零长度边)。天空节点本身不等于extern，
		// 因为它可能只是机场互联线路的节点(互联不算出城)。飞机到out侧extern后直接直线飞到in侧extern，再回城市，extern处没有停靠站点。
		// leftOut/leftIn(跑道2出、跑道1进)一对，rightOut/rightIn(跑道1出、跑道2进)一对。
		// out=true：extern在飞出方向；out=false：在飞入方向(沿跑道反方向回溯)。
		auto makeExtern = [&](int r, bool out, const string& name) {
			const RouteStop& sky = out ? airport.skyOut[r] : airport.skyIn[r];
			const RouteStationInfo info = infoOf(sky);
			auto [hx, hy] = airport.Heading(r);
			float dirX = out ? hx : -hx, dirY = out ? hy : -hy;
			constexpr float kExternOffset = 0.01f; // 天空节点已在图外：extern在原位稍微沿飞行方向错开，避免零长度的link
			auto [ex, ey] = insideMap(info.x, info.y) ? RayBoxIntersection(info.x, info.y, dirX, dirY, sizeX, sizeY)
				: make_pair(info.x + dirX * kExternOffset, info.y + dirY * kExternOffset);
			return addStation(name, ex, ey, kCruiseHeight, hx, hy, hx, hy);
		};
		RouteStop leftExtOut = makeExtern(1, true, tag + "_ext_leftOut"), leftExtIn = makeExtern(0, false, tag + "_ext_leftIn");
		RouteStop rightExtOut = makeExtern(0, true, tag + "_ext_rightOut"), rightExtIn = makeExtern(1, false, tag + "_ext_rightIn");

		auto linkExtern = [&](const RouteStop& sky, const RouteStop& ext, bool out) {
			if (out) addLink(sky, ext); else addLink(ext, sky);
		};
		linkExtern(airport.skyOut[1], leftExtOut, true);
		addLink(leftExtOut, leftExtIn);
		linkExtern(airport.skyIn[0], leftExtIn, false);
		linkExtern(airport.skyOut[0], rightExtOut, true);
		addLink(rightExtOut, rightExtIn);
		linkExtern(airport.skyIn[1], rightExtIn, false);

		// 环线：leftIn跑道1 -> rightOut外界 -> rightIn跑道2 -> leftOut外界 -> 回leftIn。
		vector<RouteStop> loop;
		for (const RouteStop& stop : { airport.skyIn[0], airport.InStop(0), airport.mid[0], airport.OutStop(0), airport.skyOut[0],
			rightExtOut, rightExtIn, airport.skyIn[1], airport.InStop(1), airport.mid[1], airport.OutStop(1),
			airport.skyOut[1], leftExtOut, leftExtIn }) {
			loop.push_back(stop);
		}
		lines.push_back(std::move(loop));
	}

	// 机场两两互联(从天空节点开始连，是否超出地图边界不影响)：X用朝向最接近目标方向的跑道起飞，Y用朝向最接近来向的跑道降落，
	// skyOut(X)->skyIn(Y)一条贝塞尔(两端水平切线，同连外界)。一对机场一架互联飞机，不考虑调度。
	vector<pair<RouteStop, RouteStop>> taxiLinks; // 已加过的滑行link，避免重复
	auto addTaxi = [&](const Airport& airport, int fromRunway, int toRunway) {
		RouteStop from = airport.OutStop(fromRunway), to = airport.InStop(toRunway);
		for (const auto& done : taxiLinks) {
			if (done.first.station == from.station && done.first.interfaceIndex == from.interfaceIndex &&
				done.second.station == to.station && done.second.interfaceIndex == to.interfaceIndex) return;
		}
		taxiLinks.push_back({ from, to });
		addLink(from, to);
	};
	for (size_t a = 0; a < airports.size(); a++)
	for (size_t b = a + 1; b < airports.size(); b++) {
		const Airport& A = airports[a];
		const Airport& B = airports[b];
		if (A.RunwayCount() < 2 || B.RunwayCount() < 2) continue;
		float dx = B.x - A.x, dy = B.y - A.y;
		float length = sqrtf(dx * dx + dy * dy);
		if (length < 1e-3f) continue;
		dx /= length; dy /= length;

		int takeoffA = A.BestRunway(dx, dy), landingA = A.BestRunway(-dx, -dy);
		int takeoffB = B.BestRunway(-dx, -dy), landingB = B.BestRunway(dx, dy);
		addLink(A.skyOut[takeoffA], B.skyIn[landingB]);
		addLink(B.skyOut[takeoffB], A.skyIn[landingA]);

		// 一座机场的停靠段：降落天空节点->降落跑道(停)->(换跑道则滑行)->起飞跑道(停)->起飞天空节点。
		// 停靠点1->out1->in2->停靠点2这一段含同站转场腿，Core整段按sameStationSpeedScale降速。
		auto stay = [&](const Airport& X, int landing, int takeoff, vector<RouteStop>& line) {
			line.push_back(X.skyIn[landing]);
			line.push_back(X.InStop(landing));
			line.push_back(X.mid[landing]);
			line.push_back(X.OutStop(landing));
			if (landing != takeoff) {
				addTaxi(X, landing, takeoff);
				line.push_back(X.InStop(takeoff));
				line.push_back(X.mid[takeoff]);
				line.push_back(X.OutStop(takeoff));
			}
			line.push_back(X.skyOut[takeoff]);
		};
		vector<RouteStop> line;
		stay(A, landingA, takeoffA, line);
		stay(B, landingB, takeoffB, line);
		lines.push_back(std::move(line));
	}
}
