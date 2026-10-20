#include "roadnet_basic.h"

#include <cmath>
#include <algorithm>
#include <map>
#include <cfloat>
#include <unordered_set>

// 隧道：道路延伸时如果遇到mountain地形，"钻进去"而不是贴着地形起伏铺过去，并且在隧道口用
// RoadnetMod::AddHatch给地形挖一个洞，避免隧道段完全埋没在山体实心地形里看不见。挖洞宽度
// 按每条路实际配好的车道宽度算，不用固定常量，见addRoad里AddHatch调用处的说明。
#define TUNNEL_HEIGHT -1.f
#define TUNNEL_HATCH_LENGTH 4.f

// 放射臂沿线每ARM_SAMPLE_STEP个地图单位(4=40m)采样一次地形，每ARM_SAMPLES_PER_CELL个采样点
// (=32单位=320m)是一个lot格子的边长。
#define ARM_SAMPLE_STEP 4.f
#define ARM_SAMPLES_PER_CELL 8

// 地面端在真正开始下坡之前先接一段完全水平的引道，长度必须盖住RoadJunction::Build按setback
// (默认车道配置下=1.0地图单位)裁掉的路口进深——否则S形下坡从groundNode本身就开始，路口按
// "groundNode这个平面"裁掉的那一截曲线其实已经下降了一部分高度，路口mesh(强制铺成一个平面，
// 见roadnet.md"路口高度"一节)和曲线实际高度对不上，看起来像"路口范围内已经开始下坡"。
// 引道两端Z相同，addControls在这种情况下产出的Connection整条严格保持水平，不管路口实际裁掉
// 多长，裁掉的部分必然还在引道以内，因此不需要精确对齐setback——只要比它大留出余量即可。
#define TUNNEL_FLAT_APPROACH_LENGTH 1.5f
// 行道树(JingRoadnet::LayoutTrees，先粗估，后面再细调)：种在人行道正中间(TREE_SIDEWALK_
// RATIO=从人行道内侧量起的比例)，沿路每TREE_SPACING一棵。路口/开口让出的距离都按实际几何
// 算(路口收缩距离、开口宽度)，见JingRoadnet::LayoutTrees和roadnet_basic.md。
#define TREE_MESH_PATH "/Game/3rdParty/Mossy_Tree/StaticMeshes/mossy-tree.mossy-tree"
#define TREE_SCALE 0.6f
#define TREE_SPACING 1.f
#define TREE_SIDEWALK_RATIO 0.f


using namespace std;

