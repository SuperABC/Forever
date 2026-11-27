#pragma once

#include "class.h"

#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>


// 路口通行管理：只认路口里的Connection*，不持有车/人的指针，车/人用一个int64 entity标识自己。
// 车/人在路口前成为队头后，每隔requestInterval(现实秒)对下一条路口连线Request，进入后继续Request(续租)，
// 出了连线终点Leave；租约leaseSeconds内没有再Request就当作Leave。规则、冲突表和出口检查见junction.md。
// 时间由调用方以now(秒，单调、暂停时不走)传入，本类不读任何时钟，所以可以脱离UE单测。
// 红绿灯：多于两条路的路口有一份信号表(进入道路两两配成一组，各组轮流绿灯)，只有调用方激活的路口才按灯放行，
// 没激活的路口当作没有红绿灯。见junction.md"红绿灯"。
class JunctionTraffic {
public:
	enum class Kind { Vehicle, Crosswalk, Corner };
	enum class EntityKind { Vehicle, Pedestrian };
	// 某条进入道路此刻的灯色。None=这个路口没有红绿灯或没激活(只走冲突表)。
	enum class Signal { None, Green, Yellow, Red };
	// 每组绿灯/黄灯多久(秒，和now同一个时钟)。先写死。一个周期=组数×(绿+黄)。
	static constexpr double kGreenSeconds = 15.0;
	static constexpr double kYellowSeconds = 3.0;

	struct Config {
		float leaseSeconds = 1.f;          // 租约：最后一次Request起多久没续就失效
		float vehicleTolerance = 0.1f;     // 两条车行连线折线距离小于它视为交叉(地图单位，1=10m)
		float crosswalkTolerance = 0.15f;  // 车行连线和人行横道
		float cornerTolerance = 0.02f;     // 车行连线和转角人行道：只在真正相交时冲突
		float maxOpenSeconds = 0.f;        // >0：一条连线连续开放超过它且有更早的冲突方在等，就不再放新车进来；0=不限(允许饿死)
	};

	explicit JunctionTraffic(const Config& config = Config());

	static int64_t MakeEntity(EntityKind kind, int id);
	static EntityKind GetEntityKind(int64_t entity);

	// ---- 建表 ----
	// 登记一条路口连线(由调用方持有生命周期，用指针当key)；junctionId相同的连线互相比较冲突。
	// approach=车行连线从路口的第几条道路(RoadJunction::GetApproaches()的下标)驶入、人行横道横穿的是第几条道路，
	// 红绿灯按它看灯色；-1=不受红绿灯管(转角总是-1)。
	void AddConnection(const Connection* connection, Kind kind, int junctionId, int approach = -1);
	// 登记完后调用一次：按几何算每个路口内的冲突表。
	void BuildConflicts();
	// 从Map的路口(RoadJunction)与车行/行人导航图收集连线并建冲突表(行人连线靠两端都是同一路口的人行锚点识别)。
	// 多于两条路的路口顺带建信号表(见SetSignalGroups/PairApproaches)。
	void BuildFromMap(const Map& map);

	// ---- 红绿灯 ----
	// 进入道路配对：只给有驶入车道的道路(hasInbound)分组，每次在剩下的道路里挑夹角最接近180°的两条成一组，
	// 最后可能剩一条单独一组。angles是每条道路离开路口方向的弧度(RoadJunctionApproach::angle)。
	// 返回每条道路的组号(0起，按配出来的先后)，没驶入车道的是-1。
	static std::vector<int> PairApproaches(const std::vector<float>& angles, const std::vector<bool>& hasInbound);
	// 给路口设信号表(groupOfApproach见PairApproaches)；没有任何一组就不设。BuildFromMap会调，测试也可以直接调。
	void SetSignalGroups(int junctionId, const std::vector<int>& groupOfApproach);
	bool HasSignal(int junctionId) const;
	// 激活=这个路口的红绿灯开始运转(从第0组绿灯开始，now是周期起点)；关掉=当作没有红绿灯。没有信号表的路口什么都不做。
	void SetSignalActive(int junctionId, bool active, double now);
	bool IsSignalActive(int junctionId) const;
	// 第approach条道路此刻的灯色；没信号表/没激活/这条路不在任何组里返回None。
	Signal GetSignal(int junctionId, int approach, double now) const;
	// 信号表的组号(下标=道路)；没有信号表返回nullptr。
	const std::vector<int>* GetSignalGroups(int junctionId) const;

