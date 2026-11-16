#pragma once

#include "class.h"

#include "traffic/vehicle_mod.h"
#include "traffic/vehicle_factory.h"

#include <string>
#include <vector>

struct ParkingSlot;


// Vehicle：一辆车的实体，独占持有一个VehicleMod实例(照抄Job持有JobMod)，详见vehicle.md。
class Vehicle {
	// 存档：TrafficSaveCodec直接读写私有字段(traffic_save.md)。
	friend class TrafficSaveCodec;
public:
	Vehicle(VehicleFactory* factory, ScriptFactory* scriptFactory, const std::string& id, const std::string& name);
	~Vehicle(); // factory->DestroyVehicle(mod)；delete script

	// mod为空(CreateVehicle传入的id没有被注册/没有在config.json"vehicle_mods"里启用)
	// 说明构造失败，调用方应当整个丢弃这个Vehicle，见Traffic::CreateVehicle。
	bool IsValid() const;

	const std::string& GetName() const;
	std::string GetType() const; // mod->GetType()，mod为空返回空字符串
	const std::string& GetBlueprintPath() const; // mod->blueprintPath，mod为空返回空字符串
	const std::string& GetTransitMeshPath() const; // mod->transitMeshPath，mod为空返回空字符串
	float GetMeshScale() const; // mod->meshTransform.scale，mod为空返回1.f
	float GetMeshYawOffsetDegrees() const; // mod->meshTransform.yawOffsetDegrees，mod为空返回0.f
	float GetExitOffsetX() const;
	float GetExitOffsetY() const;
	float GetExitOffsetZ() const;

	const std::string& GetCategory() const; // 车型注册的种类("car"/"bus"/"train"/"plane"...)，查不到返回"car"
	bool IsDrivable() const;
	bool IsBoardable() const;
	int GetCapacity() const;
	void GetSize(float& outX, float& outY, float& outZ) const; // 立方体占位尺寸(UE单位)

	// 这辆车是哪条线路的第几条line上的车——公共交通车辆(category!=car)由Traffic::InitRoutes()
	// 创建后设置一次，供UE层GenerateVehicles()判断该生成AVehicleElement还是
	// ATransitVehicleElement用；普通小汽车不调这个，route恒为nullptr。
	void SetRouteBinding(Route* route, int line);
	Route* GetRoute() const;
	int GetRouteLine() const;

	// 线路公交处于"物理段"（近处由UE的物理载具+Controller驱动）时为true：Route::Update对它跳过时刻表驱动和
	// 停站事件，位姿由载具Actor每帧写回。纯运行时，不进存档。停止运行后的线路公交会一直保持true。
	void SetPhysicsDriven(bool driven) { physicsDriven = driven; }
	bool IsPhysicsDriven() const { return physicsDriven; }

	Script* GetScript() const;
	const std::vector<std::string>& GetOptions() const;
	void AddOption(const std::string& option);
	void RemoveOption(const std::string& option);

	// 停车位信息：由Traffic::Init()在创建这辆预置车辆时设置一次，之后不会再变——车辆被
	// 开走之后"停在哪"这件事不再有意义，这几个字段只服务于"生成时应该摆在世界哪个位置"
	// 这一次性用途。
	void SetParking(Room* room, float localX, float localY, float localRotationDegrees);
	Room* GetRoom() const;
	void GetParkingLocalPosition(float& outX, float& outY) const;
	float GetParkingRotationDegrees() const;

	// ---- 车位占用(见Core/map/room.h ParkingSlot、vehicle.md"车位") ----
	// None：没停在车位里；Parked：已停在车位里(车位的parkedVehicle指向本车)；Vanished：被玩家停车挤掉位置又
	// 找不到别的车位，已从地图上消失(视为已到达room)，下次寻路直接瞬移到目的车位。
	// 另有独立的"预定"：寻路开始就占住目的车位(车位的reservedBy指向本车)，和上面的状态互不影响
	// (出库时先规划再释放原车位)，到达时转成Parked。
	enum class ParkState { None, Parked, Vanished };
	ParkState GetParkState() const { return parkState; }
	int GetParkingSlotIndex() const { return parkingSlotIndex; }
	ParkingSlot* GetParkingSlot() const; // 只有Parked返回非空
	// 停进room的第slotIndex个车位(先释放原来停的)：同时把parking*字段刷成车位的位置/朝向，currentRoom=room。
	void BindSlot(Room* room, int slotIndex);
	void ReleaseSlot();                    // 离开车位，状态回到None(room保留成最后停靠的房间)
	void SetVanished(Room* room);          // 离开车位和预定，状态Vanished，room/currentRoom=room
	// 读档恢复用：只设字段，不碰车位指针。
	void SetParkStateRaw(ParkState state, int slotIndex) { parkState = state; parkingSlotIndex = slotIndex; }

