#pragma once

#include "class.h"

#include <cstdint>
#include <unordered_map>
#include <vector>


// 路口通行管理：只认路口里的Connection*，不持有车/人的指针，车/人用一个int64 entity标识自己。
// 车/人在路口前成为队头后，每隔requestInterval(现实秒)对下一条路口连线Request，进入后继续Request(续租)，
// 出了连线终点Leave；租约leaseSeconds内没有再Request就当作Leave。规则、冲突表和出口检查见junction.md。
// 时间由调用方以now(秒，单调、暂停时不走)传入，本类不读任何时钟，所以可以脱离UE单测。
class JunctionTraffic {
public:
	enum class Kind { Vehicle, Crosswalk, Corner };
	enum class EntityKind { Vehicle, Pedestrian };

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
	void AddConnection(const Connection* connection, Kind kind, int junctionId);
	// 登记完后调用一次：按几何算每个路口内的冲突表。
	void BuildConflicts();
	// 从Map的路口(RoadJunction)与车行/行人导航图收集连线并建冲突表(行人连线靠两端都是同一路口的人行锚点识别)。
	void BuildFromMap(const Map& map);

	// ---- 查询 ----
	bool IsManaged(const Connection* connection) const;
	bool GetKind(const Connection* connection, Kind& out) const;
	bool AreConflicting(const Connection* a, const Connection* b) const;
	size_t GetConnectionCount() const;
	// 行人走的一段(a->b，地图单位)恰好是某条人行横道连线(两端点都在tolerance内，方向不限)就返回它，否则nullptr。
	// 行人路径只是一串节点坐标，不带边信息，市民靠这个识别"这一段是过马路"。
	const Connection* FindCrosswalk(float ax, float ay, float bx, float by, float tolerance = 0.05f) const;

	// ---- 运行 ----
	// 返回true=可以进(含续租)，false=停。length是车长(地图单位，行人传0)；
	// exitFree是连线终点之后的出口车道实际空闲长度(地图单位，调用方量，不含这里已放行但还没到的预留；行人/不关心传一个大数)。
	// connection不在表里(不是路口连线)直接返回true。
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
	static int64_t CellKey(int cx, int cy);
};