namespace {
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

JingRoadnet::JingRoadnet() : id(count++) {

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
	const function<string(int, int)>& getTerrain,
	const function<pair<bool, float>(int, int)>& getWater,
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

	// 车道配置按左右两侧各自的(车行,停车,人行)数量参数化——默认(1,0,1,1,0,1)是两侧对称配置
	// (车行道每个方向1条宽0.5，不设停车道，人行道每侧1条宽0.5紧贴车行道外侧)，绝大多数Road
	// 用这一套；井字最中间的四条"中山X路"改用不对称/单行配置，用来验证车道居中、单行道开口
	// 这些逻辑，见下方addRoad调用处和roadnet_basic.md"车道资产命名"一节。
	// left=side0，right=side1，和perp0定义下"side0=右手边"这个既有约定一致。车道宽度统一用
	// LANE_WIDTH，不按数量再细分。
	auto configureLanesEx = [&](Road& road, int leftV, int leftP, int leftPd, int rightV, int rightP, int rightPd) {
		for (int i = 0; i < leftV; i++) road.AddVehicleLane(0, LANE_WIDTH);
		for (int i = 0; i < leftP; i++) road.AddParkingLane(0, LANE_WIDTH);
		for (int i = 0; i < leftPd; i++) road.AddPedestrianLane(0, LANE_WIDTH);
		for (int i = 0; i < rightV; i++) road.AddVehicleLane(1, LANE_WIDTH);
		for (int i = 0; i < rightP; i++) road.AddParkingLane(1, LANE_WIDTH);
		for (int i = 0; i < rightPd; i++) road.AddPedestrianLane(1, LANE_WIDTH);
		};

	// 道路3D资产命名规则(美术资产按此命名，见roadnet_basic.md)：default_左车行_左停车_左人行_
	// 右车行_右停车_右人行——和configureLanesEx的六个参数一一对应，保证mesh资产的视觉横断面
	// 和车道数据描述的横断面精确匹配。
	auto meshPathFor = [](int leftV, int leftP, int leftPd, int rightV, int rightP, int rightPd) -> string {
		string suffix = to_string(leftV) + "_" + to_string(leftP) + "_" + to_string(leftPd) + "_"
			+ to_string(rightV) + "_" + to_string(rightP) + "_" + to_string(rightPd);
		return "/Game/Asset/Meshes/default_" + suffix + ".default_" + suffix;
		};

	// 两端切线各贴一个控制点(1/3、2/3处，都不与真正的端点重合)：c1贴n1的高度、c2贴n2的高度，
	// 保证节点处切线水平且非零，相邻两段路在共享node上平滑接上，不会因为控制点和端点重合
	// 导致切线突然塌缩成0。n1.Z/n2.Z不相等时(隧道过渡段)这条曲线会在两个平缓端之间平滑升降。
	auto addControls = [](Road& road, const Node& n1, const Node& n2) {
		float dx = n2.GetX() - n1.GetX();
		float dy = n2.GetY() - n1.GetY();
		Node c1("roadnet", n1.GetX() + dx / 3.f, n1.GetY() + dy / 3.f, n1.GetZ());
		Node c2("roadnet", n1.GetX() + dx * 2.f / 3.f, n1.GetY() + dy * 2.f / 3.f, n2.GetZ());
		road.AddControls({ { c1, 1.f }, { c2, 1.f } });
		};

	// 建一条路：一端隧道一端地面(Z<0和Z>=0各一个)时，拆成三段独立Connection——①地面端水平
	// 引道(groundNode->flatNode，长TUNNEL_FLAT_APPROACH_LENGTH，两端同高)，②真正的S形下坡
	// (flatNode->splitNode，addControls给出平滑切线)，③平路(splitNode->隧道，两端同高)；
	// 两端同号(都隧道或都地面)时维持整段一条S形Connection不拆分。车道配置(6个参数)默认是
	// 两侧对称的(1,0,1,1,0,1)，只有井字最中间的四条"中山X路"显式传了不对称/单行的配置。
	auto addRoad = [&](const string& name, const Node& n1, const Node& n2,
		int leftV = 1, int leftP = 0, int leftPd = 1, int rightV = 1, int rightP = 0, int rightPd = 1) {
		string thisMeshPath = meshPathFor(leftV, leftP, leftPd, rightV, rightP, rightPd);
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

			// 水平引道本身也可能已经压在mountain地形上(hasMountainNearby的探测半径比这段
			// 引道长)，所以也要单独开一个hatch，两段hatch首尾相接，合起来覆盖
			// (groundNode到splitNode)整段范围，不会因为拆分出引道而漏挖。
			// 挖洞宽度必须用这条路配好车道之后的GetTotalWidth()(和roadMargin同一个取值)，
			// 不能用固定宽度常量——车道配置不对称/加宽时固定宽度的洞只能挖穿路面中间一部分，
			// 两侧车道/人行道底下会留着没挖穿的实心山体，导致走近隧道口时在地形和路面mesh
			// 碰撞咬合不上的边界处穿模掉下去。
			roads.emplace_back(name, groundNode, flatNode, thisMeshPath, meshUnit);
			addControls(roads.back(), groundNode, flatNode);
			configureThis(roads.back());
			AddHatch(&roads.back(), 0.f, 1.f, roads.back().GetTotalWidth());

			roads.emplace_back(name, flatNode, splitNode, thisMeshPath, meshUnit);
			addControls(roads.back(), flatNode, splitNode);
			configureThis(roads.back());
			AddHatch(&roads.back(), 0.f, 1.f, roads.back().GetTotalWidth());

			if (!noLevelPart) {
				roads.emplace_back(name, splitNode, tunnelNode, thisMeshPath, meshUnit);
				configureThis(roads.back());
			}
			return;
		}

		roads.emplace_back(name, n1, n2, thisMeshPath, meshUnit);
		addControls(roads.back(), n1, n2);
		configureThis(roads.back());
		};

