#pragma once

#include "common/mod_state.h"
#include "map/general_geometry.h"

#include <string>
#include <vector>
#include <functional>


// Core→mod的只读站点接口信息，世界坐标(地图单位)——RouteMod::LayoutRoute()收到的参数，
// 包含所有stationType匹配的真实站点接口，以及本次调用之前其它Route已经请求创建的边缘站点
// (isEdge=true)。mod只读，不持有所有权。
struct RouteStationInfo {
	std::string name;
	float x = 0.f, y = 0.f, z = 0.f;
	float arriveDirX = 0.f, arriveDirY = 0.f;
	float departDirX = 0.f, departDirY = 0.f;
	bool isEdge = false;
	// 这个接口在Core侧站点列表里的下标(真实站点在前，edgeStations拼接在后)，RouteStop用这个
	// 下标引用站点，interfaceIndex是这个站点自己第几个接口(同一个站点可能有多个接口，比如
	// 火车站两条站台)。
	int stationIndex = -1;
	int interfaceIndex = 0;
};

// 线路上的一站——station是RouteStationInfo列表(真实站点+edgeStations拼接后)的下标。dwell
// 为false表示这一站是纯过路点(到达后不停留，直接接着跑下一腿)，默认true保持老行为——公交
// 每一站都要停，但火车/飞机的入站/出站接口、地图边缘点都不应该停，只有站内真正的停靠点
// (比如跑道/轨道中点)才要停，见route_basic.cpp LayoutDualTrackStationLoop的说明。
struct RouteStop {
	int station = -1;
	int interfaceIndex = 0;
	bool dwell = true;
	// dwell=true时有效：停靠但不触发进站/离站事件(enter_stop/leave_stop)——地图边缘的虚拟车站用，
	// 车会在那里停，但不是玩家能感知的真实车站。
	bool silent = false;
};

// BuildTrack的输入：links[i]两端实际落在哪两个站点接口上(世界坐标，z是接口高度)，Core按
// RouteMod::links的顺序填，下标和输出trackEdges一一对应。
struct RouteTrackLinkEnds {
	RouteStationInfo from;
	RouteStationInfo to;
};

// BuildTrack的输出：一条link的一段轨道几何，三次贝塞尔(起点、两个控制点、终点)，世界坐标。
// 纯值类型——不放Node/Connection，避免跨DLL的Node id/所有权问题(同RoadnetMod::terrainFits的
// 理由)，Core收到后用自己的Node重新构造Connection。tunnel=true表示这段在隧道里，UE侧换用
// trackTunnelMesh。
struct TrackSegment {
	float x1 = 0.f, y1 = 0.f, z1 = 0.f;
	float c1x = 0.f, c1y = 0.f, c1z = 0.f;
	float c2x = 0.f, c2y = 0.f, c2z = 0.f;
	float x2 = 0.f, y2 = 0.f, z2 = 0.f;
	bool tunnel = false;
};

// 一条link的轨道：segments按行驶顺序首尾相接。
struct TrackEdgeGeometry {
	std::vector<TrackSegment> segments;
};

// 有向图的一条边——bidirectional=true表示同一份几何两个方向都能走(比如单线铁路)，Core建图
// 时会额外登记一条反向边，倒序遍历同一份segments、切线取反，不复制几何，见
// Core/traffic/route.md"有向图与双向边"一节。
struct RouteLink {
	RouteStop from;
	RouteStop to;
	bool bidirectional = false;
};

// RouteMod：一类线路的连接逻辑(公交/火车/飞机各一个具体子类)。负责把Core收集到的站点接口
// 组织成有向图+若干条环线，Core只管建几何、算时刻表、生成/驱动载具，不关心具体怎么连。
class RouteMod {
public:
	RouteMod() = default;
	virtual ~RouteMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// 具体子类必须在这里填下面这组选项字段，不要在构造函数里赋值——构造函数只负责
	// id/count这类登记，和StorageMod::SetProperty()同一套"两段式"约定。Core创建完mod
	// 实例后会立刻调一次这个方法，再读这些字段。
	virtual void SetProperty() {}