	// ---- 查询 ----
	bool IsManaged(const Connection* connection) const;
	bool GetKind(const Connection* connection, Kind& out) const;
	bool AreConflicting(const Connection* a, const Connection* b) const;
	size_t GetConnectionCount() const;
	// 行人走的一段(a->b，地图单位)恰好是某条人行横道连线(两端点都在tolerance内，方向不限)就返回它，否则nullptr。
	// 行人路径只是一串节点坐标，不带边信息，市民靠这个识别"这一段是过马路"。
	const Connection* FindCrosswalk(float ax, float ay, float bx, float by, float tolerance = 0.05f) const;
	// 车走的一串路径点(地图单位XY)经过了哪些路口车行连线：某个点和连线起点(驶入车道锚点)重合、之后某个点和它的终点
	// (驶出车道锚点)重合，就是经过了这条连线。返回(连线, 起点下标, 终点下标)，按路径先后。
	// 只有点没有边信息的路径(Core推算交给控制器的那段)靠它补上路口区间。
	struct PathSpan { const Connection* connection; int fromIndex; int toIndex; };
	std::vector<PathSpan> FindVehicleSpans(const std::vector<std::pair<float, float>>& points, float tolerance = 0.05f) const;

	// ---- 运行 ----
	// 返回true=可以进(含续租)，false=停。length是车长(地图单位，行人传0)；
	// exitFree是连线终点之后的出口车道实际空闲长度(地图单位，调用方量，不含这里已放行但还没到的预留；行人/不关心传一个大数)。
	// connection不在表里(不是路口连线)直接返回true。
	// 红绿灯：车在已激活的路口、自己那条道路不是绿灯时拒绝；行人要横穿的那条道路不是红灯时拒绝。
	// 已经放行的续租照常(已经开始过马路的人走完)，被拒的不进排队。
	bool Request(const Connection* connection, int64_t entity, double now, float length, float exitFree);
	// 车尾出了连线终点/放弃进入时调用；没持有/没排队则什么都不做。
	void Leave(const Connection* connection, int64_t entity);
	bool Holds(const Connection* connection, int64_t entity) const;
	// 互相冲突的连线不会同时有未过期的持有者；测试与调试用。
	bool CheckInvariant(double now) const;

private:
	struct Point { float x, y; };
	struct Holder { double expire; float length; };
	struct Waiter { double t0; double expire; };
	struct Entry {
		const Connection* connection = nullptr;
		Kind kind = Kind::Vehicle;
		int junctionId = 0;
		int startId = 0, endId = 0; // 起终锚点Node id；终点id同时当出口车道的key
		int approach = -1;           // 车：从第几条道路驶入；横道：横穿第几条道路；-1=不受红绿灯管(转角、测试里没给)
		std::vector<Point> polyline;
		std::vector<int> conflicts;  // 冲突的Entry下标
		std::unordered_map<int64_t, Holder> holders;
		std::unordered_map<int64_t, Waiter> waiters;
		double openSince = 0.0;
	};

	void Purge(Entry& entry, double now) const;
	float ReservedLength(int exitId, double now) const;
	bool HasEarlierConflictWaiter(const Entry& entry, double t0, int64_t entity, bool pedestriansOnly) const;
	bool HasConflictHolder(const Entry& entry) const;

	Config config;
	std::vector<Entry> entries;
	std::unordered_map<const Connection*, int> index;
	std::unordered_map<int, std::vector<int>> entriesByExit; // 出口锚点id -> 以它为终点的车行连线
	std::unordered_map<int64_t, std::vector<int>> crosswalkGrid; // 人行横道中点所在格(1.0地图单位) -> 连线
	std::unordered_map<int64_t, std::vector<int>> vehicleStartGrid; // 车行连线起点所在格(1.0地图单位) -> 连线
	struct SignalPlan {
		std::vector<int> groupOfApproach;
		int groupCount = 0;
		bool active = false;
		double cycleStart = 0.0;
	};
	std::unordered_map<int, SignalPlan> signals; // 路口id -> 信号表
	static int64_t CellKey(int cx, int cy);
};
