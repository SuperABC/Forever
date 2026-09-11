#pragma once

#include "map/roadnet_mod.h"
#include "map/roadnet_factory.h"

#include <string>
#include <vector>
#include <utility>
#include <unordered_map>
#include <functional>

// 路口一条"进路"的车行/行人锚点+路缘角点数据，由RoadJunction::Build构造。
struct RoadJunctionApproach {
	Road* road = nullptr;

	// 该intersection是road的GetStart()(true)还是GetEnd()(false)
	bool isStart = true;

	// 该路从路口离开的方向角(atan2)，用于按夹角排序
	float angle = 0.f;

	// 车行导航锚点：每条物理车道各自一个锚点(不是只有最内侧一条)，下标对应
	// road->GetVehicleLanes(side)的车道下标，vector为空表示该端这个方向没有车道。isStart时
	// side0(正向)是outbound、side1(反向)是inbound；GetEnd()端相反——车道级锚点让每条车道在
	// 导航图/可视化上都有自己独立的一条线，不会因为共用一个锚点而和同侧别的车道重叠成一条线
	// (第九轮迁移，起因见roadnet.md"车道级导航锚点"一节)。
	std::vector<Node*> vehicleInbound;
	std::vector<Node*> vehicleOutbound;

	// 行人导航锚点：对应road的pedestrianLanes[0]/[1]在这一端是否存在，双向都能走，不分
	// inbound/outbound——这次没有像车行那样扩展成逐车道(现有场景人行道每侧固定1条，暂时
	// 没有需要验证的多车道人行道场景，见roadnet.md"车道级导航锚点"一节的范围说明)。
	Node* pedestrianSide[2] = { nullptr, nullptr };

	// 世界坐标(地图元素单位)，路口mesh多边形用：curbLeft=面向路口外侧时的左手边路缘角点，
	// curbRight=右手边，具体朝向判定见roadnet.cpp Build()实现。这两个点不只是贴着
	// Intersection原坐标横向偏移——还沿这条路离开路口的方向额外外移了setback距离，让路口
	// 多边形对每条路都有一段真实的"喇叭口"进深，不是零深度的点状扇形（早期版本没有这个纵向
	// 偏移，导致路口mesh几乎没有实际面积，和按同样距离收缩的道路tiling对不上，见
	// ForeverRoadnetFrameworkComponent.md）。
	std::pair<float, float> curbLeft;
	std::pair<float, float> curbRight;

	// curbLeft/curbRight/车行行人锚点共用的高度(地图单位)——这些点都是沿road弧长在setback
	// 对应的弧长比例处采样得到的(road->GetPoint(t))，不是Intersection原坐标的高度，因为隧道
	// 场景下沿路的高度会连续变化(见roadnet.md"路口高度"一节)。
	float curbZ = 0.f;

	// 这条路在这一端的收缩距离(地图单位)=curbLeft/curbRight沿道路方向外移的距离，取这个
	// 路口所有连接路里、总宽度(GetTotalWidth())最大的那条的一半——不是这条路自己的宽度，
	// 也不是"两侧中较宽的一侧"（车道横断面居中后，一条路两侧最外缘到Connection连线的距离
	// 永远都是它自己GetTotalWidth()的一半，不需要再分side0/side1哪个更宽，见roadnet.md
	// "路口收缩距离"一节）；取整个路口的全局最大值，是为了保证不管哪条路多宽，它的车道在
	// 路口范围内都不会被逼着穿过别的路已经开始铺设的可见路面。Forever层的道路mesh tiling
	// 要用同一个值收缩，才能让路面和路口mesh的边界严丝合缝，不留空隙也不重叠。
	float setback = 0.f;
};

// 路口：以一个Intersection为中心，收集所有以它为端点的Road，生成车行/行人锚点与路缘角点，
// 并能构建路口内部的连接线(车行全联通；行人人行横道+转角连通，不是全联通)。
// 详见Source/Core/map/roadnet.md。
class RoadJunction {
public:
	RoadJunction() = delete;
	RoadJunction(Intersection* node, std::vector<RoadJunctionApproach> approaches);
	~RoadJunction();

	Intersection* GetNode() const;
	const std::vector<RoadJunctionApproach>& GetApproaches() const;

	// 按node上所有连接的road，构建对应的锚点Node+路缘角点，返回一个新的RoadJunction实例
	// (调用方持有生命周期)。laneOffset(side, lanes, index): 从道路中轴线到某条车道中心的距离，
	// 由调用方传入(Map持有全局的锚点Node管理，方便统一释放)，本函数只负责创建Node并填进approach。
	static RoadJunction* Build(Intersection* node, const std::vector<Road*>& roads,
		std::vector<Node*>& outCreatedNodes);

	// 车行锚点两两全连通(inbound->outbound)；行人锚点按人行横道(同一road两侧互连)+转角连通
	// (夹角相邻两条road朝向彼此的一侧互连)生成，追加进out*（调用方持有新Connection的生命周期）。
	void BuildConnectors(std::vector<Connection*>& outVehicle, std::vector<Connection*>& outPedestrian) const;

private:
	Intersection* node;
	std::vector<RoadJunctionApproach> approaches;
};

// Roadnet：持有一个具体RoadnetMod实例产出的路网数据，深拷贝成自己持有的Node/Intersection/Road/Lot，
// 供Map长期使用(mod实例本身在DistributeRoadnet跑完后由调用方销毁，其值语义容器随之析构)。
class Roadnet {
public:
	Roadnet() = delete;
	Roadnet(RoadnetFactory* factory, const std::string& roadnetId);
	~Roadnet();

	std::string GetType() const;
	std::string GetName() const;

	void DistributeRoadnet(int width, int height,
		const std::function<std::string(int, int)>& getTerrain,
		const std::function<std::pair<bool, float>(int, int)>& getWater,
		int nodeStaticCount);

	const std::vector<Node*>& GetExterns() const;
	const std::vector<Intersection*>& GetIntersections() const;
	const std::vector<Road*>& GetRoads() const;
	const std::vector<Lot*>& GetLots() const;

	// mod通过RoadnetMod::AddHatch产出的地形挖洞标记(隧道口用)，纯值类型，直接拷贝自
	// mod->hatches，不需要像Node/Road那样深拷贝。
	const std::vector<std::pair<Quad, float>>& GetHatches() const;

	// Zone/Building裁剪Lot自由空间时用的小路材质路径，直接拷贝自mod->pathRoadMaterial
	// (纯字符串，不需要深拷贝)。留空表示mod没有指定，Forever层退化用RoadPlain。
	const std::string& GetPathRoadMaterial() const;

	// 遍历每个lot的边界Road映射，给lot分配(路名,序号)地址；照抄老工程Roadnet::AllocateAddress语义。
	void AllocateAddress();
	Lot* LocateLot(const std::string& road, int index) const;

private:
	RoadnetMod* mod;
	RoadnetFactory* factory;
	std::string type;
	std::string name;

	std::vector<Node*> externs;
	std::vector<Intersection*> intersections;
	std::vector<Road*> roads;
	std::vector<Lot*> lots;
	std::vector<std::pair<Quad, float>> hatches;
	std::string pathRoadMaterial;

	std::unordered_map<std::string, std::vector<Lot*>> addressesByRoad;
};
