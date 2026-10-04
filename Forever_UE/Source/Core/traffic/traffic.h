#pragma once

#include "traffic/vehicle.h"
#include "traffic/vehicle_factory.h"
#include "traffic/station.h"
#include "traffic/station_factory.h"
#include "traffic/route.h"
#include "traffic/route_factory.h"

#include "common/class.h"
#include "common/handle.h"
#include "common/utility.h"

#include <string>
#include <unordered_map>

class Map;
class ScriptFactory;
struct ScriptContext;

// Traffic：按name索引持有全部当前存在的Vehicle*/Station*/Route*。Init(Map*)按顺序做五件事
// (见traffic.md"Init"一节)：
// 1. InitStations——遍历map所有building，对BuildingMod::stationMod非空的，创建对应Station
//    (调一次StationMod::Layout())。
// 2. InitRoadsideStations——对每个已注册的StationMod类型，建一个临时实例探测
//    AssignRoads(map->GetRoads())要贴哪些路/哪一侧，立刻销毁这个临时实例，再对每条请求
//    单独创建一个真正的Station——不挂building，不占用任何Lot面积，直接摆在道路旁边
//    (目前只有公交站用这条路径)。
// 3. InitRoutes——对每个已注册且启用的RouteMod类型，收集stationType匹配的Station接口交给
//    Route::Build()，Build()内部按需创建的边缘Station交回这里持有。
// 4. 停车位车辆——只挑VehicleMod::category=="car"的车型(公交/火车/飞机不停车位，由Route
//    生成/驱动)，逻辑和之前一致。
// 5. 线路车辆——对每条Route的每条line按GetVehiclesPerLine()创建车辆、SetRouteBinding、
//    AddVehicleToLine。
//
// 注意：这个类和UForeverTrafficFrameworkComponent（UE层Traffic域组件，上下车的Possess
// 切换逻辑在那边，见ForeverTrafficFrameworkComponent.md）不是一回事，Traffic不知道UE
// Actor/Controller的存在，两者没有互相持有关系。
class Traffic {
public:
	Traffic(); // 绑定Registry::Get().GetVehicleFactory()/GetStationFactory()/GetRouteFactory()/GetScriptFactory()
	~Traffic(); // delete全部routes/stations/vehicles

	void Init(Map* map);

	// 用VehicleFactory以id创建一辆新车、按name存进vehicles(name已存在会先delete旧的)。
	// 创建失败(id未注册/未在config.json"vehicle_mods"启用)返回nullptr，不会往vehicles里塞
	// 半成品。
	Vehicle* CreateVehicle(const std::string& modId, const std::string& name);
	void DestroyVehicle(const std::string& name);
	Vehicle* FindVehicleByName(const std::string& name) const;
	const std::unordered_map<std::string, Vehicle*>& GetVehicles() const;

	Station* FindStationByName(const std::string& name) const;
	const std::unordered_map<std::string, Station*>& GetStations() const;
	const std::unordered_map<std::string, Route*>& GetRoutes() const;

	// 按Time::DifferenceInSeconds累加经过的游戏秒数(elapsedSeconds单调递增，跨天由
	// DifferenceInSeconds自己的日历感知差值处理，不需要另外补偿)，再对每条route调用
	// route->Update(elapsedSeconds)驱动线路车辆。
	void Tick(const Time& currentTime, bool crossedDay, PostHandle* post);

	// 认识AddOptionChange：按name找Vehicle、调vehicle->AddOption(option)，照抄
	// Populace::ApplyChange对应分支。EnterVehicleChange是UE-only效果(找AVehicleElement、
	// Possess)，不在这里处理，和ChangeControlChange一样直接在
	// AForeverFrameworkActor::ApplyChange里dynamic_cast转发给UE层。
	void ApplyChange(const Change* change, const ScriptContext& context);

private:
	void InitStations(Map* map);
	void InitRoadsideStations(Map* map);
	void InitRoutes(Map* map);
	void InitParkedVehicles(Map* map);
	void InitTransitVehicles();

	VehicleFactory& vehicleFactory;
	StationFactory& stationFactory;
	RouteFactory& routeFactory;
	ScriptFactory& scriptFactory;

	std::unordered_map<std::string, Vehicle*> vehicles; // 持有所有权
	std::unordered_map<std::string, Station*> stations; // 持有所有权(含Route::Build()产出的边缘站点)
	std::unordered_map<std::string, Route*> routes;     // 持有所有权

	int vehicleCounter = 0; // 生成车辆时拼唯一name用，如"Vehicle0"/"Transit0"
	int stationCounter = 0; // 生成站点时拼唯一name用，如"Station0"

	bool hasLastTickTime = false;
	Time lastTickTime;
	float elapsedSeconds = 0.f;
};
