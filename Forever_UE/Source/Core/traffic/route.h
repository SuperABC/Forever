#pragma once

#include "class.h"

#include "traffic/route_mod.h"
#include "traffic/route_factory.h"
#include "traffic/station.h"

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>


// 一条连续的路径：按行驶顺序首尾相接的Connection链。站内连线、站间连线、公交借用的路网车道都是它，shared_ptr共享。
// segments是mod给的几何值(存档/UE铺轨用，借用路网的链为空)；owned标记connections是不是自己new出来的(析构要delete)，
// 借用Station旁路(inEdge/outEdge)或Map既有车道图(FindVehiclePath结果)的不持有所有权。
struct TrackPath {
	std::vector<TrackSegment> segments;
	std::vector<Connection*> connections;
	std::vector<float> lengths; // 每个connection的CalcDistance()
	bool owned = false;
	float length = 0.f;

	TrackPath() = default;
	TrackPath(const TrackPath&) = delete;
	TrackPath& operator=(const TrackPath&) = delete;
	~TrackPath();

	// 按mod给的分段几何(值类型)建Connection链(owned=true)，segments为空返回nullptr。
	static std::shared_ptr<TrackPath> FromSegments(const std::vector<TrackSegment>& segments);
	// 在路径上按弧长arcLength取点，写出世界坐标+偏航角(角度制)+俯仰角(角度制)。
	void Evaluate(float arcLength, float& outX, float& outY, float& outZ, float& outYawDegrees, float& outPitchDegrees) const;
};

// Route：一类线路的实体，独占持有一个RouteMod实例(照抄Vehicle/Job持有mod的模式)。Build()
// 收集stationType匹配的Station端口、调mod->LayoutRoute()得到extern点/站间连线/停站顺序，建寻路图，按停站顺序寻路，
// 预算好每条线路的运动学；Update()按预算好的运动学驱动绑定在这条线路上的载具，见Core/traffic/route.md。
class Route {
	// 存档：TrafficSaveCodec直接读写私有字段(traffic_save.md)。
	friend class TrafficSaveCodec;
public:
	Route(RouteFactory* factory, const std::string& modId, const std::string& name);
	~Route();

	bool IsValid() const;
	const std::string& GetName() const;
	const std::string& GetStationType() const;
	const std::string& GetVehicleType() const;
	bool UsesRoadnet() const;
	bool ShouldInstantiate() const;
	float GetTrackWidth() const;
	// 第lineIndex条线路上的车辆数：mod为这条线路指定了(lineVehicleCounts)就用它，否则1辆。
	int GetLineVehicleCount(size_t lineIndex) const;

	// 一段行驶路径的一块：path的全长，加这一块的速度上限(通过不停站的站内直通连线上限是passSpeed，其余是run的基础速度)。
	struct RoutePiece {
		std::shared_ptr<TrackPath> path;
		float length = 0.f;
		float speedCap = 0.f;
	};
	// 运动学相位(匀加速/匀速/匀减速)：相位内 s = s0 + v0*dt + 0.5*acc*dt^2，dt=t-t0。
	struct RunPhase {
		float t0 = 0.f, s0 = 0.f, v0 = 0.f, acc = 0.f, duration = 0.f;
	};
	// 线路上的一段：从一个停靠位开到下一个停靠位，两端速度为0。预算好的数据，运行时不再访问寻路图。
	struct RouteRun {
		Station* fromStation = nullptr;
		int fromStop = 0;
		Station* toStation = nullptr;
		int toStop = 0;
		std::vector<RoutePiece> pieces;
		float length = 0.f;
		std::vector<RunPhase> phases;
		float travelTime = 0.f;
	};
	// 一条运行线路(成环)：runs按行驶顺序，period=一圈的总时长(含每站停靠)。
	struct RouteLine {
		std::vector<RouteRun> runs;
		float period = 0.f;
		int vehicleCount = 0; // 这条线路上的车辆数，Assemble按mod->lineVehicleCounts填
	};

	// 收集map里stationType==mod->stationType的所有Station，调mod->LayoutRoute()；instantiate线路再调BuildTrack；
	// 然后Assemble()建寻路图+规划每条线路。
	void Build(Map* map, const std::vector<Station*>& stations);

