#include "roadnet_checkboard.h"

#include "common/utility.h"

#include <cmath>
#include <algorithm>
#include <set>
#include <queue>
#include <sstream>
#include <cfloat>
#include <unordered_set>

// 过山的做法和井字路网完全一致(抬升/隧道，常量含义见roadnet_basic.cpp开头的注释)。
#define MOUNTAIN_MAX_SLOPE 0.5f
#define TERRAIN_FIT_FALLOFF 1.5f
#define TUNNEL_HEIGHT -1.f
#define TUNNEL_HATCH_LENGTH 8.f
#define TUNNEL_FLAT_APPROACH_LENGTH 1.5f
#define ARM_SAMPLE_STEP 1.f

// 密度分环：格子中心到地图中心的距离除以地图半对角线，<第一个阈值全高密度，其后依次高/中随机、
// 中/低随机，最外圈全低密度。
// 路网只从离地图四边各EDGE_MARGIN_RATIO(10%)以内的位置开始生成，不靠边。
#define EDGE_MARGIN_RATIO 0.1f

// 行道树/路灯(复制自roadnet_basic.cpp的同名常量)。
#define TREE_MESH_PATH "/Game/3rdParty/Mossy_Tree/StaticMeshes/mossy-tree.mossy-tree"
#define TREE_SCALE 0.6f
#define TREE_SPACING 1.f
#define TREE_SIDEWALK_RATIO 0.25f
#define LAMP_BLUEPRINT_PATH "/Game/3rdParty/Stylized_street_light/Actor/BP_StreetLight.BP_StreetLight_C"
#define LAMP_SPACING 1.5f
#define LAMP_SIDEWALK_RATIO 0.9f
#define LAMP_YAW_OFFSET 0.f

// 补长连接：同一条线上两个有效node之间至少隔LONG_GAP_MIN个无效node(其中有山里的)才可能补，补的概率。
#define LONG_GAP_MIN 4
#define LONG_EDGE_PROBABILITY 0.5f

// 主干道：隔一条选一条的横线/竖线上的实际连续道路里，所含格点(node)数最多的前1/4(向上取整)，每侧车行道数改成MAIN_ROAD_LANES(2=双向4车道)。
#define MAIN_ROAD_LANES 2
#define DENSITY_RING_HIGH 0.1f
#define DENSITY_RING_HIGH_MIDDLE 0.3f
#define DENSITY_RING_MIDDLE_LOW 0.5f


using namespace std;

namespace {
		// 路口缩进(写进Road::SetSetback)：必须在所有路的车道都配好之后调用，宽度是后改的。规则同roadnet_basic.cpp的同名函数
		// (复制而来，改一边要同步另一边)：两侧都是平角缩进0；两侧最小夹角是直角取构成直角的路(含自己)半宽最大值；有锐角取构成锐角的路半宽最大值的两倍。
		void AssignJunctionSetbacks(vector<Road>& roads, const vector<Intersection>& intersections) {
			const float PI = 3.14159265358979f;
			const float EPS = 0.05f;  // 约3度的角度容差
			struct Arm { Road* road; bool isStart; float angle; float halfWidth; };
			unordered_map<int, vector<Arm>> armsByNode;
			unordered_set<int> intersectionIds;
			for (const Intersection& intersection : intersections) intersectionIds.insert(intersection.GetId());
			for (Road& road : roads) {
				road.SetSetback(true, 0.f);
				road.SetSetback(false, 0.f);
				for (bool isStart : { true, false }) {
					int nodeId = isStart ? road.GetStart().GetId() : road.GetEnd().GetId();
					if (!intersectionIds.count(nodeId)) continue;
					float tx, ty, tz;
					road.GetTangent(isStart ? 0.f : 1.f, tx, ty, tz);
					float angle = isStart ? atan2f(ty, tx) : atan2f(-ty, -tx);
					armsByNode[nodeId].push_back({ &road, isStart, angle, road.GetTotalWidth() * 0.5f });
				}
			}
			for (auto& [nodeId, arms] : armsByNode) {
				sort(arms.begin(), arms.end(), [](const Arm& a, const Arm& b) { return a.angle < b.angle; });
				size_t count = arms.size();
				for (size_t k = 0; k < count; k++) {
					// 逆时针方向到下一条、顺时针方向到上一条的夹角，只有一条路时两侧都是整圈。
					auto gapTo = [&](size_t other, bool counterClockwise) {
						if (other == k) return 2.f * PI;
						float gap = counterClockwise ? arms[other].angle - arms[k].angle : arms[k].angle - arms[other].angle;
						while (gap <= 0.f) gap += 2.f * PI;
						return gap;
						};
					float nextGap = gapTo((k + 1) % count, true), prevGap = gapTo((k + count - 1) % count, false);
					float nextWidth = arms[(k + 1) % count].halfWidth, prevWidth = arms[(k + count - 1) % count].halfWidth;
					float minGap = min(nextGap, prevGap);
					float setback = 0.f;
					if (minGap >= PI - EPS) {
						setback = 0.f;
					}
					else if (minGap < PI * 0.5f - EPS) {
						setback = arms[k].halfWidth;
						if (nextGap < PI * 0.5f - EPS) setback = max(setback, nextWidth);
						if (prevGap < PI * 0.5f - EPS) setback = max(setback, prevWidth);
						setback *= 2.f;
					}
					else {
						// 直角/钝角：只看两侧夹角<180度的邻路半宽，不含自己。
						if (nextGap < PI - EPS) setback = max(setback, nextWidth);
						if (prevGap < PI - EPS) setback = max(setback, prevWidth);
					}
					float length = arms[k].road->CalcDistance();
					if (length > 1e-6f) setback = min(setback, length * 0.45f);
					arms[k].road->SetSetback(arms[k].isStart, setback);
				}
			}
		}

	// 住宅/商业/工业/办公四种类型均匀随机，密度由level给(0=高,1=中,2=低)。
	AREA_TYPE RandomArea(int level) {
		static constexpr AREA_TYPE kAreas[4][3] = {
			{ AREA_RESIDENTIAL_HIGH, AREA_RESIDENTIAL_MIDDLE, AREA_RESIDENTIAL_LOW },
			{ AREA_COMMERCIAL_HIGH, AREA_COMMERCIAL_MIDDLE, AREA_COMMERCIAL_LOW },
			{ AREA_GREEN, AREA_INDUSTRIAL_MIDDLE, AREA_INDUSTRIAL_LOW },
			{ AREA_OFFICIAL_HIGH, AREA_OFFICIAL_MIDDLE, AREA_OFFICIAL_LOW }
		};
		return kAreas[GetRandom(4)][level];
	}

	// d：到地图中心的归一化距离(0=中心)。
	int DensityLevel(float d) {
		if (d < DENSITY_RING_HIGH) return 0;
		if (d < DENSITY_RING_HIGH_MIDDLE) return GetRandom(2);
		if (d < DENSITY_RING_MIDDLE_LOW) return 1 + GetRandom(2);
		return 2;
	}
}

int CheckboardRoadnet::count = 0;

CheckboardRoadnet::CheckboardRoadnet(const string& args) : id(count++) {
	istringstream stream(args);
	string token;
	while (stream >> token) {
		if (token == "--merge") {
			float value;
			if (stream >> value) mergeProbability = max(0.f, min(1.f, value));
		}
		else if (token == "--bridge") {
			float value;
			if (stream >> value) bridgeProbability = max(0.f, min(1.f, value));
		}
		else if (token == "--min") {
			int value;
			if (stream >> value) minSpacing = value;
		}
		else if (token == "--max") {
			int value;
			if (stream >> value) maxSpacing = value;
		}
	}
	minSpacing = max(2, minSpacing);
	maxSpacing = max(minSpacing, maxSpacing);
}

CheckboardRoadnet::~CheckboardRoadnet() {

}

