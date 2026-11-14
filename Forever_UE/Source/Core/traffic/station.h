#pragma once

#include "class.h"

#include "traffic/station_mod.h"
#include "traffic/station_factory.h"

#include <string>
#include <unordered_map>
#include <vector>


// 一个站点端口(进站口/出站口/停站位置)的世界坐标信息——由StationMod端口(建筑占地矩形局部坐标)按
// Building::LocalToWorld换算而来，见traffic.md"InitStations"一节。
struct StationPort {
	float x = 0.f, y = 0.f, z = 0.f;
	float dirX = 0.f, dirY = 0.f; // 行驶方向(单位向量)
};

// 站内的一条单向连线(世界坐标)：端口之间怎么走完全是StationMod算好的，Core只按它建曲线。
struct StationConnection {
	PortKind fromKind = PortKind::Inlet;
	int fromIndex = 0;
	PortKind toKind = PortKind::Stop;
	int toIndex = 0;
	std::vector<TrackSegment> segments;
};

// 公交接入路网的旁路：laneFrom/laneTo/sourceEdge借用Map既有对象(不delete)；stationNode/
// inEdge/outEdge是Station自己新建的，Station析构时delete，不登记进Map。t是站点在sourceEdge上
// 的弧长比例，供Route判断同车道特例。详见station.md。
struct StationRoadLink {
	Node* laneFrom = nullptr;
	Node* laneTo = nullptr;
	Node* stationNode = nullptr;
	Connection* inEdge = nullptr;
	Connection* outEdge = nullptr;
	Connection* sourceEdge = nullptr;
	float t = 0.f;
};

// Station：一个上下车点的实体。两种构造场景(建筑站点/贴路站点)和"必须在建筑落地后创建"
// 的约束见station.md。
class Station {
	// 存档：TrafficSaveCodec直接读写私有字段(traffic_save.md)。
	friend class TrafficSaveCodec;
public:
	// 真实站点：factory创建modId对应的StationMod实例，调一次
	// mod->Layout(building->GetDirection(), building->GetSizeX(), building->GetSizeY())，
	// 把mod的端口/连线按Building::LocalToWorld换算成世界坐标存进inlets/outlets/stops/connections。building为空
	// 或modId未注册/未在config.json"station_mods"启用时mod为nullptr，IsValid()返回false。
	Station(StationFactory* factory, const std::string& modId, const std::string& name, Building* building);

	// 贴路站点(公交站用，见StationMod::AssignRoads)：真实站点，有mod，但不挂building——
	// 世界坐标/朝向/贴哪条路全部由调用方(Traffic::InitRoadsideStations)算好直接传入，不走
	// mod->Layout()那套"相对建筑占地矩形"的换算(没有building，没有意义)，只有一个停靠位。
	// attachedRoad记下来供EnsureRoadLink用(没有building可以读GetBoundaryRoad，只能直接用
	// 这个)。
	Station(StationFactory* factory, const std::string& modId, const std::string& name,
		float worldX, float worldY, float worldZ, float headingDegrees, Road* attachedRoad);

	~Station(); // factory->DestroyStation(mod)；delete每个RoadLink的stationNode/inEdge/outEdge

	bool IsValid() const; // mod非空
	const Road* GetAttachedRoad() const { return attachedRoad; } // 贴路站点所在的道路，挂建筑的站为nullptr
	const std::string& GetName() const;
	const std::string& GetStationType() const; // mod->stationType
	Building* GetBuilding() const; // 贴路站点返回nullptr

	const std::vector<StationPort>& GetInlets() const;
	const std::vector<StationPort>& GetOutlets() const;
	const std::vector<StationPort>& GetStops() const;
	const std::vector<StationConnection>& GetConnections() const;

	// 按stopIndex查/建这个停靠位的公交旁路——有building的话用building->GetBoundaryRoad
	// (building->GetDirection())找边界Road，没有building(贴路站点)直接用attachedRoad；再用
	// stops[stopIndex]的位置+方向调map->GetLaneSegmentAt()查车道段(只读查询，
	// 不修改Map状态)。已经建过的stopIndex直接返回缓存；两种方式都没有可用的Road/查不到
	// 车道时返回nullptr。
	const StationRoadLink* EnsureRoadLink(Map* map, int stopIndex);
	const StationRoadLink* GetRoadLink(int stopIndex) const; // 不存在返回nullptr

private:
	StationFactory* factory = nullptr;
	StationMod* mod = nullptr;
	std::string name;
	Building* building = nullptr;
	Road* attachedRoad = nullptr; // 贴路站点专用，没有building时EnsureRoadLink靠这个找路
	Map* linkedMap = nullptr; // EnsureRoadLink时登记过辅助导航节点的Map，~Station据此注销（Traffic总是先于Map销毁）

	std::vector<StationPort> inlets, outlets, stops;
	std::vector<StationConnection> connections;
	std::unordered_map<int, StationRoadLink> roadLinks; // stopIndex -> 旁路，惰性构造
};