	// 找到roads里名字为name、两端Node id正好是{n1,n2}(不分方向)的那一条，返回它自己的地址——
	// 不另外new/构造一份独立的Road。lot的边界必须和roads里真正会被渲染/以后可能被
	// Lot::SplitWithPath加开口的那个对象是同一个，不能是看起来配置一样但其实是另一个对象的
	// 拷贝(否则改动传不到roads里，小路接到大路上时给大路加的开口会画不出来)。调用这个lambda
	// 时roads必须已经不会再增长(所有addRoad调用都已经跑完)，取到的地址才是稳定的——本文件
	// 里所有makeBoundaryRoad调用都在addRoad调用序列结束之后，满足这个前提。
	// 找不到精确匹配就返回nullptr——常见原因是这一整段正好跨越隧道过渡点(n1/n2一个在地面一个
	// 在隧道里，或者addRoad按TUNNEL_HEIGHT相关逻辑把这段拆成了引道/下坡/平路三小段，见addRoad
	// 实现)，roads里不会再有任何一条正好首尾等于(n1,n2)的entry。这种情况下调用方(见下方几处
	// lots.emplace_back)必须直接跳过这个lot，不铺任何Zone/Building——没必要为了保住一块紧贴
	// 隧道的lot而给它凑一个假的边界Road。
	auto makeBoundaryRoad = [&](const string& name, const Node& n1, const Node& n2) -> Road* {
		int idA = n1.GetId(), idB = n2.GetId();
		for (Road& r : roads) {
			if (r.GetName() != name) continue;
			int a = r.GetStart().GetId(), b = r.GetEnd().GetId();
			if ((a == idA && b == idB) || (a == idB && b == idA)) return &r;
		}
		return nullptr;
		};

	// lot的margin直接从临街road自己的车道宽度算，不再手动指定固定常量——以后车道配置一旦改动
	// (configureLanes)，margin自动跟着变，不需要另外找一个ROAD_MARGIN常量手动保持同步。
	// 不需要判断lot落在road哪一侧：车道横断面现在以Connection连线为几何中心居中（见
	// Source/Core/map/roadnet.md"车道居中"一节），不管side0/side1怎么分配，两侧最外缘到
	// 连线的距离永远都精确等于road.GetTotalWidth()的一半，两侧margin天然相等，不用再分side。
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

	// 放置一个lot之前，要检查这个lot(四个角围出来的四边形，不只是四个角本身)覆盖的每一个
	// element，只要有一个不是"construction"就不放这个lot——原来只抽样检查四个角够不够
	// buildable(plain或construction都算)，这次改成扫描lot内部全部element、而且只认
	// "construction"严格一种：construction是Map::InitTerrains那边"周围3x3全是plain/
	// construction才晋升"的产物，边缘上混杂着真正plain的那一圈格子贴着mountain/river/
	// ocean这类不规则地形，只抽样四个角可能漏判，lot实际铺出来的那块地里混进了贴着不规则
	// 地形的格子；要求整块lot严格落在已经晋升的construction区域内部，天然避开所有这种
	// 边缘地带。
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

	// 井字最中间的四条路——之前故意配成不对称/单行用来验证车道居中/路口收缩/单行道开口这些
	// 逻辑，现在改回和其余道路一样的对称默认配置(双向各1条车行道)，应用户要求给公共交通
	// 测试腾出一个规整的中心方形地块，不再需要这几条路本身验证车道不对称。
	// (必须先于放射臂建路：lot里直接用&roads[0..3]引用这四条路。)
	addRoad("中山西路", intersections[0], intersections[3]);
	addRoad("中山东路", intersections[1], intersections[2]);
	addRoad("中山北路", intersections[0], intersections[1]);
	addRoad("中山南路", intersections[3], intersections[2]);

	// 从角点corner出发沿(stepX,stepY)(长度=一个lot格子边长=ARM_SAMPLES_PER_CELL个采样步)建一条
	// 放射臂，直到地图边界：
	//   - 每ARM_SAMPLE_STEP(40m)采样一次地形，由此决定隧道区间[P,Q]：地面状态下如果下一个采样点是
	//     山体就要入隧道，入口P必须是plain/construction采样点(当前点是河/海等不合格地形就向回退到
	//     最近的合格采样点，中间那段按平地铺过去)；隧道里一直走到第一个plain/construction采样点
	//     才出隧道，出口Q同样保证落在合格地形上(出口前是水面就继续隧道穿过去)。
	//   - 臂上不放Intersection：每个格点(采样点序号为ARM_SAMPLES_PER_CELL的整数倍，即lot角点)、
	//     入口P、出口Q都只是普通Node，Road在这些点首尾相接，没有路口setback造成的空缺。
	//     lot的边界Road仍然靠两端Node id精确匹配(makeBoundaryRoad)。
	//   - extern是臂所在直线与地图边界的交点，不是最后一个采样点。
	// grid收集所有格点Node(不含角点、不含extern)，z已按隧道区间赋好。
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