	bool ReserveSlot(Room* room, int slotIndex); // 目的车位被占用/越界返回false；先取消旧预定
	void ClearReservation();
	Room* GetReservedRoom() const { return reservedRoom; }
	int GetReservedSlotIndex() const { return reservedSlotIndex; }
	ParkingSlot* GetReservedSlot() const;
	// 到达：预定的车位转成Parked(没有预定则什么都不做，返回false)。
	bool ArriveAtReservedSlot();

	// 车辆当前所在的房间（运行时状态，不进存档）：SetParking时初始化成停车位所在房间，
	// 导航出发时置空，到达目的房间时设成该房间。nullptr表示在室外。导航规划靠它决定起点
	// 是房间的导航节点还是路网上最近的车道，见Map::PlanVehicleRoute。
	Room* GetCurrentRoom() const;
	void SetCurrentRoom(Room* inRoom);

	void SetTransform(float x, float y, float z, float yaw);
	void GetTransform(float& outX, float& outY, float& outZ, float& outYaw) const;
	// 俯仰角(角度制，抬头为正)：只有沿坡道/隧道口行驶的线路载具(Route::Update)会写，纯运行时，不进存档。
	void SetPitch(float pitchDegrees);
	float GetPitch() const;

	// ---- Core行驶推算（没有Actor的车，和Citizen::BeginWalk/AdvanceWalk对称，见vehicle.md）----
	// 路径点和速度都用UE单位(cm、cm/s)，和SetTransform一致；nowSeconds由调用方给（UE层用世界时间）。
	// 纯运行时，不进存档（读档后车停在最后一次写入的Transform上）。
	// reverse：从本点到下一点这一段是倒车(车头朝向与行进方向相反)，最后一个点的reverse无意义。
	struct DrivePoint { float x, y, z; bool reverse = false; };
	bool IsDriving() const;
	Room* GetDriveDestination() const;
	// 开始推算：车位置置为path[0]。path为空/destination为空/speed<=0则什么都不做（等于EndDrive）。
	void BeginDrive(const std::vector<DrivePoint>& path, Room* destination, double nowSeconds, float speed);
	// 推进到nowSeconds并写Transform(含yaw)；走完则EndDrive+SetCurrentRoom(destination)并返回true。
	bool AdvanceDrive(double nowSeconds);
	// 从当前位置起的剩余路径（首点=当前位置）。
	void GetRemainingDrive(double nowSeconds, std::vector<DrivePoint>& outPath) const;
	// 现在是否正走在倒车段上(出库倒车)。
	bool IsDriveReversing(double nowSeconds) const;
	void EndDrive();
	float GetDriveSpeed() const { return driveSpeed; }

	// ---- 存档用的行驶快照和"存档时是否有Actor"（见traffic_save.md） ----
	// 行驶状态平时纯运行时；存档前UE层把正在行驶的车的剩余路径/目的房间/速度写成快照(不论车有没有Actor)，
	// 存完清掉；读档后快照留到UE层GenerateVehicles里用BeginDrive恢复，再清掉。
	struct DriveSnapshot {
		std::vector<DrivePoint> path;
		Room* destination = nullptr;
		float speed = 0.f;
	};
	void SetDriveSnapshot(const DriveSnapshot& value) { driveSnapshot = value; hasDriveSnapshot = true; }
	void ClearDriveSnapshot() { driveSnapshot = DriveSnapshot(); hasDriveSnapshot = false; }
	bool HasDriveSnapshot() const { return hasDriveSnapshot; }
	const DriveSnapshot& GetDriveSnapshot() const { return driveSnapshot; }
	// 存档时这辆车是否有Actor(UE层存档前写)，读档后据此把它强制生成回来。
	bool WasSpawned() const { return wasSpawned; }
	void SetWasSpawned(bool value) { wasSpawned = value; }

private:
	DriveSnapshot driveSnapshot;
	bool hasDriveSnapshot = false;
	bool wasSpawned = false;

	VehicleFactory* factory;
	VehicleMod* mod;
	std::string modId;
	std::string category; // 构造时向factory按modId查一次(注册的static GetCategory())，未注册兜底"car"
	std::string name;
	Script* script;
	std::vector<std::string> options;

	Room* room = nullptr;
	Room* currentRoom = nullptr;
	float parkingLocalX = 0.f;
	float parkingLocalY = 0.f;
	float parkingRotationDegrees = 0.f;
	ParkState parkState = ParkState::None;
	int parkingSlotIndex = -1;
	Room* reservedRoom = nullptr;
	int reservedSlotIndex = -1;

	Route* route = nullptr;
	int routeLine = -1;
	bool physicsDriven = false;

	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	float yaw = 0.0f;
	float pitch = 0.0f;

	Room* driveDestination = nullptr;
	std::vector<DrivePoint> drivePath;
	std::vector<float> driveDistances; // 路径起点到第i个点的累计弧长
	double driveStartTime = 0.0;
	float driveSpeed = 0.f;
	void SampleDrive(double nowSeconds, DrivePoint& outPoint, size_t& outSegment, bool& outFinished, float& outYaw) const;
};
