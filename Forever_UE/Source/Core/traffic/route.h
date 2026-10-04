#pragma once

#include "traffic/route_mod.h"
#include "traffic/route_factory.h"
#include "traffic/station.h"

#include <string>
#include <unordered_map>
#include <vector>

class Map;
class Connection;
class Vehicle;

// Route：一类线路的实体，独占持有一个RouteMod实例(照抄Vehicle/Job持有mod的模式)。Build()
// 收集stationType匹配的Station接口、调mod->LayoutRoute()组织成有向图+若干条环线；Update()
// 按时刻表驱动绑定在这条线路上的载具，见Core/traffic/route.md。
class Route {
public:
	Route(RouteFactory* factory, const std::string& modId, const std::string& name);
	~Route(); // factory->DestroyRoute(mod)；delete graph里owned==true的边自己new出来的Connection

	bool IsValid() const;
	const std::string& GetName() const;
	const std::string& GetStationType() const;
	const std::string& GetVehicleType() const;
	bool UsesRoadnet() const;
	bool ShouldDrawPath() const;
	const std::string& GetTrackMesh() const;
	float GetTrackUnit() const;
	int GetVehiclesPerLine() const;

	// 一条有向图的边——segments按行驶顺序排列(reversed==true时实际按segments倒序遍历、
	// 1-f取点、切线取反，见Build()双向边登记那一段)。owned标记segments是不是Route自己new
	// 出来的(贝塞尔曲线边、同车道特例的直线弦边)，是的话析构要delete；借用Station旁路
	// (inEdge/outEdge)或Map既有车道图(FindVehiclePath结果)的边不持有所有权，不delete。
	struct RouteEdge {
		std::vector<Connection*> segments;
		bool owned = false;
		bool reversed = false;
		float length = 0.f; // 全部segments的CalcDistance()之和
	};

	// 线路上的一腿：从(fromStation,fromInterfaceIndex)到(toStation,toInterfaceIndex)，edge
	// 指向graph内部存储(不持有所有权，Route存活期间指针稳定)。
	struct RouteLeg {
		Station* fromStation = nullptr;
		int fromInterfaceIndex = 0;
		Station* toStation = nullptr;
		int toInterfaceIndex = 0;
		const RouteEdge* edge = nullptr;
		// 从RouteStop::dwell搬过来(Build()组装legs时按目的地那一站的dwell字段填)——false表示
		// 到达toStation后不停留，直接接着跑下一腿，DriveVehicle()/Update()的耗时计算都要跳过
		// 这一腿的停靠耗时(dwellSeconds)，不只是跳过停靠时的渲染。
		bool dwellAtDestination = true;
	};

	// 收集map里stationType==mod->stationType的所有Station接口，调mod->LayoutRoute()；为
	// mod->edgeStations创建的新Station通过outCreatedEdgeStations交回调用方(Traffic::InitRoutes)
	// 持有，Route自己只存一份裸指针引用(stations生命周期不归Route管)；按mod->links建有向图，
	// 按mod->lines组装成lines(相邻两站在图里查不到边就跳过这一腿，继续处理下一站，保证线路
	// 仍然成环)。
	void Build(Map* map, const std::vector<Station*>& stations, std::vector<Station*>& outCreatedEdgeStations);

	const std::vector<std::vector<RouteLeg>>& GetLines() const;

	// 把一辆车绑定到第lineIndex条线路，供Update()按时刻表驱动——Traffic::InitRoutes()按
	// GetVehiclesPerLine()创建车辆后逐个调用这个方法。
	void AddVehicleToLine(size_t lineIndex, Vehicle* vehicle);

	// 按elapsedSeconds(Traffic::Tick累加的游戏总秒数，单调递增)算每条线路每辆车当前应该在
	// 哪条边的哪个位置/停靠在哪个站，直接调vehicle->SetTransform(...)写回——纯函数式实现
	// (只读elapsedSeconds，不在Route自己身上累积任何状态)，同一个elapsedSeconds重复调用
	// 结果完全一致，见route.md"时刻表"一节。
	void Update(float elapsedSeconds);

private:
	static int PackStop(int station, int interfaceIndex);

	// 组装(from,fromIface)->(to,toIface)这一条边的实际几何。useRoadnet==true时：两边都成功
	// EnsureRoadLink()之后，若落在同一段原车道上且from在上游，直接用两个stationNode的直线弦
	// (同车道特例，见route.md)；否则用from的outEdge+Map::FindVehiclePath(所在车道下游锚点
	// ->to的上游锚点)+to的inEdge拼起来(都是借来的边，owned=false)。useRoadnet==false时，
	// 按2.3节的三次贝塞尔公式新建一条曲线边(owned=true)。任一环节失败时返回的RouteEdge
	// segments为空，调用方(Build())据此跳过这条link。
	RouteEdge BuildEdgeGeometry(Map* map, Station* from, int fromInterfaceIndex,
		Station* to, int toInterfaceIndex) const;

	// 在edge(可能是多段segments拼起来的)上按弧长arcLength取点，写出世界坐标+偏航角(角度制)。
	void EvaluateEdge(const RouteEdge& edge, float arcLength,
		float& outX, float& outY, float& outZ, float& outYawDegrees) const;

	// 沿legs(一条环线)按localTime(已经对线路总周期取模)驱动一辆车：逐腿累减"行驶耗时
	// (edge->length/speed)+停靠耗时(dwellSeconds)"，落在行驶区间就按EvaluateEdge算位置，
	// 落在停靠区间就直接停在toStation对应接口坐标上。
	void DriveVehicle(Vehicle* vehicle, const std::vector<RouteLeg>& legs, float localTime) const;

	RouteFactory* factory = nullptr;
	RouteMod* mod = nullptr;
	std::string name;

	// key是PackStop(station,interfaceIndex)——外层key是起点，内层key是终点。
	std::unordered_map<int, std::unordered_map<int, RouteEdge>> graph;
	std::vector<std::vector<RouteLeg>> lines;
	std::vector<std::vector<Vehicle*>> lineVehicles; // 下标和lines一一对应
};
