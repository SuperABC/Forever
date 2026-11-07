#include "roadnet.h"

#include "common/error.h"

#include <cmath>
#include <algorithm>
#include <unordered_set>


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
	// 先扫一遍这个路口连着的所有路，记下每条路自己的Start->End切线/outward方向/两侧宽度，
	// 顺便求出这个路口全局最宽的一侧(globalSetback)。这一步不生成任何几何，只收集数据。
	struct PendingApproach {
		Road* road;
		bool isStart;
		float fwdX, fwdY;
		float outX, outY;
		float side0Width, side1Width;
	};
	vector<PendingApproach> pending;
	float globalSetback = 0.f;

	for (Road* road : roads) {
		Node start = road->GetStart();
		Node end = road->GetEnd();
		bool isStart = (start.GetId() == node->GetId());
		bool isEnd = (end.GetId() == node->GetId());
		if (!isStart && !isEnd) continue;

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

		float side0Width = road->GetSideWidth(0);
		float side1Width = road->GetSideWidth(1);

		// 车道横断面以连线居中，两侧最外缘到连线都是GetTotalWidth()/2，路口收缩距离取其一半。
		globalSetback = max(globalSetback, road->GetTotalWidth() * 0.5f);
		pending.push_back({ road, isStart, fwdX, fwdY, outX, outY, side0Width, side1Width });
	}

	vector<RoadJunctionApproach> approaches;

	for (const PendingApproach& p : pending) {
		Road* road = p.road;
		bool isStart = p.isStart;
		float side0Width = p.side0Width;
		float side1Width = p.side1Width;

		RoadJunctionApproach approach;
		approach.road = road;
		approach.isStart = isStart;
		approach.angle = atan2(p.outY, p.outX);

		// side0方向=前进方向的右手边。+Y=南，右手边公式是(-fwdY,fwdX)而不是(fwdY,-fwdX)，见
		// Map::ComputeLaneAnchorPosition声明处。下面用采样点处的切线算sperp0X/Y。

		// 居中偏移：side0比side1宽多少，连线就要比side0Width少这么多(比side1Width多这么多)；
		// 两侧对称时shift=0，见roadnet.md"车道居中"。
		float shift = (side0Width - side1Width) * 0.5f;

		// 收缩距离取整个路口的globalSetback而不是本路自己两侧宽度，见roadnet.md"路口收缩距离"；
		// Forever层道路tiling用同一个值回缩。
		approach.setback = globalSetback;

		// 基准点取沿road弧长离端点setback处的真实曲线坐标(road->GetPoint)；tFrac的clamp与Forever层
		// BuildRoadInstances算tLow/tHigh完全相同，路口mesh边界才能和可见路面起点严丝合缝(含高度)，
		// 见roadnet.md"路口高度"。
		float totalLen = road->CalcDistance();
		float tFrac = 0.f;
		if (totalLen > 1e-6f) {
			tFrac = approach.setback / totalLen;
			tFrac = min(max(tFrac, 0.f), 0.45f);
		}
		float sampleT = isStart ? tFrac : (1.f - tFrac);
		Node samplePoint = road->GetPoint(sampleT);
		float baseX = samplePoint.GetX();
		float baseY = samplePoint.GetY();
		float baseZ = samplePoint.GetZ();
		approach.curbZ = baseZ;

		// 在采样点(而不是端点)处重新取切线定lateral方向，和baseX/baseY/baseZ用的是同一个
		// sampleT，几何上完全一致。
		float sdx, sdy, sdz;
		road->GetTangent(sampleT, sdx, sdy, sdz);
		float slen = sqrt(sdx * sdx + sdy * sdy);
		if (slen < 1e-6f) slen = 1.f;
		float sfwdX = sdx / slen, sfwdY = sdy / slen;
		// 右手边修正，见上面perp0X/Y处的说明：+Y=南，公式是(-sfwdY,sfwdX)。
		float sperp0X = -sfwdY, sperp0Y = sfwdX;

		// 车道横断面以连线为几何中心居中：两侧最外缘到连线的距离都是GetTotalWidth()/2，见
		// roadnet.md"车道居中"。
		float side0X = baseX + sperp0X * (side0Width - shift), side0Y = baseY + sperp0Y * (side0Width - shift);
		float side1X = baseX - sperp0X * (side1Width + shift), side1Y = baseY - sperp0Y * (side1Width + shift);

		// curbRight/curbLeft只被BuildJunctionMeshes()按[curbRight_0,curbLeft_0,...]的固定顺序铺扇形网格，
		// 名字不对应真实左右手；约束是两点顺序必须保持该网格算法验证过的顺序——isStart端side1给
		// curbRight，反过来扇形三角化会在每个路口开口处拧一下。
		if (isStart) {
			approach.curbRight = { side1X, side1Y };
			approach.curbLeft = { side0X, side0Y };
		}
		else {
			approach.curbLeft = { side1X, side1Y };
			approach.curbRight = { side0X, side0Y };
		}

		// offsetDist*sideSign是以side0/side1分界线为原点的有符号偏移(side0为正)，减去shift换算成以居中
		// 后的连线为原点，见roadnet.md"车道居中"。
		auto makeAnchor = [&](float sideSign, float offsetDist, const char* category) -> Node* {
			float signedOffset = offsetDist * sideSign - shift;
			float x = baseX + sperp0X * signedOffset;
			float y = baseY + sperp0Y * signedOffset;
			Node* n = new Node(category, x, y, baseZ);
			outCreatedNodes.push_back(n);
			return n;
			};

		// 车行锚点：side0(正向)在isStart端是outbound(车辆从路口驶出)、isEnd端是inbound(驶入路口)；
		// side1相反。每条车道(不止最内侧)各自生成一个锚点，下标对应车道数组下标。
		{
			const vector<float>& lanes0 = road->GetVehicleLanes(0);
			vector<Node*> anchors0;
			for (int i = 0; i < static_cast<int>(lanes0.size()); i++) {
				anchors0.push_back(makeAnchor(1.f, LaneCenterOffset(lanes0, i), "vehicle"));
			}
			if (isStart) approach.vehicleOutbound = std::move(anchors0); else approach.vehicleInbound = std::move(anchors0);
		}
		{
			const vector<float>& lanes1 = road->GetVehicleLanes(1);
			vector<Node*> anchors1;
			for (int i = 0; i < static_cast<int>(lanes1.size()); i++) {
				anchors1.push_back(makeAnchor(-1.f, LaneCenterOffset(lanes1, i), "vehicle"));
			}
			if (isStart) approach.vehicleInbound = std::move(anchors1); else approach.vehicleOutbound = std::move(anchors1);
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
	// 车行：每条inbound车道锚点连到每条outbound车道锚点(含同一条路自己的，允许U形)，车道对车道
	// 叉乘，见roadnet.md"车行锚点全联通"。
	for (const auto& a : approaches) {
		for (Node* inAnchor : a.vehicleInbound) {
			for (const auto& b : approaches) {
				for (Node* outAnchor : b.vehicleOutbound) {
					outVehicle.push_back(new Connection(*inAnchor, *outAnchor));
				}
			}
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
			// angle升序，next在cur的角度增大(out逆时针)一侧。side0偏移方向sperp0=(-fwdY,fwdX)：isStart时
			// fwd==out，side0在out逆时针侧；isEnd时fwd==-out，side0在顺时针侧。
			// cur朝向next的一侧：isStart取side0，isEnd取side1
			Node* curFacing = cur.isStart ? cur.pedestrianSide[0] : cur.pedestrianSide[1];
			// next朝向cur(角度减小/out顺时针)的一侧：isStart时是side1，isEnd时是side0
			Node* nextFacing = next.isStart ? next.pedestrianSide[1] : next.pedestrianSide[0];
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

	// Lot的边界Road可能和roads里的是同一个对象(DistributeRoadnet按映射共用)，也可能是兜底的独立
	// 拷贝；统一去重后delete一次，避免共用部分double free。
	unordered_set<Road*> uniqueRoads(roads.begin(), roads.end());
	for (Lot* lot : lots) {
		for (auto& [dir, road] : lot->GetBoundaryRoads()) {
			uniqueRoads.insert(road);
		}
	}
	for (Road* r : uniqueRoads) delete r;
	for (Lot* lot : lots) delete lot;

	factory->DestroyRoadnet(mod);
}

string Roadnet::GetType() const {
	return type;
}

string Roadnet::GetName() const {
	return name;
}

RoadnetMod* Roadnet::GetMod() const {
	return mod;
}

void Roadnet::DistributeRoadnet(int width, int height,
	const function<const string&(int, int)>& getTerrain,
	const function<pair<bool, float>(int, int)>& getWater,
	const function<float(float, float)>& getHeight,
	int nodeStaticCount) {
	mod->DistributeRoadnet(width, height, getTerrain, getWater, getHeight, nodeStaticCount);

	for (const Node& n : mod->externs) externs.push_back(new Node(n));
	for (const Intersection& i : mod->intersections) intersections.push_back(new Intersection(i));

	// mod->roads是全部真实道路的唯一数据源，逐个new成Roadnet自己的副本；lot的边界Road*必须指向
	// mod->roads里的元素(roadnet_mod.h约定)，这里建"mod地址->副本地址"映射把它们翻译成指向副本，
	// 使边界Road和GetRoads()里的是同一个对象，见roadnet.md。
	unordered_map<const Road*, Road*> roadPtrMap;
	for (size_t i = 0; i < mod->roads.size(); i++) {
		Road* copy = new Road(mod->roads[i]);
		roads.push_back(copy);
		roadPtrMap[&mod->roads[i]] = copy;
	}

	for (Lot& lot : mod->lots) {
		Lot* newLot = new Lot(lot);
		for (const auto& [dir, roadPtr] : lot.GetBoundaryRoads()) {
			auto it = roadPtrMap.find(roadPtr);
			// 正常一定能找到(roadnet_mod.h约定边界指针指向mod->roads元素)；找不到说明mod违约，兜底独立
			// 拷贝一份避免崩溃。
			Road* boundaryRoad = (it != roadPtrMap.end()) ? it->second : new Road(*roadPtr);
			newLot->SetBoundaryRoad(dir, boundaryRoad);
		}
		lots.push_back(newLot);
	}

	// Quad+float是纯值类型，不含所有权指针，直接整体拷贝，不需要像上面几个逐个new。
	hatches = mod->hatches;
	terrainFits = mod->terrainFits;
	pathRoadMaterial = mod->pathRoadMaterial;
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

const vector<Lot*>& Roadnet::GetLots() const {
	return lots;
}

const vector<TerrainHatch>& Roadnet::GetHatches() const {
	return hatches;
}

const vector<TerrainFitRequest>& Roadnet::GetTerrainFits() const {
	return terrainFits;
}

const TreeRequests& Roadnet::GetTrees() const {
	return trees;
}

void Roadnet::LayoutTrees() {
	mod->LayoutTrees(roads);
	trees = mod->trees;
}

const LampRequests& Roadnet::GetLamps() const {
	return lamps;
}

void Roadnet::LayoutLamps() {
	mod->LayoutLamps(roads);
	lamps = mod->lamps;
}

const string& Roadnet::GetPathRoadMaterial() const {
	return pathRoadMaterial;
}

void Roadnet::AllocateAddress() {
	for (Lot* lot : lots) {
		for (int dir = 0; dir < 4; dir++) {
			Road* road = lot->GetBoundaryRoad(dir);
			if (!road) continue;

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
