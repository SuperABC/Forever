#include "route_basic.h"

#include <algorithm>
#include <cmath>
#include <cfloat>

using namespace std;

namespace {
	// 射线从(x,y)沿(dirX,dirY)方向和[0,sizeX]x[0,sizeY]地图边界的交点——dirX/dirY不会同时为0
	// (接口朝向不可能是零向量)。
	pair<float, float> RayBoxIntersection(float x, float y, float dirX, float dirY, int sizeX, int sizeY) {
		float tBest = 1e9f;
		if (dirX > 0.f) tBest = min(tBest, (static_cast<float>(sizeX) - x) / dirX);
		else if (dirX < 0.f) tBest = min(tBest, (0.f - x) / dirX);
		if (dirY > 0.f) tBest = min(tBest, (static_cast<float>(sizeY) - y) / dirY);
		else if (dirY < 0.f) tBest = min(tBest, (0.f - y) / dirY);
		if (tBest < 0.f || tBest > 1e8f) tBest = 0.f; // 理论不会发生，防御性兜底(站点已经在图内)
		return { x + dirX * tBest, y + dirY * tBest };
	}

	// TrainRoute/AirRoute共用的"单站双跑道(轨道)、四接口"拓扑：
	// 左边界->leftIn->rightOut->右边界->rightIn->leftOut->左边界，闭合成一个环。interfaces
	// 传入顺序固定为[leftIn, rightOut, rightIn, leftOut](TrainStation/AirStation::Layout
	// 保证)——leftIn/rightOut这两个接口朝向相同(沿frontage轴同一方向)，中间那条直线就是
	// "跑道/轨道1"；rightIn/leftOut同理朝相反方向，是"跑道/轨道2"。
	//
	// 左边界的位置沿leftIn朝向的反方向(即leftIn进站之前、飞机/火车从图外飞来的方向)射线出图
	// 边界得到；右边界同理沿rightOut朝向(飞机/火车离站后继续飞向图外的方向)射线出图。两个
	// 边界节点的arrive/depart朝向都直接复用相邻接口自己的朝向(同向的一段直接复用，掉头的
	// 那一段取反)，左右边界各自固定对应着leftIn/rightOut这一组。
	// edgeHeight覆盖边缘站点的z(AirRoute用巡航高度，TrainRoute用接口原本的z)。
	void LayoutDualTrackStationLoop(const vector<RouteStationInfo>& interfaces, int sizeX, int sizeY,
		bool overrideEdgeZ, float edgeHeight,
		vector<RouteStationInfo>& outEdgeStations, vector<RouteLink>& outLinks,
		vector<vector<RouteStop>>& outLines) {
		if (interfaces.size() < 4) return; // 没有真实站点(或接口不全)，这条线路留空

		const RouteStationInfo& leftIn = interfaces[0];
		const RouteStationInfo& rightOut = interfaces[1];
		const RouteStationInfo& rightIn = interfaces[2];
		const RouteStationInfo& leftOut = interfaces[3];

		// 每个地图边界有两条线(出地图一条、进地图一条，分别在两条平行轨道的延长线上)，各自在边界上有一个
		// 端点(Out/In)，中间再插一个虚拟车站Mid：火车在Mid停靠但不触发进站/离站事件(RouteStop::silent)。
		// Out->Mid->In是贴着地图边界的一小段横向连线：Out到达朝图外、出发朝侧向(Out指向In)；Mid两头
		// 都朝侧向；In到达朝侧向、出发朝图内，所以三点之间都是直线，不会鼓出去。
		auto makeEdgeStations = [&](const RouteStationInfo& outSource, float outDirX, float outDirY,
			const RouteStationInfo& inSource, float inDirX, float inDirY,
			const char* outName, const char* midName, const char* inName,
			RouteStationInfo& outEdge, RouteStationInfo& midEdge, RouteStationInfo& inEdge) {
			auto [outX, outY] = RayBoxIntersection(outSource.x, outSource.y, outDirX, outDirY, sizeX, sizeY);
			auto [inX, inY] = RayBoxIntersection(inSource.x, inSource.y, inDirX, inDirY, sizeX, sizeY);
			float outZ = overrideEdgeZ ? edgeHeight : outSource.z;
			float inZ = overrideEdgeZ ? edgeHeight : inSource.z;

			float sideX = inX - outX, sideY = inY - outY;
			float sideLength = sqrtf(sideX * sideX + sideY * sideY);
			if (sideLength > 1e-6f) { sideX /= sideLength; sideY /= sideLength; }
			else { sideX = -outDirY; sideY = outDirX; } // 两点重合(理论不会)，任取一个垂直方向

			outEdge.name = outName;
			outEdge.x = outX; outEdge.y = outY; outEdge.z = outZ;
			outEdge.arriveDirX = outDirX; outEdge.arriveDirY = outDirY;
			outEdge.departDirX = sideX; outEdge.departDirY = sideY;
			outEdge.isEdge = true;

			midEdge.name = midName;
			midEdge.x = (outX + inX) * 0.5f; midEdge.y = (outY + inY) * 0.5f; midEdge.z = (outZ + inZ) * 0.5f;
			midEdge.arriveDirX = sideX; midEdge.arriveDirY = sideY;
			midEdge.departDirX = sideX; midEdge.departDirY = sideY;
			midEdge.isEdge = true;

			inEdge.name = inName;
			inEdge.x = inX; inEdge.y = inY; inEdge.z = inZ;
			inEdge.arriveDirX = sideX; inEdge.arriveDirY = sideY;
			inEdge.departDirX = -outDirX; inEdge.departDirY = -outDirY;
			inEdge.isEdge = true;
		};

		// 左边界：出地图的线是leftOut(沿它的出发方向出图)，进地图的线是leftIn(沿它出发方向的反向回溯到边界)。
		RouteStationInfo leftEdgeOut, leftEdgeMid, leftEdgeIn;
		makeEdgeStations(leftOut, leftOut.departDirX, leftOut.departDirY,
			leftIn, -leftIn.departDirX, -leftIn.departDirY,
			"edgeLeftOut", "edgeLeftMid", "edgeLeftIn", leftEdgeOut, leftEdgeMid, leftEdgeIn);
		// 右边界：出地图的线是rightOut，进地图的线是rightIn。
		RouteStationInfo rightEdgeOut, rightEdgeMid, rightEdgeIn;
		makeEdgeStations(rightOut, rightOut.departDirX, rightOut.departDirY,
			rightIn, -rightIn.departDirX, -rightIn.departDirY,
			"edgeRightOut", "edgeRightMid", "edgeRightIn", rightEdgeOut, rightEdgeMid, rightEdgeIn);

		// 真正的停靠点——跑道/轨道中点，飞机/火车只在这里停dwellSeconds。leftIn/rightOut/
		// rightIn/leftOut(入站/出站接口)和边缘的Out/In端点全部降级成纯过路点
		// (RouteStop::dwell=false，到达后不停留直接接着跑下一腿)——真实世界里乘客在跑道/站台
		// 中段上下客，不是在跑道两端分别停一次。stop1/stop2和边缘站点一样借用"没有
		// building的合成站点"机制(isEdge=true只是为了复用Route::Build()已有的创建+收集逻辑，
		// 不代表真的是地图边缘)。两点都落在各自跑道的直线上(同一朝向)，中点位置+朝向直接取
		// 两端平均即可，贝塞尔控制点会自然退化成直线，不会在中点产生拐弯。
		RouteStationInfo stop1; // 跑道/轨道1(leftIn<->rightOut)中点
		stop1.name = "stop1";
		stop1.x = (leftIn.x + rightOut.x) * 0.5f;
		stop1.y = (leftIn.y + rightOut.y) * 0.5f;
		stop1.z = (leftIn.z + rightOut.z) * 0.5f;
		stop1.arriveDirX = leftIn.departDirX; stop1.arriveDirY = leftIn.departDirY;
		stop1.departDirX = leftIn.departDirX; stop1.departDirY = leftIn.departDirY;
		stop1.isEdge = true;

		RouteStationInfo stop2; // 跑道/轨道2(rightIn<->leftOut)中点
		stop2.name = "stop2";
		stop2.x = (rightIn.x + leftOut.x) * 0.5f;
		stop2.y = (rightIn.y + leftOut.y) * 0.5f;
		stop2.z = (rightIn.z + leftOut.z) * 0.5f;
		stop2.arriveDirX = rightIn.departDirX; stop2.arriveDirY = rightIn.departDirY;
		stop2.departDirX = rightIn.departDirX; stop2.departDirY = rightIn.departDirY;
		stop2.isEdge = true;

		outEdgeStations = { leftEdgeOut, leftEdgeMid, leftEdgeIn, rightEdgeOut, rightEdgeMid, rightEdgeIn, stop1, stop2 };

		// edgeStations接在真实站点后面，这条线路假定全图只有这一个真实站点(下标就是realStation)，
		// 下标顺序和上面outEdgeStations列表里的顺序一一对应。
		int realStation = leftIn.stationIndex;
		auto edgeStop = [&](int k, bool dwell, bool silent) { return RouteStop{ realStation + 1 + k, 0, dwell, silent }; };

		RouteStop stopLeftEdgeOut = edgeStop(0, false, false);
		RouteStop stopLeftEdgeMid = edgeStop(1, true, true);  // 虚拟车站：停靠但不触发事件
		RouteStop stopLeftEdgeIn = edgeStop(2, false, false);
		RouteStop stopRightEdgeOut = edgeStop(3, false, false);
		RouteStop stopRightEdgeMid = edgeStop(4, true, true);
		RouteStop stopRightEdgeIn = edgeStop(5, false, false);
		RouteStop stopMid1 = edgeStop(6, true, false);
		RouteStop stopMid2 = edgeStop(7, true, false);
		RouteStop stopLeftIn{ realStation, leftIn.interfaceIndex, false };
		RouteStop stopRightOut{ realStation, rightOut.interfaceIndex, false };
		RouteStop stopRightIn{ realStation, rightIn.interfaceIndex, false };
		RouteStop stopLeftOut{ realStation, leftOut.interfaceIndex, false };

		outLines.push_back({ stopLeftEdgeIn, stopLeftIn, stopMid1, stopRightOut, stopRightEdgeOut, stopRightEdgeMid,
			stopRightEdgeIn, stopRightIn, stopMid2, stopLeftOut, stopLeftEdgeOut, stopLeftEdgeMid });

		outLinks.push_back({ stopLeftEdgeIn, stopLeftIn, false });
		outLinks.push_back({ stopLeftIn, stopMid1, false });       // 跑道/轨道1前半段
		outLinks.push_back({ stopMid1, stopRightOut, false });     // 跑道/轨道1后半段
		outLinks.push_back({ stopRightOut, stopRightEdgeOut, false });
		outLinks.push_back({ stopRightEdgeOut, stopRightEdgeMid, false });
		outLinks.push_back({ stopRightEdgeMid, stopRightEdgeIn, false });
		outLinks.push_back({ stopRightEdgeIn, stopRightIn, false });
		outLinks.push_back({ stopRightIn, stopMid2, false });       // 跑道/轨道2前半段
		outLinks.push_back({ stopMid2, stopLeftOut, false });       // 跑道/轨道2后半段
		outLinks.push_back({ stopLeftOut, stopLeftEdgeOut, false });
		outLinks.push_back({ stopLeftEdgeOut, stopLeftEdgeMid, false });
		outLinks.push_back({ stopLeftEdgeMid, stopLeftEdgeIn, false });
	}
}

