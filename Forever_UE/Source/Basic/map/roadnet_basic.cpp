#include "roadnet_basic.h"

#include <cmath>
#include <algorithm>

// 隧道：道路延伸时如果遇到mountain地形，改成"钻进去"而不是贴着地形起伏铺过去，并且在隧道口
// 用RoadnetMod::AddHatch给地形挖一个洞，避免隧道段完全埋没在山体实心地形里看不见——四个
// 常量照抄老工程语义。
#define TUNNEL_HEIGHT -1.f
#define TUNNEL_HATCH_WIDTH 1.f
#define TUNNEL_HATCH_LENGTH 4.f
#define TUNNEL_LOOKAHEAD_DISTANCE 5.f

// 地面端在真正开始下坡之前先接一段完全水平的引道，长度必须盖住RoadJunction::Build按setback
// (默认车道配置下=1.0地图单位)裁掉的路口进深——否则S形下坡从groundNode本身就开始，路口按
// "groundNode这个平面"裁掉的那一截曲线其实已经下降了一部分高度，路口mesh(强制铺成一个平面，
// 见roadnet.md"路口高度"一节)和曲线实际高度对不上，看起来像"路口范围内已经开始下坡"。
// 引道两端Z相同，addControls在这种情况下产出的Connection整条严格保持水平，不管路口实际裁掉
// 多长，裁掉的部分必然还在引道以内，因此不需要精确对齐setback——只要比它大留出余量即可。
#define TUNNEL_FLAT_APPROACH_LENGTH 1.5f

using namespace std;

int JingRoadnet::count = 0;

JingRoadnet::JingRoadnet() : id(count++) {

}

JingRoadnet::~JingRoadnet() {

}

const char* JingRoadnet::GetId() {
	return "jing";
}

