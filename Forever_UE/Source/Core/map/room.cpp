#include "room.h"

#include "common/error.h"

#include "map/building.h"
#include "map/door.h"
#include "player/asset.h"
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
	properties.isResidential = mod->isResidential;
	properties.residentialCapacity = mod->residentialCapacity;
	properties.isWorkspace = mod->isWorkspace;
	properties.workspaceCapacity = mod->workspaceCapacity;
	properties.isStorage = mod->isStorage;
	properties.storageConfig = mod->storageConfig;
	properties.isManufacture = mod->isManufacture;
	properties.manufactureTypes = mod->manufactureTypes;
	properties.isParking = mod->isParking;
	properties.parkingSpots = mod->parkingSpots;
	scriptModName = mod->scriptModName;
	scriptMilestones = mod->milestoneNames;
}

Room::~Room() {
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
Building* Room::GetParentBuilding() const { return parentBuilding; }
Component* Room::GetParentComponent() const { return parentComponent; }
const WallHole& Room::GetDoors() const { return doors; }
const DoorTags& Room::GetDoorTags() const { return doorTags; }
const WallHole& Room::GetWindows() const { return windows; }
void Room::SetDoors(WallHole d, DoorTags tags) { doors = std::move(d); doorTags = std::move(tags); }
void Room::SetWindows(WallHole w) { windows = std::move(w); }

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

bool Room::IsResidential() const { return properties.isResidential; }
int Room::ResidentialCapacity() const { return properties.residentialCapacity; }
bool Room::IsWorkspace() const { return properties.isWorkspace; }
int Room::WorkspaceCapacity() const { return properties.workspaceCapacity; }
bool Room::IsStorage() const { return properties.isStorage; }
unordered_map<string, float> Room::StorageConfig() const { return properties.storageConfig; }
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

const vector<Citizen*>& Room::GetTenants() const { return tenants; }

void Room::AddTenant(Citizen* citizen) {
	if (citizen) tenants.push_back(citizen);
}

void Room::RemoveTenant(Citizen* citizen) {
	tenants.erase(remove(tenants.begin(), tenants.end(), citizen), tenants.end());
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
