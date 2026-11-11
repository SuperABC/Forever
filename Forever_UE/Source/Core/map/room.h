#pragma once

#include "class.h"

#include "map/room_mod.h"
#include "map/room_factory.h"
#include "map/general_geometry.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class Script;


// Building内部布局实例化出来的房间，继承Quad(建筑楼体局部坐标系下的矩形)，设计见room.md
class Room : public Quad {
	// 存档：MapSaveCodec直接读写私有字段(map_save.md)。
	friend class MapSaveCodec;
	// 实体脚本：Map::BuildZoneScript/BuildBuildingScripts直接读写快照字段并挂Script。
	friend class Map;
public:
	Room() = delete;

	Room(RoomFactory* factory, RoomMod* mod, Building* parentBuilding, Component* parentComponent, int layer);
	~Room();

	std::string GetType() const;
	std::string GetName() const;

	RoomMod* GetMod() const;

	int GetLayer() const;
	int GetDirection() const;

	// 这个房间自己的Script(RoomMod::milestoneNames非空才有，否则nullptr)，Map::BuildBuildingScripts创建，析构时释放。
	Script* GetScript() const;
	void SetDirection(int direction);

	// 房间局部坐标系(u/v/yaw约定)见room.md"房间局部坐标系"；朝向无效(不在0..3)按朝北。
	// u/v两个轴的长度(W/E朝向时u轴是建筑局部的SizeY、v轴是SizeX)，PointParams的比例乘它们。
	float AxisSizeU() const;
	float AxisSizeV() const;
	// 局部角点系的(u,v)米数 -> 相对房间中心的建筑局部偏移(加上GetPosX/GetPosY就是建筑局部坐标)。
	void LocalToBuildingLocal(float u, float v, float& offsetX, float& offsetY) const;
	// 房间局部yaw 0度在建筑局部坐标系里的数学角(逆时针，0度=+X)：W180/E0/N90/S270。
	float FacingYawDegrees() const;
	// 局部(u,v)米数 -> 世界XY(地图单位)；局部yaw -> 世界yaw(弧度，叠加建筑世界旋转)。
	std::pair<float, float> LocalToWorld(float u, float v) const;
	float LocalYawToWorld(float yawDegrees) const;

	Building* GetParentBuilding() const;
	Component* GetParentComponent() const;

	const WallHole& GetDoors() const;
	const DoorTags& GetDoorTags() const;
	const WallHole& GetWindows() const;
	void SetDoors(WallHole doors, DoorTags tags = {});
	void SetWindows(WallHole windows);
	// 每面墙(按墙朝向FACE_DIRECTION)四个侧面的标签/材质，由Building::AssignRoom/ArrangeRow从槽位继承。
	const WallStyle& GetWallStyle(int direction) const;
	const WallStyle* GetWallStyles() const { return wallStyles; } // 4项，下标=墙朝向
	void SetWallStyle(int direction, const WallStyle& style);

	// Building::BuildDoors()创建的房间门登记在这里，由Room持有生命周期
	void AddDoorEntity(Door* door);
	const std::vector<Door*>& GetDoorEntities() const;

	// 门牌号，"b3-0001"这种格式(basement前缀+4位序号)，照抄老工程Room::SetNumber。
	const std::string& GetNumber() const;
	void SetNumber(int level, int number);

	// "<parentBuilding地址> <门牌号>"，照抄老工程Room::GetAddress。
	std::string GetAddress() const;

	// 这个房间中心点的导航锚点，Building::Layout()在实例化完所有Room之后统一创建、
	// 供BuildPedestrianNavigation()里"single"/"row"类型的导航端点引用。不持有生命周期
	// (和其它building导航节点一样交给Map::navAnchorNodes统一管理)。
	Node* GetNavigationNode() const;
	void SetNavigationNode(Node* node);

	// 4类占位能力：构造时mod->SetProperty()之后从mod同名字段拷贝的快照(读档时直接按存档恢复，
	// 不依赖重新创建的mod再算一遍)。见room_mod.h、map_save.md。
	bool IsResidential() const;
	int ResidentialCapacity() const;
	bool IsWorkspace() const;
	int WorkspaceCapacity() const;
	bool IsStorage() const;
	std::string StorageType() const; // 对应StorageMod id，空=不生成Storage
	int64_t Rent() const;            // 每日房租(分)
	bool IsManufacture() const;
	std::vector<std::string> ManufactureTypes() const;
	bool IsParking() const;
	const std::vector<ParkingSpot>& GetParkingSpots() const;

	// 归属(owner/stated，二者互斥)、住户(households)、当前在场的人(occupants)是3个独立概念，见room.md
	Citizen* GetOwner() const;
	void SetOwner(Citizen* value);
	bool GetStated() const;
	void SetStated(bool value);

	// AddResident按亲属关系并入已有的户/合并相连的户/新建一户，见populace/household.h；读档重建户用AddHousehold
	const std::vector<Household*>& GetHouseholds() const;
	Household* AddResident(Citizen* citizen);
	Household* AddHousehold();

	const std::vector<Citizen*>& GetOccupants() const;
	void AddOccupant(Citizen* citizen);
	void RemoveOccupant(Citizen* citizen);

	// 房间地上摆放的资产，房间独占持有(析构时delete)，key=Asset::GetName()
	const std::unordered_map<std::string, Asset*>& GetAssets() const;
	void AddAsset(Asset* asset);
	Asset* RemoveAsset(const std::string& name); // 摘除不delete，调用方接手所有权

private:
	RoomMod* mod;

	// mod->SetProperty()产出的属性快照，见IsResidential()注释。
	struct Properties {
		bool isResidential = false;
		int residentialCapacity = 0;
		bool isWorkspace = false;
		int workspaceCapacity = 0;
		bool isStorage = false;
		std::string storageType;
		int64_t rent = 0;
		bool isManufacture = false;
		std::vector<std::string> manufactureTypes;
		bool isParking = false;
		std::vector<ParkingSpot> parkingSpots;
	};
	Properties properties;
	RoomFactory* factory;
	std::string type;
	std::string name;
	Building* parentBuilding;
	Component* parentComponent;
	int layer;
	int direction = FACE_WEST;
	WallHole doors;
	DoorTags doorTags;
	WallHole windows;
	WallStyle wallStyles[4];
	std::string number;
	Node* navigationNode = nullptr;

	std::vector<Door*> doorEntities;

	// RoomMod::scriptModName/milestoneNames的快照(构造函数里SetProperty()之后拷贝，读档重建时同样会跑)。
	std::string scriptModName = "script_empty";
	std::vector<std::string> scriptMilestones;
	Script* script = nullptr;

	Citizen* owner = nullptr;
	bool stated = false;
	std::vector<Household*> households; // 持有所有权
	std::vector<Citizen*> occupants;

	std::unordered_map<std::string, Asset*> assets;
};
