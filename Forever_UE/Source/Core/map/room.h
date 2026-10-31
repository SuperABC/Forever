#pragma once

#include "class.h"

#include "map/room_mod.h"
#include "map/room_factory.h"
#include "map/general_geometry.h"

#include <string>
#include <unordered_map>
#include <vector>

class Script;


// WallHole(一面墙上的门/窗开口列表)定义在general_geometry.h——Room和building.h的Corridor/Single/
// Row都要用，general_geometry.h是两边都已经包含的公共底层头。

// Room(房间)：Building内部布局实例化出来的一个具体房间，继承Quad表示自己在Building楼体
// 局部坐标系(以Building::GetBodySizeX/Y()为宽高，原点在楼体左下角，未旋转)里占据的矩形。
// 这次只保留和"布局/渲染/寻址"直接相关的部分——不迁移老工程Room身上的furniture/pivots/
// storages/manufactures/parkings/vehicles/ownership/tenancy/workers等字段，那些依赖
// Industry/Populace/Society这些还没迁移的领域，等对应领域迁移到了再回来加。
//
// 和Component一样，Room永远是Building自己在Layout()里通过AssignRoom/ArrangeRow显式创建
// 的，不参与地块竞争，不需要Assign/RandomAcreage这套static注册机制——独占持有一个RoomMod
// 实例，构造时创建、析构时factory->DestroyRoom(mod)。
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

	// 这个Room持有的mod实例——不需要另外拷贝一份。
	RoomMod* GetMod() const;

	int GetLayer() const;
	int GetDirection() const;

	// 这个房间自己的Script(RoomMod::milestoneNames非空才有，否则nullptr)，Map::BuildBuildingScripts创建，析构时释放。
	Script* GetScript() const;
	void SetDirection(int direction);

	// 房间局部坐标系：房间"朝北"时和建筑/layout的坐标系一样以角为原点——原点在房间(minX,minY)角，
	// u=沿正面墙的方向、v=朝正面(direction指的那一侧，门/临路)为正，yaw 0度=朝向正面；房间朝向是
	// W/E/S时整套坐标系做纯旋转(和BuildingLayoutLibrary::InversePoint同一套)，所以家具/车位等
	// 只要按"房间朝北"写一次。direction是建筑局部(未旋转)坐标系下的朝向，已含建筑实例朝向，
	// 见building.md；朝向无效(不在0..3)按朝北。
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

	// 房间门：Building::BuildDoors()遍历这个room的每个门洞时，用mod->doorSpecs做
	// ResolveDoorSpec，需要放门就调map.CreateDoor(DOOR_KIND_ROOM, ...)创建，再
	// AddDoorEntity登记进这里——由这个Room持有生命周期(析构时释放)，不是Building。
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
	std::unordered_map<std::string, float> StorageConfig() const;
	bool IsManufacture() const;
	std::vector<std::string> ManufactureTypes() const;
	bool IsParking() const;
	const std::vector<ParkingSpot>& GetParkingSpots() const;

	// 人/归属和房间的关系——4个独立的概念，不能互相替代，分开存：
	// - owner/stated：房间的归属，有且只有一种结果——要么私有(owner指向具体某个
	//   citizen，stated=false)，要么公有(stated=true，owner保持nullptr，"公有"本来就
	//   不指向任何一个具体citizen)。Map::Checkin()给每个room解析归属时，两者互斥，
	//   永远恰好落在其中一种，不会同时/都不设(见Map::Checkin()"房产归属"一节，
	//   populace.md)。
	// - tenants：住在这个房间的人，0个或多个——私有归属时owner本人也会被加进tenants
	//   (owner肯定住在自己拥有的房间里，只是tenants里"主要"的那一个)；公有归属没有
	//   owner，但一样可以有tenants(公有住房分配给市民住)。
	// - occupants：当前物理位置在这个房间里的人，0个或多个——和tenants是两回事：一个
	//   tenant可能人不在家(以后有真正移动AI之后，citizen的当前位置可能是别的房间)，一个
	//   occupant也可能不是这个房间的tenant(以后有访客/工作场景时会出现)。这次没有真正的
	//   移动AI，citizen只会在Map::Checkin()分配住处的同时"住进去"，两者的初始状态天然
	//   重合(tenants/occupants当下是同一批人)，但数据结构上必须分开存，为将来
	//   "人在家但当前不在自己房间里"这类场景预留。
	Citizen* GetOwner() const;
	void SetOwner(Citizen* value);
	bool GetStated() const;
	void SetStated(bool value);

	const std::vector<Citizen*>& GetTenants() const;
	void AddTenant(Citizen* citizen);
	void RemoveTenant(Citizen* citizen);

	const std::vector<Citizen*>& GetOccupants() const;
	void AddOccupant(Citizen* citizen);
	void RemoveOccupant(Citizen* citizen);

	// 房间地上摆放的资产——独占持有(和tenants/occupants不同，那两个是非owning指针，指向
	// Populace持有的Citizen；这里房间真的拥有这些Asset的生命周期)。key=Asset::GetName()。
	// 这次的"当前房间"是玩家真实物理位置(见Player::currentRoom)，不是老工程的剧情/场景
	// 语义，见Source/Core/player/asset.md、Source/Forever/Element/BuildingElement.cpp的
	// OnRoomOverlapBegin/End。
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
		std::unordered_map<std::string, float> storageConfig;
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
	std::string number;
	Node* navigationNode = nullptr;

	std::vector<Door*> doorEntities;

	// RoomMod::scriptModName/milestoneNames的快照(构造函数里SetProperty()之后拷贝，读档重建时同样会跑)。
	std::string scriptModName = "script_empty";
	std::vector<std::string> scriptMilestones;
	Script* script = nullptr;

	Citizen* owner = nullptr;
	bool stated = false;
	std::vector<Citizen*> tenants;
	std::vector<Citizen*> occupants;

	std::unordered_map<std::string, Asset*> assets;
};
