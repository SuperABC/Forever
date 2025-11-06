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

	// 读取Mods
	void InitTerrains();
	void InitRoadnets();
	void InitZones();
	void InitBuildings();
	void InitComponents();
	void InitRooms();

private:
	std::vector<HMODULE> modHandles;
	shared_ptr<TerrainFactory> terrainFactory;
	shared_ptr<RoadnetFactory> roadnetFactory;
	shared_ptr<ZoneFactory> zoneFactory;
	shared_ptr<BuildingFactory> buildingFactory;
	shared_ptr<ComponentFactory> componentFactory;
	shared_ptr<RoomFactory> roomFactory;

	int width = 0, height = 0;
};

