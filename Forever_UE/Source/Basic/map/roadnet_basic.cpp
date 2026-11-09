#include "roadnet_basic.h"

#include <cmath>
#include <algorithm>
#include <set>
#include <cfloat>
#include <unordered_set>
#include <sstream>

// 过山有两种做法，按每座山单独决定(见buildArm)：
//   - 抬升：山体里每个采样点的路面高度取山表面高度(getHeight)，并对这些路段请求地形贴合
//     (RoadnetMod::AddTerrainFit)，让地形高度贴到路面高度上(路面比山高就填、比山低就切)。
//     适用于坡度都不超过MOUNTAIN_MAX_SLOPE的山，以及后面直接连着海的山。TERRAIN_FIT_FALLOFF是路面边缘
//     向外渐变回原地形的距离。
//   - 隧道：道路延伸时遇到mountain地形"钻进去"而不是贴着地形起伏铺过去，并且在隧道口用
//     RoadnetMod::AddHatch给地形挖一个洞，避免隧道段完全埋没在山体实心地形里看不见。挖洞宽度
//     按每条路实际配好的车道宽度算，不用固定常量，见addRoad里AddHatch调用处的说明。
#define MOUNTAIN_MAX_SLOPE 0.5f
#define TERRAIN_FIT_FALLOFF 1.5f
#define TUNNEL_HEIGHT -1.f
#define TUNNEL_HATCH_LENGTH 8.f

// 放射臂沿线每ARM_SAMPLE_STEP个地图单位(1=10m)采样一次地形，每ARM_SAMPLES_PER_CELL个采样点
// (=32单位=320m)是一个lot格子的边长。
#define ARM_SAMPLE_STEP 1.f
#define ARM_SAMPLES_PER_CELL 32

// 隧道口地面端先接一段完全水平的引道，必须比RoadJunction::Build按setback裁掉的路口进深长，
// 否则被裁掉的那截曲线已经下降，和强制铺成平面的路口mesh(见roadnet.md"路口高度")高度对不上。
#define TUNNEL_FLAT_APPROACH_LENGTH 1.5f
// 行道树(JingRoadnet::LayoutTrees，先粗估，后面再细调)：种在人行道内侧附近(TREE_SIDEWALK_
// RATIO=从人行道内侧量起的比例)，沿路每TREE_SPACING一棵。路口/开口让出的距离都按实际几何
// 算(路口收缩距离、开口宽度)，见JingRoadnet::LayoutTrees和roadnet_basic.md。
#define TREE_MESH_PATH "/Game/3rdParty/Mossy_Tree/StaticMeshes/mossy-tree.mossy-tree"
#define TREE_SCALE 0.6f
#define TREE_SPACING 1.f
#define TREE_SIDEWALK_RATIO 0.25f
// 路灯(JingRoadnet::LayoutLamps，先随便摆，后面看效果再调)：和行道树共用LayoutRows的几何，
// 放在行道树外侧(LAMP_SIDEWALK_RATIO比树的TREE_SIDEWALK_RATIO大)，沿路每LAMP_SPACING(1.5格=15m)一盏。side0/side1的朝向
// 自动差180度，LAMP_YAW_OFFSET(度)统一补偿蓝图里灯臂的初始朝向。
#define LAMP_BLUEPRINT_PATH "/Game/3rdParty/Stylized_street_light/Actor/BP_StreetLight.BP_StreetLight_C"
#define LAMP_SPACING 1.5f
#define LAMP_SIDEWALK_RATIO 0.9f
#define LAMP_YAW_OFFSET 0.f


using namespace std;

namespace {
		// 路口缩进(写进Road::SetSetback)：必须在所有路的车道都配好之后调用，宽度是后改的。对每个路口的每条连接路，
		// 看它在路口两侧与相邻路的夹角：两侧都是平角(>=180度)缩进为0；两侧最小夹角是直角(或钝角)取构成直角的这几条路
		// (含自己)半宽的最大值；两侧有锐角取构成锐角的这几条路(含自己)半宽最大值的两倍。再按0.45倍路长封顶(同Core的tFrac clamp)。
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

	// 井字中心lot固定AREA_OFFICIAL_HIGH，其余所有非中心lot(4个紧邻中心的臂lot+更外圈延伸出去
	// 的lot)按1:1:1随机分配成高密度住宅/商业/工业区——这是ShopBuilding/FactoryBuilding按
	// GetPower(AREA_TYPE)竞争lot的前提，分区类型如果单一固定，地图上只会长出一种建筑。
	AREA_TYPE RandomHighDensityArea() {
		static constexpr AREA_TYPE kAreas[3] = {
			AREA_RESIDENTIAL_HIGH, AREA_COMMERCIAL_HIGH, AREA_INDUSTRIAL_HIGH
		};
		return kAreas[GetRandom(3)];
	}
}

