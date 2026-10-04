#pragma once

#include <string>
#include <vector>


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
	bool drawPath = false;      // 是否铺真实轨道mesh(trackMesh非空时才有意义)
	std::string trackMesh;      // 轨道/跑道资产软路径，可留空(这次不画)
	float trackUnit = 0.f;      // 轨道mesh沿弧长重复的单位长度(地图单位)
	int vehiclesPerLine = 1;    // 每条线路生成几辆车，均匀分布相位
	float speed = 1.f;          // 地图单位/秒
	float dwellSeconds = 30.f;  // 到站停留秒数
	float controlLength = 3.f;  // 非路网边贝塞尔控制臂长度(地图单位)

	// 组织线路——interfaces是所有stationType匹配的真实站点接口(世界坐标)；sizeX/sizeY是地图
	// 整体尺寸(地图单位)，无站点线路(比如只有一个站点、两头接地图边缘的火车/飞机线)用来算
	// 边缘站点位置。调用后读取下面三个输出字段。
	virtual void LayoutRoute(const std::vector<RouteStationInfo>& interfaces, int sizeX, int sizeY) = 0;

	// 输出(mod填、Core只读，同BuildingMod::floors的既有模式)。
	std::vector<RouteStationInfo> edgeStations;  // 请求创建的无建筑边缘站点(isEdge应设true)
	std::vector<RouteLink> links;                // 有向图的边
	std::vector<std::vector<RouteStop>> lines;   // 每条是一个环，相邻两站必须有可走的link
};
