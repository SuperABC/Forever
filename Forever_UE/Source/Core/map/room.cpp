#include "room.h"

#include "common/error.h"

#include "map/building.h"
#include "player/asset.h"

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

	type = mod->GetType();
	name = mod->GetName();
}

Room::~Room() {
	factory->DestroyRoom(mod);
	for (auto& [name, asset] : assets) delete asset; // 没人捡走的地上物品，房间销毁时清理
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
const WallHole& Room::GetWindows() const { return windows; }
void Room::SetDoors(WallHole d) { doors = std::move(d); }
void Room::SetWindows(WallHole w) { windows = std::move(w); }
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

Node* Room::GetNavigationNode() const { return navigationNode; }
void Room::SetNavigationNode(Node* node) { navigationNode = node; }

bool Room::IsResidential() const { return mod->isResidential; }
int Room::ResidentialCapacity() const { return mod->residentialCapacity; }
bool Room::IsWorkspace() const { return mod->isWorkspace; }
int Room::WorkspaceCapacity() const { return mod->workspaceCapacity; }
bool Room::IsStorage() const { return mod->isStorage; }
unordered_map<string, float> Room::StorageConfig() const { return mod->storageConfig; }
bool Room::IsManufacture() const { return mod->isManufacture; }
vector<string> Room::ManufactureTypes() const { return mod->manufactureTypes; }
bool Room::IsParking() const { return mod->isParking; }
const vector<ParkingSpot>& Room::GetParkingSpots() const { return mod->parkingSpots; }

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
