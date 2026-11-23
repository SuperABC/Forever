#pragma once

#include "common/mod_test.h"
#include "common/mod_state.h"
#include "map/general_geometry.h"
#include "map/transit_data.h"
#include "traffic/station_mod.h"

#include <string>
#include <vector>
#include <functional>


// Core->mod的端口快照(世界坐标,地图单位)。
struct RoutePort {
	float x = 0.f, y = 0.f, z = 0.f;
	float dirX = 0.f, dirY = 0.f; // 行驶方向(单位向量)
};

// Core->mod的只读站点快照：stationType匹配的每个真实站点一条。
struct RouteStationInfo {
	std::string name;
	int stationIndex = -1; // 站点在Core侧站点列表里的下标，RouteEndpoint::station引用它
	float x = 0.f, y = 0.f; // 站点中心
	std::vector<RoutePort> inlets, outlets, stops;
	std::string roadName; // 贴路站点所在道路的名字(同名=棋盘格里同一条横/竖线)，其余站点为空
	int presetIndex = -1; // 站所在建筑是路网预设放的：预设在.roadnet output.presets里的原下标(线路环按它找站)，否则-1
};

// route mod放在地图上的纯路径点(地图边界、天空点等)：没有站点，不停靠、不触发事件、不限速，只能被links当端点、被lines当必经点。
struct RouteExternNode {
	std::string name; // 本route内唯一
	float x = 0.f, y = 0.f, z = 0.f;
	float dirX = 0.f, dirY = 0.f;
};

// link的一端：站点的出站口/进站口，或一个extern node。
struct RouteEndpoint {
	enum Kind : int { Outlet = 0, Inlet = 1, Extern = 2 };
	Kind kind = Outlet;
	int station = -1;       // Outlet/Inlet：RouteStationInfo::stationIndex
	int portIndex = 0;      // Outlet/Inlet：该站outlets/inlets下标
	std::string externName; // Extern
};

// 站间连线：单向，segments是mod算好的完整几何(按行驶顺序首尾相接的三次贝塞尔，含抬升/天空段/绕地图边界出城再回来)，
// 合法组合Outlet->Inlet、Outlet->Extern、Extern->Inlet、Extern->Extern。Core只按它建曲线、不做任何连线计算。
// withinStation=true：同站内转场(如飞机从降落跑道滑行到起飞跑道)，所在行驶按sameStationSpeed。
struct RouteLinkSpec {
	RouteEndpoint from;
	RouteEndpoint to;
	bool withinStation = false;
	std::vector<TrackSegment> segments;
};

// 线路上的一个必经点：stopIndex>=0 = 在站name的第stopIndex个停靠位停靠；stopIndex<0 = name是extern名，只经过不停。
struct RouteLineEntry {
	std::string name;
	int stopIndex = -1;
};

// 设计见route_mod.md。
class RouteMod {
public:
	RouteMod() = default;
	virtual ~RouteMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// 单元测试：print输出关键信息，返回是否通过。没有深度测试的mod用FOREVER_MOD_GENERIC_UNIT_TEST(见common/mod_test.h)。
	virtual bool UnitTest(TestPrintFn print) = 0;
	// 子类在这里填下面的选项字段(不在构造函数里),Core创建实例后立刻调一次。
	virtual void SetProperty() {}

	// 选项——子类SetProperty()里填好，Core只读。
	std::string stationType;    // 接哪类站点，见station_mod.h
	std::string vehicleType;    // 这条线路生成的VehicleMod id
	bool useRoadnet = false;    // 是否接入现有车辆路网(公交)：不给links/externs，Core用路网寻路图拼路线
	bool instantiate = false;   // 是否实例化路线(沿线铺真实轨道mesh、过山爬坡/开隧道)：火车true，公交/飞机false
	float trackWidth = 0.f;     // 轨道占地整宽(地图单位)，挖隧道洞/地形贴合用
	// 三个绝对速度(地图单位/游戏秒)：正常运行 / 同站内运行(同站转场) / 通过但不停站。useRoadnet的线路只用normalSpeed。
	float normalSpeed = 1.f;
	float sameStationSpeed = 1.f;
	float passSpeed = 1.f;
	float dwellSeconds = 30.f;  // 到站停留秒数
	// 车辆俯仰角跟着路径切线的竖直分量走(飞机爬升/下降机头抬低)；instantiate的线路本来就跟，不用设。
	bool tiltAlongPath = false;
	// 加减速时间：从0加速到normalSpeed用这么久(真实秒)，同一个加速度也用于其它速度，见route_mod.md。
	float easeSeconds = 3.f;