		// 采样序列terr[k]，k=0是角点；最后一个采样点至少离extern一个采样步(4个单位)，免得最后一段
		// Road短到退化。getTerrain返回""表示出图。
		vector<string> terr;
		terr.push_back(getTerrain(static_cast<int>(floorf(cx)), static_cast<int>(floorf(cy))));
		for (int k = 1; static_cast<float>(k) <= kExt - 1.f; k++) {
			string t = getTerrain(static_cast<int>(floorf(cx + dx * k)), static_cast<int>(floorf(cy + dy * k)));
			if (t == "") break;
			terr.push_back(t);
		}
		int sampleCount = static_cast<int>(terr.size()) - 1;

		auto isGood = [&](int k) { return terr[k] == "plain" || terr[k] == "construction"; };

		// 隧道区间(采样点序号)：严格位于p和q之间的点在隧道里，p/q本身是地面端口；q<0表示一直到extern。
		struct TunnelSpan { int p; int q; };
		vector<TunnelSpan> tunnels;
		int lastQ = 0;
		for (int k = 0; k < sampleCount; ) {
			if (terr[k + 1] != "terrain_mountain") { k++; continue; }
			int p = k;
			while (p > lastQ && !isGood(p)) p--;
			int q = k + 1;
			while (q <= sampleCount && !isGood(q)) q++;
			if (q > sampleCount) {
				tunnels.push_back({ p, -1 });
				break;
			}
			tunnels.push_back({ p, q });
			lastQ = q;
			k = q;
		}

		auto insideTunnel = [&](float k) {
			for (const TunnelSpan& s : tunnels) {
				if (k > static_cast<float>(s.p) && (s.q < 0 || k < static_cast<float>(s.q))) return true;
			}
			return false;
			};

		// 关键点：全部格点 + 每个隧道的入口/出口；如果一个隧道的入口和出口之间一个隧道格点都没有，
		// 在中点补一个隧道节点，保证两端的地面↔隧道过渡各有自己的坡。key=采样点序号，value=是否隧道。
		map<float, bool> keys;
		for (int k = ARM_SAMPLES_PER_CELL; k <= sampleCount; k += ARM_SAMPLES_PER_CELL) {
			keys[static_cast<float>(k)] = insideTunnel(static_cast<float>(k));
		}
		for (const TunnelSpan& s : tunnels) {
			keys[static_cast<float>(s.p)] = false;
			if (s.q >= 0) keys[static_cast<float>(s.q)] = false;
		}
		for (const TunnelSpan& s : tunnels) {
			if (s.q < 0) continue;
			bool hasTunnelKey = false;
			for (const auto& [k, tunnel] : keys) {
				if (tunnel && k > static_cast<float>(s.p) && k < static_cast<float>(s.q)) { hasTunnelKey = true; break; }
			}
			if (!hasTunnelKey) keys[(s.p + s.q) * 0.5f] = true;
		}

		Node previous = corner;
		for (const auto& [k, tunnel] : keys) {
			// 序号0是角点自己(Intersection)，不重复建节点。
			if (k <= 0.f) continue;
			Node node("roadnet", cx + dx * k, cy + dy * k, tunnel ? TUNNEL_HEIGHT : 0.f);
			addRoad(name, previous, node);
			previous = node;
			if (fmodf(k, static_cast<float>(ARM_SAMPLES_PER_CELL)) == 0.f) grid.push_back(node);
		}

		bool externTunnel = !tunnels.empty() && tunnels.back().q < 0;
		Node externNode("roadnet", cx + dx * kExt, cy + dy * kExt, externTunnel ? TUNNEL_HEIGHT : 0.f);
		addRoad(name, previous, externNode);
		externs.emplace_back(externNode);
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

