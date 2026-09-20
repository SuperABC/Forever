#include "component.h"

#include "common/error.h"


using namespace std;

Component::Component(ComponentFactory* factory, ComponentMod* mod, Building* parentBuilding) :
	mod(mod),
	factory(factory),
	type(),
	name(),
	parentBuilding(parentBuilding) {
	if (!mod) {
		THROW_EXCEPTION(NullPointerException, "Component mod is null.\n");
	}

	type = mod->GetType();
	name = mod->GetName();
}

Component::~Component() {
	factory->DestroyComponent(mod);
}

string Component::GetType() const { return type; }
string Component::GetName() const { return name; }
ComponentMod* Component::GetMod() const { return mod; }
Building* Component::GetParentBuilding() const { return parentBuilding; }
const vector<Room*>& Component::GetRooms() const { return rooms; }

void Component::AddRoom(Room* room) {
	rooms.push_back(room);
}
