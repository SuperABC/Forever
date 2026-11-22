#pragma once

#include "class.h"

#include "traffic/vehicle.h"
#include "traffic/vehicle_factory.h"
#include "traffic/station.h"
#include "traffic/station_factory.h"
#include "traffic/route.h"
#include "traffic/route_factory.h"

#include "common/handle.h"
#include "common/utility.h"

#include <functional>
#include <list>
#include <string>
#include <unordered_map>
#include <vector>


// Traffic：按name索引持有全部当前存在的Vehicle*/Station*/Route*。Init(Map*)的五步顺序和细节见traffic.md。
// 注意：这个类和UForeverTrafficFrameworkComponent（UE层Traffic域组件，上下车的Possess
// 切换逻辑在那边，见ForeverTrafficFrameworkComponent.md）不是一回事，Traffic不知道UE
// Actor/Controller的存在，两者没有互相持有关系。
class Traffic {
	// 存档：TrafficSaveCodec直接读写私有字段(traffic_save.md)。
	friend class TrafficSaveCodec;
public:
	Traffic(); // 绑定Registry::Get().GetVehicleFactory()/GetStationFactory()/GetRouteFactory()/GetScriptFactory()
	~Traffic(); // delete全部routes/stations/vehicles

	void Init(Map* map);

	// 给市民和组织分配载具(开局一次，读档不调用——载具和归属随存档恢复)：成年市民各20%概率在自己家所在
	// 建筑里找空闲car车位，组织在自己组合的带车位房间里随机1-5次找空闲truck车位；生成车辆和VehicleAsset
	// (Traffic持有)，挂到所有者资产列表。必须在人口和社会生成之后调用，详见traffic.md。
	void AssignVehicles(const Populace& populace, const Society& society);

	// 读档用：存档里的载具资产所有者(市民/组织下标)在交通读档时人口/社会还没恢复，先记下，
	// 人口和社会恢复之后调用这个，按下标把VehicleAsset建出来并挂回所有者；下标越界的丢弃。
	void ResolveVehicleAssets(const Populace& populace, const Society& society);
	// 这个Traffic是不是从存档恢复的(读档后不再AssignVehicles)。
	bool WasRestoredFromSave() const { return restoredFromSave; }

	// ---- 开车通勤（详见traffic.md"通勤"）----
	// 一次通勤分两个阶段：WalkToCar(市民走向停着的车所在的停车房)、Driving(市民已消失，车在开往目的车位)。
	// 车开到后市民在目的停车房出现，再普通步行去finalDest——那一段不属于行程，到达时行程即删。
	enum class CommutePhase { WalkToCar = 0, Driving = 1 };
	struct CommuteTrip {
		Citizen* citizen = nullptr;
		Vehicle* vehicle = nullptr;
		Room* finalDest = nullptr;
		Room* parkOrigin = nullptr; // 车现在停的停车房(第一段步行的目的地)
		Room* parkDest = nullptr;   // 目的建筑里的停车房
		int destSlot = -1;          // parkDest里已为这辆车预订的车位
		CommutePhase phase = CommutePhase::WalkToCar;
	};
	// 判定：市民当前在某栋建筑里，自己的车(VehicleAsset)停在这栋建筑里，finalDest所在的另一栋建筑里有接受该车类别
	// 的空闲车位 -> 开车；否则返回nullptr(照常走路)。成功则预订目的车位、记下行程(阶段WalkToCar)并返回它。
	CommuteTrip* BeginCommute(Citizen* citizen, Room* finalDest);

	// 通勤统计(仅日志用，不存档)：每次BeginCommute决策的结果计数。
	struct CommuteStats {
		int started = 0, arrived = 0, failed = 0;
		int walkSameBuilding = 0, walkNoCar = 0, walkCarElsewhere = 0, walkNoSlot = 0;
	};
	CommuteStats commuteStats;
	std::vector<std::string> CommuteReport(const Populace& populace) const; // 成年人/各职位人数/有车人数/开车寻路累计，由UE层逐行写日志
	CommuteTrip* FindTrip(const Citizen* citizen);
	CommuteTrip* FindTripByVehicle(const Vehicle* vehicle);
	// 结束行程：只删记录，不动预订(调用方在车到达/取消时自己处理)。
	void EndTrip(const Citizen* citizen);
	// 取消行程：释放预订的目的车位并删记录(车和市民的位置由调用方安排)。
	void CancelTrip(const Citizen* citizen);
	const std::unordered_map<const Citizen*, CommuteTrip>& GetTrips() const { return trips; }

