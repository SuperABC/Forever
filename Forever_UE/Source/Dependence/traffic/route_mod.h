#pragma once

#include "common/mod_state.h"
#include "map/general_geometry.h"

#include <string>
#include <vector>
#include <functional>


// Core->mod的只读站点接口快照(世界坐标,地图单位),含stationType匹配的真实站点接口和其它Route已请求的边缘站点(isEdge=true)。
struct RouteStationInfo {
	std::string name;
	float x = 0.f, y = 0.f, z = 0.f;
	float arriveDirX = 0.f, arriveDirY = 0.f;
	float departDirX = 0.f, departDirY = 0.f;
	bool isEdge = false;
	// 站点在Core侧站点列表里的下标(真实站点在前,edgeStations在后);interfaceIndex是该站点的第几个接口。
	int stationIndex = -1;
	int interfaceIndex = 0;
};

// 线路上的一站,station是RouteStationInfo列表的下标。dwell=false为纯过路点(到达后不停留),见route_mod.md。
struct RouteStop {
	int station = -1;
	int interfaceIndex = 0;
	bool dwell = true;
	// dwell=true时有效:停靠但不触发enter_stop/leave_stop(地图边缘虚拟车站用)。
	bool silent = false;
};

// BuildTrack的输入:links[i]两端落在哪两个站点接口上(世界坐标),与trackEdges一一对应。
struct RouteTrackLinkEnds {
	RouteStationInfo from;
	RouteStationInfo to;
};

// BuildTrack的输出:一条link的一段三次贝塞尔轨道(世界坐标)。纯值类型,不放Node/Connection(避免跨DLL id/所有权问题);
// tunnel=true时UE侧用trackTunnelMesh。
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

// 有向图的一条边,bidirectional=true时同一份几何两个方向都能走,见route_mod.md。
struct RouteLink {
	RouteStop from;
	RouteStop to;
	bool bidirectional = false;
};

// 设计见route_mod.md。
class RouteMod {
public:
	RouteMod() = default;
	virtual ~RouteMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// 子类在这里填下面的选项字段(不在构造函数里),Core创建实例后立刻调一次。
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
	// 非路网边贝塞尔控制臂长度的按端点覆盖规则(按顺序，第一条匹配的生效，都不匹配用controlLength)：
	// fromTag/toTag空=任意站点，"@real"=真实站点(建筑上的接口)，其它=站点名包含该子串(边缘站点名=<线路名>_Edge<i>_<RouteStationInfo::name>)；
	// arm<=0=不加控制点，直线连接；ratio=true时arm是两端点水平距离的倍数(如1/3)，否则是地图单位长度。规则由站点名/类型判定，读档重建边时自动一致，所以要在SetProperty里填。
	struct ControlRule {
		std::string fromTag;
		std::string toTag;
		float arm = 0.f;
		bool ratio = false;
		// true时还要求两端是同一组边缘站点(站名<RouteStationInfo::name>里第一个"_"之前的部分相同，如同一座机场的"air0")。
		bool sameGroup = false;
	};
	std::vector<ControlRule> controlRules;
	// 行驶区段(两次真正停靠之间)里含同站转场腿(from/to是同一个站点的两个接口，如飞机从降落跑道滑行到起飞跑道)时，
	// 整个区段的速度倍率，<1更慢，见route_mod.md。
	float sameStationSpeedScale = 1.f;
	// 车辆俯仰角跟着路径切线的竖直分量走(飞机爬升/下降机头抬低)；instantiate的线路本来就跟，不用设。
	bool tiltAlongPath = false;

	// 进站前/出站后这么久(真实秒)的减速/加速缓动时长,见route_mod.md。
	float easeSeconds = 3.f;

	// 组织线路:interfaces是stationType匹配的真实站点接口,sizeX/sizeY是地图尺寸(算边缘站点位置)。输出下面三个字段。
	virtual void LayoutRoute(const std::vector<RouteStationInfo>& interfaces, int sizeX, int sizeY) = 0;

	// 铺轨:只对instantiate的线路调用,在LayoutRoute之后。把分段几何填进trackEdges(下标与links对应,留空=沿用Core单段贝塞尔),
	// 隧道口/抬升段用AddTunnelHatch/AddTerrainFit提交请求。默认不处理。
	virtual void BuildTrack(const std::vector<RouteTrackLinkEnds>& linkEnds,
		const std::function<const std::string&(int, int)>& getTerrain,
		const std::function<std::pair<bool, float>(int, int)>& getWater,
		const std::function<float(float, float)>& getHeight) {}

	// 工具方法(非虚),同RoadnetMod同名方法。
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

	// ---- 存档(见common/mod_state.md) ----
	virtual void SaveState(StateWriter&) const {}
	virtual void LoadState(StateReader&) {}
	virtual int GetStateVersion() const { return 0; }
	// GetName用的static count,存档保存/读档恢复;没有计数器的类保持默认。
	virtual int GetInstanceCounter() const { return -1; }
	virtual void SetInstanceCounter(int) {}
};
