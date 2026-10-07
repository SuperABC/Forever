#include "door.h"

#include "map/building.h"
#include "map/room.h"
#include "map/zone.h"
#include "populace/citizen.h"
#include "story/script.h"
#include "story/script_factory.h"
#include "common/config.h"

#include <algorithm>

using namespace std;

Door::Door(const string& id, DOOR_KIND_TYPE kind, const string& tag, const DoorSpec& spec,
	Zone* zone, Building* building, Room* room, const string& interactName, bool flippedSide,
	float x, float y, float z, float width, float height, float yaw, int level, bool isVehicleGate,
	ScriptFactory* scriptFactory) :
	id(id), kind(kind), tag(tag), spec(spec), zone(zone), building(building), room(room),
	interactName(interactName), x(x), y(y), z(z), width(width), height(height), yaw(yaw),
	level(level), vehicleGate(isVehicleGate), flippedSide(flippedSide),
	access(static_cast<DOOR_ACCESS_TYPE>(spec.access)) {

	if (!interactName.empty()) {
		script = new Script(scriptFactory, spec.scriptModName);
		for (const string& milestoneName : spec.milestoneNames) {
			script->ReadMilestones(Config::GetScriptPath(milestoneName));
		}
		script->SetValue("name", ValueType(interactName));
		script->SetValue("label", ValueType(spec.name));
	}
}

Door::~Door() {
	delete script;
}

const string& Door::GetId() const { return id; }
const string& Door::GetName() const { return spec.name; }
const string& Door::GetInteractName() const { return interactName; }
const string& Door::GetTag() const { return tag; }
DOOR_KIND_TYPE Door::GetKind() const { return kind; }
Zone* Door::GetZone() const { return zone; }
Building* Door::GetBuilding() const { return building; }
Room* Door::GetRoom() const { return room; }

const string& Door::GetMesh() const { return spec.mesh; }
const string& Door::GetFrameMesh() const { return spec.frameMesh; }
int Door::GetStyle() const { return spec.style; }
int Door::GetLeaves() const { return spec.leaves; }
float Door::GetOpenAmount() const { return spec.openAmount; }
float Door::GetOpenSeconds() const { return spec.openSeconds; }

float Door::GetX() const { return x; }
float Door::GetY() const { return y; }
float Door::GetZ() const { return z; }
float Door::GetWidth() const { return width; }
float Door::GetHeight() const { return height; }
float Door::GetYaw() const { return yaw; }
int Door::GetLevel() const { return level; }
bool Door::IsVehicleGate() const { return vehicleGate; }

bool Door::IsFlippedSide() const { return flippedSide; }

DOOR_ACCESS_TYPE Door::GetAccess() const { return access; }
void Door::SetAccess(DOOR_ACCESS_TYPE value) { access = value; }

void Door::Allow(const string& citizenName) {
	if (!citizenName.empty()) allowedCitizenNames.insert(citizenName);
}

bool Door::CanPass(const Citizen* who, bool isPlayer) const {
	if (access == DOOR_ACCESS_LOCKED) return false;
	if (access == DOOR_ACCESS_OPEN) return true;

	// Owner：所属对象公有(GetStated()==true)时视同Open；isPlayer暂时按Open处理(这次没有
	// 真正的玩家门禁判定，见door.h注释)；否则who必须是所属对象的GetOwner()或者在Allow()
	// 登记过名字的citizen。
	bool stated = false;
	Citizen* owner = nullptr;
	if (zone) { stated = zone->GetStated(); owner = zone->GetOwner(); }
	else if (building) { stated = building->GetStated(); owner = building->GetOwner(); }
	else if (room) { stated = room->GetStated(); owner = room->GetOwner(); }

	if (stated) return true;
	if (isPlayer) return true;
	if (!who) return false;
	if (owner == who) return true;
	return allowedCitizenNames.find(who->GetName()) != allowedCitizenNames.end();
}

Script* Door::GetScript() const { return script; }
const vector<string>& Door::GetOptions() const { return options; }

void Door::AddOption(const string& option) {
	options.push_back(option);
}

void Door::RemoveOption(const string& option) {
	options.erase(remove(options.begin(), options.end(), option), options.end());
}

const DoorSpec* ResolveDoorSpec(const unordered_map<string, DoorSpec>& doorSpecs, const string& tag) {
	if (tag.empty()) return nullptr;
	auto it = doorSpecs.find(tag);
	if (it == doorSpecs.end()) return nullptr;
	if (it->second.mesh.empty()) return nullptr;
	return &it->second;
}