	// 组织线路:stations是stationType匹配的真实站点(端口为世界坐标),sizeX/sizeY是地图尺寸。把externs/links/lines填好。
	virtual void LayoutRoute(const std::vector<RouteStationInfo>& stations, int sizeX, int sizeY) = 0;

	// 铺轨:只对instantiate的线路调用,在LayoutRoute之后。mod按地形把links[i].segments改成最终几何(过山抬升/挖隧道),
	// 隧道口/抬升段用AddTunnelHatch/AddTerrainFit提交请求。默认不处理。
	virtual void BuildTrack(const std::function<const std::string&(int, int)>& getTerrain,
		const std::function<std::pair<bool, float>(int, int)>& getWater,
		const std::function<float(float, float)>& getHeight) {}

	// 手动线路(--skip)：true=跳过自己的自动布局，LayoutRoute改用编辑器设计的线路环/实体线路(manual，见transit_data.h)。
	// mod在自己的构造函数里按参数设置(creator收到的args里的--skip)。manual由宿主在LayoutRoute之前通过SetManualTransit给，指向宿主拥有的数据(只读，生命周期覆盖Build)。
	bool skipAuto = false;
	void SetManualTransit(const TransitData* data) { manual = data; }
	const TransitData* manual = nullptr;

	// 工具方法(非虚),同RoadnetMod同名方法。
	void AddHatch(const Connection* connection, float t1, float t2, float width);
	void AddTunnelHatch(const Connection* connection, float t1, float t2, float width,
		float startWallBottom = kNoEndWall, float endWallBottom = kNoEndWall);
	void AddTerrainFit(const Connection* connection, float t1, float t2, float width, float falloff);
	// 提交拆除请求：折线(世界xy)扫过width宽的条带，Core拆掉所有相交的楼体及其园区(带stationMod的建筑除外)，见route.md。
	void AddDemolition(const std::vector<std::pair<float, float>>& centerline, float width);
	// 提交桥墩请求：chains每条是一根轨道的Connection链(按行驶顺序)，Core按defaultSpacing/minSpacing沿链选位，见route.md"桥墩"。
	// mesh空串=默认圆柱，radius=墩半径(地图单位)。
	void AddPier(const std::string& mesh, std::vector<std::vector<Connection>> chains, float defaultSpacing, float minSpacing, float radius);

	// 输出(mod填、Core只读，同BuildingMod::floors的既有模式)。
	std::vector<RouteExternNode> externs;
	std::vector<RouteLinkSpec> links;                // 单向站间连线，含每条的几何
	std::vector<std::vector<RouteLineEntry>> lines;  // 每条是一个环，按entry顺序依次经过
	// 和lines一一对应：每条线路上跑几辆车。LayoutRoute里可选填；缺项或<=0时按1辆。车辆相位在一圈内均匀分布。
	std::vector<int> lineVehicleCounts;
	// BuildTrack的输出，宿主(Route::Build)读走：隧道口挖洞/抬升贴地/拆除请求。
	std::vector<TerrainHatch> hatches;
	std::vector<TerrainFitRequest> terrainFits;
	std::vector<DemolishRequest> demolitions;
	std::vector<PierRequest> piers;

	// ---- 存档(见common/mod_state.md) ----
	virtual void SaveState(StateWriter&) const {}
	virtual void LoadState(StateReader&) {}
	virtual int GetStateVersion() const { return 0; }
	// GetName用的static count,存档保存/读档恢复;没有计数器的类保持默认。
	virtual int GetInstanceCounter() const { return -1; }
	virtual void SetInstanceCounter(int) {}
};
