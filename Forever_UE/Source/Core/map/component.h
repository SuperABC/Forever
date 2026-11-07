#pragma once

#include "class.h"

#include "map/component_mod.h"
#include "map/component_factory.h"

#include <string>
#include <vector>


// 若干Room的集合，设计见component.md
class Component {
	// 存档：MapSaveCodec直接读写私有字段(map_save.md)。
	friend class MapSaveCodec;
public:
	Component() = delete;

	Component(ComponentFactory* factory, ComponentMod* mod, Building* parentBuilding);
	~Component();

	std::string GetType() const;
	std::string GetName() const;

	ComponentMod* GetMod() const;

	Building* GetParentBuilding() const;

	const std::vector<Room*>& GetRooms() const;
	void AddRoom(Room* room);

private:
	ComponentMod* mod;
	ComponentFactory* factory;
	std::string type;
	std::string name;
	Building* parentBuilding;
	std::vector<Room*> rooms;
};
