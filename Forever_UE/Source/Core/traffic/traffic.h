#pragma once

#include "class.h"

#include "traffic/vehicle.h"
#include "traffic/vehicle_factory.h"
#include "traffic/station.h"
#include "traffic/station_factory.h"
#include "traffic/route.h"
#include "traffic/route_factory.h"

#include "common/handle.h"
#include "common/utility.h"

#include <string>
#include <unordered_map>
#include <vector>


// Traffic：按name索引持有全部当前存在的Vehicle*/Station*/Route*。Init(Map*)的五步顺序和细节见traffic.md。
// 注意：这个类和UForeverTrafficFrameworkComponent（UE层Traffic域组件，上下车的Possess
// 切换逻辑在那边，见ForeverTrafficFrameworkComponent.md）不是一回事，Traffic不知道UE
// Actor/Controller的存在，两者没有互相持有关系。
class Traffic {
	// 存档：TrafficSaveCodec直接读写私有字段(traffic_save.md)。
	friend class TrafficSaveCodec;
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

	// 取走所有线路自上次调用以来的停站/离站事件(见Route::StopEvent)，AForeverFrameworkActor在
	// Tick之后调用、转发给对应载具的script。读档后调用SilenceInitialStopEvents()让下一次Tick不补发。
	std::vector<Route::StopEvent> ConsumeStopEvents();
	void SilenceInitialStopEvents();

	// 认识add_option/remove_option(按name找Vehicle)；返回是否认识该类型，找不到目标静默忽略。
	// EnterVehicleChange是UE-only效果，不在这里处理。
	bool ApplyChange(const Change* change, const ScriptContext& context);

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
