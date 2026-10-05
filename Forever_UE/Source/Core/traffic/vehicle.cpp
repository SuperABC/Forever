#include "traffic/vehicle.h"

#include "story/script.h"
#include "story/script_factory.h"
#include "common/config.h"

using namespace std;

Vehicle::Vehicle(VehicleFactory* factory, ScriptFactory* scriptFactory, const string& id, const string& name) :
	factory(factory), name(name) {
	mod = factory->CreateVehicle(id);
	if (mod) mod->SetProperty();

	// scriptModName/milestoneNames由mod自己的SetProperty()决定(见vehicle_mod.h"Script配置"
	// 一节)，这里不替mod做任何选择——mod为空是CreateVehicle本身失败的防御性兜底，仍然
	// 硬编码"empty"作为最后一道保险，照抄Job::Job同一套写法。
	script = new Script(scriptFactory, mod ? mod->scriptModName : "empty");
	if (mod) {
		for (const string& milestoneName : mod->milestoneNames) {
			script->ReadMilestones(Config::GetScriptPath(milestoneName));
		}
		// 供milestone脚本里$$self.name引用这辆车的唯一名字——用Vehicle自己的name(调用方在
		// Traffic::CreateVehicle里传入、Traffic用它做unordered_map键)，不是mod->GetName()，
		// 因为多个Vehicle实例可能共享同一个VehicleMod子类，mod->GetName()不保证跨实例唯一。
		script->SetValue("name", ValueType(name));
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
	static const string car = "car";
	return mod ? mod->category : car;
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
}

Route* Vehicle::GetRoute() const { return route; }
int Vehicle::GetRouteLine() const { return routeLine; }

Script* Vehicle::GetScript() const { return script; }

const vector<string>& Vehicle::GetOptions() const { return options; }

void Vehicle::AddOption(const string& option) {
	options.push_back(option);
}

void Vehicle::SetParking(Room* inRoom, float localX, float localY, float localRotationDegrees) {
	room = inRoom;
	parkingLocalX = localX;
	parkingLocalY = localY;
	parkingRotationDegrees = localRotationDegrees;
}

Room* Vehicle::GetRoom() const { return room; }

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