	const std::vector<RouteLine>& GetLines() const;
	// 非路网线路寻路图里的全部路径(站内连线+站间连线，已去重)，UE按它铺轨/画图。
	const std::vector<std::shared_ptr<TrackPath>>& GetTrackPaths() const;

	// 把一辆车绑定到第lineIndex条线路，供Update()按时刻表驱动——Traffic::InitRoutes()按
	// GetLineVehicleCount()创建车辆后逐个调用这个方法。
	void AddVehicleToLine(size_t lineIndex, Vehicle* vehicle);

	// 按elapsedSeconds(Traffic::Tick累加的游戏总秒数，单调递增)算每条线路每辆车当前应该在
	// 哪个位置/停靠在哪个站，直接调vehicle->SetTransform(...)写回——纯函数式实现
	// (只读elapsedSeconds，不在Route自己身上累积任何状态)，同一个elapsedSeconds重复调用
	// 结果完全一致，见route.md"时刻表"一节。
	void Update(float elapsedSeconds);

	// 停站/离站事件：载具进入/离开停靠状态的边沿。Update()每次算完一辆车的相位后，和上一次
	// 记下的状态比较——上次不在站这次在某个站=entering，上次在站这次不在=leaving，上次在站A这次
	// 在站B(两次Update之间隔了整个停站)=先A的leaving再B的entering。事件放进待取队列，由
	// Traffic::ConsumeStopEvents()汇总交给UE层发给载具自己的script，Core自己不碰script。
	struct StopEvent {
		Vehicle* vehicle = nullptr;
		Station* station = nullptr;
		bool entering = true;
	};
	std::vector<StopEvent> ConsumeStopEvents();

	// 读档后调用：下一次Update()只记录每辆车当前的停靠状态、不发事件(载具script的变量/选项已经
	// 随存档恢复，再发一次enter_stop会让script重复加选项)。新开局不调，第一次Update()正常发。
	void SilenceInitialStopEvents();

	// 载具当前在时刻表上的状态（公交远近切换用：近处换成物理公交+Controller时据此接管，见
	// Source/Forever/Framework/ForeverTrafficFrameworkComponent.md）。按最近一次Update()的elapsedSeconds算，
	// 不改变任何状态。载具不在这条Route的任何线路上 -> valid=false。
	//   line：线路下标；legIndex：正在行驶/刚到达的那一段(run)；dwelling：正停靠在run终点站里，
	//   dwellRemainingSeconds是剩余停靠时间（真实秒）；speedMapPerRealSecond：巡航速度（地图单位/真实秒，
	//   缓动阶段的瞬时速度不精确，仅供给物理车一个初速度）。
	struct ScheduleState {
		bool valid = false;
		int line = -1;
		int legIndex = -1;
		bool dwelling = false;
		float dwellRemainingSeconds = 0.f;
		float speedMapPerRealSecond = 0.f;
	};
	ScheduleState QueryVehicleState(const Vehicle* vehicle) const;

	// 载具从物理段交还时刻表前调用：把它在时刻表上的进度对齐到实车当前位置，之后时刻表从这里接着走，
	// 不会因为物理车比名义速度慢而一交还就瞬移到"早该跑完一圈"的位置。做法是给这辆车记一个相位修正量，
	// 让"现在"对应的时刻表时间正好等于目标进度。legIndex是实车所在的那一段(run)，
	// legFraction是这一段已走的路程比例[0,1]；dwelling=true 表示实车停在run终点站里，dwellFraction是停靠已过的比例。
	// 找不到这辆车返回false。
	bool SyncVehicleProgress(const Vehicle* vehicle, int legIndex, float legFraction, bool dwelling, float dwellFraction);

	// 第lineIndex条线路每一站的车站节点id（Map辅助导航节点，见Map::AuxNavNode），顺序即行驶顺序：
	// 第i个元素是 run i 的起点站。任何一站没有road link（没接上路网）时返回空。
	std::vector<int> GetLineStopNodeIds(size_t lineIndex) const;

