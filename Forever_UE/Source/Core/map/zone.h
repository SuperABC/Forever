#pragma once

#include "class.h"

#include "map/zone_mod.h"
#include "map/zone_factory.h"
#include "map/general_geometry.h"

#include <string>
#include <unordered_map>
#include <vector>

class Script;


// 已落地的Zone占位(继承Quad表示自己占据的矩形)，独占一个ZoneMod实例，设计见zone.md。
// GetRotation()转发parentLot的值，必须在SetParentLot之后调用，之前返回0。
class Zone : public Quad {
	// 存档：MapSaveCodec直接读写私有字段(map_save.md)。
	friend class MapSaveCodec;
	// 实体脚本：Map::BuildZoneScript/BuildBuildingScripts直接读写快照字段并挂Script。
	friend class Map;
public:
	Zone() = delete;

	// 独占持有mod实例；Layout()在构造并设好位置/边界路之后单独调用
	Zone(ZoneFactory* factory, ZoneMod* mod);
	~Zone();

	std::string GetType() const;
	std::string GetName() const;

	// Map::InitBuildings()用它读mod->internalBuildings
	ZoneMod* GetMod() const;

	// 转发parentLot->GetRotation()；parentLot为空时返回0.f。
	float GetRotation() const;

	// 格式"<road> <index> <zoneName>"，和Map::LocateZone对应
	std::string GetAddress() const;

	// 转发mod->Layout(direction, ...)并快照walls/gates/脚本字段，记下direction，把mod->decorations按朝向换算成GetDecorations()
	void Layout(int direction);

	// Layout()收到的方向(Assign选中的大门方向)，Layout之前是-1。
	int GetDirection() const;

	Script* GetScript() const;

	// p是园区quad上"朝北"(大门在北)定义的PointParams，按direction/GetRotation()换算成世界坐标；direction为-1按朝北原样处理
	std::pair<float, float> NorthPointToWorld(const PointParams& p) const;
	// 拆成两步：朝北PointParams->园区局部坐标，局部坐标->世界坐标(种一片树要在局部坐标取矩形)
	std::pair<float, float> NorthPointToLocal(const PointParams& p) const;
	std::pair<float, float> LocalToWorld(float localX, float localY) const;
	// 朝北定义的yaw(度)->世界朝向(弧度)
	float NorthYawToWorld(float yawDegrees) const;

	// mod->decorations换算后的世界摆位(DecorationInstance见feature_spec.h)，常驻显示。
	const std::vector<DecorationInstance>& GetDecorations() const;

	Lot* GetParentLot() const;
	void SetParentLot(Lot* lot);

	// 下标按FACE_DIRECTION(0-3)，不持有Road
	void SetBoundaryRoad(int direction, Road* road);
	Road* GetBoundaryRoad(int direction) const;
	const std::unordered_map<int, Road*>& GetBoundaryRoads() const;

	// Layout()时从mod拷贝的快照(读档时mod是空实例)，见map_save.md
	const std::vector<ZoneWallSpec>& GetWalls() const;
	const std::vector<ZoneGateSpec>& GetGates() const;

	// Map::InitZones()实例化出的内部Road，Zone持有(~Zone() delete)
	void SetInternalRoads(const std::vector<Road*>& roads);
	const std::vector<Road*>& GetInternalRoads() const;

	// 只登记指针，所有权在Map::buildings
	void AddInternalBuilding(Building* building);
	const std::vector<Building*>& GetInternalBuildings() const;

	// 房产归属：owner(私有)与stated(公有)互斥，见zone.md
	Citizen* GetOwner() const;
	void SetOwner(Citizen* value);
	bool GetStated() const;
	void SetStated(bool value);

	// 遍历快照gates创建门(mesh为空的不放)，由Map::InitZones()在Layout()之后调用一次
	void BuildDoors(Map& map);
	const std::vector<Door*>& GetDoorEntities() const;

private:
	ZoneMod* mod;
	ZoneFactory* factory;
	std::string type;
	std::string name;
	Lot* parentLot = nullptr;
	int direction = -1;
	std::vector<DecorationInstance> decorations;
	std::vector<ZoneWallSpec> walls;
	std::vector<ZoneGateSpec> gates;
	std::unordered_map<int, Road*> boundaryRoads;
	std::vector<Road*> internalRoads;
	std::vector<Building*> internalBuildings;

	Citizen* owner = nullptr;
	bool stated = false;

	// 真正放了门扇的园区大门——只在BuildDoors()非空填入，析构时释放。
	std::vector<Door*> doorEntities;

	// ZoneMod::scriptModName/milestoneNames的快照(读档时Layout不会再跑，存档里直接存这两个)。
	std::string scriptModName = "script_empty";
	std::vector<std::string> scriptMilestones;
	Script* script = nullptr;
};
