#include "traffic/vehicle.h"

#include "common/error.h"

#include "map/room.h"
#include "story/script.h"
#include "story/script_factory.h"
#include "common/config.h"

#include <algorithm>
#include <cmath>

using namespace std;

Vehicle::Vehicle(VehicleFactory* factory, ScriptFactory* scriptFactory, const string& id, const string& name) :
	factory(factory), mod(nullptr), modId(id), name(name) {
	// 种类就是mod注册的那个字符串，Core不认识任何具体的种类名。mod没声明种类就是空串：这样的车不匹配任何车位、
	// 不会被任何按种类的逻辑挑中(注册时VehicleFactory已经打过警告，ModTest也会判失败)。
	category = factory->GetCategory(id);
	mod = factory->CreateVehicle(id);
	if (mod) MOD_CALL(mod, mod->SetProperty());

	// scriptModName/milestoneNames由mod自己的SetProperty()决定(见vehicle_mod.h"Script配置"一节)，
	// 都空则没有脚本(script为空)，见MakeScript。
	script = mod ? MakeScript(scriptFactory, mod->scriptModName, mod->milestoneNames) : nullptr;
	if (script) {
		script->SetOwner(ScriptOwnerKind::Vehicle, this);
		script->SetStableKey("vehicle:" + name); // 同ScriptSaveCodec::CollectScripts的键
		// 供milestone脚本里$$self.name引用这辆车的唯一名字——用Vehicle自己的name(调用方在
		// Traffic::CreateVehicle里传入、Traffic用它做unordered_map键)，不是mod->GetName()，
		// 因为多个Vehicle实例可能共享同一个VehicleMod子类，mod->GetName()不保证跨实例唯一。
		script->SetValue("name", ValueType(name));
		// $$self.on_route：这辆车是不是挂在线路上(SetRouteBinding时置1)。同一个VehicleMod既能
		// 跑线路又能停车位，车辆script靠它区分要不要提供"驾驶"选项。
		script->SetValue("on_route", ValueType(0));
	}
}

Vehicle::~Vehicle() {
	delete script;
	if (mod) factory->DestroyVehicle(mod);
}

bool Vehicle::IsValid() const { return mod != nullptr; }

const string& Vehicle::GetName() const { return name; }

string Vehicle::GetType() const {
	return mod ? mod->GetType() : string();
}

const string& Vehicle::GetBlueprintPath() const {
	static const string empty;
	return mod ? mod->blueprintPath : empty;
}

const string& Vehicle::GetTransitMeshPath() const {
	static const string empty;
	return mod ? mod->transitMeshPath : empty;
}

float Vehicle::GetMeshScale() const { return mod ? mod->meshTransform.scale : 1.f; }
float Vehicle::GetMeshYawOffsetDegrees() const { return mod ? mod->meshTransform.yawOffsetDegrees : 0.f; }

float Vehicle::GetExitOffsetX() const { return mod ? mod->exitOffsetX : 0.f; }
float Vehicle::GetExitOffsetY() const { return mod ? mod->exitOffsetY : 0.f; }
float Vehicle::GetExitOffsetZ() const { return mod ? mod->exitOffsetZ : 0.f; }

const string& Vehicle::GetCategory() const {
	return category;
}

bool Vehicle::IsDrivable() const { return mod ? mod->drivable : false; }
bool Vehicle::IsBoardable() const { return mod ? mod->boardable : false; }
int Vehicle::GetCapacity() const { return mod ? mod->capacity : 0; }

void Vehicle::GetSize(float& outX, float& outY, float& outZ) const {
	outX = mod ? mod->sizeX : 100.f;
	outY = mod ? mod->sizeY : 100.f;
	outZ = mod ? mod->sizeZ : 100.f;
}

void Vehicle::SetRouteBinding(Route* inRoute, int line) {
	route = inRoute;
	routeLine = line;
	if (script) script->SetValue("on_route", ValueType(inRoute ? 1 : 0));
}

Route* Vehicle::GetRoute() const { return route; }
int Vehicle::GetRouteLine() const { return routeLine; }

Script* Vehicle::GetScript() const { return script; }

const vector<string>& Vehicle::GetOptions() const { return options; }

void Vehicle::AddOption(const string& option) {
	options.push_back(option);
}

void Vehicle::RemoveOption(const string& option) {
	options.erase(remove(options.begin(), options.end(), option), options.end());
}

