#include "room.h"

#include "common/error.h"

#include "map/building.h"
#include "map/door.h"
#include "player/asset.h"
#include "populace/citizen.h"
#include "populace/household.h"
#include "story/script.h"

#include <sstream>
#include <iomanip>
#include <algorithm>


using namespace std;

Room::Room(RoomFactory* factory, RoomMod* mod, Building* parentBuilding, Component* parentComponent, int layer) :
	Quad(),
	mod(mod),
	factory(factory),
	type(),
	name(),
	parentBuilding(parentBuilding),
	parentComponent(parentComponent),
	layer(layer) {
	if (!mod) {
		THROW_EXCEPTION(NullPointerException, "Room mod is null.\n");
	}

	mod->SetProperty();
	type = mod->GetType();
	name = mod->GetName();
	properties.isResidence = mod->isResidence;
	properties.residenceCapacity = mod->residenceCapacity;
	properties.isWorkspace = mod->isWorkspace;
	properties.workspaceCapacity = mod->workspaceCapacity;
	properties.isStorage = mod->isStorage;
	properties.storageType = mod->storageType;
	properties.rent = mod->rent;
	properties.isManufacture = mod->isManufacture;
	properties.manufactureTypes = mod->manufactureTypes;
	properties.isParking = mod->isParking;
	properties.parkingSpots = mod->parkingSpots;
	scriptModName = mod->scriptModName;
	scriptMilestones = mod->milestoneNames;
}

Room::~Room() {
	for (Household* household : households) delete household;
	factory->DestroyRoom(mod);
	for (auto& [name, asset] : assets) delete asset; // 没人捡走的地上物品，房间销毁时清理
	for (Door* door : doorEntities) delete door;
	delete script;
}

string Room::GetType() const { return type; }
string Room::GetName() const { return name; }
RoomMod* Room::GetMod() const { return mod; }
int Room::GetLayer() const { return layer; }
int Room::GetDirection() const { return direction; }
void Room::SetDirection(int dir) { direction = dir; }

// 朝向无效按朝北；W/E时u、v两个轴和建筑局部X/Y对调。
static int EffectiveFace(int direction) {
	return (direction >= 0 && direction < 4) ? direction : FACE_NORTH;
}

float Room::AxisSizeU() const {
	int face = EffectiveFace(direction);
	return (face == FACE_WEST || face == FACE_EAST) ? GetSizeY() : GetSizeX();
}

float Room::AxisSizeV() const {
	int face = EffectiveFace(direction);
	return (face == FACE_WEST || face == FACE_EAST) ? GetSizeX() : GetSizeY();
}

void Room::LocalToBuildingLocal(float u, float v, float& offsetX, float& offsetY) const {
	float halfX = GetSizeX() * 0.5f, halfY = GetSizeY() * 0.5f;
	switch (EffectiveFace(direction)) {
	case FACE_WEST:  offsetX = v - halfX; offsetY = halfY - u; break;
	case FACE_EAST:  offsetX = halfX - v; offsetY = u - halfY; break;
	case FACE_SOUTH: offsetX = halfX - u; offsetY = halfY - v; break;
	default:         offsetX = u - halfX; offsetY = v - halfY; break; // FACE_NORTH
	}
}

float Room::FacingYawDegrees() const {
	static const float kFacingYawDegrees[4] = { 180.f, 0.f, 90.f, 270.f }; // 下标=FACE_WEST/EAST/NORTH/SOUTH
	return kFacingYawDegrees[EffectiveFace(direction)];
}

pair<float, float> Room::LocalToWorld(float u, float v) const {
	float offsetX = 0.f, offsetY = 0.f;
	LocalToBuildingLocal(u, v, offsetX, offsetY);
	return parentBuilding->LocalToWorld(GetPosX() + offsetX, GetPosY() + offsetY);
}

float Room::LocalYawToWorld(float yawDegrees) const {
	const float degToRad = 3.14159265358979323846f / 180.f;
	return parentBuilding->GetRotation() + (FacingYawDegrees() + yawDegrees) * degToRad;
}