	if (horizontalNode1w.size() >= 1 && horizontalNode2w.size() >= 1) {
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
	if (horizontalNode1e.size() >= 1 && horizontalNode2e.size() >= 1) {
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
	if (verticalNode1n.size() >= 1 && verticalNode2n.size() >= 1) {
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
	if (verticalNode1s.size() >= 1 && verticalNode2s.size() >= 1) {
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

	// 沿每条放射臂继续按isLotFullyConstruction细分lot——整块lot覆盖的所有element都必须是
	// construction才放，不是只抽样四个角。
	for (size_t i = 1; i < min(horizontalNode1w.size(), horizontalNode2w.size()); i++) {
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

	for (size_t i = 1; i < min(horizontalNode1e.size(), horizontalNode2e.size()); i++) {
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

	for (size_t i = 1; i < min(verticalNode1n.size(), verticalNode2n.size()); i++) {
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

	for (size_t i = 1; i < min(verticalNode1s.size(), verticalNode2s.size()); i++) {
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
}

void JingRoadnet::LayoutTrees(const vector<Road*>& hostRoads) {
	// hostRoads是Core那份路网(开口已经齐全)，只读。每条地面上的大路两侧各种一排：横向偏移和
	// Map::ComputeLaneAnchorPosition同一套约定——perp0=(-dy,dx)是side0方向，车道整体按
	// shift=(side0宽-side1宽)/2居中。
	// 纵向统一规则："缺口"边缘之外再留半个TREE_SPACING才种第一棵——
	//   ①端点是Intersection(真正的路口)：缺口=路口收缩距离，和RoadJunction::Build同一个定义
	//     (这个路口所有连接路里GetTotalWidth()最大值的一半)，也就是这条路可见路面开始的位置；
	//   ②端点是普通Node(lot角格点/地图边缘残端)：道路本身连续，缺口=0，相邻两段在node两侧各留
	//     半格，拼起来正好一个完整间距，树连续不断也不会在node上重复种两棵；
	//   ③开口(Road::GetOpenings，只有小路接口和建筑/园区出入口两种来源，普通node不会产生开口)：
	//     缺口=开口自己的width，以开口中心为准两侧各width/2，再各留半格。
	// 剩下连续的几棵合成一条AddTreeLine。开口两侧人行道都避开(SplitWithPath产的开口没有可靠的
	// side信息)。隧道口附近的引道两端Z也是0，靠Core的CanPlaceTree跳过挖洞格子。
	// Core的Intersection是从this->intersections深拷贝的，拷贝保留id，按id判断即可。
	unordered_set<int> intersectionIds;
	for (const Intersection& intersection : intersections) intersectionIds.insert(intersection.GetId());
	// 路口收缩距离：hostRoads就是Core建RoadJunction时用的那批大路，含隧道段，全部参与。
	unordered_map<int, float> junctionSetback;
	for (const Road* road : hostRoads) {
		if (!road || road->IsPathRoad()) continue;
		for (int endId : { road->GetStart().GetId(), road->GetEnd().GetId() }) {
			if (!intersectionIds.count(endId)) continue;
			float& setback = junctionSetback[endId];
			setback = max(setback, road->GetTotalWidth() * 0.5f);
		}
	}
	auto endGap = [&](int nodeId) {
		auto found = junctionSetback.find(nodeId);
		return found != junctionSetback.end() ? found->second : 0.f;
		};

	for (const Road* road : hostRoads) {
		if (!road || road->IsPathRoad()) continue;
		Node start = road->GetStart(), end = road->GetEnd();
		if (start.GetZ() < 0.f || end.GetZ() < 0.f) continue;
		float dx = end.GetX() - start.GetX(), dy = end.GetY() - start.GetY();
		float len = sqrt(dx * dx + dy * dy);
		float startInset = endGap(start.GetId()) + TREE_SPACING * 0.5f;
		float endInset = endGap(end.GetId()) + TREE_SPACING * 0.5f;
		float usable = len - startInset - endInset;
		if (usable <= 0.f) continue;
		float ux = dx / len, uy = dy / len;
		float perpX = -uy, perpY = ux;
		float shift = (road->GetSideWidth(0) - road->GetSideWidth(1)) * 0.5f;

		// 沿路距离s(从start量起)上哪些树位要去掉。
		int num = static_cast<int>(usable / TREE_SPACING) + 1;
		float step = num > 1 ? usable / (num - 1) : 0.f;
		// 第i棵树沿路的距离；只有一棵时种在可用范围正中间。
		auto treeS = [&](int i) { return num > 1 ? startInset + i * step : startInset + usable * 0.5f; };
		vector<bool> keep(num, true);
		for (const RoadOpening& opening : road->GetOpenings()) {
			float center = opening.t * len;
			float half = opening.width * 0.5f + TREE_SPACING * 0.5f;
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
			float offset = inner + pedLanes[0] * TREE_SIDEWALK_RATIO;
			float signedOffset = offset * (side == 0 ? 1.f : -1.f) - shift;
			float ox = start.GetX() + perpX * signedOffset, oy = start.GetY() + perpY * signedOffset;

			for (int i = 0; i < num;) {
				if (!keep[i]) { i++; continue; }
				int j = i;
				while (j + 1 < num && keep[j + 1]) j++;
				float s0 = treeS(i), s1 = treeS(j);
				// 路网的植被参考系是整张地图(不旋转)，世界坐标直接写成PointParams{0,x,0,y}。
				AddTreeLine({ 0.f, ox + ux * s0, 0.f, oy + uy * s0 }, { 0.f, ox + ux * s1, 0.f, oy + uy * s1 },
					j - i + 1, TREE_SCALE, TREE_MESH_PATH);
				i = j + 1;
			}
		}
	}
}
