#include "roadnet.h"

#include "common/error.h"

#include <cmath>
#include <algorithm>

using namespace std;

namespace {
	float SumWidths(const vector<float>& lanes) {
		float sum = 0.f;
		for (float w : lanes) sum += w;
		return sum;
	}

	// 从道路中轴线到lanes[index]车道中心的偏移距离(lanes按从内到外排列，index0=最内侧)
	float LaneCenterOffset(const vector<float>& lanes, int index) {
		float offset = 0.f;
		for (int i = 0; i < index && i < static_cast<int>(lanes.size()); i++) {
			offset += lanes[i];
		}
		if (index < static_cast<int>(lanes.size())) {
			offset += lanes[index] * 0.5f;
		}
		return offset;
	}
}

RoadJunction::RoadJunction(Intersection* node, vector<RoadJunctionApproach> approaches) :
	node(node),
	approaches(std::move(approaches)) {

}

RoadJunction::~RoadJunction() {

}

Intersection* RoadJunction::GetNode() const {
	return node;
}

const vector<RoadJunctionApproach>& RoadJunction::GetApproaches() const {
	return approaches;
}

RoadJunction* RoadJunction::Build(Intersection* node, const vector<Road*>& roads, vector<Node*>& outCreatedNodes) {
	vector<RoadJunctionApproach> approaches;

	for (Road* road : roads) {
		Node start = road->GetStart();
		Node end = road->GetEnd();
		bool isStart = (start.GetId() == node->GetId());
		bool isEnd = (end.GetId() == node->GetId());
		if (!isStart && !isEnd) continue;

		RoadJunctionApproach approach;
		approach.road = road;
		approach.isStart = isStart;

		// road的Start->End方向切线(非单位向量)，isStart/isEnd都取同一个"前进方向"参考，
		// 车道side0/side1的物理位置由这个方向的右手垂线一致定义，不随取哪一端而翻转。
		float tdx, tdy, tdz;
		road->GetTangent(isStart ? 0.f : 1.f, tdx, tdy, tdz);
		float tlen = sqrt(tdx * tdx + tdy * tdy);
		if (tlen < 1e-6f) tlen = 1.f;
		float fwdX = tdx / tlen, fwdY = tdy / tlen;

		// 路口外侧方向(背离路口中心，沿道路离开路口的方向)：isStart时就是前进方向本身，
		// isEnd时是前进方向的反向(因为前进方向在End端是"驶入路口")。
		float outX = isStart ? fwdX : -fwdX;
		float outY = isStart ? fwdY : -fwdY;
		approach.angle = atan2(outY, outX);

		// side0方向 = 前进方向顺时针旋转90度(右手边)
		float perp0X = fwdY, perp0Y = -fwdX;

		float side0Width = SumWidths(road->GetVehicleLanes(0)) + SumWidths(road->GetParkingLanes(0)) + SumWidths(road->GetPedestrianLanes(0));
		float side1Width = SumWidths(road->GetVehicleLanes(1)) + SumWidths(road->GetParkingLanes(1)) + SumWidths(road->GetPedestrianLanes(1));

		// 这条路在这一端的收缩距离：取较宽一侧的宽度，让路口mesh沿这条路的方向也有一段真实
		// 进深(不再是零深度的点状扇形)，Forever层的道路tiling要用同一个值往回收缩，两者边界
		// 才能对上，见roadnet.h的setback字段注释。
		approach.setback = max(side0Width, side1Width);

		float nodeX = isStart ? start.GetX() : end.GetX();
		float nodeY = isStart ? start.GetY() : end.GetY();

		// 路缘角点/导航锚点的基准点沿outward方向外移setback距离，不再直接用Intersection原坐标——
		// 这样路口多边形对每条路都有真实的"喇叭口"进深，和收缩后的道路tiling终点重合。
		float baseX = nodeX + outX * approach.setback;
		float baseY = nodeY + outY * approach.setback;

		float side0X = baseX + perp0X * side0Width, side0Y = baseY + perp0Y * side0Width;
		float side1X = baseX - perp0X * side1Width, side1Y = baseY - perp0Y * side1Width;

		// 面朝outward方向站立时：isStart端side0在右手边，isEnd端side0在左手边(因为outward反向了)
		if (isStart) {
			approach.curbRight = { side0X, side0Y };
			approach.curbLeft = { side1X, side1Y };
		}
		else {
			approach.curbLeft = { side0X, side0Y };
			approach.curbRight = { side1X, side1Y };
		}

		auto makeAnchor = [&](float sideSign, float offsetDist, const char* category) -> Node* {
			float x = baseX + perp0X * offsetDist * sideSign;
			float y = baseY + perp0Y * offsetDist * sideSign;
			Node* n = new Node(category, x, y, 0.f);
			outCreatedNodes.push_back(n);
			return n;
			};

		// 车行锚点：side0(正向)在isStart端是outbound(车辆从路口驶出)、isEnd端是inbound(驶入路口)；side1相反。
		if (!road->GetVehicleLanes(0).empty()) {
			Node* anchor = makeAnchor(1.f, LaneCenterOffset(road->GetVehicleLanes(0), 0), "vehicle");
			if (isStart) approach.vehicleOutbound = anchor; else approach.vehicleInbound = anchor;
		}
		if (!road->GetVehicleLanes(1).empty()) {
			Node* anchor = makeAnchor(-1.f, LaneCenterOffset(road->GetVehicleLanes(1), 0), "vehicle");
			if (isStart) approach.vehicleInbound = anchor; else approach.vehicleOutbound = anchor;
		}

		// 行人锚点：只按物理侧是否存在人行道，不区分方向(双向都能走)。
		if (!road->GetPedestrianLanes(0).empty()) {
			float base0 = SumWidths(road->GetVehicleLanes(0)) + SumWidths(road->GetParkingLanes(0));
			approach.pedestrianSide[0] = makeAnchor(1.f, base0 + LaneCenterOffset(road->GetPedestrianLanes(0), 0), "pedestrian");
		}
		if (!road->GetPedestrianLanes(1).empty()) {
			float base1 = SumWidths(road->GetVehicleLanes(1)) + SumWidths(road->GetParkingLanes(1));
			approach.pedestrianSide[1] = makeAnchor(-1.f, base1 + LaneCenterOffset(road->GetPedestrianLanes(1), 0), "pedestrian");
		}

		approaches.push_back(approach);
	}

	sort(approaches.begin(), approaches.end(), [](const RoadJunctionApproach& a, const RoadJunctionApproach& b) {
		return a.angle < b.angle;
		});

	return new RoadJunction(node, std::move(approaches));
}