	// 载具从物理段交还给时刻表时调用：忘掉它的停靠状态，下一次Update()当作第一次观察（正好停着就补发进站事件）。
	void ResetStopState(Vehicle* vehicle);

private:
	// 站点表+mod输出(externs/links/lines)都就位后建寻路图并规划每条线路；Build()和读档共用，读档时几何来自存档，不重新LayoutRoute。
	void Assemble(Map* map);

	// ---- 寻路图(非路网线路) ----
	enum EdgeClass { EDGE_STATION_IN, EDGE_STATION_OUT, EDGE_THROUGH, EDGE_LINK };
	struct GraphNode {
		int station = -1;           // 站点下标，extern为-1
		PortKind kind = PortKind::Inlet;
		int index = 0;              // 端口下标，extern为extern下标
		bool isExtern = false;
	};
	struct GraphEdge {
		int to = -1;
		std::shared_ptr<TrackPath> path;
		EdgeClass edgeClass = EDGE_LINK;
		bool withinStation = false;
	};
	int GraphNodeIndex(const GraphNode& node);
	void AddGraphEdge(int from, int to, std::shared_ptr<TrackPath> path, EdgeClass edgeClass, bool withinStation);
	// 从src到dst的最短路径(Dijkstra，边权=长度)；除src/dst外不能经过任何停靠位节点(过站不停只能走站内直通连线)。
	bool FindGraphPath(int src, int dst, std::vector<const GraphEdge*>& outEdges) const;
	// 非路网线路：按一圈经过的停靠位+途经点的节点序列规划，生成run并预算运动学。失败返回false。
	bool PlanGraphLine(const std::vector<int>& waypoints, RouteLine& outLine) const; // waypoints=一圈依次经过的节点(停靠位/extern)
	// 路网线路(公交)：相邻两个停靠位之间用Station旁路+Map::FindVehiclePath(含同车道特例)拼路径。
	bool BuildRoadRun(Map* map, Station* from, int fromStop, Station* to, int toStop, RouteRun& outRun) const;
	// 给run按pieces的速度上限预算相位表(0->限速->0，同一个加速度)。
	void ComputeRunPhases(RouteRun& run) const;
	// run内按时间/弧长换算。
	static float RunArcAtTime(const RouteRun& run, float t);
	static float RunTimeAtArc(const RouteRun& run, float s);
	void EvaluateRun(const RouteRun& run, float t, float& x, float& y, float& z, float& yaw, float& pitch) const;
	void FinalizeLine(RouteLine& line) const;

	// 沿line按localTime(已经对线路总周期取模)驱动一辆车。outDwellStation：这一刻载具正停靠在哪个站(没有停靠=nullptr)，
	// Update()拿它检测停站/离站边沿。
	void DriveVehicle(Vehicle* vehicle, const RouteLine& line, float localTime, Station** outDwellStation) const;

	RouteFactory* factory = nullptr;
	RouteMod* mod = nullptr;
	std::string name;

	// Build()里mod->LayoutRoute用的站点表，RouteEndpoint::station就是这张表的下标；存档靠它把下标换成站点名。
	std::vector<Station*> stopStations;

	std::vector<GraphNode> graphNodes;
	std::vector<std::vector<GraphEdge>> graphEdges;
	std::map<std::string, int> graphNodeIndexByKey;
	std::vector<std::shared_ptr<TrackPath>> trackPaths;
	std::vector<RouteLine> lines;
	std::vector<std::vector<Vehicle*>> lineVehicles; // 下标和lines一一对应

	// 停站/离站边沿检测：key是载具，value是上次Update()时它正停靠的站(nullptr=在行驶)；key不存在
	// 表示还没观察过这辆车。不进存档(读档后第一次Update重新观察，见SilenceInitialStopEvents)。
	std::unordered_map<Vehicle*, Station*> lastStopStation;

	// SyncVehicleProgress记的每辆车的相位修正量（游戏秒），运行时数据，不存档。
	std::unordered_map<const Vehicle*, float> phaseAdjust;
	float GetPhaseAdjust(const Vehicle* vehicle) const;
	std::vector<StopEvent> pendingStopEvents;
	bool silentInitialStopState = false;
	float lastElapsedSeconds = 0.f; // 最近一次Update()的elapsedSeconds，QueryVehicleState用
};