	// 选项——子类SetProperty()里填好，Core只读。
	std::string stationType;    // 接哪类站点，见station_mod.h
	std::string vehicleType;    // 这条线路生成的VehicleMod id
	bool useRoadnet = false;    // 是否接入现有车辆路网(旁路，不破坏原图，见route.md)
	bool instantiate = false;   // 是否实例化路线(沿线铺真实轨道mesh、过山爬坡/开隧道)：火车true，公交/飞机false
	std::string trackMesh;      // 轨道资产软路径，instantiate时用，留空则不画
	std::string trackTunnelMesh;// 隧道内的轨道资产，留空=复用trackMesh
	float trackUnit = 0.f;      // 轨道mesh沿弧长重复的单位长度(地图单位)
	float trackWidth = 0.f;     // 轨道占地整宽(地图单位)，挖隧道洞/地形贴合用
	int vehiclesPerLine = 1;    // 每条线路生成几辆车，均匀分布相位
	float speed = 1.f;          // 地图单位/秒
	float dwellSeconds = 30.f;  // 到站停留秒数
	float controlLength = 3.f;  // 非路网边贝塞尔控制臂长度(地图单位)

	// 进站前这么久(真实秒)开始减速到0，出站后这么久之内从0加速到巡航速度——
	// Route::DriveVehicle()按"行驶区段"(两次真正停靠之间首尾相接的若干条leg)整体套梯形
	// 速度曲线用到的缓动时长，不是每条leg自己的[0,1]，见route.md"DriveVehicle的缓动"
	// 一节。火车/飞机这类载具大、巡航距离长，缓动给大一点才符合"缓慢启动和刹车"的实际手感；
	// 公交站间距短，缓动给太大会占掉一大截本该匀速的路程，默认值按公交的尺度留着。
	float easeSeconds = 3.f;

	// 组织线路——interfaces是所有stationType匹配的真实站点接口(世界坐标)；sizeX/sizeY是地图
	// 整体尺寸(地图单位)，无站点线路(比如只有一个站点、两头接地图边缘的火车/飞机线)用来算
	// 边缘站点位置。调用后读取下面三个输出字段。
	virtual void LayoutRoute(const std::vector<RouteStationInfo>& interfaces, int sizeX, int sizeY) = 0;

	// 铺轨——只对instantiate的线路调用，在LayoutRoute之后、Core构建边几何之前。linkEnds[i]是links[i]
	// 两端的真实接口位置；getTerrain/getWater/getHeight同RoadnetMod::DistributeRoadnet，采样当前
	// 地形。mod自己沿线采样、判断过山是抬升还是挖隧道(做法同Roadnet)，把分段几何填进
	// trackEdges(下标和links一一对应，某条留空=沿用Core的单段贝塞尔)，隧道口/抬升段用下面的
	// AddTunnelHatch/AddTerrainFit提交请求，Core会转发给Map。默认不处理。
	virtual void BuildTrack(const std::vector<RouteTrackLinkEnds>& linkEnds,
		const std::function<const std::string&(int, int)>& getTerrain,
		const std::function<std::pair<bool, float>(int, int)>& getWater,
		const std::function<float(float, float)>& getHeight) {}

	// 工具方法(非虚，同RoadnetMod同名方法，实现转调general_geometry的BuildHatch/BuildTerrainFit)。
	void AddHatch(const Connection* connection, float t1, float t2, float width);
	void AddTunnelHatch(const Connection* connection, float t1, float t2, float width);
	void AddTerrainFit(const Connection* connection, float t1, float t2, float width, float falloff);

	// 输出(mod填、Core只读，同BuildingMod::floors的既有模式)。
	std::vector<RouteStationInfo> edgeStations;  // 请求创建的无建筑边缘站点(isEdge应设true)
	std::vector<RouteLink> links;                // 有向图的边
	std::vector<std::vector<RouteStop>> lines;   // 每条是一个环，相邻两站必须有可走的link
	// BuildTrack的输出，宿主(Route::Build)读走：分段几何，以及隧道口挖洞/抬升贴地请求。
	std::vector<TrackEdgeGeometry> trackEdges;
	std::vector<TerrainHatch> hatches;
	std::vector<TerrainFitRequest> terrainFits;

	// ---- 存档(见Source/Dependence/common/mod_state.md) ----
	// mod私有数据的存档/读档钩子，默认不存任何东西。基类里的公共声明字段由Core直接快照，
	// 这里只放mod自己的私有数据。
	virtual void SaveState(StateWriter&) const {}
	virtual void LoadState(StateReader&) {}
	// SaveState写出数据的格式版本：改了自己SaveState的格式就加一，读档时从
	// StateReader::GetSavedVersion()拿到当时的版本自己迁移。默认0。
	virtual int GetStateVersion() const { return 0; }
	// 这个mod类用来生成唯一实例名的静态计数器——存档保存、读档后恢复，避免读档后新建的实例和
	// 恢复出来的名字撞车。没有计数器的类保持默认(返回-1、Set不做事)。
	virtual int GetInstanceCounter() const { return -1; }
	virtual void SetInstanceCounter(int) {}
};