const char* JingRoadnet::GetType() const {
	return "jing";
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

	vector<pair<Node, int>> horizontalNode1w;
	vector<pair<Node, int>> horizontalNode1e;
	vector<pair<Node, int>> horizontalNode2w;
	vector<pair<Node, int>> horizontalNode2e;
	vector<pair<Node, int>> verticalNode1n;
	vector<pair<Node, int>> verticalNode1s;
	vector<pair<Node, int>> verticalNode2n;
	vector<pair<Node, int>> verticalNode2s;

	// 检查(x,y)沿(stepX,stepY)方向及其反方向TUNNEL_LOOKAHEAD_DISTANCE个单位以内是否有山体——
	// 提前/延后进出隧道的过渡点，避免隧道口卡在山体地形正中间。
	auto hasMountainNearby = [&](float x, float y, float stepX, float stepY) -> bool {
		float stepLen = sqrt(stepX * stepX + stepY * stepY);
		if (stepLen < 1e-6f) stepLen = 1.f;
		float unitX = stepX / stepLen, unitY = stepY / stepLen;
		for (float d = 1.f; d <= TUNNEL_LOOKAHEAD_DISTANCE; d += 1.f) {
			if (getTerrain(static_cast<int>(x + unitX * d), static_cast<int>(y + unitY * d)) == "mountain") return true;
			if (getTerrain(static_cast<int>(x - unitX * d), static_cast<int>(y - unitY * d)) == "mountain") return true;
		}
		return false;
		};

	// 沿给定方向延伸链条直到地图边界；山体节点、以及前后TUNNEL_LOOKAHEAD_DISTANCE个单位内能
	// 探测到山体的节点都按隧道高度处理，其余节点仍然固定0（这次只恢复隧道这一部分，不恢复
	// 老工程"全程按真实地形/水面高度起伏"的地形高度跟随，范围说明见roadnet_basic.md"隧道"
	// 一节）。getTerrain同时也用来判断是否还在地图内。
	auto extendChain = [&](float startX, float startY, float stepX, float stepY, vector<pair<Node, int>>& chain) {
		for (float x = startX, y = startY; ; x += stepX, y += stepY) {
			string t = getTerrain(static_cast<int>(x), static_cast<int>(y));
			if (t == "") break;
			bool tunnel = (t == "mountain") || hasMountainNearby(x, y, stepX, stepY);
			float h = tunnel ? TUNNEL_HEIGHT : 0.f;
			intersections.emplace_back(x, y, h);
			chain.emplace_back(intersections.back(), static_cast<int>(intersections.size()) - 1);
		}
		};

	// 车道配置改成按左右两侧各自的(车行,停车,人行)数量参数化——默认(1,0,1,1,0,1)是两侧对称的
	// 老配置(车行道每个方向1条宽0.5，不设停车道，人行道每侧1条宽0.5紧贴车行道外侧)，绝大多数
	// Road仍然用这一套；井字最中间的四条"中山X路"改用不对称/单行配置，用来验证车道居中、
	// 单行道开口这些新逻辑，见下方addRoad调用处和roadnet_basic.md"车道资产命名"一节。
	// left=side0，right=side1——PIE实测发现之前反了(原来写的是left=side1/right=side0)，
	// 和perp0定义下"side0=右手边"这个既有约定对不上，导致不对称资产的车道数左右和贴图左右
	// 对调。车道宽度统一用LANE_WIDTH，不按数量再细分，这次验证的重点是数量不对称/单行本身，
	// 不是宽度精细调整。
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
			Node splitNode("roadnet", groundNode.GetX() + ux * TUNNEL_HATCH_LENGTH, groundNode.GetY() + uy * TUNNEL_HATCH_LENGTH, tunnelNode.GetZ());

			// 水平引道本身也可能已经压在mountain地形上(hasMountainNearby的探测半径比这段
			// 引道长)，所以也要单独开一个hatch，两段hatch首尾相接，合起来正好覆盖老版本
			// "整段(groundNode到splitNode)一次性开洞"的范围，不会因为拆分出引道而漏挖。
			roads.emplace_back(name, groundNode, flatNode, thisMeshPath, meshUnit);
			addControls(roads.back(), groundNode, flatNode);
			configureThis(roads.back());
			AddHatch(&roads.back(), 0.f, 1.f, TUNNEL_HATCH_WIDTH);

			roads.emplace_back(name, flatNode, splitNode, thisMeshPath, meshUnit);
			addControls(roads.back(), flatNode, splitNode);
			configureThis(roads.back());
			AddHatch(&roads.back(), 0.f, 1.f, TUNNEL_HATCH_WIDTH);

			roads.emplace_back(name, splitNode, tunnelNode, thisMeshPath, meshUnit);
			configureThis(roads.back());
			return;
		}

		roads.emplace_back(name, n1, n2, thisMeshPath, meshUnit);
		addControls(roads.back(), n1, n2);
		configureThis(roads.back());
		};

	// 找到roads里名字为name、两端Node id正好是{n1,n2}(不分方向)的那一条，返回它自己的地址——
	// 不另外new/构造一份独立的Road。lot的边界必须和roads里真正会被渲染/以后可能被
	// Lot::SplitWithPath加开口的那个对象是同一个，不能是看起来配置一样但其实是另一个对象的
	// 拷贝(否则改动传不到roads里，PIE验证发现过这个问题：小路接到大路上时给大路加的开口画不
	// 出来)。调用这个lambda时roads必须已经不会再增长(所有addRoad调用都已经跑完)，取到的地址
	// 才是稳定的——本文件里所有makeBoundaryRoad调用都在addRoad调用序列结束之后，满足这个前提。
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

	// 判断某坐标地形是否可以铺设block(plain或construction)
	auto isBuildable = [&](float x, float y) -> bool {
		string t = getTerrain(static_cast<int>(x), static_cast<int>(y));
		return t == "plain" || t == "construction";
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

	extendChain(northWest.GetX() - dirCos, northWest.GetY() - dirSin, -dirCos, -dirSin, horizontalNode1w);
	intersections.pop_back();
	extendChain(northEast.GetX() + dirCos, northEast.GetY() + dirSin, dirCos, dirSin, horizontalNode1e);
	intersections.pop_back();
	extendChain(southWest.GetX() - dirCos, southWest.GetY() - dirSin, -dirCos, -dirSin, horizontalNode2w);
	intersections.pop_back();
	extendChain(southEast.GetX() + dirCos, southEast.GetY() + dirSin, dirCos, dirSin, horizontalNode2e);
	intersections.pop_back();
	extendChain(northWest.GetX() + dirSin, northWest.GetY() - dirCos, dirSin, -dirCos, verticalNode1n);
	intersections.pop_back();
	extendChain(southWest.GetX() - dirSin, southWest.GetY() + dirCos, -dirSin, dirCos, verticalNode1s);
	intersections.pop_back();
	extendChain(northEast.GetX() + dirSin, northEast.GetY() - dirCos, dirSin, -dirCos, verticalNode2n);
	intersections.pop_back();
	extendChain(southEast.GetX() - dirSin, southEast.GetY() + dirCos, -dirSin, dirCos, verticalNode2s);
	intersections.pop_back();
	externs.emplace_back(horizontalNode1w.back().first);
	horizontalNode1w.pop_back();
	externs.emplace_back(horizontalNode1e.back().first);
	horizontalNode1e.pop_back();
	externs.emplace_back(horizontalNode2w.back().first);
	horizontalNode2w.pop_back();
	externs.emplace_back(horizontalNode2e.back().first);
	horizontalNode2e.pop_back();
	externs.emplace_back(verticalNode1n.back().first);
	verticalNode1n.pop_back();
	externs.emplace_back(verticalNode1s.back().first);
	verticalNode1s.pop_back();
	externs.emplace_back(verticalNode2n.back().first);
	verticalNode2n.pop_back();
	externs.emplace_back(verticalNode2s.back().first);
	verticalNode2s.pop_back();

	// 井字最中间的四条路故意配成不对称/单行，验证车道居中、路口收缩、单行道开口这些新逻辑
	// (对称默认配置从来没暴露过这些问题，必须有真正不对称的数据才能在PIE里看出来)：
	// 中山西路/中山东路是左右车道数不同的双向路(互为镜像)，中山北路/中山南路是单行道
	// (整条路车行道全部在一侧，另一侧只有人行道，也互为镜像)。
	addRoad("中山西路", intersections[0], intersections[3], 1, 0, 1, 2, 0, 1);
	addRoad("中山东路", intersections[1], intersections[2], 2, 0, 1, 1, 0, 1);
	addRoad("中山北路", intersections[0], intersections[1], 0, 0, 1, 2, 0, 1);
	addRoad("中山南路", intersections[3], intersections[2], 2, 0, 1, 0, 0, 1);

	if (horizontalNode1w.size() > 0) {
		addRoad("城西北路", intersections[0], intersections[horizontalNode1w[0].second]);
	}
	for (size_t i = 1; i < horizontalNode1w.size(); i++) {
		const auto& [node1, idx1] = horizontalNode1w[i];
		const auto& [node2, idx2] = horizontalNode1w[i - 1];
		addRoad("城西北路", intersections[idx1], intersections[idx2]);
	}
	addRoad("城西北路", intersections[horizontalNode1w.back().second], externs[0]);
	if (horizontalNode1e.size() > 0) {
		addRoad("城东北路", intersections[1], intersections[horizontalNode1e[0].second]);
	}
	for (size_t i = 1; i < horizontalNode1e.size(); i++) {
		const auto& [node1, idx1] = horizontalNode1e[i];
		const auto& [node2, idx2] = horizontalNode1e[i - 1];
		addRoad("城东北路", intersections[idx1], intersections[idx2]);
	}
	addRoad("城东北路", intersections[horizontalNode1e.back().second], externs[1]);
	if (horizontalNode2w.size() > 0) {
		addRoad("城西南路", intersections[3], intersections[horizontalNode2w[0].second]);
	}
	for (size_t i = 1; i < horizontalNode2w.size(); i++) {
		const auto& [node1, idx1] = horizontalNode2w[i];
		const auto& [node2, idx2] = horizontalNode2w[i - 1];
		addRoad("城西南路", intersections[idx1], intersections[idx2]);
	}
	addRoad("城西南路", intersections[horizontalNode2w.back().second], externs[2]);
	if (horizontalNode2e.size() > 0) {
		addRoad("城东南路", intersections[2], intersections[horizontalNode2e[0].second]);
	}
	for (size_t i = 1; i < horizontalNode2e.size(); i++) {
		const auto& [node1, idx1] = horizontalNode2e[i];
		const auto& [node2, idx2] = horizontalNode2e[i - 1];
		addRoad("城东南路", intersections[idx1], intersections[idx2]);
	}
	addRoad("城东南路", intersections[horizontalNode2e.back().second], externs[3]);
	if (verticalNode1n.size() > 0) {
		addRoad("城北西路", intersections[0], intersections[verticalNode1n[0].second]);
	}
	for (size_t i = 1; i < verticalNode1n.size(); i++) {
		const auto& [node1, idx1] = verticalNode1n[i];
		const auto& [node2, idx2] = verticalNode1n[i - 1];
		addRoad("城北西路", intersections[idx1], intersections[idx2]);
	}
	addRoad("城北西路", intersections[verticalNode1n.back().second], externs[4]);
	if (verticalNode1s.size() > 0) {
		addRoad("城南西路", intersections[3], intersections[verticalNode1s[0].second]);
	}
	for (size_t i = 1; i < verticalNode1s.size(); i++) {
		const auto& [node1, idx1] = verticalNode1s[i];
		const auto& [node2, idx2] = verticalNode1s[i - 1];
		addRoad("城南西路", intersections[idx1], intersections[idx2]);
	}
	addRoad("城南西路", intersections[verticalNode1s.back().second], externs[5]);
	if (verticalNode2n.size() > 0) {
		addRoad("城北东路", intersections[1], intersections[verticalNode2n[0].second]);
	}
	for (size_t i = 1; i < verticalNode2n.size(); i++) {
		const auto& [node1, idx1] = verticalNode2n[i];
		const auto& [node2, idx2] = verticalNode2n[i - 1];
		addRoad("城北东路", intersections[idx1], intersections[idx2]);
	}
	addRoad("城北东路", intersections[verticalNode2n.back().second], externs[6]);
	if (verticalNode2s.size() > 0) {
		addRoad("城南东路", intersections[2], intersections[verticalNode2s[0].second]);
	}
	for (size_t i = 1; i < verticalNode2s.size(); i++) {
		const auto& [node1, idx1] = verticalNode2s[i];
		const auto& [node2, idx2] = verticalNode2s[i - 1];
		addRoad("城南东路", intersections[idx1], intersections[idx2]);
	}
	addRoad("城南东路", intersections[verticalNode2s.back().second], externs[7]);

	lots.emplace_back(
		Lot(northWest, northEast, southEast, southWest,
			{ roadMargin(roads[0]), roadMargin(roads[1]), roadMargin(roads[2]), roadMargin(roads[3]) }),
		unordered_map<int, Road*>{ {0, &roads[0]}, {1, &roads[1]}, {2, &roads[2]}, {3, &roads[3]} }
	);
	lots.back().first.SetArea(AREA_OFFICIAL_HIGH);

	if (horizontalNode1w.size() >= 1 && horizontalNode2w.size() >= 1) {
		Road* boundary2 = makeBoundaryRoad("城西北路", intersections[horizontalNode1w[0].second], intersections[0]);
		Road* boundary3 = makeBoundaryRoad("城西南路", intersections[horizontalNode2w[0].second], intersections[3]);
		// 找不到边界Road(这一段跨了隧道过渡点，被addRoad拆成了好几小段，见makeBoundaryRoad
		// 注释)就直接不铺这块lot——不给隧道旁边硬凑一个假边界勉强留住它。
		if (boundary2 && boundary3) {
			lots.emplace_back(Lot(horizontalNode1w[0].first, intersections[0], intersections[3], horizontalNode2w[0].first,
					{ 0.0f, roadMargin(roads[0]), roadMargin(*boundary2), roadMargin(*boundary3) }),
				unordered_map<int, Road*>{
					{ 1, &roads[0] },
					{ 2, boundary2 },
					{ 3, boundary3 }
				});
			lots.back().first.SetArea(AREA_RESIDENTIAL_HIGH);
		}
	}
	if (horizontalNode1e.size() >= 1 && horizontalNode2e.size() >= 1) {
		Road* boundary2 = makeBoundaryRoad("城东北路", intersections[horizontalNode1e[0].second], intersections[1]);
		Road* boundary3 = makeBoundaryRoad("城东南路", intersections[horizontalNode2e[0].second], intersections[2]);
		if (boundary2 && boundary3) {
			lots.emplace_back(Lot(intersections[1], horizontalNode1e[0].first, horizontalNode2e[0].first, intersections[2],
					{ roadMargin(roads[1]), 0.0f, roadMargin(*boundary2), roadMargin(*boundary3) }),
				unordered_map<int, Road*>{
					{ 0, &roads[1] },
					{ 2, boundary2 },
					{ 3, boundary3 }
				});
			lots.back().first.SetArea(AREA_RESIDENTIAL_LOW);
		}
	}
	if (verticalNode1n.size() >= 1 && verticalNode2n.size() >= 1) {
		Road* boundary0 = makeBoundaryRoad("城北西路", intersections[verticalNode1n[0].second], intersections[0]);
		Road* boundary1 = makeBoundaryRoad("城北东路", intersections[verticalNode2n[0].second], intersections[1]);
		if (boundary0 && boundary1) {
			lots.emplace_back(Lot(verticalNode1n[0].first, verticalNode2n[0].first, intersections[1], intersections[0],
					{ roadMargin(*boundary0), roadMargin(*boundary1), 0.0f, roadMargin(roads[2]) }),
				unordered_map<int, Road*>{
					{ 0, boundary0 },
					{ 1, boundary1 },
					{ 3, &roads[2] }
				});
			lots.back().first.SetArea(AREA_COMMERCIAL_HIGH);
		}
	}
	if (verticalNode1s.size() >= 1 && verticalNode2s.size() >= 1) {
		Road* boundary0 = makeBoundaryRoad("城南西路", intersections[verticalNode1s[0].second], intersections[3]);
		Road* boundary1 = makeBoundaryRoad("城南东路", intersections[verticalNode2s[0].second], intersections[2]);
		if (boundary0 && boundary1) {
			lots.emplace_back(Lot(intersections[3], intersections[2], verticalNode2s[0].first, verticalNode1s[0].first,
					{ roadMargin(*boundary0), roadMargin(*boundary1), roadMargin(roads[3]), 0.0f }),
				unordered_map<int, Road*>{
					{ 0, boundary0 },
					{ 1, boundary1 },
					{ 2, &roads[3] }
				});
			lots.back().first.SetArea(AREA_INDUSTRIAL_HIGH);
		}
	}

	// 沿每条放射臂继续按isBuildable(plain/construction)细分lot——老工程这段代码被一个
	// 无条件return挡住、从未真正跑过，这次去掉那个return，让它真正生效(详见roadnet_basic.md)。
	for (size_t i = 1; i < min(horizontalNode1w.size(), horizontalNode2w.size()); i++) {
		if (!isBuildable(horizontalNode1w[i].first.GetX(), horizontalNode1w[i].first.GetY())) break;
		if (!isBuildable(horizontalNode1w[i - 1].first.GetX(), horizontalNode1w[i - 1].first.GetY())) break;
		if (!isBuildable(horizontalNode2w[i].first.GetX(), horizontalNode2w[i].first.GetY())) break;
		if (!isBuildable(horizontalNode2w[i - 1].first.GetX(), horizontalNode2w[i - 1].first.GetY())) break;

		const auto& [nwNode, nwIdx] = horizontalNode1w[i];
		const auto& [neNode, neIdx] = horizontalNode1w[i - 1];
		const auto& [seNode, seIdx] = horizontalNode2w[i - 1];
		const auto& [swNode, swIdx] = horizontalNode2w[i];

		Road* boundary2 = makeBoundaryRoad("城西北路", intersections[nwIdx], intersections[neIdx]);
		Road* boundary3 = makeBoundaryRoad("城西南路", intersections[swIdx], intersections[seIdx]);
		// 找不到边界Road(跨了隧道过渡点)就跳过这一小块，不给它凑假边界，继续尝试链条上后面
		// 的分段(隧道段只是局部的，不代表整条链条剩下的部分也都建不出边界)。
		if (!boundary2 || !boundary3) continue;
		lots.emplace_back(Lot(nwNode, neNode, seNode, swNode, { 0.0f, 0.0f, roadMargin(*boundary2), roadMargin(*boundary3) }),
			unordered_map<int, Road*>{
				{ 2, boundary2 },
				{ 3, boundary3 }
			});
		lots.back().first.SetArea(AREA_RESIDENTIAL_LOW);
	}

	for (size_t i = 1; i < min(horizontalNode1e.size(), horizontalNode2e.size()); i++) {
		if (!isBuildable(horizontalNode1e[i].first.GetX(), horizontalNode1e[i].first.GetY())) break;
		if (!isBuildable(horizontalNode1e[i - 1].first.GetX(), horizontalNode1e[i - 1].first.GetY())) break;
		if (!isBuildable(horizontalNode2e[i].first.GetX(), horizontalNode2e[i].first.GetY())) break;
		if (!isBuildable(horizontalNode2e[i - 1].first.GetX(), horizontalNode2e[i - 1].first.GetY())) break;

		const auto& [nwNode, nwIdx] = horizontalNode1e[i - 1];
		const auto& [neNode, neIdx] = horizontalNode1e[i];
		const auto& [seNode, seIdx] = horizontalNode2e[i];
		const auto& [swNode, swIdx] = horizontalNode2e[i - 1];

		Road* boundary2 = makeBoundaryRoad("城东北路", intersections[nwIdx], intersections[neIdx]);
		Road* boundary3 = makeBoundaryRoad("城东南路", intersections[swIdx], intersections[seIdx]);
		if (!boundary2 || !boundary3) continue;
		lots.emplace_back(Lot(nwNode, neNode, seNode, swNode, { 0.0f, 0.0f, roadMargin(*boundary2), roadMargin(*boundary3) }),
			unordered_map<int, Road*>{
				{ 2, boundary2 },
				{ 3, boundary3 }
			});
		lots.back().first.SetArea(AREA_RESIDENTIAL_LOW);
	}

	for (size_t i = 1; i < min(verticalNode1n.size(), verticalNode2n.size()); i++) {
		if (!isBuildable(verticalNode1n[i].first.GetX(), verticalNode1n[i].first.GetY())) break;
		if (!isBuildable(verticalNode1n[i - 1].first.GetX(), verticalNode1n[i - 1].first.GetY())) break;
		if (!isBuildable(verticalNode2n[i].first.GetX(), verticalNode2n[i].first.GetY())) break;
		if (!isBuildable(verticalNode2n[i - 1].first.GetX(), verticalNode2n[i - 1].first.GetY())) break;

		const auto& [nwNode, nwIdx] = verticalNode1n[i];
		const auto& [neNode, neIdx] = verticalNode2n[i];
		const auto& [seNode, seIdx] = verticalNode2n[i - 1];
		const auto& [swNode, swIdx] = verticalNode1n[i - 1];

		Road* boundary0 = makeBoundaryRoad("城北西路", intersections[nwIdx], intersections[swIdx]);
		Road* boundary1 = makeBoundaryRoad("城北东路", intersections[neIdx], intersections[seIdx]);
		if (!boundary0 || !boundary1) continue;
		lots.emplace_back(Lot(nwNode, neNode, seNode, swNode, { roadMargin(*boundary0), roadMargin(*boundary1), 0.0f, 0.0f }),
			unordered_map<int, Road*>{
				{ 0, boundary0 },
				{ 1, boundary1 }
			});
		lots.back().first.SetArea(AREA_RESIDENTIAL_LOW);
	}

	for (size_t i = 1; i < min(verticalNode1s.size(), verticalNode2s.size()); i++) {
		if (!isBuildable(verticalNode1s[i].first.GetX(), verticalNode1s[i].first.GetY())) break;
		if (!isBuildable(verticalNode1s[i - 1].first.GetX(), verticalNode1s[i - 1].first.GetY())) break;
		if (!isBuildable(verticalNode2s[i].first.GetX(), verticalNode2s[i].first.GetY())) break;
		if (!isBuildable(verticalNode2s[i - 1].first.GetX(), verticalNode2s[i - 1].first.GetY())) break;

		const auto& [nwNode, nwIdx] = verticalNode1s[i - 1];
		const auto& [neNode, neIdx] = verticalNode2s[i - 1];
		const auto& [seNode, seIdx] = verticalNode2s[i];
		const auto& [swNode, swIdx] = verticalNode1s[i];

		Road* boundary0 = makeBoundaryRoad("城南西路", intersections[nwIdx], intersections[swIdx]);
		Road* boundary1 = makeBoundaryRoad("城南东路", intersections[neIdx], intersections[seIdx]);
		if (!boundary0 || !boundary1) continue;
		lots.emplace_back(Lot(nwNode, neNode, seNode, swNode, { roadMargin(*boundary0), roadMargin(*boundary1), 0.0f, 0.0f }),
			unordered_map<int, Road*>{
				{ 0, boundary0 },
				{ 1, boundary1 }
			});
		lots.back().first.SetArea(AREA_RESIDENTIAL_LOW);
	}
}