int BusRoute::count = 0;

BusRoute::BusRoute() : id(count++) {
}

const char* BusRoute::GetName() {
	name = "BusRoute" + to_string(id);
	return name.data();
}

void BusRoute::SetProperty() {
	stationType = "bus";
	vehicleType = "vehicle_bus";
	useRoadnet = true;
	instantiate = false;

	// speed/dwellSeconds的单位是"地图单位/游戏内秒"、"游戏内秒"，不是真实秒——Traffic::Tick
	// 喂给Route::Update的elapsedSeconds来自Player::Tick()里的time->AddMilliseconds(delta*60*
	// 1000*timeFlowRatio)，默认timeFlowRatio=2.0，也就是1个真实秒=120个游戏内秒。按"车环一圈
	// (公交站环线实测约120~180地图单位)大约跑40~60真实秒、停站约3真实秒"校准：
	// speed=0.025(=3地图单位/真实秒)，dwellSeconds=360(=3真实秒)。如果以后time_flow_ratio
	// 被剧情脚本改了，这几个值的真实秒观感会跟着等比例变化，到时候要一起重新校准。
	speed = 0.025f;
	dwellSeconds = 360.f;

	// 公交每一站都停(dwellAtDestination恒为true)，"行驶区段"退化成单条leg，调大easeSeconds时
	// DriveVehicle()会自动把ta/td钳在"这条leg行驶时长的一半"，站间距短的leg会自然变成全程
	// 加速+全程减速、没有真正匀速的中段，这正是"尽量缓"这个要求在站间距有限时能做到的最大
	// 效果，不需要额外处理，直接给一个足够大的值即可(注意DriveVehicle()里这个字段真实秒->
	// 游戏内秒的换算系数要乘对，见route.md"DriveVehicle的缓动"一节)。
	easeSeconds = 20.f;
}

