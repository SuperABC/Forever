#pragma once

#include "common/mod_state.h"
#include "map/general_geometry.h"

#include <string>
#include <vector>


// 一个站点接口:position用建筑占地矩形(Building自身Quad,不是楼体footprint)的局部坐标(原点左下角、未旋转,
// ratio+offset约定),headingDegrees相对建筑旋转(度),z相对地面(地图单位)。见station_mod.md。
struct StationInterfaceSpec {
	PointParams position{};
	float headingDegrees = 0.f;
	float z = 0.f;
};

// 不挂建筑、直接贴着道路摆的站点请求(AssignRoads用):road+沿弧长比例t+rightSide(沿Start->End的右手边)。
// Core负责换算成世界坐标+朝向。
struct RoadStationRequest {
	Road* road = nullptr;
	float t = 0.5f;
	bool rightSide = true;
};

// AssignRoads()交回结果的回调(裸函数指针,体编译在调用方一侧,跨DLL安全),同PlacementEmitFunc。
using RoadStationEmitFunc = void(*)(void* context, const RoadStationRequest& request);

// 设计见station_mod.md。stationType是通用交通类别词("bus"/"train"/"plane"),不是mod名,RouteMod按它匹配。
class StationMod {
public:
	StationMod() = default;
	virtual ~StationMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// 子类在这里填stationType(不在构造函数里;AssignRoads是static,不能在那里设),Core创建实例后立刻调一次。
	virtual void SetProperty() {}

	std::string stationType;
	std::vector<StationInterfaceSpec> interfaces;

	// 建筑Layout()之后Core调用一次,按这栋楼的实际朝向和占地尺寸(地图单位,Quad尺寸)填interfaces。
	// 固定接口的类型可在SetProperty()里直接填;挂道路的类型留空。
	virtual void Layout(int direction, float sizeX, float sizeY) {}

	// 挂道路的站点类型还须实现static void AssignRoads(const std::vector<Road*>&, RoadStationEmitFunc, void*)
	// (基类不声明,通过RegisterStation注册),不需要的给空实现,见station_mod.md。

	// ---- 存档(见common/mod_state.md) ----
	virtual void SaveState(StateWriter&) const {}
	virtual void LoadState(StateReader&) {}
	virtual int GetStateVersion() const { return 0; }
	// GetName用的static count,存档保存/读档恢复;没有计数器的类保持默认。
	virtual int GetInstanceCounter() const { return -1; }
	virtual void SetInstanceCounter(int) {}
};
