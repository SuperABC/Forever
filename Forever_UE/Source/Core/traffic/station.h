#pragma once

#include "class.h"

#include "traffic/station_mod.h"
#include "traffic/station_factory.h"

#include <string>
#include <unordered_map>
#include <vector>


// 一个站点接口的世界坐标信息——由StationMod::interfaces(建筑占地矩形局部坐标)按
// Building::LocalToWorld换算而来，见traffic.md"InitStations"一节。
struct StationInterface {
	float x = 0.f, y = 0.f, z = 0.f;
	float arriveDirX = 0.f, arriveDirY = 0.f;
	float departDirX = 0.f, departDirY = 0.f;
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

// Station：一个上下车点的实体。三种构造场景(建筑站点/边缘站点/贴路站点)和"必须在建筑落地后创建"
// 的约束见station.md。
class Station {
	// 存档：TrafficSaveCodec直接读写私有字段(traffic_save.md)。
	friend class TrafficSaveCodec;
public:
	// 真实站点：factory创建modId对应的StationMod实例，调一次
	// mod->Layout(building->GetDirection(), building->GetSizeX(), building->GetSizeY())，
	// 把mod->interfaces按Building::LocalToWorld换算成世界坐标存进interfaces。building为空
	// 或modId未注册/未在config.json"station_mods"启用时mod为nullptr，IsValid()返回false。
	Station(StationFactory* factory, const std::string& modId, const std::string& name, Building* building);

	// 边缘站点：只有一个世界坐标接口，没有building/mod。
	Station(const std::string& name, float x, float y, float z,
		float arriveDirX, float arriveDirY, float departDirX, float departDirY);

	// 贴路站点(公交站用，见StationMod::AssignRoads)：真实站点，有mod，但不挂building——
	// 世界坐标/朝向/贴哪条路全部由调用方(Traffic::InitRoadsideStations)算好直接传入，不走
	// mod->Layout()/mod->interfaces那套"相对建筑占地矩形"的换算(没有building，没有意义)。
	// attachedRoad记下来供EnsureRoadLink用(没有building可以读GetBoundaryRoad，只能直接用
	// 这个)。
	Station(StationFactory* factory, const std::string& modId, const std::string& name,
		float worldX, float worldY, float worldZ, float headingDegrees, Road* attachedRoad);

	~Station(); // factory->DestroyStation(mod)(真实站点才有)；delete每个RoadLink的stationNode/inEdge/outEdge

	bool IsValid() const; // 真实站点mod非空；边缘站点恒true
	const std::string& GetName() const;
	const std::string& GetStationType() const; // mod->stationType，边缘站点返回空字符串
	bool IsEdge() const;
	Building* GetBuilding() const; // 边缘站点返回nullptr

	const std::vector<StationInterface>& GetInterfaces() const;

	// 按interfaceIndex查/建这条接口的公交旁路——有building的话用building->GetBoundaryRoad
	// (building->GetDirection())找边界Road，没有building(贴路站点)直接用attachedRoad；再用
	// interfaces[interfaceIndex]的位置+departDir调map->GetLaneSegmentAt()查车道段(只读查询，
	// 不修改Map状态)。已经建过的interfaceIndex直接返回缓存；两种方式都没有可用的Road/查不到
	// 车道时返回nullptr。
	const StationRoadLink* EnsureRoadLink(Map* map, int interfaceIndex);
	const StationRoadLink* GetRoadLink(int interfaceIndex) const; // 不存在返回nullptr

private:
	StationFactory* factory = nullptr;
	StationMod* mod = nullptr;
	std::string name;
	Building* building = nullptr;
	Road* attachedRoad = nullptr; // 贴路站点专用，没有building时EnsureRoadLink靠这个找路
	Map* linkedMap = nullptr; // EnsureRoadLink时登记过辅助导航节点的Map，~Station据此注销（Traffic总是先于Map销毁）
	bool edge = false;

	std::vector<StationInterface> interfaces;
	std::unordered_map<int, StationRoadLink> roadLinks; // interfaceIndex -> 旁路，惰性构造
};