Building* Room::GetParentBuilding() const { return parentBuilding; }
Component* Room::GetParentComponent() const { return parentComponent; }
const WallHole& Room::GetDoors() const { return doors; }
const DoorTags& Room::GetDoorTags() const { return doorTags; }
const WallHole& Room::GetWindows() const { return windows; }
void Room::SetDoors(WallHole d, DoorTags tags) { doors = std::move(d); doorTags = std::move(tags); }
void Room::SetWindows(WallHole w) { windows = std::move(w); }
const WallStyle& Room::GetWallStyle(int dir) const { return wallStyles[dir < 0 || dir >= 4 ? 0 : dir]; }
void Room::SetWallStyle(int dir, const WallStyle& style) { if (dir >= 0 && dir < 4) wallStyles[dir] = style; }

void Room::AddDoorEntity(Door* door) {
	if (door) doorEntities.push_back(door);
}

const vector<Door*>& Room::GetDoorEntities() const { return doorEntities; }
const string& Room::GetNumber() const { return number; }

void Room::SetNumber(int level, int seq) {
	ostringstream oss;
	if (level < 0) {
		oss << 'b' << -level;
	} else {
		oss << level;
	}
	oss << setw(4) << setfill('0') << seq;
	number = oss.str();
}

Script* Room::GetScript() const { return script; }

Node* Room::GetNavigationNode() const { return navigationNode; }
void Room::SetNavigationNode(Node* node) { navigationNode = node; }

bool Room::IsResidence() const { return properties.isResidence; }
int Room::ResidenceCapacity() const { return properties.residenceCapacity; }
bool Room::IsWorkspace() const { return properties.isWorkspace; }
int Room::WorkspaceCapacity() const { return properties.workspaceCapacity; }
bool Room::IsStorage() const { return properties.isStorage; }
string Room::StorageType() const { return properties.storageType; }
int64_t Room::Rent() const { return properties.rent; }
bool Room::IsManufacture() const { return properties.isManufacture; }
vector<string> Room::ManufactureTypes() const { return properties.manufactureTypes; }
bool Room::IsParking() const { return properties.isParking; }
const vector<ParkingSpot>& Room::GetParkingSpots() const { return properties.parkingSpots; }

string Room::GetAddress() const {
	if (!parentBuilding) return number;
	return parentBuilding->GetAddress() + " " + number;
}

Citizen* Room::GetOwner() const { return owner; }
void Room::SetOwner(Citizen* value) { owner = value; }
bool Room::GetStated() const { return stated; }
void Room::SetStated(bool value) { stated = value; }

const vector<Household*>& Room::GetHouseholds() const { return households; }

Household* Room::AddHousehold() {
	Household* household = new Household(this);
	households.push_back(household);
	return household;
}

Household* Room::AddResident(Citizen* citizen) {
	if (!citizen) return nullptr;

	// 找出与新住户有亲属关系的全部户：只有一户就并入，有多户说明新住户把它们连起来了，合并成一户。
	Household* target = nullptr;
	for (size_t i = 0; i < households.size();) {
		Household* household = households[i];
		if (household->IsRelatedTo(citizen)) {
			if (!target) {
				target = household;
				i++;
			}
			else {
				target->Absorb(*household);
				for (Citizen* member : target->GetMembers()) member->SetHousehold(target);
				delete household;
				households.erase(households.begin() + i);
			}
		}
		else {
			i++;
		}
	}
	if (!target) target = AddHousehold();
	target->AddMember(citizen);
	citizen->SetHousehold(target);
	return target;
}

const vector<Citizen*>& Room::GetOccupants() const { return occupants; }

void Room::AddOccupant(Citizen* citizen) {
	if (citizen) occupants.push_back(citizen);
}

void Room::RemoveOccupant(Citizen* citizen) {
	occupants.erase(remove(occupants.begin(), occupants.end(), citizen), occupants.end());
}

const unordered_map<string, Asset*>& Room::GetAssets() const { return assets; }

void Room::AddAsset(Asset* asset) {
	if (asset) assets.insert_or_assign(asset->GetName(), asset);
}

Asset* Room::RemoveAsset(const string& name) {
	auto it = assets.find(name);
	if (it == assets.end()) return nullptr;
	Asset* removed = it->second;
	assets.erase(it);
	return removed;
}