void RoadJunction::BuildConnectors(vector<Connection*>& outVehicle, vector<Connection*>& outPedestrian) const {
	// 车行：全联通，每个inbound连到每个outbound(含同一条路自己的inbound连自己的outbound，允许U形连接)。
	for (const auto& a : approaches) {
		if (!a.vehicleInbound) continue;
		for (const auto& b : approaches) {
			if (!b.vehicleOutbound) continue;
			outVehicle.push_back(new Connection(*a.vehicleInbound, *b.vehicleOutbound));
		}
	}

	// 行人人行横道：同一条路自己两侧都有人行道锚点才连(穿过这条路本身，正好在路口边缘)。
	for (const auto& a : approaches) {
		if (a.pedestrianSide[0] && a.pedestrianSide[1]) {
			outPedestrian.push_back(new Connection(*a.pedestrianSide[0], *a.pedestrianSide[1]));
		}
	}

	// 行人转角：夹角上相邻(按angle排序，环状含首尾)两条路，各自朝向对方那一侧的人行道锚点互连，
	// 代表沿人行道绕过这个转角、不横穿任何一条路。任一侧缺人行道就跳过这对相邻路。
	int n = static_cast<int>(approaches.size());
	if (n >= 2) {
		for (int i = 0; i < n; i++) {
			const auto& cur = approaches[i];
			const auto& next = approaches[(i + 1) % n];
			// cur朝向next(逆时针方向)的一侧：isStart时是side1(左)，isEnd时是side0(左)
			Node* curFacing = cur.isStart ? cur.pedestrianSide[1] : cur.pedestrianSide[0];
			// next朝向cur(顺时针方向)的一侧：isStart时是side0(右)，isEnd时是side1(右)
			Node* nextFacing = next.isStart ? next.pedestrianSide[0] : next.pedestrianSide[1];
			if (curFacing && nextFacing) {
				outPedestrian.push_back(new Connection(*curFacing, *nextFacing));
			}
		}
	}
}

Roadnet::Roadnet(RoadnetFactory* factory, const string& roadnetId) :
	mod(factory->CreateRoadnet(roadnetId)),
	factory(factory),
	type(),
	name() {
	if (!mod) {
		THROW_EXCEPTION(NullPointerException, "Roadnet " + roadnetId + " mod is null.\n");
	}

	type = mod->GetType();
	name = mod->GetName();
}

Roadnet::~Roadnet() {
	for (Node* n : externs) delete n;
	for (Intersection* i : intersections) delete i;
	for (Road* r : roads) delete r;
	for (auto& [lot, boundary] : lots) {
		for (auto& [dir, road] : boundary) {
			delete road;
		}
		delete lot;
	}
	factory->DestroyRoadnet(mod);
}

string Roadnet::GetType() const {
	return type;
}

string Roadnet::GetName() const {
	return name;
}

void Roadnet::DistributeRoadnet(int width, int height,
	const function<string(int, int)>& getTerrain,
	const function<pair<bool, float>(int, int)>& getWater,
	int nodeStaticCount) {
	mod->DistributeRoadnet(width, height, getTerrain, getWater, nodeStaticCount);

	for (const Node& n : mod->externs) externs.push_back(new Node(n));
	for (const Intersection& i : mod->intersections) intersections.push_back(new Intersection(i));
	for (const Road& r : mod->roads) roads.push_back(new Road(r));

	for (auto& [lot, boundary] : mod->lots) {
		unordered_map<int, Road*> boundaryCopy;
		for (auto& [dir, road] : boundary) {
			boundaryCopy[dir] = new Road(road);
		}
		lots.emplace_back(new Lot(lot), std::move(boundaryCopy));
	}
}

const vector<Node*>& Roadnet::GetExterns() const {
	return externs;
}

const vector<Intersection*>& Roadnet::GetIntersections() const {
	return intersections;
}

const vector<Road*>& Roadnet::GetRoads() const {
	return roads;
}

const vector<pair<Lot*, unordered_map<int, Road*>>>& Roadnet::GetLots() const {
	return lots;
}

void Roadnet::AllocateAddress() {
	for (auto& [lot, boundary] : lots) {
		for (int dir = 0; dir < 4; dir++) {
			auto it = boundary.find(dir);
			if (it == boundary.end() || !it->second) continue;

			Road* road = it->second;
			string roadName = road->GetName();
			int index = static_cast<int>(addressesByRoad[roadName].size());
			addressesByRoad[roadName].push_back(lot);
			lot->AddAddress(roadName, index);
		}
	}
}

Lot* Roadnet::LocateLot(const string& road, int index) const {
	auto it = addressesByRoad.find(road);
	if (it == addressesByRoad.end()) return nullptr;
	if (index < 0 || index >= static_cast<int>(it->second.size())) return nullptr;
	return it->second[index];
}
