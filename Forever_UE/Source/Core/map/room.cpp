#include "room.h"

#include "common/error.h"

#include <sstream>
#include <iomanip>

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
