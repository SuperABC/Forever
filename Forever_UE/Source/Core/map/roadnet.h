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

	// 车行锚点：每条物理车道各自一个，下标对应road->GetVehicleLanes(side)；空表示该端这个方向没有车道。
	// isStart时side0(正向)是outbound、side1(反向)是inbound，GetEnd()端相反，见roadnet.md"车道级导航锚点"
	std::vector<Node*> vehicleInbound;
	std::vector<Node*> vehicleOutbound;

	// 行人锚点：对应road的pedestrianLanes[0]/[1]在这一端是否存在，双向通行，不分inbound/outbound
	Node* pedestrianSide[2] = { nullptr, nullptr };

	// 路口mesh多边形用的世界坐标：面向路口外侧时的左/右路缘角点，已沿道路方向外移setback，
	// 必须和Forever层按同一距离收缩的道路tiling保持一致，见ForeverRoadnetFrameworkComponent.md
	std::pair<float, float> curbLeft;
	std::pair<float, float> curbRight;

	// curbLeft/curbRight/车行行人锚点共用的高度，沿road弧长在setback处采样(隧道场景高度连续变化)
	float curbZ = 0.f;

	// 这条路在这一端的收缩距离：直接取road->GetSetback(isStart)，由路网mod按路口实际夹角写好，
	// 见roadnet.md"路口收缩距离"
	float setback = 0.f;
};

// 路口：以一个Intersection为中心收集所有以它为端点的Road，生成锚点与路缘角点，并构建路口内部连接线，详见roadnet.md
class RoadJunction {
public:
	RoadJunction() = delete;
	RoadJunction(Intersection* node, std::vector<RoadJunctionApproach> approaches);
	~RoadJunction();

	Intersection* GetNode() const;
	const std::vector<RoadJunctionApproach>& GetApproaches() const;

	// 创建锚点Node(追加进outCreatedNodes，由Map统一释放)和路缘角点，返回新RoadJunction(调用方持有)
	static RoadJunction* Build(Intersection* node, const std::vector<Road*>& roads,
		std::vector<Node*>& outCreatedNodes);

	// 车行锚点两两全连通(inbound->outbound)；行人锚点按人行横道(同一road两侧互连)+转角连通
	// (夹角相邻两条road朝向彼此的一侧互连)生成，追加进out*（调用方持有新Connection的生命周期）。
	void BuildConnectors(std::vector<Connection*>& outVehicle, std::vector<Connection*>& outPedestrian) const;

private:
	Intersection* node;
	std::vector<RoadJunctionApproach> approaches;
};

// 持有RoadnetMod产出的路网数据的深拷贝(Node/Intersection/Road/Lot)，供Map长期使用
class Roadnet {
	// 存档：MapSaveCodec直接读写私有字段(map_save.md)。
	friend class MapSaveCodec;
public:
	Roadnet() = delete;
	Roadnet(RoadnetFactory* factory, const std::string& roadnetId);
	~Roadnet();

	std::string GetType() const;
	std::string GetName() const;

	RoadnetMod* GetMod() const;

	void DistributeRoadnet(int width, int height,
		const std::function<const std::string&(int, int)>& getTerrain,
		const std::function<std::pair<bool, float>(int, int)>& getWater,
		const std::function<float(float, float)>& getHeight,
		int nodeStaticCount);

	const std::vector<Node*>& GetExterns() const;
	const std::vector<Intersection*>& GetIntersections() const;
	const std::vector<Road*>& GetRoads() const;
	const std::vector<Lot*>& GetLots() const;

	// 地形挖洞标记(隧道口用)，值拷贝自mod->hatches
	const std::vector<TerrainHatch>& GetHatches() const;

	// 地形贴合请求，值拷贝自mod->terrainFits，由Map::ApplyTerrainFits烤进地形高度，不进存档
	const std::vector<TerrainFitRequest>& GetTerrainFits() const;

	// 树请求(参考系是整张地图)，由Map::InitVegetation交给Vegetation展开
	const TreeRequests& GetTrees() const;

	// 调mod->LayoutTrees(roads)并拷贝mod->trees，Map::InitVegetation调用一次
	void LayoutTrees();

	// 路灯请求(参考系是整张地图)，由Map::InitLamps交给Lamps展开
	const LampRequests& GetLamps() const;

	// 调mod->LayoutLamps(roads)并拷贝mod->lamps，Map::InitLamps调用一次
	void LayoutLamps();

	// Zone/Building裁剪Lot自由空间时用的小路材质路径，空表示未指定
	const std::string& GetPathRoadMaterial() const;

	// 给每个lot分配(路名,序号)地址
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
	std::vector<TerrainHatch> hatches;
	std::vector<TerrainFitRequest> terrainFits;
	TreeRequests trees;
	LampRequests lamps;
	std::string pathRoadMaterial;

	std::unordered_map<std::string, std::vector<Lot*>> addressesByRoad;
};