int JingRoadnet::count = 0;

JingRoadnet::JingRoadnet(const string& args) : id(count++) {
	istringstream stream(args);
	string token;
	while (stream >> token) {
		if (token == "--length") {
			int value;
			if (stream >> value) length = value;
		}
	}
}

JingRoadnet::~JingRoadnet() {

}

const char* JingRoadnet::GetId() {
	return "roadnet_jing";
}

const char* JingRoadnet::GetType() const {
	return "roadnet_jing";
}

const char* JingRoadnet::GetName() {
	name = "井字路网" + to_string(id);
	return name.data();
}

void JingRoadnet::DistributeRoadnet(int width, int height,
	const function<const string&(int, int)>& getTerrain,
	const function<pair<bool, float>(int, int)>& getWater,
	const function<float(float, float)>& getHeight,
	int nodeStaticCount) {
	Node::SetCount(nodeStaticCount);

	float meshUnit = 0.5f;
	const float LANE_WIDTH = 0.5f;

	// 每条放射臂上的"格点"(lot角点，距离角点32单位的整数倍)，不含角点本身，也不含extern。
	// 格点是普通Node，不是Intersection——RoadJunction::Build的setback裁剪只对Intersection生效，
	// 普通Node处前后两段Road首尾直接相接没有空缺，见buildArm。
	vector<Node> horizontalNode1w;
	vector<Node> horizontalNode1e;
	vector<Node> horizontalNode2w;
	vector<Node> horizontalNode2e;
	vector<Node> verticalNode1n;
	vector<Node> verticalNode1s;
	vector<Node> verticalNode2n;
	vector<Node> verticalNode2s;

	// 车道配置按左右两侧各自的(车行,停车,人行)数量参数化，addRoad默认(1,0,1,1,0,1)两侧对称。
	// left=side0，right=side1，和perp0下"side0=右手边"的既有约定一致。车道宽度统一LANE_WIDTH。
	auto configureLanesEx = [&](Road& road, int leftV, int leftP, int leftPd, int rightV, int rightP, int rightPd) {
		for (int i = 0; i < leftV; i++) road.AddVehicleLane(0, LANE_WIDTH);
		for (int i = 0; i < leftP; i++) road.AddParkingLane(0, LANE_WIDTH);
		for (int i = 0; i < leftPd; i++) road.AddPedestrianLane(0, LANE_WIDTH);
		for (int i = 0; i < rightV; i++) road.AddVehicleLane(1, LANE_WIDTH);
		for (int i = 0; i < rightP; i++) road.AddParkingLane(1, LANE_WIDTH);
		for (int i = 0; i < rightPd; i++) road.AddPedestrianLane(1, LANE_WIDTH);
		};

	// 道路资产命名default_左车行_左停车_左人行_右车行_右停车_右人行，见roadnet_basic.md。
	// tunnel=true加"_T"后缀(隧道内路面)，隧道口的引道/下坡仍用普通资产。
	auto meshPathFor = [](int leftV, int leftP, int leftPd, int rightV, int rightP, int rightPd, bool tunnel = false) -> string {
		string suffix = to_string(leftV) + "_" + to_string(leftP) + "_" + to_string(leftPd) + "_"
			+ to_string(rightV) + "_" + to_string(rightP) + "_" + to_string(rightPd);
		if (tunnel) suffix += "_T";
		return "/Game/Asset/Roadnet/Default/Mesh/default_" + suffix + ".default_" + suffix;
		};

	// 控制点放在1/3、2/3处(不与端点重合)，保证节点处切线非零。控制点Z按节点坡度(startSlope/endSlope，
	// Z每水平单位的变化量)给：c1.Z=n1.Z+startSlope*L/3、c2.Z=n2.Z-endSlope*L/3；坡度0是两端水平的S形
	// (隧道过渡段、平地)。抬升山路必须给坡度让曲线贴着山走，两端水平的台阶式起伏曲率太大，路面
	// 按弦铺的瓦片会偏离曲线十几厘米，地形贴着曲线就会穿出路面。
	auto addControls = [](Road& road, const Node& n1, const Node& n2, float startSlope = 0.f, float endSlope = 0.f) {
		float dx = n2.GetX() - n1.GetX();
		float dy = n2.GetY() - n1.GetY();
		float length = sqrt(dx * dx + dy * dy);
		Node c1("roadnet", n1.GetX() + dx / 3.f, n1.GetY() + dy / 3.f, n1.GetZ() + startSlope * length / 3.f);
		Node c2("roadnet", n1.GetX() + dx * 2.f / 3.f, n1.GetY() + dy * 2.f / 3.f, n2.GetZ() - endSlope * length / 3.f);
		road.AddControls({ { c1, 1.f }, { c2, 1.f } });
		};

	// 建一条路：一端隧道一端地面时拆成三段Connection(水平引道/S形下坡/平路)，两端同号则整段一条，
	// 见roadnet_basic.md。startSlope/endSlope是抬升山路两端的坡度(见addControls)，其它默认0。
	auto addRoad = [&](const string& name, const Node& n1, const Node& n2,
		int leftV = 1, int leftP = 0, int leftPd = 1, int rightV = 1, int rightP = 0, int rightPd = 1,
		float startSlope = 0.f, float endSlope = 0.f) {
		string thisMeshPath = meshPathFor(leftV, leftP, leftPd, rightV, rightP, rightPd);
		string tunnelMeshPath = meshPathFor(leftV, leftP, leftPd, rightV, rightP, rightPd, true);
		auto configureThis = [&](Road& road) { configureLanesEx(road, leftV, leftP, leftPd, rightV, rightP, rightPd); };

		bool n1Tunnel = n1.GetZ() < 0.f;
		bool n2Tunnel = n2.GetZ() < 0.f;

		if (n1Tunnel != n2Tunnel) {
			const Node& groundNode = n1Tunnel ? n2 : n1;
			const Node& tunnelNode = n1Tunnel ? n1 : n2;
			float dx = tunnelNode.GetX() - groundNode.GetX();
			float dy = tunnelNode.GetY() - groundNode.GetY();
			float segLen = sqrt(dx * dx + dy * dy);
			if (segLen < 1e-6f) segLen = 1.f;
			float ux = dx / segLen, uy = dy / segLen;

			Node flatNode("roadnet", groundNode.GetX() + ux * TUNNEL_FLAT_APPROACH_LENGTH, groundNode.GetY() + uy * TUNNEL_FLAT_APPROACH_LENGTH, groundNode.GetZ());
			// 隧道节点离地面端不超过TUNNEL_HATCH_LENGTH时(山体窄、入出口很近)没有"下坡之后再接一段
			// 平路"的空间：下坡直接以tunnelNode收尾，不再产生splitNode和第三段(否则是零长Road)。
			bool noLevelPart = segLen <= TUNNEL_HATCH_LENGTH + 1e-3f;
			Node splitNode = noLevelPart ? tunnelNode
				: Node("roadnet", groundNode.GetX() + ux * TUNNEL_HATCH_LENGTH, groundNode.GetY() + uy * TUNNEL_HATCH_LENGTH, tunnelNode.GetZ());

			// 引道本身也可能压在山体上，所以也单独开hatch，两段hatch首尾相接覆盖groundNode到splitNode整段。
			// 挖洞宽度必须取配好车道后的GetTotalWidth()，不能用固定常量：洞窄于路面会在两侧留下
			// 实心山体，走近隧道口时在地形和路面碰撞咬合不上的边界穿模掉下去。
			roads.emplace_back(name, groundNode, flatNode, thisMeshPath, meshUnit);
			addControls(roads.back(), groundNode, flatNode);
			configureThis(roads.back());
			AddTunnelHatch(&roads.back(), 0.f, 1.f, roads.back().GetTotalWidth());

			roads.emplace_back(name, flatNode, splitNode, thisMeshPath, meshUnit);
			addControls(roads.back(), flatNode, splitNode);
			configureThis(roads.back());
			AddTunnelHatch(&roads.back(), 0.f, 1.f, roads.back().GetTotalWidth());

			if (!noLevelPart) {
				roads.emplace_back(name, splitNode, tunnelNode, tunnelMeshPath, meshUnit);
				configureThis(roads.back());
			}
			return;
		}

		// 两端都在隧道里(Z<0)的路段是隧道内路面，用隧道内的资产。
		roads.emplace_back(name, n1, n2, (n1Tunnel && n2Tunnel) ? tunnelMeshPath : thisMeshPath, meshUnit);
		// 两端等高且坡度为0的路段(地面上的路、隧道里的路)直接是直线，不加控制点；只有高度变化的路段才是曲线。
		bool level = fabsf(n1.GetZ() - n2.GetZ()) < 1e-4f && fabsf(startSlope) < 1e-6f && fabsf(endSlope) < 1e-6f;
		if (!level) addControls(roads.back(), n1, n2, startSlope, endSlope);
		configureThis(roads.back());
		// 抬升的山路(有一端Z>0)请求地形贴合：地形高度贴到路面高度上，路面宽度是配好车道之后的
		// GetTotalWidth()。隧道(Z<0)不贴合，只在隧道口挖洞。
		if (n1.GetZ() > 1e-4f || n2.GetZ() > 1e-4f) {
			AddTerrainFit(&roads.back(), 0.f, 1.f, roads.back().GetTotalWidth(), TERRAIN_FIT_FALLOFF);
		}
		};

	// 找roads里名字为name、两端Node id为{n1,n2}(不分方向)的那一条，返回它自己的地址(不构造拷贝，
	// 否则给边界加开口传不到roads里)。调用时roads必须不再增长，地址才稳定。
	// 找不到返回nullptr(这段跨了隧道过渡点被拆成多段)，调用方必须放弃这块lot。
	auto makeBoundaryRoad = [&](const string& name, const Node& n1, const Node& n2) -> Road* {
		int idA = n1.GetId(), idB = n2.GetId();
		for (Road& r : roads) {
			if (r.GetName() != name) continue;
			int a = r.GetStart().GetId(), b = r.GetEnd().GetId();
			if ((a == idA && b == idB) || (a == idB && b == idA)) return &r;
		}
		return nullptr;
		};

	// lot的margin直接取临街road车道宽度的一半：车道横断面以连线为中心居中(见
	// Source/Core/map/roadnet.md"车道居中")，两侧margin天然相等，不用分side。
	auto roadMargin = [](const Road& road) -> float {
		return road.GetTotalWidth() * 0.5f;
		};

	// 判断一个点是不是落在凸四边形(nw,ne,se,sw按环绕顺序)内部——用叉积法：四条边各自算一次
	// (边起点->边终点)叉乘(边起点->测试点)，四边形是凸的且四个角按一致的环绕顺序给出时，
	// 点在内部等价于四个叉积同号(全部>=0或全部<=0)；只要出现一正一负就在外部。
	auto isPointInQuad = [](float px, float py, const Node& a, const Node& b, const Node& c, const Node& d) -> bool {
		auto cross = [](float ax, float ay, float bx, float by, float testX, float testY) -> float {
			return (bx - ax) * (testY - ay) - (by - ay) * (testX - ax);
			};
		float c1 = cross(a.GetX(), a.GetY(), b.GetX(), b.GetY(), px, py);
		float c2 = cross(b.GetX(), b.GetY(), c.GetX(), c.GetY(), px, py);
		float c3 = cross(c.GetX(), c.GetY(), d.GetX(), d.GetY(), px, py);
		float c4 = cross(d.GetX(), d.GetY(), a.GetX(), a.GetY(), px, py);
		bool hasNeg = (c1 < 0.0f) || (c2 < 0.0f) || (c3 < 0.0f) || (c4 < 0.0f);
		bool hasPos = (c1 > 0.0f) || (c2 > 0.0f) || (c3 > 0.0f) || (c4 > 0.0f);
		return !(hasNeg && hasPos);
		};

	// lot覆盖的每个element(不只四个角)都必须是"construction"才放：construction是
	// Map::InitTerrains"周围3x3全是plain/construction才晋升"的产物，plain边缘圈贴着山/河/海等不规则
	// 地形，只抽样四个角会漏判。
	auto isLotFullyConstruction = [&](const Node& nw, const Node& ne, const Node& se, const Node& sw) -> bool {
		float minX = min({ nw.GetX(), ne.GetX(), se.GetX(), sw.GetX() });
		float maxX = max({ nw.GetX(), ne.GetX(), se.GetX(), sw.GetX() });
		float minY = min({ nw.GetY(), ne.GetY(), se.GetY(), sw.GetY() });
		float maxY = max({ nw.GetY(), ne.GetY(), se.GetY(), sw.GetY() });
		int x0 = static_cast<int>(floorf(minX)), x1 = static_cast<int>(ceilf(maxX));
		int y0 = static_cast<int>(floorf(minY)), y1 = static_cast<int>(ceilf(maxY));
		for (int y = y0; y <= y1; y++) {
			for (int x = x0; x <= x1; x++) {
				// 用格子中心点做点在四边形内测试，比测试格子角点更准确地反映"这个element
				// 算不算在lot范围内"。
				if (!isPointInQuad(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f, nw, ne, se, sw)) continue;
				if (getTerrain(x, y) != "construction") return false;
			}
		}
		return true;
		};

	float theta = GetRandom(1000) / 1000.f * 0.4f - 0.2f;

	float nwX = width / 2.f + 16.f * (sin(theta) - cos(theta)), nwY = height / 2.f + 16.f * (-cos(theta) - sin(theta));
	float neX = width / 2.f + 16.f * (sin(theta) + cos(theta)), neY = height / 2.f + 16.f * (-cos(theta) + sin(theta));
	float seX = width / 2.f + 16.f * (-sin(theta) + cos(theta)), seY = height / 2.f + 16.f * (cos(theta) + sin(theta));
	float swX = width / 2.f + 16.f * (-sin(theta) - cos(theta)), swY = height / 2.f + 16.f * (cos(theta) - sin(theta));
	Intersection northWest(nwX, nwY, 0.f);
	Intersection northEast(neX, neY, 0.f);
	Intersection southEast(seX, seY, 0.f);
	Intersection southWest(swX, swY, 0.f);
	intersections.push_back(northWest);
	intersections.push_back(northEast);
	intersections.push_back(southEast);
	intersections.push_back(southWest);

	float dirSin = northEast.GetY() - northWest.GetY();
	float dirCos = northEast.GetX() - northWest.GetX();

	// 井字最中间的四条路，用默认对称车道配置。必须先于放射臂建路：lot里直接用&roads[0..3]引用它们。
	addRoad("中山西路", intersections[0], intersections[3]);
	addRoad("中山东路", intersections[1], intersections[2]);
	addRoad("中山北路", intersections[0], intersections[1]);
	addRoad("中山南路", intersections[3], intersections[2]);

	// 从角点corner沿(stepX,stepY)(一个lot格子边长=ARM_SAMPLES_PER_CELL个采样步)建一条放射臂直到地图边界，
	// 分两遍(采样判定每座山抬升/隧道，再按结果建节点铺路)，规则见roadnet_basic.md。
	// grid收集所有格点Node(不含角点、不含extern)，z已按隧道/抬升赋好。
	auto buildArm = [&](const string& name, const Node& corner, float stepX, float stepY, vector<Node>& grid) {
		float dx = stepX / ARM_SAMPLES_PER_CELL, dy = stepY / ARM_SAMPLES_PER_CELL;
		float cx = corner.GetX(), cy = corner.GetY();

		// 射线与地图边界矩形的交点(以采样步数k为单位的参数)。
		float kExt = FLT_MAX;
		if (dx > 1e-6f) kExt = min(kExt, (static_cast<float>(width) - cx) / dx);
		else if (dx < -1e-6f) kExt = min(kExt, (0.f - cx) / dx);
		if (dy > 1e-6f) kExt = min(kExt, (static_cast<float>(height) - cy) / dy);
		else if (dy < -1e-6f) kExt = min(kExt, (0.f - cy) / dy);
		if (kExt < 0.f || kExt == FLT_MAX) kExt = 0.f;

		// ---- 第一遍：记录采样点的地形类型和高度，k=0是角点；最后一个采样点至少离extern一个采样步，
		// 免得最后一段Road短到退化。getTerrain返回""表示出图。 ----
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

		// ---- 切出每座山[first,last]并决定做法；隧道口做在山外平地上，不够放就改抬升，见roadnet_basic.md。 ----
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

		// 隧道节点(Z=TUNNEL_HEIGHT)是[tIn,tOut]闭区间内的点；p/q是地面端口(Z=0)，p到tIn、tOut到q之间只有
		// 一段引道+下坡，中间不放别的节点。
		auto insideTunnel = [&](float k) {
			for (const TunnelSpan& t : tunnels) {
				if (k >= static_cast<float>(t.tIn) && k <= static_cast<float>(t.tOut)) return true;
			}
			return false;
			};
		// 采样点的路面高度：隧道里=TUNNEL_HEIGHT，抬升山里=山表面高度，其余0。
		auto zAt = [&](float k) {
			if (insideTunnel(k)) return static_cast<float>(TUNNEL_HEIGHT);
			int index = static_cast<int>(roundf(k));
			return (index >= 0 && index <= sampleCount && raised[index]) ? heights[index] : 0.f;
			};

		// 关键点(key=采样点序号)：全部格点(lot边界Road按格点两端精确匹配)、每个隧道的p/tIn/tOut/q、抬升山的每个
		// 采样点及其前后各一个采样点(平地过渡)。隧道口(p到tIn、tOut到q)里的格点去掉，让过渡正好是一段
		// 完整的80m；那里离山不到80m，本来也铺不了lot。
		set<float> keySet;
		for (int k = ARM_SAMPLES_PER_CELL; k <= sampleCount; k += ARM_SAMPLES_PER_CELL) keySet.insert(static_cast<float>(k));
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

		float externZ = (sampleCount >= 1 && raised[sampleCount]) ? getHeight(cx + dx * kExt, cy + dy * kExt) : 0.f;

		// 沿臂顺序的全部节点(采样点序号k，Z)：角点、关键点、extern。抬升的山路每个节点处的坡度取前后相邻节点连线的
		// 坡度(Catmull-Rom)；地面节点(Z=0，含山脚)和隧道节点(Z<0)坡度取0——上山的坡从山脚开始、起始导数
		// 不小于0，不会带着惯性冲到0以下；两端(角点/extern)也取0。
		struct ArmPoint { float k; float z; };
		vector<ArmPoint> points;
		points.push_back({ 0.f, corner.GetZ() });
		for (float k : keySet) points.push_back({ k, zAt(k) });
		points.push_back({ kExt, externZ });
		float stepLength = sqrt(dx * dx + dy * dy);
		auto slopeAt = [&](size_t i) {
			if (i == 0 || i + 1 >= points.size() || points[i].z < 1e-4f) return 0.f;
			float run = (points[i + 1].k - points[i - 1].k) * stepLength;
			return run > 1e-6f ? (points[i + 1].z - points[i - 1].z) / run : 0.f;
			};

		Node previous = corner;
		for (size_t i = 1; i < points.size(); i++) {
			bool isExtern = (i + 1 == points.size());
			Node node(isExtern ? Node("roadnet", cx + dx * kExt, cy + dy * kExt, externZ)
				: Node("roadnet", cx + dx * points[i].k, cy + dy * points[i].k, points[i].z));
			// 每段两端坡度夹到单调范围(与这段高度差同号、不超过3倍平均坡度，Fritsch-Carlson条件)，
			// 三次曲线在这一段内不会冲出两端高度之外(比如山脚冲到0以下、山顶冲到最高点以上)。
			float segmentRun = sqrt((node.GetX() - previous.GetX()) * (node.GetX() - previous.GetX())
				+ (node.GetY() - previous.GetY()) * (node.GetY() - previous.GetY()));
			float delta = segmentRun > 1e-6f ? (node.GetZ() - previous.GetZ()) / segmentRun : 0.f;
			auto monotone = [delta](float slope) {
				if (delta == 0.f) return 0.f;
				float aligned = slope * (delta > 0.f ? 1.f : -1.f);
				aligned = max(0.f, min(aligned, 3.f * fabsf(delta)));
				return aligned * (delta > 0.f ? 1.f : -1.f);
				};
			addRoad(name, previous, node, 1, 0, 1, 1, 0, 1, monotone(slopeAt(i - 1)), monotone(slopeAt(i)));
			previous = node;
			if (isExtern) {
				externs.emplace_back(node);
			}
			else if (fmodf(points[i].k, static_cast<float>(ARM_SAMPLES_PER_CELL)) == 0.f) {
				grid.push_back(node);
			}
		}
		};

	// externs顺序固定：西北/东北/西南/东南四条横臂，再北西/南西/北东/南东四条竖臂。
	buildArm("城西北路", northWest, -dirCos, -dirSin, horizontalNode1w);
	buildArm("城东北路", northEast, dirCos, dirSin, horizontalNode1e);
	buildArm("城西南路", southWest, -dirCos, -dirSin, horizontalNode2w);
	buildArm("城东南路", southEast, dirCos, dirSin, horizontalNode2e);
	buildArm("城北西路", northWest, dirSin, -dirCos, verticalNode1n);
	buildArm("城南西路", southWest, -dirSin, dirCos, verticalNode1s);
	buildArm("城北东路", northEast, dirSin, -dirCos, verticalNode2n);
	buildArm("城南东路", southEast, -dirSin, dirCos, verticalNode2s);

	if (isLotFullyConstruction(northWest, northEast, southEast, southWest)) {
		lots.emplace_back(northWest, northEast, southEast, southWest,
			vector<float>{ roadMargin(roads[0]), roadMargin(roads[1]), roadMargin(roads[2]), roadMargin(roads[3]) },
			unordered_map<int, Road*>{ {0, &roads[0]}, {1, &roads[1]}, {2, &roads[2]}, {3, &roads[3]} });
		lots.back().SetArea(AREA_OFFICIAL_HIGH);
	}

	// length>=0时每个臂最多length个lot：紧邻中心的象限lot是第1个(length==0则一个都不放)，
	// 后面沿臂细分的第i块是第i+1个，所以细分循环上限是length。
	const bool armLots = length != 0;
	const size_t extendLimit = length < 0 ? SIZE_MAX : static_cast<size_t>(length);

	if (armLots && horizontalNode1w.size() >= 1 && horizontalNode2w.size() >= 1) {
		Road* boundary2 = makeBoundaryRoad("城西北路", horizontalNode1w[0], intersections[0]);
		Road* boundary3 = makeBoundaryRoad("城西南路", horizontalNode2w[0], intersections[3]);
		// 找不到边界Road(这一段跨了隧道过渡点，被addRoad拆成了好几小段，见makeBoundaryRoad
		// 注释)就直接不铺这块lot——不给隧道旁边硬凑一个假边界勉强留住它。
		if (boundary2 && boundary3 &&
			isLotFullyConstruction(horizontalNode1w[0], intersections[0], intersections[3], horizontalNode2w[0])) {
			lots.emplace_back(horizontalNode1w[0], intersections[0], intersections[3], horizontalNode2w[0],
				vector<float>{ 0.0f, roadMargin(roads[0]), roadMargin(*boundary2), roadMargin(*boundary3) },
				unordered_map<int, Road*>{
					{ 1, &roads[0] },
					{ 2, boundary2 },
					{ 3, boundary3 }
				});
			lots.back().SetArea(RandomHighDensityArea());
		}
	}
	if (armLots && horizontalNode1e.size() >= 1 && horizontalNode2e.size() >= 1) {
		Road* boundary2 = makeBoundaryRoad("城东北路", horizontalNode1e[0], intersections[1]);
		Road* boundary3 = makeBoundaryRoad("城东南路", horizontalNode2e[0], intersections[2]);
		if (boundary2 && boundary3 &&
			isLotFullyConstruction(intersections[1], horizontalNode1e[0], horizontalNode2e[0], intersections[2])) {
			lots.emplace_back(intersections[1], horizontalNode1e[0], horizontalNode2e[0], intersections[2],
				vector<float>{ roadMargin(roads[1]), 0.0f, roadMargin(*boundary2), roadMargin(*boundary3) },
				unordered_map<int, Road*>{
					{ 0, &roads[1] },
					{ 2, boundary2 },
					{ 3, boundary3 }
				});
			lots.back().SetArea(RandomHighDensityArea());
		}
	}
	if (armLots && verticalNode1n.size() >= 1 && verticalNode2n.size() >= 1) {
		Road* boundary0 = makeBoundaryRoad("城北西路", verticalNode1n[0], intersections[0]);
		Road* boundary1 = makeBoundaryRoad("城北东路", verticalNode2n[0], intersections[1]);
		if (boundary0 && boundary1 &&
			isLotFullyConstruction(verticalNode1n[0], verticalNode2n[0], intersections[1], intersections[0])) {
			lots.emplace_back(verticalNode1n[0], verticalNode2n[0], intersections[1], intersections[0],
				vector<float>{ roadMargin(*boundary0), roadMargin(*boundary1), 0.0f, roadMargin(roads[2]) },
				unordered_map<int, Road*>{
					{ 0, boundary0 },
					{ 1, boundary1 },
					{ 3, &roads[2] }
				});
			lots.back().SetArea(RandomHighDensityArea());
		}
	}
	if (armLots && verticalNode1s.size() >= 1 && verticalNode2s.size() >= 1) {
		Road* boundary0 = makeBoundaryRoad("城南西路", verticalNode1s[0], intersections[3]);
		Road* boundary1 = makeBoundaryRoad("城南东路", verticalNode2s[0], intersections[2]);
		if (boundary0 && boundary1 &&
			isLotFullyConstruction(intersections[3], intersections[2], verticalNode2s[0], verticalNode1s[0])) {
			lots.emplace_back(intersections[3], intersections[2], verticalNode2s[0], verticalNode1s[0],
				vector<float>{ roadMargin(*boundary0), roadMargin(*boundary1), roadMargin(roads[3]), 0.0f },
				unordered_map<int, Road*>{
					{ 0, boundary0 },
					{ 1, boundary1 },
					{ 2, &roads[3] }
				});
			lots.back().SetArea(RandomHighDensityArea());
		}
	}

	// 沿每条放射臂继续细分lot，检查不通过或找不到边界Road就break(这个方向后面不再放lot)。
	for (size_t i = 1; i < min({ horizontalNode1w.size(), horizontalNode2w.size(), extendLimit }); i++) {
		const Node& nwNode = horizontalNode1w[i];
		const Node& neNode = horizontalNode1w[i - 1];
		const Node& seNode = horizontalNode2w[i - 1];
		const Node& swNode = horizontalNode2w[i];

		if (!isLotFullyConstruction(nwNode, neNode, seNode, swNode)) break;

		Road* boundary2 = makeBoundaryRoad("城西北路", nwNode, neNode);
		Road* boundary3 = makeBoundaryRoad("城西南路", swNode, seNode);
		// 找不到边界Road(这条边跨了隧道出入口，被addRoad拆成了好几段)就和地形不合格一样：取消这块
		// lot，并且这个方向后面不再放任何lot(路本身仍继续铺到extern)。
		if (!boundary2 || !boundary3) break;
		lots.emplace_back(nwNode, neNode, seNode, swNode,
			vector<float>{ 0.0f, 0.0f, roadMargin(*boundary2), roadMargin(*boundary3) },
			unordered_map<int, Road*>{
				{ 2, boundary2 },
				{ 3, boundary3 }
			});
		lots.back().SetArea(RandomHighDensityArea());
	}

	for (size_t i = 1; i < min({ horizontalNode1e.size(), horizontalNode2e.size(), extendLimit }); i++) {
		const Node& nwNode = horizontalNode1e[i - 1];
		const Node& neNode = horizontalNode1e[i];
		const Node& seNode = horizontalNode2e[i];
		const Node& swNode = horizontalNode2e[i - 1];

		if (!isLotFullyConstruction(nwNode, neNode, seNode, swNode)) break;

		Road* boundary2 = makeBoundaryRoad("城东北路", nwNode, neNode);
		Road* boundary3 = makeBoundaryRoad("城东南路", swNode, seNode);
		if (!boundary2 || !boundary3) break;
		lots.emplace_back(nwNode, neNode, seNode, swNode,
			vector<float>{ 0.0f, 0.0f, roadMargin(*boundary2), roadMargin(*boundary3) },
			unordered_map<int, Road*>{
				{ 2, boundary2 },
				{ 3, boundary3 }
			});
		lots.back().SetArea(RandomHighDensityArea());
	}

	for (size_t i = 1; i < min({ verticalNode1n.size(), verticalNode2n.size(), extendLimit }); i++) {
		const Node& nwNode = verticalNode1n[i];
		const Node& neNode = verticalNode2n[i];
		const Node& seNode = verticalNode2n[i - 1];
		const Node& swNode = verticalNode1n[i - 1];

		if (!isLotFullyConstruction(nwNode, neNode, seNode, swNode)) break;

		Road* boundary0 = makeBoundaryRoad("城北西路", nwNode, swNode);
		Road* boundary1 = makeBoundaryRoad("城北东路", neNode, seNode);
		if (!boundary0 || !boundary1) break;
		lots.emplace_back(nwNode, neNode, seNode, swNode,
			vector<float>{ roadMargin(*boundary0), roadMargin(*boundary1), 0.0f, 0.0f },
			unordered_map<int, Road*>{
				{ 0, boundary0 },
				{ 1, boundary1 }
			});
		lots.back().SetArea(RandomHighDensityArea());
	}

	for (size_t i = 1; i < min({ verticalNode1s.size(), verticalNode2s.size(), extendLimit }); i++) {
		const Node& nwNode = verticalNode1s[i - 1];
		const Node& neNode = verticalNode2s[i - 1];
		const Node& seNode = verticalNode2s[i];
		const Node& swNode = verticalNode1s[i];

		if (!isLotFullyConstruction(nwNode, neNode, seNode, swNode)) break;

		Road* boundary0 = makeBoundaryRoad("城南西路", nwNode, swNode);
		Road* boundary1 = makeBoundaryRoad("城南东路", neNode, seNode);
		if (!boundary0 || !boundary1) break;
		lots.emplace_back(nwNode, neNode, seNode, swNode,
			vector<float>{ roadMargin(*boundary0), roadMargin(*boundary1), 0.0f, 0.0f },
			unordered_map<int, Road*>{
				{ 0, boundary0 },
				{ 1, boundary1 }
			});
		lots.back().SetArea(RandomHighDensityArea());
	}

	// 所有路的车道都配好之后统一写路口缩进。
	AssignJunctionSetbacks(roads, intersections);
}

