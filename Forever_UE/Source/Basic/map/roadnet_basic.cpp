#include "roadnet_basic.h"

#include <cmath>
#include <algorithm>

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

	string meshPath = "/Game/Asset/Meshes/default_1_1.default_1_1";
	float meshUnit = 0.5f;

	vector<pair<Node, int>> horizontalNode1w;
	vector<pair<Node, int>> horizontalNode1e;
	vector<pair<Node, int>> horizontalNode2w;
	vector<pair<Node, int>> horizontalNode2e;
	vector<pair<Node, int>> verticalNode1n;
	vector<pair<Node, int>> verticalNode1s;
	vector<pair<Node, int>> verticalNode2n;
	vector<pair<Node, int>> verticalNode2s;

	// 沿给定方向延伸链条直到地图边界；这次道路高度固定0，不做隧道/地形高度判断(范围裁剪，
	// 详见roadnet_basic.md)，getTerrain只用来判断是否还在地图内。
	auto extendChain = [&](float startX, float startY, float stepX, float stepY, vector<pair<Node, int>>& chain) {
		for (float x = startX, y = startY; ; x += stepX, y += stepY) {
			string t = getTerrain(static_cast<int>(x), static_cast<int>(y));
			if (t == "") break;
			intersections.emplace_back(x, y, 0.f);
			chain.emplace_back(intersections.back(), static_cast<int>(intersections.size()) - 1);
		}
		};

	// 默认车道配置：车行道每个方向1条(宽0.5)，不设停车道，人行道每侧1条(宽0.5，紧贴车行道
	// 外侧)——所有Road统一用同一套，不区分环路/放射路。数值是用户给定的确定值，详见
	// roadnet_basic.md，不是拍脑袋的估计值，不要自行调大。
	auto configureLanes = [](Road& road) {
		road.AddVehicleLane(0, 0.5f);
		road.AddVehicleLane(1, 0.5f);
		road.AddPedestrianLane(0, 0.5f);
		road.AddPedestrianLane(1, 0.5f);
		};

	// 两端切线各贴一个控制点(1/3、2/3处，都不与真正的端点重合)：保证节点处切线水平且非零，
	// 相邻两段路在共享node上平滑接上，不会因为控制点和端点重合导致切线突然塌缩成0。
	auto addControls = [](Road& road, const Node& n1, const Node& n2) {
		float dx = n2.GetX() - n1.GetX();
		float dy = n2.GetY() - n1.GetY();
		Node c1("roadnet", n1.GetX() + dx / 3.f, n1.GetY() + dy / 3.f, 0.f);
		Node c2("roadnet", n1.GetX() + dx * 2.f / 3.f, n1.GetY() + dy * 2.f / 3.f, 0.f);
		road.AddControls({ { c1, 1.f }, { c2, 1.f } });
		};

	auto addRoad = [&](const string& name, const Node& n1, const Node& n2) {
		roads.emplace_back(name, n1, n2, meshPath, meshUnit);
		addControls(roads.back(), n1, n2);
		configureLanes(roads.back());
		};

	auto makeBoundaryRoad = [&](const string& name, const Node& n1, const Node& n2) -> Road {
		Road road(name, n1, n2, meshPath, meshUnit);
		addControls(road, n1, n2);
		configureLanes(road);
		return road;
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

	addRoad("中山西路", intersections[0], intersections[3]);
	addRoad("中山东路", intersections[1], intersections[2]);
	addRoad("中山北路", intersections[0], intersections[1]);
	addRoad("中山南路", intersections[3], intersections[2]);

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

	// 道路总半宽(车行0.5+人行0.5=1.0)决定lot margin：lot矩形边界要卡在人行道外边缘，
	// 不能压进道路横断面里，四条边margin一致(默认车道配置左右对称)，详见roadnet_basic.md。
	const float ROAD_MARGIN = 1.0f;

	lots.emplace_back(
		Lot(northWest, northEast, southEast, southWest, { ROAD_MARGIN, ROAD_MARGIN, ROAD_MARGIN, ROAD_MARGIN }),
		unordered_map<int, Road>{ {0, roads[0]}, {1, roads[1]}, {2, roads[2]}, {3, roads[3]} }
	);
	lots.back().first.SetArea(AREA_OFFICIAL_HIGH);

	if (horizontalNode1w.size() >= 1 && horizontalNode2w.size() >= 1) {
		lots.emplace_back(Lot(horizontalNode1w[0].first, intersections[0], intersections[3], horizontalNode2w[0].first, { 0.0f, ROAD_MARGIN, ROAD_MARGIN, ROAD_MARGIN }),
			unordered_map<int, Road>{
				{ 1, roads[0] },
				{ 2, makeBoundaryRoad("城西北路", intersections[horizontalNode1w[0].second], intersections[0]) },
				{ 3, makeBoundaryRoad("城西南路", intersections[horizontalNode2w[0].second], intersections[3]) }
			});
		lots.back().first.SetArea(AREA_RESIDENTIAL_HIGH);
	}
	if (horizontalNode1e.size() >= 1 && horizontalNode2e.size() >= 1) {
		lots.emplace_back(Lot(intersections[1], horizontalNode1e[0].first, horizontalNode2e[0].first, intersections[2], { ROAD_MARGIN, 0.0f, ROAD_MARGIN, ROAD_MARGIN }),
			unordered_map<int, Road>{
				{ 0, roads[1] },
				{ 2, makeBoundaryRoad("城东北路", intersections[horizontalNode1e[0].second], intersections[1]) },
				{ 3, makeBoundaryRoad("城东南路", intersections[horizontalNode2e[0].second], intersections[2]) }
			});
		lots.back().first.SetArea(AREA_RESIDENTIAL_LOW);
	}
	if (verticalNode1n.size() >= 1 && verticalNode2n.size() >= 1) {
		lots.emplace_back(Lot(verticalNode1n[0].first, verticalNode2n[0].first, intersections[1], intersections[0], { ROAD_MARGIN, ROAD_MARGIN, 0.0f, ROAD_MARGIN }),
			unordered_map<int, Road>{
				{ 0, makeBoundaryRoad("城北西路", intersections[verticalNode1n[0].second], intersections[0]) },
				{ 1, makeBoundaryRoad("城北东路", intersections[verticalNode2n[0].second], intersections[1]) },
				{ 3, roads[2] }
			});
		lots.back().first.SetArea(AREA_COMMERCIAL_HIGH);
	}
	if (verticalNode1s.size() >= 1 && verticalNode2s.size() >= 1) {
		lots.emplace_back(Lot(intersections[3], intersections[2], verticalNode2s[0].first, verticalNode1s[0].first, { ROAD_MARGIN, ROAD_MARGIN, ROAD_MARGIN, 0.0f }),
			unordered_map<int, Road>{
				{ 0, makeBoundaryRoad("城南西路", intersections[verticalNode1s[0].second], intersections[3]) },
				{ 1, makeBoundaryRoad("城南东路", intersections[verticalNode2s[0].second], intersections[2]) },
				{ 2, roads[3] }
			});
		lots.back().first.SetArea(AREA_INDUSTRIAL_HIGH);
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

		lots.emplace_back(Lot(nwNode, neNode, seNode, swNode, { 0.0f, 0.0f, ROAD_MARGIN, ROAD_MARGIN }),
			unordered_map<int, Road>{
				{ 2, makeBoundaryRoad("城西北路", intersections[nwIdx], intersections[neIdx]) },
				{ 3, makeBoundaryRoad("城西南路", intersections[swIdx], intersections[seIdx]) }
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

		lots.emplace_back(Lot(nwNode, neNode, seNode, swNode, { 0.0f, 0.0f, ROAD_MARGIN, ROAD_MARGIN }),
			unordered_map<int, Road>{
				{ 2, makeBoundaryRoad("城东北路", intersections[nwIdx], intersections[neIdx]) },
				{ 3, makeBoundaryRoad("城东南路", intersections[swIdx], intersections[seIdx]) }
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

		lots.emplace_back(Lot(nwNode, neNode, seNode, swNode, { ROAD_MARGIN, ROAD_MARGIN, 0.0f, 0.0f }),
			unordered_map<int, Road>{
				{ 0, makeBoundaryRoad("城北西路", intersections[nwIdx], intersections[swIdx]) },
				{ 1, makeBoundaryRoad("城北东路", intersections[neIdx], intersections[seIdx]) }
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

		lots.emplace_back(Lot(nwNode, neNode, seNode, swNode, { ROAD_MARGIN, ROAD_MARGIN, 0.0f, 0.0f }),
			unordered_map<int, Road>{
				{ 0, makeBoundaryRoad("城南西路", intersections[nwIdx], intersections[swIdx]) },
				{ 1, makeBoundaryRoad("城南东路", intersections[neIdx], intersections[seIdx]) }
			});
		lots.back().first.SetArea(AREA_RESIDENTIAL_LOW);
	}
}
