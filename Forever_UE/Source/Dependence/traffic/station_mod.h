#pragma once

#include "common/mod_state.h"
#include "map/general_geometry.h"

#include <string>
#include <vector>


// 一段三次贝塞尔轨道(世界坐标)，带这一段用的路线资产。纯值类型，不放Node/Connection(避免跨DLL id/所有权问题)。
// 站内连线、站间连线(RouteMod::links)都用它；Core只按它建曲线，不做任何连线计算。
struct TrackSegment {
	float x1 = 0.f, y1 = 0.f, z1 = 0.f;
	float c1x = 0.f, c1y = 0.f, c1z = 0.f;
	float c2x = 0.f, c2y = 0.f, c2z = 0.f;
	float x2 = 0.f, y2 = 0.f, z2 = 0.f;
	std::string mesh;       // 这一段铺的路线资产软路径，空=不铺(飞机/公交/站内直通等)
	float meshUnit = 0.f;   // mesh沿弧长重复的单位长度(地图单位)
	// 只给mod自己的BuildTrack看(Core不读不存)：true=这一段要沿地形采样(过山抬升/挖隧道)，false=原样保留。
	bool terrainFollow = false;
};

enum class PortKind : int { Inlet = 0, Outlet = 1, Stop = 2 };

// 一个站点端口(进站口/出站口/停站位置):position用建筑占地矩形(Building自身Quad,不是楼体footprint)的局部坐标(原点左下角、未旋转,
// ratio+offset约定),headingDegrees相对建筑旋转(度),z相对地面(地图单位)。见station_mod.md。
struct StationPortSpec {
	PointParams position{};
	float headingDegrees = 0.f;
	float z = 0.f;
};

// 站内连线的一段三次贝塞尔：四个点都是占地矩形局部坐标(同StationPortSpec::position)，z相对地面。
struct StationCurveSpec {
	PointParams p1{}, c1{}, c2{}, p2{};
	float z1 = 0.f, c1z = 0.f, c2z = 0.f, z2 = 0.f;
	std::string mesh;
	float meshUnit = 0.f;
};

// 站内的一条单向连线(一串首尾相接的贝塞尔)。只允许 进站口->停站位置、停站位置->出站口、进站口->出站口(直通，不停站时走它)。
struct StationConnectionSpec {
	PortKind fromKind = PortKind::Inlet;
	int fromIndex = 0;
	PortKind toKind = PortKind::Stop;
	int toIndex = 0;
	std::vector<StationCurveSpec> curves;
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
	// 端口和站内连线：车站自己声明"哪里能停、从哪进出、站内怎么走"，停靠与否由RouteMod在线路里决定。
	std::vector<StationPortSpec> inlets;
	std::vector<StationPortSpec> outlets;
	std::vector<StationPortSpec> stops;
	std::vector<StationConnectionSpec> connections;

	// 建筑Layout()之后Core调用一次,按这栋楼的实际朝向和占地尺寸(地图单位,Quad尺寸)填上面的端口和连线。
	// 固定类型可在SetProperty()里直接填;挂道路的类型留空。
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
