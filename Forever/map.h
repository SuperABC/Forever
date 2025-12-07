#pragma once

#include "terrain.h"
#include "roadnet.h"
#include "zone.h"
#include "building.h"
#include "component.h"
#include "room.h"
#include "person.h"
#include "change.h"
#include "story.h"

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
	std::string GetZone() const;
	bool SetZone(std::string zone);

	//获取/设置建筑标识
	std::string GetBuilding() const;
	bool SetBuilding(std::string building);

private:
	std::string terrain = "plain";
	std::string zone;
	std::string building;
};

class Block {
public:
	Block() = delete;
	Block(int x, int y);
	~Block();

	//获取/设置地形名称
	std::string GetTerrain(int x, int y) const;
	bool SetTerrain(int x, int y, std::string terrain);

	//检查全局坐标是否在地块内
	bool CheckXY(int x, int y) const;

	//获取地块内某全局坐标的信息
	std::shared_ptr<Element> GetElement(int x, int y);

private:
	// 基础内容
	int offsetX, offsetY;
	std::vector<std::vector<std::shared_ptr<Element>>> elements;
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

	// 读取配置文件
	void ReadConfigs(std::string path) const;

	// 初始化全部静态地图
	int Init(int blockX, int blockY);

	// 市民入驻
	void Checkin(std::vector<std::shared_ptr<Person>> citizens, Time time);

	// 释放空间
	void Destroy();

	// 时钟前进
	void Tick();

	// 输出当前地图
	void Print();

	// 应用变更
	void ApplyChange(std::shared_ptr<Change> change, std::unique_ptr<Story>& story);

	// 保存/加载地图
	void Load(std::string path);
	void Save(std::string path);

	// 获取地图尺寸
	std::pair<int, int> GetSize();

	// 检查全局坐标是否在地图内
	bool CheckXY(int x, int y) const;

	// 获取地块
	std::shared_ptr<Block> GetBlock(int x, int y) const;

	// 获取元素
	std::shared_ptr<Element> GetElement(int x, int y) const;

	// 获取路网
	std::shared_ptr<Roadnet> GetRoadnet() const;

	// 获取组合
	std::vector<std::shared_ptr<Component>> GetComponents() const;

	// 获取/设置元素所属园区/建筑
	std::shared_ptr<Zone> GetZone(std::string name);
	std::shared_ptr<Building> GetBuilding(std::string name);
	void SetZone(std::shared_ptr<Zone> zone, std::string name);
	void SetBuilding(std::shared_ptr<Building> building, std::string name);
	void SetBuilding(std::shared_ptr<Building> building, std::string name, std::pair<float, float> offset);

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

	// 地图架构
	std::shared_ptr<Roadnet> roadnet;
	std::unordered_map<std::string, std::shared_ptr<Zone>> zones;
	std::unordered_map<std::string, std::shared_ptr<Building>> buildings;
	std::unique_ptr<Layout> layout;

	// 获取/设置地形名称
	std::string GetTerrain(int x, int y) const;
	bool SetTerrain(int x, int y, std::string terrain);

	// 排列地块中的园区与建筑
	void ArrangePlots();
};