void Vehicle::SetParking(Room* inRoom, float localX, float localY, float localRotationDegrees) {
	room = inRoom;
	currentRoom = inRoom;
	parkingLocalX = localX;
	parkingLocalY = localY;
	parkingRotationDegrees = localRotationDegrees;
}

Room* Vehicle::GetRoom() const { return room; }

ParkingSlot* Vehicle::GetParkingSlot() const {
	if (!room || parkingSlotIndex < 0 || parkState != ParkState::Parked) return nullptr;
	const vector<ParkingSlot*>& slots = room->GetParkingSlots();
	if (parkingSlotIndex >= static_cast<int>(slots.size())) return nullptr;
	return slots[parkingSlotIndex];
}

void Vehicle::BindSlot(Room* inRoom, int slotIndex) {
	ReleaseSlot();
	if (!inRoom || slotIndex < 0 || slotIndex >= static_cast<int>(inRoom->GetParkingSlots().size())) return;
	ParkingSlot* slot = inRoom->GetParkingSlots()[slotIndex];
	room = inRoom;
	parkingSlotIndex = slotIndex;
	parkState = ParkState::Parked;
	slot->parkedVehicle = this;
	if (slot->reservedBy == this) slot->reservedBy = nullptr;
	// UE层GenerateVehicles按"房间中心+局部偏移"换算世界位置，局部偏移相对房间中心。
	parkingLocalX = slot->centerX - inRoom->GetPosX();
	parkingLocalY = slot->centerY - inRoom->GetPosY();
	parkingRotationDegrees = slot->VehicleYawDegrees();
	currentRoom = inRoom;
}

void Vehicle::ReleaseSlot() {
	if (ParkingSlot* slot = GetParkingSlot()) {
		if (slot->parkedVehicle == this) slot->parkedVehicle = nullptr;
	}
	parkingSlotIndex = -1;
	parkState = ParkState::None;
}

ParkingSlot* Vehicle::GetReservedSlot() const {
	if (!reservedRoom || reservedSlotIndex < 0) return nullptr;
	const vector<ParkingSlot*>& slots = reservedRoom->GetParkingSlots();
	if (reservedSlotIndex >= static_cast<int>(slots.size())) return nullptr;
	return slots[reservedSlotIndex];
}

bool Vehicle::ReserveSlot(Room* inRoom, int slotIndex) {
	ClearReservation();
	if (!inRoom || slotIndex < 0 || slotIndex >= static_cast<int>(inRoom->GetParkingSlots().size())) return false;
	ParkingSlot* slot = inRoom->GetParkingSlots()[slotIndex];
	if (slot->reservedBy && slot->reservedBy != this) return false;
	if (slot->parkedVehicle && slot->parkedVehicle != this) return false;
	slot->reservedBy = this;
	reservedRoom = inRoom;
	reservedSlotIndex = slotIndex;
	return true;
}

void Vehicle::ClearReservation() {
	if (ParkingSlot* slot = GetReservedSlot()) {
		if (slot->reservedBy == this) slot->reservedBy = nullptr;
	}
	reservedRoom = nullptr;
	reservedSlotIndex = -1;
}

bool Vehicle::ArriveAtReservedSlot() {
	Room* targetRoom = reservedRoom;
	int targetIndex = reservedSlotIndex;
	if (!GetReservedSlot()) return false;
	ClearReservation();
	BindSlot(targetRoom, targetIndex);
	return true;
}

void Vehicle::SetVanished(Room* inRoom) {
	ReleaseSlot();
	ClearReservation();
	room = inRoom;
	currentRoom = inRoom;
	parkState = ParkState::Vanished;
}

Room* Vehicle::GetCurrentRoom() const { return currentRoom; }
void Vehicle::SetCurrentRoom(Room* inRoom) { currentRoom = inRoom; }

void Vehicle::GetParkingLocalPosition(float& outX, float& outY) const {
	outX = parkingLocalX;
	outY = parkingLocalY;
}

float Vehicle::GetParkingRotationDegrees() const { return parkingRotationDegrees; }

void Vehicle::SetTransform(float inX, float inY, float inZ, float inYaw) {
	x = inX; y = inY; z = inZ; yaw = inYaw;
}

void Vehicle::GetTransform(float& outX, float& outY, float& outZ, float& outYaw) const {
	outX = x; outY = y; outZ = z; outYaw = yaw;
}

