#pragma once

#include "terrain.h"
#include "roadnet.h"
#include "zone.h"
#include "building.h"
#include "component.h"
#include "room.h"

#include <windows.h>


class Map {
public:
	Map();
	~Map();

	void InitTerrains(std::shared_ptr<TerrainFactory> factory);
	void InitRoadnets(std::shared_ptr<RoadnetFactory> factory);
	void InitZones(std::shared_ptr<ZoneFactory> factory);
	void InitBuildings(std::shared_ptr<BuildingFactory> factory);
	void InitComponents(std::shared_ptr<ComponentFactory> factory);
	void InitRooms(std::shared_ptr<RoomFactory> factory);

private:
	std::vector<HMODULE> modHandles;
};