const char* CheckboardRoadnet::GetId() {
	return "roadnet_checkboard";
}

const char* CheckboardRoadnet::GetType() const {
	return "roadnet_checkboard";
}

const char* CheckboardRoadnet::GetName() {
	name = "棋盘格路网" + to_string(id);
	return name.data();
}

void CheckboardRoadnet::DistributeRoadnet(int width, int height,
	const function<const string&(int, int)>& getTerrain,
	const function<pair<bool, float>(int, int)>& getWater,
	const function<float(float, float)>& getHeight,
	int nodeStaticCount) {
	Node::SetCount(nodeStaticCount);

	const float meshUnit = 0.5f;
	const float LANE_WIDTH = 0.5f;

	// ---- 建路辅助：照抄JingRoadnet::DistributeRoadnet里同名lambda，车道配置固定为默认的对称(1,0,1,1,0,1) ----
	// 正在铺的这条路每侧的车行道数(1=普通路，MAIN_ROAD_LANES=主干道)，铺每条链之前设置，addRoad/layRoad读取。
	int currentLanes = 1;
	auto configureLanes = [&](Road& road) {
		for (int side = 0; side < 2; side++) {
			for (int k = 0; k < currentLanes; k++) road.AddVehicleLane(side, LANE_WIDTH);
			road.AddPedestrianLane(side, LANE_WIDTH);
		}
		};
	// 资产命名default_左车行_左停车_左人行_右车行_右停车_右人行(隧道内加_T)，见roadnet_basic.md。
	auto meshPathFor = [&](bool tunnel) -> string {
		string suffix = to_string(currentLanes) + "_0_1_" + to_string(currentLanes) + "_0_1";
		if (tunnel) suffix += "_T";
		return "/Game/Asset/Roadnet/Default/Mesh/default_" + suffix + ".default_" + suffix;
		};

	// 开口落在某一侧时，这侧换用"该侧不含人行道"的资产(另一侧不变)，见JingRoadnet的同名lambda。
	auto setSidewalkFree = [&](Road& road) {
		string lanes = to_string(currentLanes);
		string side0 = "default_" + lanes + "_0_0_" + lanes + "_0_1";
		string side1 = "default_" + lanes + "_0_1_" + lanes + "_0_0";
		// 资产名的左三位对应side0、右三位对应side1(同configureLanes)。
		road.SetSidewalkFreeMesh(0, "/Game/Asset/Roadnet/Default/Mesh/" + side0 + "." + side0);
		road.SetSidewalkFreeMesh(1, "/Game/Asset/Roadnet/Default/Mesh/" + side1 + "." + side1);
		};

	auto addControls = [](Road& road, const Node& n1, const Node& n2, float startSlope = 0.f, float endSlope = 0.f) {
		float dx = n2.GetX() - n1.GetX();
		float dy = n2.GetY() - n1.GetY();
		float length = sqrt(dx * dx + dy * dy);
		Node c1("roadnet", n1.GetX() + dx / 3.f, n1.GetY() + dy / 3.f, n1.GetZ() + startSlope * length / 3.f);
		Node c2("roadnet", n1.GetX() + dx * 2.f / 3.f, n1.GetY() + dy * 2.f / 3.f, n2.GetZ() - endSlope * length / 3.f);
		road.AddControls({ { c1, 1.f }, { c2, 1.f } });
		};

	// 一端隧道一端地面时拆成引道/下坡/平路三段(隧道口挖洞)，其余整段一条。见JingRoadnet的同名lambda。
	auto addRoad = [&](const string& name, const Node& n1, const Node& n2, float startSlope = 0.f, float endSlope = 0.f) {
		bool n1Tunnel = n1.GetZ() < 0.f;
		bool n2Tunnel = n2.GetZ() < 0.f;
		const string meshPath = meshPathFor(false);
		const string tunnelMeshPath = meshPathFor(true);

		if (n1Tunnel != n2Tunnel) {
			const Node& groundNode = n1Tunnel ? n2 : n1;
			const Node& tunnelNode = n1Tunnel ? n1 : n2;
			float dx = tunnelNode.GetX() - groundNode.GetX();
			float dy = tunnelNode.GetY() - groundNode.GetY();
			float segLen = sqrt(dx * dx + dy * dy);
			if (segLen < 1e-6f) segLen = 1.f;
			float ux = dx / segLen, uy = dy / segLen;

			Node flatNode("roadnet", groundNode.GetX() + ux * TUNNEL_FLAT_APPROACH_LENGTH, groundNode.GetY() + uy * TUNNEL_FLAT_APPROACH_LENGTH, groundNode.GetZ());
			bool noLevelPart = segLen <= TUNNEL_HATCH_LENGTH + 1e-3f;
			Node splitNode = noLevelPart ? tunnelNode
				: Node("roadnet", groundNode.GetX() + ux * TUNNEL_HATCH_LENGTH, groundNode.GetY() + uy * TUNNEL_HATCH_LENGTH, tunnelNode.GetZ());

			roads.emplace_back(name, groundNode, flatNode, meshPath, meshUnit);
			addControls(roads.back(), groundNode, flatNode);
			configureLanes(roads.back());
			setSidewalkFree(roads.back());
			AddTunnelHatch(&roads.back(), 0.f, 1.f, roads.back().GetTotalWidth());

			roads.emplace_back(name, flatNode, splitNode, meshPath, meshUnit);
			addControls(roads.back(), flatNode, splitNode);
			configureLanes(roads.back());
			setSidewalkFree(roads.back());
			AddTunnelHatch(&roads.back(), 0.f, 1.f, roads.back().GetTotalWidth());

			if (!noLevelPart) {
				roads.emplace_back(name, splitNode, tunnelNode, tunnelMeshPath, meshUnit);
				configureLanes(roads.back());
			}
			return;
		}

		roads.emplace_back(name, n1, n2, (n1Tunnel && n2Tunnel) ? tunnelMeshPath : meshPath, meshUnit);
		bool level = fabsf(n1.GetZ() - n2.GetZ()) < 1e-4f && fabsf(startSlope) < 1e-6f && fabsf(endSlope) < 1e-6f;
		if (!level) addControls(roads.back(), n1, n2, startSlope, endSlope);
		configureLanes(roads.back());
		if (!(n1Tunnel && n2Tunnel)) setSidewalkFree(roads.back());
		if (n1.GetZ() > 1e-4f || n2.GetZ() > 1e-4f) {
			AddTerrainFit(&roads.back(), 0.f, 1.f, roads.back().GetTotalWidth(), TERRAIN_FIT_FALLOFF);
		}
		};

	// 铺一条从a到b的直线路(a/b是已有的Intersection/Node，id保持)。沿线每ARM_SAMPLE_STEP采样地形，
	// 把连续的山切成一座座单独决定抬升/隧道——逻辑照抄JingRoadnet::buildArm的第一遍/第二遍，
	// 区别只是终点是固定的b(不是地图边界的extern)、没有格点。平地(没有山)时就是a->b一条Road。
	auto layRoad = [&](const string& name, const Node& a, const Node& b) {
		float cx = a.GetX(), cy = a.GetY();
		float ex = b.GetX() - cx, ey = b.GetY() - cy;
		float length = sqrt(ex * ex + ey * ey);
		if (length < 1e-4f) return;
		float dx = ex / length * ARM_SAMPLE_STEP, dy = ey / length * ARM_SAMPLE_STEP;
		float kExt = length / ARM_SAMPLE_STEP;

		vector<string> terr;
		vector<float> heights;
		terr.push_back(getTerrain(static_cast<int>(floorf(cx)), static_cast<int>(floorf(cy))));
		heights.push_back(getHeight(cx, cy));
		for (int k = 1; static_cast<float>(k) <= kExt - 1.f; k++) {
			string t = getTerrain(static_cast<int>(floorf(cx + dx * k)), static_cast<int>(floorf(cy + dy * k)));
			if (t == "") break;
			terr.push_back(t);
			heights.push_back(getHeight(cx + dx * k, cy + dy * k));
		}
		int sampleCount = static_cast<int>(terr.size()) - 1;

		auto isGood = [&](int k) { return terr[k] == "plain" || terr[k] == "construction"; };
		auto allGood = [&](int from, int to) {
			for (int i = from; i <= to; i++) if (!isGood(i)) return false;
			return true;
			};

		struct TunnelSpan { int p; int tIn; int tOut; int q; };
		vector<TunnelSpan> tunnels;
		vector<bool> raised(static_cast<size_t>(sampleCount) + 1, false);
		const int rampSamples = static_cast<int>(ceilf(TUNNEL_HATCH_LENGTH / ARM_SAMPLE_STEP));
		int lastQ = 0;
		for (int k = 1; k <= sampleCount; ) {
			if (terr[k] != "terrain_mountain") { k++; continue; }
			int first = k;
			while (k + 1 <= sampleCount && terr[k + 1] == "terrain_mountain") k++;
			int last = k;
			k++;

			float maxSlope = 0.f;
			int from = max(0, first - 1), to = min(sampleCount, last + 1);
			for (int i = from; i < to; i++) {
				maxSlope = max(maxSlope, fabsf(heights[i + 1] - heights[i]) / ARM_SAMPLE_STEP);
			}
			bool seaBehind = last + 1 <= sampleCount && terr[last + 1] == "terrain_ocean";
			bool raise = seaBehind || maxSlope <= MOUNTAIN_MAX_SLOPE;
			const char* why = raise ? "gentle or sea behind" : "steep";
			if (!raise) {
				TunnelSpan span{ first - 1 - rampSamples, first - 1, last + 1, last + 1 + rampSamples };
				bool roomBefore = span.p >= lastQ && allGood(span.p, span.tIn);
				bool roomAfter = span.q <= sampleCount && allGood(span.tOut, span.q);
				if (roomBefore && roomAfter) {
					tunnels.push_back(span);
					lastQ = span.q;
				}
				else {
					raise = true;
					why = roomBefore ? "steep but not enough plain behind" : "steep but not enough plain before";
				}
			}
			if (raise) {
				for (int i = first; i <= last; i++) raised[i] = true;
			}
			debugf("Log: Roadnet %s mountain samples [%d,%d] max slope %.3f sea behind %d -> %s (%s).\n",
				name.c_str(), first, last, maxSlope, seaBehind ? 1 : 0, raise ? "raise" : "tunnel", why);
		}

		auto insideTunnel = [&](float k) {
			for (const TunnelSpan& t : tunnels) {
				if (k >= static_cast<float>(t.tIn) && k <= static_cast<float>(t.tOut)) return true;
			}
			return false;
			};
		auto zAt = [&](float k) {
			if (insideTunnel(k)) return static_cast<float>(TUNNEL_HEIGHT);
			int index = static_cast<int>(roundf(k));
			return (index >= 0 && index <= sampleCount && raised[index]) ? heights[index] : 0.f;
			};

		// 关键点：每个隧道的p/tIn/tOut/q，抬升山的每个采样点及其前后各一个采样点(平地过渡)。
		set<float> keySet;
		for (int k = 1; k <= sampleCount; k++) {
			if (!raised[k]) continue;
			for (int m = max(1, k - 1); m <= min(sampleCount, k + 1); m++) keySet.insert(static_cast<float>(m));
		}
		for (const TunnelSpan& t : tunnels) {
			for (int k = t.p + 1; k < t.tIn; k++) keySet.erase(static_cast<float>(k));
			for (int k = t.tOut + 1; k < t.q; k++) keySet.erase(static_cast<float>(k));
			keySet.insert(static_cast<float>(t.p));
			keySet.insert(static_cast<float>(t.tIn));
			keySet.insert(static_cast<float>(t.tOut));
			keySet.insert(static_cast<float>(t.q));
		}

		float endZ = b.GetZ();
		struct ArmPoint { float k; float z; };
		vector<ArmPoint> points;
		points.push_back({ 0.f, a.GetZ() });
		for (float k : keySet) points.push_back({ k, zAt(k) });
		points.push_back({ kExt, endZ });
		float stepLength = sqrt(dx * dx + dy * dy);
		auto slopeAt = [&](size_t i) {
			if (i == 0 || i + 1 >= points.size() || points[i].z < 1e-4f) return 0.f;
			float run = (points[i + 1].k - points[i - 1].k) * stepLength;
			return run > 1e-6f ? (points[i + 1].z - points[i - 1].z) / run : 0.f;
			};

		Node previous(a);
		for (size_t i = 1; i < points.size(); i++) {
			bool isEnd = (i + 1 == points.size());
			Node node = isEnd ? Node(b) : Node("roadnet", cx + dx * points[i].k, cy + dy * points[i].k, points[i].z);
			float segmentRun = sqrt((node.GetX() - previous.GetX()) * (node.GetX() - previous.GetX())
				+ (node.GetY() - previous.GetY()) * (node.GetY() - previous.GetY()));
			float delta = segmentRun > 1e-6f ? (node.GetZ() - previous.GetZ()) / segmentRun : 0.f;
			auto monotone = [delta](float slope) {
				if (delta == 0.f) return 0.f;
				float aligned = slope * (delta > 0.f ? 1.f : -1.f);
				aligned = max(0.f, min(aligned, 3.f * fabsf(delta)));
				return aligned * (delta > 0.f ? 1.f : -1.f);
				};
			addRoad(name, previous, node, monotone(slopeAt(i - 1)), monotone(slopeAt(i)));
			previous = node;
		}
		};

	// 判断一个点是不是落在凸四边形内部(叉积同号)，照抄JingRoadnet。
	auto isPointInQuad = [](float px, float py, const float qx[4], const float qy[4]) -> bool {
		bool hasNeg = false, hasPos = false;
		for (int i = 0; i < 4; i++) {
			int j = (i + 1) % 4;
			float c = (qx[j] - qx[i]) * (py - qy[i]) - (qy[j] - qy[i]) * (px - qx[i]);
			if (c < 0.f) hasNeg = true;
			if (c > 0.f) hasPos = true;
		}
		return !(hasNeg && hasPos);
		};
	// 整块lot覆盖的每个element都必须是construction，见JingRoadnet::isLotFullyConstruction。
	auto isLotFullyConstruction = [&](const float qx[4], const float qy[4]) -> bool {
		float minX = min({ qx[0], qx[1], qx[2], qx[3] }), maxX = max({ qx[0], qx[1], qx[2], qx[3] });
		float minY = min({ qy[0], qy[1], qy[2], qy[3] }), maxY = max({ qy[0], qy[1], qy[2], qy[3] });
		int x0 = static_cast<int>(floorf(minX)), x1 = static_cast<int>(ceilf(maxX));
		int y0 = static_cast<int>(floorf(minY)), y1 = static_cast<int>(ceilf(maxY));
		for (int y = y0; y <= y1; y++) {
			for (int x = x0; x <= x1; x++) {
				if (!isPointInQuad(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f, qx, qy)) continue;
				if (x < 0 || y < 0 || x >= width || y >= height) return false;
				if (getTerrain(x, y) != "construction") return false;
			}
		}
		return true;
		};

	// =========================================================================================
	// 1. 旋转坐标系(u向东，v向南)里先铺全部横/竖路的位置，再变换到世界坐标——node天然就是旋转后的位置。
	// =========================================================================================
	const float centerX = width / 2.f, centerY = height / 2.f;
	const float theta = GetRandom(1000) / 1000.f * 0.4f - 0.2f;
	const float cosT = cos(theta), sinT = sin(theta);
	const float radius = ceilf(0.5f * sqrt(static_cast<float>(width) * width + static_cast<float>(height) * height)) + 1.f;

	auto layLines = [&]() {
		vector<float> lines;
		float p = -radius;
		while (p <= radius) {
			lines.push_back(p);
			p += static_cast<float>(minSpacing + GetRandom(maxSpacing - minSpacing + 1));
		}
		return lines;
		};
	// 先横(东西向)后竖(南北向)：横路从北到南按v排，竖路从西到东按u排。
	const vector<float> lineV = layLines();
	const vector<float> lineU = layLines();
	const int nu = static_cast<int>(lineU.size()), nv = static_cast<int>(lineV.size());

	auto nodeIndex = [&](int i, int j) { return j * nu + i; };
	vector<float> posX(static_cast<size_t>(nu) * nv), posY(static_cast<size_t>(nu) * nv);
	for (int j = 0; j < nv; j++) {
		for (int i = 0; i < nu; i++) {
			posX[nodeIndex(i, j)] = centerX + lineU[i] * cosT - lineV[j] * sinT;
			posY[nodeIndex(i, j)] = centerY + lineU[i] * sinT + lineV[j] * cosT;
		}
	}

	// =========================================================================================
	// 2. 去掉所有不在construction元素上的node。
	// =========================================================================================
	vector<char> alive(static_cast<size_t>(nu) * nv, 0);
	for (int idx = 0; idx < nu * nv; idx++) {
		int ix = static_cast<int>(floorf(posX[idx])), iy = static_cast<int>(floorf(posY[idx]));
		if (posX[idx] < width * EDGE_MARGIN_RATIO || posX[idx] > width * (1.f - EDGE_MARGIN_RATIO)
			|| posY[idx] < height * EDGE_MARGIN_RATIO || posY[idx] > height * (1.f - EDGE_MARGIN_RATIO)) continue;
		if (ix < 0 || iy < 0 || ix >= width || iy >= height) continue;
		alive[idx] = getTerrain(ix, iy) == "construction";
	}

	// =========================================================================================
	// 3. 相邻存活node之间连路(先记成抽象边，真正的Road在最后才铺)。横边(i,j)-(i+1,j)，竖边(i,j)-(i,j+1)。
	//    special：这条边跨山(沿线有mountain采样点)，铺的时候会走抬升/隧道，一条边会拆成多段Road。
	//    河/海不特殊处理，正常Z=0铺过去。
	// =========================================================================================
	vector<char> hEdge(static_cast<size_t>(nu) * nv, 0), vEdge(static_cast<size_t>(nu) * nv, 0);
	vector<char> hSpecial(static_cast<size_t>(nu) * nv, 0), vSpecial(static_cast<size_t>(nu) * nv, 0);
	// 跨山或跨河的边：没有lot相邻也不删(河上的路是连接两岸的，山上的路是隧道/山路)。
	vector<char> hKeep(static_cast<size_t>(nu) * nv, 0), vKeep(static_cast<size_t>(nu) * nv, 0);
	auto crosses = [&](int a, int b, const char* kind) {
		float ex = posX[b] - posX[a], ey = posY[b] - posY[a];
		float length = sqrt(ex * ex + ey * ey);
		for (float k = 1.f; k <= length - 1.f; k += 1.f) {
			int x = static_cast<int>(floorf(posX[a] + ex / length * k)), y = static_cast<int>(floorf(posY[a] + ey / length * k));
			if (getTerrain(x, y) == kind) return true;
		}
		return false;
		};
	for (int j = 0; j < nv; j++) {
		for (int i = 0; i < nu; i++) {
			int a = nodeIndex(i, j);
			if (!alive[a]) continue;
			if (i + 1 < nu && alive[nodeIndex(i + 1, j)]) {
				hEdge[a] = 1;
				hSpecial[a] = crosses(a, nodeIndex(i + 1, j), "terrain_mountain");
				hKeep[a] = hSpecial[a] || crosses(a, nodeIndex(i + 1, j), "terrain_river");
			}
			if (j + 1 < nv && alive[nodeIndex(i, j + 1)]) {
				vEdge[a] = 1;
				vSpecial[a] = crosses(a, nodeIndex(i, j + 1), "terrain_mountain");
				vKeep[a] = vSpecial[a] || crosses(a, nodeIndex(i, j + 1), "terrain_river");
			}
		}
	}

	// 有效node必须至少连着两条边：只连一条(断头)或一条都没有的node不算有效，删掉(连带它那条边)，
	// 迭代到稳定。后面补长连接按"有效node"判断，所以先清一遍。
	for (bool changed = true; changed; ) {
		changed = false;
		for (int j = 0; j < nv; j++) {
			for (int i = 0; i < nu; i++) {
				int a = nodeIndex(i, j);
				if (!alive[a]) continue;
				bool west = i > 0 && hEdge[nodeIndex(i - 1, j)], east = i + 1 < nu && hEdge[a];
				bool north = j > 0 && vEdge[nodeIndex(i, j - 1)], south = j + 1 < nv && vEdge[a];
				if (static_cast<int>(west) + east + north + south >= 2) continue;
				if (west) hEdge[nodeIndex(i - 1, j)] = 0;
				if (east) hEdge[a] = 0;
				if (north) vEdge[nodeIndex(i, j - 1)] = 0;
				if (south) vEdge[a] = 0;
				alive[a] = 0;
				changed = true;
			}
		}
	}

	// =========================================================================================
	// 3b. 补长连接：遍历每条初始横线/竖线，同一条线上相邻的两个有效(存活)node之间如果隔了不少于
	//     LONG_GAP_MIN个无效node、且这些无效node里有落在山里的，就按LONG_EDGE_PROBABILITY的概率用一条
	//     长边把它们连上(铺路时走和普通边一样的抬升/隧道逻辑)。新补的长边之间不能交叉：横线长边和竖线
	//     长边只会在两者都"跨过"的网格点上相交，用覆盖标记判断，冲突的丢弃。
	//     hLongTo[a]=横向长边从a指向的右端node下标(-1无)，hLongFrom[b]是反向记录；竖向同理。
	// =========================================================================================
	// 横线、竖线各自随机的起始奇偶：补长连接和主干道都只在这些隔一条选一条的线上做。
	const int hParity = GetRandom(2), vParity = GetRandom(2);
	vector<int> hLongTo(static_cast<size_t>(nu) * nv, -1), hLongFrom(static_cast<size_t>(nu) * nv, -1);
	vector<int> vLongTo(static_cast<size_t>(nu) * nv, -1), vLongFrom(static_cast<size_t>(nu) * nv, -1);
	{
		struct LongCandidate { int a; int b; bool horizontal; };
		vector<LongCandidate> candidates;
		auto inMountain = [&](int n) {
			int x = static_cast<int>(floorf(posX[n])), y = static_cast<int>(floorf(posY[n]));
			if (posX[n] < 0.f || posY[n] < 0.f || x >= width || y >= height) return false;
			return getTerrain(x, y) == "terrain_mountain";
			};
		// 横线、竖线都隔一条选一条(奇偶在上面随机)，避免两条相邻的山路挨得太近。
		for (int j = 0; j < nv; j++) {
			if (j % 2 != hParity) continue;
			int previous = -1;
			for (int i = 0; i < nu; i++) {
				if (!alive[nodeIndex(i, j)]) continue;
				if (previous >= 0 && i - previous - 1 >= LONG_GAP_MIN) {
					bool mountain = false;
					for (int k = previous + 1; k < i && !mountain; k++) mountain = inMountain(nodeIndex(k, j));
					if (mountain) candidates.push_back({ nodeIndex(previous, j), nodeIndex(i, j), true });
				}
				previous = i;
			}
		}
		for (int i = 0; i < nu; i++) {
			if (i % 2 != vParity) continue;
			int previous = -1;
			for (int j = 0; j < nv; j++) {
				if (!alive[nodeIndex(i, j)]) continue;
				if (previous >= 0 && j - previous - 1 >= LONG_GAP_MIN) {
					bool mountain = false;
					for (int k = previous + 1; k < j && !mountain; k++) mountain = inMountain(nodeIndex(i, k));
					if (mountain) candidates.push_back({ nodeIndex(i, previous), nodeIndex(i, j), false });
				}
				previous = j;
			}
		}
		for (int k = static_cast<int>(candidates.size()) - 1; k > 0; k--) swap(candidates[k], candidates[GetRandom(k + 1)]);

		vector<char> hCover(static_cast<size_t>(nu) * nv, 0), vCover(static_cast<size_t>(nu) * nv, 0);
		for (const LongCandidate& c : candidates) {
			if (GetRandom(10000) / 10000.f >= LONG_EDGE_PROBABILITY) continue;
			int ai = c.a % nu, aj = c.a / nu, bi = c.b % nu, bj = c.b / nu;
			bool conflict = false;
			if (c.horizontal) {
				for (int i = ai + 1; i < bi; i++) conflict = conflict || vCover[nodeIndex(i, aj)];
				if (conflict) continue;
				for (int i = ai + 1; i < bi; i++) hCover[nodeIndex(i, aj)] = 1;
				hLongTo[c.a] = c.b;
				hLongFrom[c.b] = c.a;
			}
			else {
				for (int j = aj + 1; j < bj; j++) conflict = conflict || hCover[nodeIndex(ai, j)];
				if (conflict) continue;
				for (int j = aj + 1; j < bj; j++) vCover[nodeIndex(ai, j)] = 1;
				vLongTo[c.a] = c.b;
				vLongFrom[c.b] = c.a;
			}
		}
		int accepted = 0;
		for (int a = 0; a < nu * nv; a++) accepted += (hLongTo[a] >= 0) + (vLongTo[a] >= 0);
		debugf("Log: Roadnet checkboard long edges: %d candidates, %d accepted.\n", static_cast<int>(candidates.size()), accepted);
	}

	// 度/邻接辅助。
	auto degree = [&](int i, int j) {
		int d = 0;
		int self = nodeIndex(i, j);
		if (hLongTo[self] >= 0) d++;
		if (hLongFrom[self] >= 0) d++;
		if (vLongTo[self] >= 0) d++;
		if (vLongFrom[self] >= 0) d++;
		if (i > 0 && hEdge[nodeIndex(i - 1, j)]) d++;
		if (i + 1 < nu && hEdge[nodeIndex(i, j)]) d++;
		if (j > 0 && vEdge[nodeIndex(i, j - 1)]) d++;
		if (j + 1 < nv && vEdge[nodeIndex(i, j)]) d++;
		return d;
		};

	// =========================================================================================
	// 4. 联通性：只保留最大联通分量，其余孤岛的node和边全部删除。
	// =========================================================================================
	auto keepLargestComponent = [&]() {
		vector<int> component(static_cast<size_t>(nu) * nv, -1);
		vector<int> componentSize;
		for (int start = 0; start < nu * nv; start++) {
			if (!alive[start] || component[start] >= 0) continue;
			int label = static_cast<int>(componentSize.size());
			componentSize.push_back(0);
			queue<int> pending;
			pending.push(start);
			component[start] = label;
			while (!pending.empty()) {
				int cur = pending.front();
				pending.pop();
				componentSize[label]++;
				int i = cur % nu, j = cur / nu;
				auto visit = [&](bool edge, int ni, int nj) {
					int n = nodeIndex(ni, nj);
					if (edge && component[n] < 0) {
						component[n] = label;
						pending.push(n);
					}
					};
				if (i > 0) visit(hEdge[nodeIndex(i - 1, j)], i - 1, j);
				if (i + 1 < nu) visit(hEdge[cur], i + 1, j);
				if (j > 0) visit(vEdge[nodeIndex(i, j - 1)], i, j - 1);
				if (j + 1 < nv) visit(vEdge[cur], i, j + 1);
				for (int far : { hLongTo[cur], hLongFrom[cur], vLongTo[cur], vLongFrom[cur] }) {
					if (far >= 0) visit(true, far % nu, far / nu);
				}
			}
		}
		if (componentSize.empty()) return;
		int best = static_cast<int>(max_element(componentSize.begin(), componentSize.end()) - componentSize.begin());
		for (int idx = 0; idx < nu * nv; idx++) {
			if (alive[idx] && component[idx] != best) {
				alive[idx] = 0;
				hEdge[idx] = vEdge[idx] = 0;
			}
		}
		// 长边：任一端死了就整条删掉。
		for (int a = 0; a < nu * nv; a++) {
			if (hLongTo[a] >= 0 && (!alive[a] || !alive[hLongTo[a]])) { hLongFrom[hLongTo[a]] = -1; hLongTo[a] = -1; }
			if (vLongTo[a] >= 0 && (!alive[a] || !alive[vLongTo[a]])) { vLongFrom[vLongTo[a]] = -1; vLongTo[a] = -1; }
		}
		// 边的另一端可能在孤岛里(边只存在于同一分量内，所以不会出现)，保险起见再按端点清一遍。
		for (int j = 0; j < nv; j++) {
			for (int i = 0; i < nu; i++) {
				int a = nodeIndex(i, j);
				if (hEdge[a] && (!alive[a] || !alive[nodeIndex(i + 1, j)])) hEdge[a] = 0;
				if (vEdge[a] && (!alive[a] || !alive[nodeIndex(i, j + 1)])) vEdge[a] = 0;
			}
		}
	};
	keepLargestComponent();

	// =========================================================================================
	// 5. 四个node都在、四条边都在、整块都落在construction里的格子生成一个lot。lotCell[(i,j)]对应
	//    (i,j)(i+1,j)(i,j+1)(i+1,j+1)围出的格子。边界上有跨山边的格子不生成lot(那条边会被拆成好几段Road，
	//    没有单独一条Road可以挂成lot边界，和井字一样)。
	// =========================================================================================
	vector<char> lotCell(static_cast<size_t>(nu) * nv, 0);
	for (int j = 0; j + 1 < nv; j++) {
		for (int i = 0; i + 1 < nu; i++) {
			int a = nodeIndex(i, j), b = nodeIndex(i + 1, j), c = nodeIndex(i, j + 1), d = nodeIndex(i + 1, j + 1);
			if (!(alive[a] && alive[b] && alive[c] && alive[d])) continue;
			if (!(hEdge[a] && hEdge[c] && vEdge[a] && vEdge[b])) continue;
			if (hSpecial[a] || hSpecial[c] || vSpecial[a] || vSpecial[b]) continue;
			const float qx[4] = { posX[a], posX[b], posX[d], posX[c] };
			const float qy[4] = { posY[a], posY[b], posY[d], posY[c] };
			if (!isLotFullyConstruction(qx, qy)) continue;
			lotCell[a] = 1;
		}
	}

	// 把没有相邻lot的边删掉——跨山的边例外(通常挨着山的格子建不了lot，删了就再没有隧道/山路了)，
	// 但跨山边的端点如果没有别的路可连(度为1)就是断头，迭代剪掉。
	auto cellAt = [&](int i, int j) -> bool {
		return i >= 0 && j >= 0 && i + 1 < nu && j + 1 < nv && lotCell[nodeIndex(i, j)];
		};
	// 只跨河(不跨山)又没有相邻lot的边是"候选桥"：先只保留其余的边，再按并查集逐条检查候选，
	// 只有把两块原本不连通的区域连起来的才保留(必需的桥)，其余删掉。
	vector<int> candidates;  // 编码：node下标*2+(0横/1竖)
	for (int j = 0; j < nv; j++) {
		for (int i = 0; i < nu; i++) {
			int a = nodeIndex(i, j);
			if (hEdge[a] && !cellAt(i, j - 1) && !cellAt(i, j)) {
				if (!hKeep[a]) hEdge[a] = 0;
				else if (!hSpecial[a]) candidates.push_back(a * 2);
			}
			if (vEdge[a] && !cellAt(i - 1, j) && !cellAt(i, j)) {
				if (!vKeep[a]) vEdge[a] = 0;
				else if (!vSpecial[a]) candidates.push_back(a * 2 + 1);
			}
		}
	}
	{
		vector<int> parent(static_cast<size_t>(nu) * nv);
		for (size_t k = 0; k < parent.size(); k++) parent[k] = static_cast<int>(k);
		auto find = [&](int x) {
			while (parent[x] != x) x = parent[x] = parent[parent[x]];
			return x;
			};
		// 每条候选按bridgeProbability随机保留，其余的再按连通必需性处理。
		set<int> candidateSet;
		for (int code : candidates) {
			if (GetRandom(10000) / 10000.f >= bridgeProbability) candidateSet.insert(code);
		}
		for (int j = 0; j < nv; j++) {
			for (int i = 0; i < nu; i++) {
				int a = nodeIndex(i, j);
				if (hEdge[a] && !candidateSet.count(a * 2)) parent[find(a)] = find(nodeIndex(i + 1, j));
				if (vEdge[a] && !candidateSet.count(a * 2 + 1)) parent[find(a)] = find(nodeIndex(i, j + 1));
			}
		}
		for (int code : candidateSet) {
			int a = code / 2;
			int b = (code % 2 == 0) ? a + 1 : a + nu;
			if (find(a) != find(b)) parent[find(a)] = find(b);
			else (code % 2 == 0 ? hEdge : vEdge)[a] = 0;
		}
	}
	for (bool changed = true; changed; ) {
		changed = false;
		for (int j = 0; j < nv; j++) {
			for (int i = 0; i < nu; i++) {
				int a = nodeIndex(i, j);
				if (!alive[a]) continue;
				int d = degree(i, j);
				if (d == 0) { alive[a] = 0; continue; }
				if (d == 1) {
					// 度为1的node唯一的边只可能是跨山边(其余边都有lot)。
					if (i > 0 && hEdge[nodeIndex(i - 1, j)]) hEdge[nodeIndex(i - 1, j)] = 0;
					else if (i + 1 < nu && hEdge[a]) hEdge[a] = 0;
					else if (j > 0 && vEdge[nodeIndex(i, j - 1)]) vEdge[nodeIndex(i, j - 1)] = 0;
					else if (j + 1 < nv && vEdge[a]) vEdge[a] = 0;
					else if (hLongFrom[a] >= 0) { hLongTo[hLongFrom[a]] = -1; hLongFrom[a] = -1; }
					else if (hLongTo[a] >= 0) { hLongFrom[hLongTo[a]] = -1; hLongTo[a] = -1; }
					else if (vLongFrom[a] >= 0) { vLongTo[vLongFrom[a]] = -1; vLongFrom[a] = -1; }
					else if (vLongTo[a] >= 0) { vLongFrom[vLongTo[a]] = -1; vLongTo[a] = -1; }
					alive[a] = 0;
					changed = true;
				}
			}
		}
	}

	// 删边之后可能又出现孤岛(原来靠被删的边连着)：再保留一次最大联通分量，四角node有死的格子不再是lot。
	keepLargestComponent();
	for (int j = 0; j + 1 < nv; j++) {
		for (int i = 0; i + 1 < nu; i++) {
			int a = nodeIndex(i, j);
			if (lotCell[a] && !(alive[a] && alive[nodeIndex(i + 1, j)] && alive[nodeIndex(i, j + 1)] && alive[nodeIndex(i + 1, j + 1)]
				&& hEdge[a] && hEdge[nodeIndex(i, j + 1)] && vEdge[a] && vEdge[nodeIndex(i + 1, j)])) lotCell[a] = 0;
		}
	}

	// =========================================================================================
	// 6. 大lot：横竖都隔一个选一个(i,j都取奇数)当中心node，周围四个lot都在就按概率合成一个。
	//    每个格子恰好属于一个候选中心，互不冲突。内部十字四段路删除。
	// =========================================================================================
	struct BigLot { int i; int j; };  // 左上角node
	vector<BigLot> bigLots;
	for (int j = 1; j + 1 < nv; j += 2) {
		for (int i = 1; i + 1 < nu; i += 2) {
			if (!(cellAt(i - 1, j - 1) && cellAt(i, j - 1) && cellAt(i - 1, j) && cellAt(i, j))) continue;
			if (GetRandom(10000) / 10000.f >= mergeProbability) continue;
			lotCell[nodeIndex(i - 1, j - 1)] = lotCell[nodeIndex(i, j - 1)] = lotCell[nodeIndex(i - 1, j)] = lotCell[nodeIndex(i, j)] = 0;
			hEdge[nodeIndex(i - 1, j)] = hEdge[nodeIndex(i, j)] = 0;
			vEdge[nodeIndex(i, j - 1)] = vEdge[nodeIndex(i, j)] = 0;
			alive[nodeIndex(i, j)] = 0;
			bigLots.push_back({ i - 1, j - 1 });
		}
	}

	// =========================================================================================
	// 6b. 主干道：在隔一条选一条的横线/竖线上，把线上"实际连续"的道路(沿线相邻存活node靠普通边或长边一路连着，
	//     断开就是另一段)按所含格点数从多到少排，横向、竖向各取前1/4(向上取整)，改成每侧MAIN_ROAD_LANES条车行道。
	//     放在补长连接、删边、大lot合并之后，所以连续性看的是最终留下来的边。hWide[a]标记边(a,a+1)，
	//     vWide[a]标记边(a,a+nu)，hLongWide/vLongWide标记从a出发的长边。
	// =========================================================================================
	vector<char> hWide(static_cast<size_t>(nu) * nv, 0), vWide(static_cast<size_t>(nu) * nv, 0);
	vector<char> hLongWide(static_cast<size_t>(nu) * nv, 0), vLongWide(static_cast<size_t>(nu) * nv, 0);
	{
		struct MainRun { int nodes = 0; vector<pair<int, bool>> edges; };  // edges：边起点node下标、是否长边
		auto collectRuns = [&](bool horizontal) {
			vector<MainRun> runs;
			int lines = horizontal ? nv : nu, length = horizontal ? nu : nv, parity = horizontal ? hParity : vParity;
			for (int line = 0; line < lines; line++) {
				if (line % 2 != parity) continue;
				auto at = [&](int k) { return horizontal ? nodeIndex(k, line) : nodeIndex(line, k); };
				for (int k = 0; k < length; ) {
					if (!alive[at(k)]) { k++; continue; }
					MainRun run;
					run.nodes = 1;
					int cur = k;
					while (true) {
						int a = at(cur), next = -1;
						bool isLong = false;
						const vector<char>& edge = horizontal ? hEdge : vEdge;
						const vector<int>& longTo = horizontal ? hLongTo : vLongTo;
						if (cur + 1 < length && edge[a]) next = cur + 1;
						else if (longTo[a] >= 0) { next = horizontal ? longTo[a] % nu : longTo[a] / nu; isLong = true; }
						if (next < 0) break;
						run.edges.push_back({ a, isLong });
						run.nodes++;
						cur = next;
					}
					if (!run.edges.empty()) runs.push_back(std::move(run));
					k = cur + 1;
				}
			}
			return runs;
			};
		for (bool horizontal : { true, false }) {
			vector<MainRun> runs = collectRuns(horizontal);
			// 格点数多的优先，数量取1/4(向上取整)。
			stable_sort(runs.begin(), runs.end(), [](const MainRun& x, const MainRun& y) { return x.nodes > y.nodes; });
			size_t widened = (runs.size() + 3) / 4;
			for (size_t r = 0; r < widened; r++) {
				for (const pair<int, bool>& e : runs[r].edges) {
					vector<char>& mark = e.second ? (horizontal ? hLongWide : vLongWide) : (horizontal ? hWide : vWide);
					mark[e.first] = 1;
				}
			}
			debugf("Log: Roadnet checkboard main roads %s: %d of %d widened.\n", horizontal ? "horizontal" : "vertical",
				static_cast<int>(widened), static_cast<int>(runs.size()));
		}
	}

	// =========================================================================================
	// 7. 过滤没用的node：只连了东西两条或南北两条(共线)且都不跨山的node删掉，两条路并成一条。
	//    chain：沿一条横/竖线的一整串被并起来的边，a/b是两端的node。
	// =========================================================================================
	// 东南西北各选一个"出口"node：必须保留成Intersection，不能被并掉。
	vector<char> forceKeep(static_cast<size_t>(nu) * nv, 0);
	struct ExternPick { int node; float dirX; float dirY; const char* name; float endX; float endY; };
	vector<ExternPick> externPicks;
	{
		// 射线(px,py)+t*(dx,dy)到地图边界的t。
		auto rayToBoundary = [&](float px, float py, float dx, float dy) {
			float t = FLT_MAX;
			if (dx > 1e-6f) t = min(t, (static_cast<float>(width) - px) / dx);
			else if (dx < -1e-6f) t = min(t, (0.f - px) / dx);
			if (dy > 1e-6f) t = min(t, (static_cast<float>(height) - py) / dy);
			else if (dy < -1e-6f) t = min(t, (0.f - py) / dy);
			return t;
			};
		const float dirs[4][2] = { { sinT, -cosT }, { cosT, sinT }, { -sinT, cosT }, { -cosT, -sinT } };  // 北东南西
		const char* names[4] = { "北出口路", "东出口路", "南出口路", "西出口路" };
		for (int d = 0; d < 4; d++) {
			// 最靠外的线：北=最北的东西向线(j最小)，南=j最大，西=最西的南北向线(i最小)，东=i最大；
			// 这条线上还有存活node就从中随机选一个，一个都没有就看下一条线。
			vector<int> candidates;
			if (d == 0 || d == 2) {
				for (int k = 0; k < nv && candidates.empty(); k++) {
					int j = d == 0 ? k : nv - 1 - k;
					for (int i = 0; i < nu; i++) {
						if (alive[nodeIndex(i, j)] && degree(i, j) > 0) candidates.push_back(nodeIndex(i, j));
					}
				}
			}
			else {
				for (int k = 0; k < nu && candidates.empty(); k++) {
					int i = d == 1 ? nu - 1 - k : k;
					for (int j = 0; j < nv; j++) {
						if (alive[nodeIndex(i, j)] && degree(i, j) > 0) candidates.push_back(nodeIndex(i, j));
					}
				}
			}
			if (candidates.empty()) continue;
			int pick = candidates[GetRandom(static_cast<int>(candidates.size()))];
			float t = rayToBoundary(posX[pick], posY[pick], dirs[d][0], dirs[d][1]);
			if (t == FLT_MAX || t < 3.f) continue;
			forceKeep[pick] = 1;
			externPicks.push_back({ pick, dirs[d][0], dirs[d][1], names[d], posX[pick] + dirs[d][0] * t, posY[pick] + dirs[d][1] * t });
		}
	}

	for (int a = 0; a < nu * nv; a++) {
		if (hLongTo[a] >= 0) forceKeep[a] = forceKeep[hLongTo[a]] = 1;
		if (vLongTo[a] >= 0) forceKeep[a] = forceKeep[vLongTo[a]] = 1;
	}

	auto removable = [&](int i, int j) {
		int a = nodeIndex(i, j);
		if (!alive[a] || forceKeep[a]) return false;
		bool west = i > 0 && hEdge[nodeIndex(i - 1, j)], east = i + 1 < nu && hEdge[a];
		bool north = j > 0 && vEdge[nodeIndex(i, j - 1)], south = j + 1 < nv && vEdge[a];
		if (west && east && !north && !south) return !hSpecial[nodeIndex(i - 1, j)] && !hSpecial[a];
		if (north && south && !west && !east) return !vSpecial[nodeIndex(i, j - 1)] && !vSpecial[a];
		return false;
		};

	struct Chain { int a; int b; bool horizontal; int line; int roadIndex; bool wide = false; };
	vector<Chain> chains;
	vector<int> hChain(static_cast<size_t>(nu) * nv, -1), vChain(static_cast<size_t>(nu) * nv, -1);
	for (int j = 0; j < nv; j++) {
		for (int i = 0; i < nu; i++) {
			int a = nodeIndex(i, j);
			if (hEdge[a] && hChain[a] < 0) {
				// 向左找到链头(最左的不可删node)，再向右一路走到链尾。
				int start = i;
				while (start > 0 && removable(start, j) && hEdge[nodeIndex(start - 1, j)]) start--;
				int end = start + 1;
				while (removable(end, j) && hEdge[nodeIndex(end, j)]) end++;
				int id = static_cast<int>(chains.size());
				chains.push_back({ nodeIndex(start, j), nodeIndex(end, j), true, j, -1 });
				for (int k = start; k < end; k++) hChain[nodeIndex(k, j)] = id;
			}
			if (vEdge[a] && vChain[a] < 0) {
				int start = j;
				while (start > 0 && removable(i, start) && vEdge[nodeIndex(i, start - 1)]) start--;
				int end = start + 1;
				while (removable(i, end) && vEdge[nodeIndex(i, end)]) end++;
				int id = static_cast<int>(chains.size());
				chains.push_back({ nodeIndex(i, start), nodeIndex(i, end), false, i, -1 });
				for (int k = start; k < end; k++) vChain[nodeIndex(i, k)] = id;
			}
		}
	}

	// 普通链：一条链里的边都在同一段连续道路上，看第一条边有没有被标成主干道就够了。
	for (Chain& chain : chains) chain.wide = chain.horizontal ? hWide[chain.a] : vWide[chain.a];

	// 长边各自一条链(special：跨山，不挂lot)。
	for (int a = 0; a < nu * nv; a++) {
		if (hLongTo[a] >= 0) chains.push_back({ a, hLongTo[a], true, a / nu, -1, hLongWide[a] != 0 });
		if (vLongTo[a] >= 0) chains.push_back({ a, vLongTo[a], false, a % nu, -1, vLongWide[a] != 0 });
	}

	// =========================================================================================
	// 8. 真正落地：存活且不可删的node建成Intersection，链铺成Road，lot挂边界Road。
	// =========================================================================================
	vector<int> intersectionOf(static_cast<size_t>(nu) * nv, -1);
	for (int j = 0; j < nv; j++) {
		for (int i = 0; i < nu; i++) {
			int a = nodeIndex(i, j);
			if (!alive[a] || degree(i, j) == 0 || removable(i, j)) continue;
			intersectionOf[a] = static_cast<int>(intersections.size());
			intersections.emplace_back(posX[a], posY[a], 0.f);
		}
	}

	for (Chain& chain : chains) {
		if (intersectionOf[chain.a] < 0 || intersectionOf[chain.b] < 0) continue;
		string chainName = chain.horizontal ? "东西第" + to_string(chain.line + 1) + "路" : "南北第" + to_string(chain.line + 1) + "路";
		chain.roadIndex = static_cast<int>(roads.size());
		currentLanes = chain.wide ? MAIN_ROAD_LANES : 1;
		layRoad(chainName, intersections[intersectionOf[chain.a]], intersections[intersectionOf[chain.b]]);
	}

	currentLanes = 1;
	// 四条出口路：从选中的node沿棋盘格方向一直铺到地图边界，边界点作为extern。
	for (const ExternPick& pick : externPicks) {
		Node externNode("roadnet", pick.endX, pick.endY, 0.f);
		layRoad(pick.name, intersections[intersectionOf[pick.node]], externNode);
		externs.push_back(externNode);
	}

	// lot边界Road指针：所有Road都铺完之后才能取，非跨山链只有一条Road。
	auto boundaryRoad = [&](int chainId) -> Road* {
		if (chainId < 0 || chains[chainId].roadIndex < 0) return nullptr;
		return &roads[chains[chainId].roadIndex];
		};
	auto roadMargin = [](const Road& road) -> float { return road.GetTotalWidth() * 0.5f; };
	const float halfDiagonal = 0.5f * sqrt(static_cast<float>(width) * width + static_cast<float>(height) * height);
	auto areaAt = [&](float x, float y) {
		float d = sqrt((x - centerX) * (x - centerX) + (y - centerY) * (y - centerY)) / halfDiagonal;
		return RandomArea(DensityLevel(d));
		};

	// 四角node顺序：西北、东北、东南、西南；边界Road按FACE_DIRECTION：西0东1北2南3。
	auto addLot = [&](int i0, int j0, int i1, int j1, int west, int east, int north, int south) {
		Road* w = boundaryRoad(west);
		Road* e = boundaryRoad(east);
		Road* n = boundaryRoad(north);
		Road* s = boundaryRoad(south);
		if (!w || !e || !n || !s) return;
		int nw = nodeIndex(i0, j0), ne = nodeIndex(i1, j0), se = nodeIndex(i1, j1), sw = nodeIndex(i0, j1);
		Node nwNode("roadnet", posX[nw], posY[nw]), neNode("roadnet", posX[ne], posY[ne]);
		Node seNode("roadnet", posX[se], posY[se]), swNode("roadnet", posX[sw], posY[sw]);
		lots.emplace_back(nwNode, neNode, seNode, swNode,
			vector<float>{ roadMargin(*w), roadMargin(*e), roadMargin(*n), roadMargin(*s) },
			unordered_map<int, Road*>{ {0, w}, {1, e}, {2, n}, {3, s} });
		lots.back().SetArea(areaAt((posX[nw] + posX[se]) * 0.5f, (posY[nw] + posY[se]) * 0.5f));
		};
	for (int j = 0; j + 1 < nv; j++) {
		for (int i = 0; i + 1 < nu; i++) {
			if (!lotCell[nodeIndex(i, j)]) continue;
			addLot(i, j, i + 1, j + 1,
				vChain[nodeIndex(i, j)], vChain[nodeIndex(i + 1, j)], hChain[nodeIndex(i, j)], hChain[nodeIndex(i, j + 1)]);
		}
	}
	// 大lot外圈每边由两段边组成，取其中第一段所在的链作为边界Road。
	for (const BigLot& big : bigLots) {
		int i = big.i, j = big.j;
		addLot(i, j, i + 2, j + 2,
			vChain[nodeIndex(i, j)], vChain[nodeIndex(i + 2, j)], hChain[nodeIndex(i, j)], hChain[nodeIndex(i, j + 2)]);
	}

	// 所有路的车道都配好之后统一写路口缩进。
	AssignJunctionSetbacks(roads, intersections);

	debugf("Log: Roadnet checkboard %d nodes, %d roads, %d lots (%d big), theta %.3f.\n",
		static_cast<int>(intersections.size()), static_cast<int>(roads.size()), static_cast<int>(lots.size()),
		static_cast<int>(bigLots.size()), theta);
}