void JingRoadnet::LayoutTrees(const vector<Road*>& hostRoads) {
	LayoutRows(hostRoads, TREE_SPACING, TREE_SIDEWALK_RATIO,
		[this](int, float x0, float y0, float x1, float y1, int num, float) {
			// 路网的植被参考系是整张地图(不旋转)，世界坐标直接写成PointParams{0,x,0,y}。
			AddTreeLine({ 0.f, x0, 0.f, y0 }, { 0.f, x1, 0.f, y1 }, num, TREE_SCALE, TREE_MESH_PATH);
		});
}

void JingRoadnet::LayoutLamps(const vector<Road*>& hostRoads) {
	LayoutRows(hostRoads, LAMP_SPACING, LAMP_SIDEWALK_RATIO,
		[this](int side, float x0, float y0, float x1, float y1, int num, float roadAngleDegrees) {
			// 灯臂朝向路面：side0在perp0=(-dy,dx)一侧，朝向路面就是沿路方向-90度，side1相反(+90度)，
			// 两排自然差180度。LAMP_YAW_OFFSET统一补偿蓝图里灯臂的初始朝向。
			float rotation = roadAngleDegrees + (side == 0 ? -90.f : 90.f) + LAMP_YAW_OFFSET;
			AddLampLine({ 0.f, x0, 0.f, y0 }, { 0.f, x1, 0.f, y1 }, num, rotation, LAMP_BLUEPRINT_PATH);
		});
}

void JingRoadnet::LayoutRows(const vector<Road*>& hostRoads, float spacing, float sidewalkRatio, const RowEmitter& emit) {
	// 行道树/路灯共用的几何，缺口规则见roadnet_basic.md"行道树"。hostRoads是Core那份路网(开口齐全)，只读。
	// 横向偏移同Map::ComputeLaneAnchorPosition：perp0=(-dy,dx)是side0方向，整体按
	// shift=(side0宽-side1宽)/2居中。Core的Intersection深拷贝自this->intersections且保留id，按id判断。
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
