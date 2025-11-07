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

	//获取/设置地形名称
	std::string GetTerrain() const;
	bool SetTerrain(std::string terrain);

	//获取/设置园区标识
	int GetZone() const;
	bool SetZone(int zone);

	//获取/设置建筑标识
	int GetBuilding() const;
	bool SetBuilding(int building);

private:
	std::string terrain = "平原地形";
	int zone = -1;
	int building = -1;
};

class Block {
public:
	Block() = delete;
	Block(int x, int y);
	~Block();

	//获取/设置地形名称
	std::string GetTerrain(int x, int y) const;
	bool SetTerrain(int x, int y, std::string terrain);

private:
	// 基础内容
	int offsetX, offsetY;
	std::vector<std::vector<std::shared_ptr<Element>>> elements;

	//检查全局坐标是否在地块内
	bool CheckXY(int x, int y) const;
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
	// Mod管理
	std::vector<HMODULE> modHandles;
	std::unique_ptr<TerrainFactory> terrainFactory;
	std::unique_ptr<RoadnetFactory> roadnetFactory;
	std::unique_ptr<ZoneFactory> zoneFactory;
	std::unique_ptr<BuildingFactory> buildingFactory;
	std::unique_ptr<ComponentFactory> componentFactory;
	std::unique_ptr<RoomFactory> roomFactory;

	// 基础内容
	int width = 0, height = 0;
	std::vector<std::vector<std::shared_ptr<Block>>> blocks;

	//检查全局坐标是否在地图内
	bool CheckXY(int x, int y) const;

	//获取/设置地形名称
	std::string GetTerrain(int x, int y) const;
	bool SetTerrain(int x, int y, std::string terrain);

};

