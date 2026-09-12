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

		// 这条路的车道横断面现在以Connection连线为几何中心居中(见roadnet.md"车道居中"一节)，
		// 不管side0/side1怎么分配，两侧最外缘到连线的距离永远都是GetTotalWidth()/2——所以
		// 路口收缩距离只需要取"整个总宽度的一半"，不再需要side0Width/side1Width的较大值。
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

		// side0方向 = 前进方向顺时针旋转90度(右手边)
		float perp0X = p.fwdY, perp0Y = -p.fwdX;

		// 车道横断面重新居中(见下面side0X/side1X的注释)后，这条路自己的中心偏移量：
		// side0比side1宽多少，连线就要比side0Width少这么多、比side1Width多这么多，才能让
		// 两侧最外缘到连线的距离相等(都等于GetTotalWidth()/2)。shift=0时(两侧对称，默认车道
		// 配置就是这样)，下面的计算和居中之前完全等价，不会有任何回归。
		float shift = (side0Width - side1Width) * 0.5f;

		// 这条路在这一端的收缩距离：不是这条路自己两侧宽度的较大值，而是**整个路口**所有
		// 连接路里最宽的一侧(globalSetback)——原因见roadnet.md"路口收缩距离"一节：如果只按
		// 自己两侧算，窄路retreat得不够深，宽路那一侧真正需要贯穿路口的车道会被逼着从窄路
		// 已经开始铺的可见路面上穿过去(车道逻辑上"认错主人")，哪怕两块mesh本身没有空间上
		// 重叠也是错的。Forever层的道路tiling要用同一个值往回收缩，两者边界才能对上，见
		// roadnet.h的setback字段注释。
		approach.setback = globalSetback;

		// 路缘角点/导航锚点的基准点是沿road弧长、离端点setback距离处的真实曲线坐标
		// (road->GetPoint)，不是"Intersection原坐标+直线外移"的近似——弧长比例算法和Forever层
		// BuildRoadInstances算tLow/tHigh用的是同一个clamp(setback/totalLen, 0, 0.45)，两边
		// 采样到的必然是曲线上同一个点，路口mesh的边界和可见路面的起点因此严丝合缝，不会有
		// 高度断层(隧道场景下curve高度沿途连续变化，直线近似会漏掉这段变化，见roadnet.md
		// "路口高度"一节)。
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
		float sperp0X = sfwdY, sperp0Y = -sfwdX;

		// 车道横断面以Connection连线为几何中心居中：side0这一侧最外缘不再是"离连线side0Width
		// 远"，而是"离连线(side0Width-shift)远"(side1同理，方向相反、减去(side1Width+shift))——
		// 两侧宽度对称时shift=0，退化成居中之前的公式；单行道等side0/side1严重不对称时，
		// 两侧最外缘到连线的距离都精确等于GetTotalWidth()/2(可以代入验证：
		// (side0Width-shift) = (side1Width+shift) = (side0Width+side1Width)/2)，不会出现
		// "单行道所有车道都堆在连线一侧、路口形状被撑得很怪"的问题，见roadnet.md"车道居中"一节。
		float side0X = baseX + sperp0X * (side0Width - shift), side0Y = baseY + sperp0Y * (side0Width - shift);
		float side1X = baseX - sperp0X * (side1Width + shift), side1Y = baseY - sperp0Y * (side1Width + shift);

		// 面朝outward方向站立时：isStart端side0在右手边，isEnd端side0在左手边(因为outward反向了)
		if (isStart) {
			approach.curbRight = { side0X, side0Y };
			approach.curbLeft = { side1X, side1Y };
		}
		else {
			approach.curbLeft = { side0X, side0Y };
			approach.curbRight = { side1X, side1Y };
		}

		// offsetDist*sideSign是"以老的side0/side1分界线为原点"算出来的有符号偏移(side0方向为正)，
		// 统一减去shift就换算成"以居中后的连线为原点"的偏移——和上面side0X/side1X是同一个换算，
		// 只是这里offsetDist可以是任意一条车道的中心(不止是最外缘)。
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
	// 车行：全联通，每条inbound车道各自的锚点连到每条outbound车道各自的锚点(含同一条路自己的
	// inbound连自己的outbound，允许U形连接)——车道级锚点后，这个全联通是车道对车道的叉乘，
	// 不再是approach对approach，路口连接线数量因此比按approach算的时候更多，是逐车道锚点
	// 化必然的代价，见roadnet.md"车道级导航锚点"一节。
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

	// 一个Lot的boundary Road现在可能和roads里的某条是同一个对象(DistributeRoadnet按
	// RoadIdentityKey匹配到就会直接共用指针，不再各自独立拷贝一份，见DistributeRoadnet注释)，
	// 也可能是独立的一份(没匹配上时的兜底拷贝)——统一收集去重再delete一次，不按"来源"分两次
	// 遍历删，避免共用的那部分被delete两次(double free)。
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

void Roadnet::DistributeRoadnet(int width, int height,
	const function<string(int, int)>& getTerrain,
	const function<pair<bool, float>(int, int)>& getWater,
	int nodeStaticCount) {
	mod->DistributeRoadnet(width, height, getTerrain, getWater, nodeStaticCount);

	for (const Node& n : mod->externs) externs.push_back(new Node(n));
	for (const Intersection& i : mod->intersections) intersections.push_back(new Intersection(i));

	// mod->roads是"这张路网全部真实道路"唯一的一份数据来源，逐个new成Roadnet自己拥有的副本；
	// mod->lots里每个lot的边界Road*(RoadnetMod::lots的类型，见roadnet_mod.h的注释)必须指向
	// mod->roads里的同一个元素，这里按下标一一对应，翻译成"mod的地址 -> Roadnet自己副本的
	// 地址"这份映射，好让lot的边界指针跟着换成指向Roadnet自己的副本，而不是mod侧那份（mod
	// 随时可能在DistributeRoadnet返回后的某个时机被factory销毁）。这样Lot的边界Road*和
	// GetRoads()里的Road*从此是同一个对象——Zone/Building裁剪Lot空间时(Lot::SplitWithPath)
	// 真的在边界Road上加RoadOpening标记开口，改的就是这个对象本身，Forever层渲染时读
	// GetRoads()能看到同一份修改，不会出现"开口加了但画不出来"（因为以前边界Road是独立拷贝，
	// 见map.md/roadnet.md）。
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
			// 正常情况下一定能找到——mod的边界指针本来就要求指向mod->roads里的元素（接口注释
			// 明确写了这个约束）；找不到说明mod实现违反了这个约束，兜底独立拷贝一份，不让整个
			// 流程崩掉，但这种情况理论上不应该出现。
			Road* boundaryRoad = (it != roadPtrMap.end()) ? it->second : new Road(*roadPtr);
			newLot->SetBoundaryRoad(dir, boundaryRoad);
		}
		lots.push_back(newLot);
	}

	// Quad+float是纯值类型，不含所有权指针，直接整体拷贝，不需要像上面几个逐个new。
	hatches = mod->hatches;
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

const vector<pair<Quad, float>>& Roadnet::GetHatches() const {
	return hatches;
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