	// ---- 组织任务行程(assign_task，详见traffic.md"组织任务")----
	// 组织派员工走路或开组织的车去某地，只有单程：开车任务=走到车->开车去目的车位->下车走进目的房间；
	// 走路任务=走进目的房间。走进目的房间后行程结束，UE层给组织脚本发task_complete，往返由脚本再派一个任务决定。
	// 开车任务按车索引(员工中途死亡/离城车仍要开到站，worker置空)。
	enum class TaskPhase { WalkToVehicle = 0, Driving = 1, WalkOut = 2 };
	struct TaskTrip {
		std::string task;
		Citizen* worker = nullptr;
		Vehicle* vehicle = nullptr;   // 走路任务为空
		Room* parkOrigin = nullptr;   // 车停着的停车房(员工上车的地方；开车失败时员工落回这里)，走路任务为空
		Room* parkDest = nullptr;
		int destSlot = -1;            // 目的车位(开出去前就预订好)
		Room* destRoom = nullptr;     // 任务指向的房间(走路任务的目的地/开车任务的目的建筑里的存储房间)
		Room* originRoom = nullptr;   // 派任务时员工所在的房间(task_complete/task_failed的origin)
		TaskPhase phase = TaskPhase::WalkToVehicle;
	};
	// 一次尝试：员工必须是组织在岗员工、闲置(没在走、没有通勤/任务、有当前房间；workerName为空时还要在工位房间)；
	// categories按顺序尝试，取第一个可行的：空串=走路(目的房间不是员工当前房间)；非空=开组织的这类载具(停着、没路线、没预订，
	// 停车房和员工当前所在是同一栋建筑，且和目的建筑不是同一栋)且目的建筑里有接受该类别的空闲车位。找到才建行程(预订目的车位、
	// 写员工脚本的task变量)并返回；否则返回nullptr(不占车不占人)，*failReason(可空)写失败原因：
	// no_worker/no_vehicle/no_slot/already_there。workerName非空只考虑该员工(此时员工在哪都行)。
	// workerAvailable(可空)：UE层补充的闲置判断(如不被玩家操控)。
	TaskTrip* BeginTask(Organization* organization, const std::string& task, Room* destRoom, const std::vector<std::string>& categories,
		const std::string& workerName, const std::function<bool(const Citizen*)>& workerAvailable = nullptr, std::string* failReason = nullptr);
	TaskTrip* FindTaskByVehicle(const Vehicle* vehicle);
	TaskTrip* FindTaskByWorker(const Citizen* worker);
	// 结束行程：清员工task变量并删记录(车/员工位置由调用方安排)。
	void EndTask(TaskTrip* trip);
	// 取消行程：同EndTask，另外车还没到站(WalkToVehicle/Driving)时释放目的车位预订。
	void CancelTask(TaskTrip* trip);
	const std::list<TaskTrip>& GetTasks() const { return taskTrips; }

	// 载具资产(Traffic持有)：每辆被分配给市民/组织的车一个。
	const std::vector<VehicleAsset*>& GetVehicleAssets() const { return vehicleAssets; }

	// 用VehicleFactory以id创建一辆新车、按name存进vehicles(name已存在会先delete旧的)。
	// 创建失败(id未注册/未在config.json"vehicle_mods"启用)返回nullptr，不会往vehicles里塞
	// 半成品。
	Vehicle* CreateVehicle(const std::string& modId, const std::string& name);
	void DestroyVehicle(const std::string& name);
	Vehicle* FindVehicleByName(const std::string& name) const;
	const std::unordered_map<std::string, Vehicle*>& GetVehicles() const;