void BusRoute::LayoutRoute(const vector<RouteStationInfo>& interfaces, int sizeX, int sizeY) {
	// 不依赖任何"哪个方向是南/北/顺时针/逆时针"的假设，直接从每个接口自己真实的departDir
	// 反推它天生属于哪个绕行方向：一个点如果沿着"绕形心转圈"的切线方向走，它的朝向和"位置
	// 相对形心的半径向量"之间的2D叉积(radial.x*dir.y-radial.y*dir.x)符号是固定的(纯数学
	// 事实，和地图哪边是南北、Y轴朝哪无关)——同号的一组自然构成同一个绕行方向，叉积符号本身
	// 就决定了这组应该按角度升序还是降序首尾相接(推导见下buildLoop调用处)。这样无论Map层的
	// 左右手约定以后再怎么变，这里都会自动跟着真实车道方向重新分组，不需要手推。
	if (interfaces.size() < 4) return; // 凑不出两条至少2站的环线，留空

	float centerX = 0.f, centerY = 0.f;
	for (const RouteStationInfo& info : interfaces) { centerX += info.x; centerY += info.y; }
	centerX /= static_cast<float>(interfaces.size());
	centerY /= static_cast<float>(interfaces.size());

	auto angleAroundCenter = [&](const RouteStationInfo& info) {
		return atan2f(info.y - centerY, info.x - centerX);
	};
	// radial x departDir的2D叉积：>=0这一组的朝向和"角度增大方向"同向，必须按角度升序首尾
	// 相接才会贴着每个点自己的departDir走；<0这一组则相反，必须按角度降序——见下buildLoop。
	auto rotationSign = [&](const RouteStationInfo& info) {
		float rx = info.x - centerX, ry = info.y - centerY;
		return rx * info.departDirY - ry * info.departDirX;
	};

	vector<RouteStationInfo> groupAscending, groupDescending;
	for (const RouteStationInfo& info : interfaces) {
		(rotationSign(info) >= 0.f ? groupAscending : groupDescending).push_back(info);
	}
	sort(groupAscending.begin(), groupAscending.end(), [&](const RouteStationInfo& a, const RouteStationInfo& b) {
		return angleAroundCenter(a) < angleAroundCenter(b);
	});
	sort(groupDescending.begin(), groupDescending.end(), [&](const RouteStationInfo& a, const RouteStationInfo& b) {
		return angleAroundCenter(a) > angleAroundCenter(b);
	});

	auto buildLoop = [&](const vector<RouteStationInfo>& loopStations) {
		if (loopStations.size() < 2) return; // 单站凑不成环，丢弃

		vector<RouteStop> line;
		for (const RouteStationInfo& info : loopStations) {
			line.push_back({ info.stationIndex, info.interfaceIndex });
		}
		lines.push_back(line);

		for (size_t i = 0; i < loopStations.size(); i++) {
			const RouteStationInfo& from = loopStations[i];
			const RouteStationInfo& to = loopStations[(i + 1) % loopStations.size()];
			RouteLink link;
			link.from = { from.stationIndex, from.interfaceIndex };
			link.to = { to.stationIndex, to.interfaceIndex };
			link.bidirectional = false;
			links.push_back(link);
		}
	};
	buildLoop(groupAscending);
	buildLoop(groupDescending);
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
	// 轨道mesh沿X轴延伸、轴心在起点边(不像道路在中心)，长约250cm、宽约243cm，单位=0.25地图单位
	// (1地图单位=10m)。暂时没有隧道专用资产，隧道内复用同一个mesh。
	trackMesh = "/Game/3rdParty/SM_ufekaevdw_tier_3/StaticMeshes/SM_ufekaevdw_tier_3.SM_ufekaevdw_tier_3";
	trackTunnelMesh = "";
	trackUnit = 0.25f;
	trackWidth = 0.8f; // 比轨道本身宽，给列车和隧道口留余量

	// 单位换算见BusRoute::SetProperty()的注释——同样的timeFlowRatio=2.0默认值下1真实秒=
	// 120游戏内秒。火车的环线大部分长度是两段"车站到地图边缘"的长途
	// (LayoutDualTrackStationLoop，实测约2000地图单位)，按"跑一圈大约170真实秒、进站停约
	// 5真实秒"校准：speed=0.1(=12地图单位/真实秒)，dwellSeconds=600(=5真实秒)。
	speed = 0.1f;
	dwellSeconds = 600.f;

	// 火车体型大，启动/刹车要比普通载具更缓才符合实际手感(DriveVehicle()里这个字段真实
	// 秒->游戏内秒的换算系数要乘对，见route.md"DriveVehicle的缓动"一节)。
	easeSeconds = 40.f;
}