void CheckboardRoadnet::LayoutTrees(const vector<Road*>& hostRoads) {
	LayoutRows(hostRoads, TREE_SPACING, TREE_SIDEWALK_RATIO,
		[this](int, float x0, float y0, float x1, float y1, int num, float) {
			AddTreeLine({ 0.f, x0, 0.f, y0 }, { 0.f, x1, 0.f, y1 }, num, TREE_SCALE, TREE_MESH_PATH);
		});
}

void CheckboardRoadnet::LayoutLamps(const vector<Road*>& hostRoads) {
	LayoutRows(hostRoads, LAMP_SPACING, LAMP_SIDEWALK_RATIO,
		[this](int side, float x0, float y0, float x1, float y1, int num, float roadAngleDegrees) {
			float rotation = roadAngleDegrees + (side == 0 ? -90.f : 90.f) + LAMP_YAW_OFFSET;
			AddLampLine({ 0.f, x0, 0.f, y0 }, { 0.f, x1, 0.f, y1 }, num, rotation, LAMP_BLUEPRINT_PATH);
		});
}

void CheckboardRoadnet::LayoutRows(const vector<Road*>& hostRoads, float spacing, float sidewalkRatio, const RowEmitter& emit) {
	// 与JingRoadnet::LayoutRows完全相同的几何(复制而来)，缺口规则见roadnet_basic.md"行道树"。
	unordered_set<int> intersectionIds;
	for (const Intersection& intersection : intersections) intersectionIds.insert(intersection.GetId());
	// 路口收缩距离：hostRoads就是Core建RoadJunction时用的那批大路，含隧道段，全部参与。
	// 路口收缩距离直接读Road上mod写好的缩进(AssignJunctionSetbacks)，只有连着Intersection的那端才有缩进。
	auto endGap = [&](const Road* road, bool isStart) {
		int endId = isStart ? road->GetStart().GetId() : road->GetEnd().GetId();
		return intersectionIds.count(endId) ? road->GetSetback(isStart) : 0.f;
		};

	for (const Road* road : hostRoads) {
		if (!road || road->IsPathRoad()) continue;
		Node start = road->GetStart(), end = road->GetEnd();
		if (start.GetZ() < 0.f || end.GetZ() < 0.f) continue;
		float dx = end.GetX() - start.GetX(), dy = end.GetY() - start.GetY();
		float len = sqrt(dx * dx + dy * dy);
		float startInset = endGap(road, true) + spacing * 0.5f;
		float endInset = endGap(road, false) + spacing * 0.5f;
		float usable = len - startInset - endInset;
		if (usable <= 0.f) continue;
		float ux = dx / len, uy = dy / len;
		float perpX = -uy, perpY = ux;
		float roadAngle = atan2f(dy, dx) * 57.2957795f;
		float shift = (road->GetSideWidth(0) - road->GetSideWidth(1)) * 0.5f;

		// 沿路距离s(从start量起)上哪些树位要去掉。
		int num = static_cast<int>(usable / spacing) + 1;
		float step = num > 1 ? usable / (num - 1) : 0.f;
		// 第i棵树沿路的距离；只有一棵时种在可用范围正中间。
		auto treeS = [&](int i) { return num > 1 ? startInset + i * step : startInset + usable * 0.5f; };
		vector<bool> keep(num, true);
		for (const RoadOpening& opening : road->GetOpenings()) {
			float center = opening.t * len;
			float half = opening.width * 0.5f + spacing * 0.5f;
			for (int i = 0; i < num; i++) {
				if (fabs(treeS(i) - center) < half) keep[i] = false;
			}
		}

		for (int side = 0; side < 2; side++) {
			const vector<float>& pedLanes = road->GetPedestrianLanes(side);
			if (pedLanes.empty()) continue;
			float inner = 0.f;
			for (float w : road->GetVehicleLanes(side)) inner += w;
			for (float w : road->GetParkingLanes(side)) inner += w;
			float offset = inner + pedLanes[0] * sidewalkRatio;
			float signedOffset = offset * (side == 0 ? 1.f : -1.f) - shift;
			float ox = start.GetX() + perpX * signedOffset, oy = start.GetY() + perpY * signedOffset;

			for (int i = 0; i < num;) {
				if (!keep[i]) { i++; continue; }
				int j = i;
				while (j + 1 < num && keep[j + 1]) j++;
				float s0 = treeS(i), s1 = treeS(j);
				emit(side, ox + ux * s0, oy + uy * s0, ox + ux * s1, oy + uy * s1, j - i + 1, roadAngle);
				i = j + 1;
			}
		}
	}
}