	Station* FindStationByName(const std::string& name) const;
	const std::unordered_map<std::string, Station*>& GetStations() const;
	const std::unordered_map<std::string, Route*>& GetRoutes() const;

	// 按Time::DifferenceInSeconds累加经过的游戏秒数(elapsedSeconds单调递增，跨天由
	// DifferenceInSeconds自己的日历感知差值处理，不需要另外补偿)，再对每条route调用
	// route->Update(elapsedSeconds)驱动线路车辆。
	void Tick(const Time& currentTime, bool crossedDay, PostHandle* post);

	// 取走所有线路自上次调用以来的停站/离站事件(见Route::StopEvent)，AForeverFrameworkActor在
	// Tick之后调用、转发给对应载具的script。读档后调用SilenceInitialStopEvents()让下一次Tick不补发。
	std::vector<Route::StopEvent> ConsumeStopEvents();
	void SilenceInitialStopEvents();

	// 认识add_option/remove_option(按name找Vehicle)；返回是否认识该类型，找不到目标静默忽略。
	// EnterVehicleChange是UE-only效果，不在这里处理。
	bool ApplyChange(const Change* change, const ScriptContext& context);

private:
	void InitStations(Map* map);
	void InitRoadsideStations(Map* map);
	void InitRoutes(Map* map);
	// 给所有stocked车位(BuildingMod::AssignStockedParking，如机场的飞机、港口的船)预置一辆车，不挂资产；Core不认识具体种类。
	void InitStockedVehicles(Map* map);
	// 生成一辆category类的车放进room的第slotIndex个车位，建VehicleAsset(Traffic持有)并挂到所有者的资产列表。
	VehicleAsset* CreateOwnedVehicle(const std::string& category, Room* room, int slotIndex,
		Citizen* ownerCitizen, Organization* ownerOrganization);
	void RemoveVehicleAsset(const Vehicle* vehicle);
	void InitTransitVehicles();

	VehicleFactory& vehicleFactory;
	StationFactory& stationFactory;
	RouteFactory& routeFactory;
	ScriptFactory& scriptFactory;

	std::unordered_map<std::string, Vehicle*> vehicles; // 持有所有权
	std::vector<VehicleAsset*> vehicleAssets;           // 持有所有权，指向vehicles里的车，销毁车时同步删

	struct PendingVehicleAsset {
		std::string vehicleName;
		int ownerKind = 0;   // 0市民 1组织
		int ownerIndex = -1; // populace.citizens / society.organizations下标
	};
	std::vector<PendingVehicleAsset> pendingVehicleAssets; // 读档后等ResolveVehicleAssets
	bool restoredFromSave = false;

	std::unordered_map<const Citizen*, CommuteTrip> trips;
	struct PendingTrip {
		int citizenIndex = -1;
		std::string vehicleName;
		Room* finalDest = nullptr;
		Room* parkOrigin = nullptr;
		Room* parkDest = nullptr;
		int destSlot = -1;
		int phase = 0;
	};
	std::vector<PendingTrip> pendingTrips; // 读档后等ResolveVehicleAssets(市民下标要等人口恢复)
	std::list<TaskTrip> taskTrips;         // list：指针/引用在增删其它行程时保持有效
	struct PendingTask {
		std::string task;
		int workerIndex = -1;
		std::string vehicleName;
		Room* parkOrigin = nullptr;
		Room* parkDest = nullptr;
		int destSlot = -1;
		Room* destRoom = nullptr;
		Room* originRoom = nullptr;
		int phase = 0;
	};
	std::vector<PendingTask> pendingTasks; // 读档后等ResolveVehicleAssets
	std::unordered_map<std::string, Station*> stations; // 持有所有权(含Route::Build()产出的边缘站点)
	std::unordered_map<std::string, Route*> routes;     // 持有所有权

	int vehicleCounter = 0; // 生成车辆时拼唯一name用，如"Vehicle0"/"Transit0"
	int stationCounter = 0; // 生成站点时拼唯一name用，如"Station0"

	bool hasLastTickTime = false;
	Time lastTickTime;
	float elapsedSeconds = 0.f;
};