void TrainRoute::LayoutRoute(const vector<RouteStationInfo>& interfaces, int sizeX, int sizeY) {
	LayoutDualTrackStationLoop(interfaces, sizeX, sizeY, false, 0.f, edgeStations, links, lines);
}

// ---- TrainRoute::BuildTrack：过山判定和分段常量照抄roadnet_basic.cpp(JingRoadnet::buildArm)，
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

void TrainRoute::BuildTrack(const vector<RouteTrackLinkEnds>& linkEnds,
	const function<const string&(int, int)>& getTerrain,
	const function<pair<bool, float>(int, int)>& getWater,
	const function<float(float, float)>& getHeight) {
	trackEdges.assign(linkEnds.size(), TrackEdgeGeometry());

	for (size_t linkIndex = 0; linkIndex < linkEnds.size(); linkIndex++) {
		const RouteStationInfo& a = linkEnds[linkIndex].from;
		const RouteStationInfo& b = linkEnds[linkIndex].to;

		// ---- 底线：和Core单段贝塞尔(Route::BuildEdgeGeometry)同一条平面曲线，按弧长等距采样。 ----
		float c1x = a.x + a.departDirX * controlLength, c1y = a.y + a.departDirY * controlLength;
		float c2x = b.x - b.arriveDirX * controlLength, c2y = b.y - b.arriveDirY * controlLength;
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
		if (totalLength < 1e-3f) continue;

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

		TrackEdgeGeometry& geometry = trackEdges[linkIndex];
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
			seg.tunnel = segmentTag(i) == 3;
			geometry.segments.push_back(seg);

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
	}
}

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

	// 单位换算见BusRoute::SetProperty()的注释。飞机比火车快，环线也略长(实测约2050地图
	// 单位)，按"跑一圈大约100真实秒、停约6真实秒(登机/下客)"校准：speed=0.175(=21地图
	// 单位/真实秒)，dwellSeconds=720(=6真实秒)。
	speed = 0.175f;
	dwellSeconds = 720.f;

	// 飞机比火车更大更重，启动/刹车要比火车还缓一点(DriveVehicle()里这个字段真实秒->
	// 游戏内秒的换算系数要乘对，见route.md"DriveVehicle的缓动"一节；区段总时长大约是半圈
	// 减去停靠，DriveVehicle()会自动把ta/td钳在区段总时长一半以内，不会出现负的匀速段)。
	easeSeconds = 50.f;
}

void AirRoute::LayoutRoute(const vector<RouteStationInfo>& interfaces, int sizeX, int sizeY) {
	constexpr float kCruiseHeight = 100.f; // 飞机巡航高度(地图单位)
	LayoutDualTrackStationLoop(interfaces, sizeX, sizeY, true, kCruiseHeight, edgeStations, links, lines);
}
