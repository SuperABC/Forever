#pragma once

#include "terrain.h"
#include "roadnet.h"
#include "zone.h"
#include "building.h"
#include "component.h"
#include "room.h"

#include <windows.h>

#define BLOCK_SIZE 256


class Element {
public:
	Element() = default;
	~Element() = default;

	//获取/设置园区标识
	int GetZoneId();
	void SetZoneId(int zoneId);

	//获取/设置建筑标识
	int GetBuildingId();
	void SetBuildingId(int buildingId);

private:
	int zoneId = -1;
	int buildingId = -1;
};

class Block {
public:
	Block() = delete;
	Block(int x, int y);
	~Block();

	//获取地块内某全局坐标的信息
	std::shared_ptr<Element> GetElement(int x, int y);

private:
	int offsetX, offsetY;
	std::vector<std::vector<std::shared_ptr<Element>>> elements;

	//检查全局坐标是否在地块内
	bool CheckXY(int x, int y);
};

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

	// 初始化全部静态地图
	int Init(int blockX, int blockY);

	// 释放空间
	void Destroy();

	// 时钟前进
	void Tick();

	// 保存/加载地图
	void Load(std::string path);
	void Save(std::string path);

private:
	std::vector<HMODULE> modHandles;
	std::shared_ptr<TerrainFactory> terrainFactory;
	std::shared_ptr<RoadnetFactory> roadnetFactory;
	std::shared_ptr<ZoneFactory> zoneFactory;
	std::shared_ptr<BuildingFactory> buildingFactory;
	std::shared_ptr<ComponentFactory> componentFactory;
	std::shared_ptr<RoomFactory> roomFactory;

	int width = 0, height = 0;
	std::vector<std::vector<std::shared_ptr<Block>>> blocks;

	//检查全局坐标是否在地图内
	bool CheckXY(int x, int y);

};