bool Vehicle::IsDriving() const { return driveDestination != nullptr; }
Room* Vehicle::GetDriveDestination() const { return driveDestination; }

void Vehicle::BeginDrive(const std::vector<DrivePoint>& path, Room* destination, double nowSeconds, float speed) {
	EndDrive();
	if (path.empty() || !destination || speed <= 0.f) return;
	drivePath = path;
	driveDistances.assign(path.size(), 0.f);
	for (size_t i = 1; i < path.size(); i++) {
		float dx = path[i].x - path[i - 1].x, dy = path[i].y - path[i - 1].y, dz = path[i].z - path[i - 1].z;
		driveDistances[i] = driveDistances[i - 1] + sqrt(dx * dx + dy * dy + dz * dz);
	}
	driveDestination = destination;
	driveStartTime = nowSeconds;
	driveSpeed = speed;
	float startYaw = yaw;
	if (path.size() > 1) startYaw = atan2(path[1].y - path[0].y, path[1].x - path[0].x) * 180.f / 3.14159265f + (path[0].reverse ? 180.f : 0.f);
	SetTransform(path[0].x, path[0].y, path[0].z, startYaw);
}

void Vehicle::SampleDrive(double nowSeconds, DrivePoint& outPoint, size_t& outSegment, bool& outFinished, float& outYaw) const {
	float travelled = static_cast<float>(max(0.0, nowSeconds - driveStartTime)) * driveSpeed;
	size_t last = drivePath.size() - 1;
	if (travelled >= driveDistances.back() || drivePath.size() < 2) {
		outPoint = drivePath.back();
		outSegment = last;
		outFinished = true;
		outYaw = yaw;
		if (last >= 1) outYaw = atan2(drivePath[last].y - drivePath[last - 1].y, drivePath[last].x - drivePath[last - 1].x) * 180.f / 3.14159265f
			+ (drivePath[last - 1].reverse ? 180.f : 0.f);
		return;
	}
	outFinished = false;
	size_t i = 1;
	while (driveDistances[i] < travelled) i++;
	float span = driveDistances[i] - driveDistances[i - 1];
	float t = span > 0.f ? (travelled - driveDistances[i - 1]) / span : 0.f;
	outPoint = { drivePath[i - 1].x + (drivePath[i].x - drivePath[i - 1].x) * t,
		drivePath[i - 1].y + (drivePath[i].y - drivePath[i - 1].y) * t,
		drivePath[i - 1].z + (drivePath[i].z - drivePath[i - 1].z) * t, drivePath[i - 1].reverse };
	outSegment = i;
	outYaw = atan2(drivePath[i].y - drivePath[i - 1].y, drivePath[i].x - drivePath[i - 1].x) * 180.f / 3.14159265f
		+ (drivePath[i - 1].reverse ? 180.f : 0.f);
}

bool Vehicle::AdvanceDrive(double nowSeconds) {
	if (!IsDriving()) return false;
	DrivePoint point;
	size_t segment;
	bool finished;
	float outYaw;
	SampleDrive(nowSeconds, point, segment, finished, outYaw);
	SetTransform(point.x, point.y, point.z, outYaw);
	if (finished) {
		Room* destination = driveDestination;
		EndDrive();
		SetCurrentRoom(destination);
		return true;
	}
	return false;
}

bool Vehicle::IsDriveReversing(double nowSeconds) const {
	if (!IsDriving()) return false;
	DrivePoint point;
	size_t segment;
	bool finished;
	float outYaw;
	SampleDrive(nowSeconds, point, segment, finished, outYaw);
	return !finished && point.reverse;
}

void Vehicle::GetRemainingDrive(double nowSeconds, std::vector<DrivePoint>& outPath) const {
	outPath.clear();
	if (!IsDriving()) return;
	DrivePoint point;
	size_t segment;
	bool finished;
	float outYaw;
	SampleDrive(nowSeconds, point, segment, finished, outYaw);
	outPath.push_back(point);
	for (size_t i = segment; i < drivePath.size() && !finished; i++) outPath.push_back(drivePath[i]);
}

void Vehicle::EndDrive() {
	driveDestination = nullptr;
	drivePath.clear();
	driveDistances.clear();
}

void Vehicle::SetPitch(float pitchDegrees) { pitch = pitchDegrees; }
float Vehicle::GetPitch() const { return pitch; }
