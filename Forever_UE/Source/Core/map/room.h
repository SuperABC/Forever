#pragma once

#include "class.h"

#include "map/room_mod.h"
#include "map/room_factory.h"
#include "map/general_geometry.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>


// 房间里的一个停车位：由Building::Layout按BuildingMod::AssignParking把layout的停车位矩形分割而来，设计见room.md。
// 坐标是建筑局部坐标(和Room::GetPosX/GetPosY同一套，原点楼体左下角，未旋转)。
struct ParkingSlot {
	float centerX = 0.f, centerY = 0.f;
	float halfWidth = 0.f; // 沿垂直开口朝向的轴(车位宽度的一半)
	float halfDepth = 0.f; // 沿开口朝向的轴(车位深度的一半)
	int direction = FACE_WEST; // 车位开口朝向(FACE_DIRECTION，朝通道)；车头朝里即车头方向=开口朝向的反向
	std::vector<std::string> categories;
	bool stocked = false; // 开局预置一辆车(ParkingSlotSpec::stocked)；只有新开局的Traffic::Init用，不进存档(读档不再Init)
	// 不接公路的车位(船位/机位这类，ParkingSlotSpec::offRoad)：节点不在车行导航图里，公路车辆寻路到不了。进存档。
	bool offRoad = false;
	Node* navigationNode = nullptr; // 车位中心的车行导航节点，由Map::navAnchorNodes持有生命周期，不在这里delete
	Vehicle* reservedBy = nullptr;  // 正在开往这个车位的车(寻路开始即占用)
	Vehicle* parkedVehicle = nullptr; // 已经停在车位里的车

	bool IsFree() const { return !reservedBy && !parkedVehicle; }
	bool Accepts(const std::string& category) const;
	// 开口朝向在建筑局部坐标系里的数学角(度，逆时针，0度=+X)：W180/E0/N270/S90(北=-Y)，和Room::FacingYawDegrees不是同一套。
	float FacingYawDegrees() const;
	// 车头朝向(度)=开口朝向的反向。
	float VehicleYawDegrees() const;
	// 建筑局部点是否落在车位矩形里。
	bool Contains(float x, float y) const;
};


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
	bool IsResidence() const;
	int ResidenceCapacity() const;
	bool IsWorkspace() const;
	int WorkspaceCapacity() const;
	bool IsStorage() const;
	std::string StorageType() const; // 对应StorageMod id，空=不生成Storage
	int64_t Rent() const;            // 每日房租(分)
	bool IsManufacture() const;
	std::vector<std::string> ManufactureTypes() const;

	// 停车位：由Building::Layout创建，Room持有。没有车位的房间返回空。
	// 家具：位置/朝向/门窗都设好之后Building::AssignRoom/ArrangeRow调一次LayoutFurniture()，用房间mod自己的随机流
	// 调mod->Layout(BuildLayoutInfo())，把mod用AddFurniture摆的家具(房间局部米)换算成世界摆位存到这里；
	// 随Room存档，读档不再重新Layout。UE层只读GetFurniture()。
	void LayoutFurniture();
	// 房间局部矩形(米)、层高和门窗洞(换算到房间局部系的墙)，见feature_spec.h的RoomLayoutInfo。
	RoomLayoutInfo BuildLayoutInfo() const;
	const std::vector<DecorationInstance>& GetFurniture() const;
	// 房间mod在Layout里用PlaceAsset/PlaceFixture放的物品：按mod的两张清单创建Asset实例，算好地面摆位(同家具的换算)，
	// 放进assets。新开局时Map::BuildRoomAssets在房间地址确定之后调一次；读档不调(物品随player段恢复)。
	// 物品mod没启用的那一件跳过，warned按mod id去重只警告一次。固定物品的名字是"<房间地址> <type><序号>"。
	void CreatePlacedAssets(AssetFactory& assetFactory, std::unordered_set<std::string>& warned);

	const std::vector<ParkingSlot*>& GetParkingSlots() const;
	ParkingSlot* AddParkingSlot(const ParkingSlot& slot);
	// 第一个空闲且允许category的车位下标，没有返回-1。
	int FindFreeSlot(const std::string& category) const;
	// 房间里有没有任何允许category的车位(不管是否被占)。
	bool HasSlotFor(const std::string& category) const;
	// 玩家把车停下时判定它是否停在某个车位里(车头朝里：车朝向与车位开口朝向点积<-0.8，车中心在车位矩形内，
	// 车位上没有已停的车)，返回下标，没有返回-1。x/y/yawDegrees是建筑局部坐标。
	int FindSlotContaining(float x, float y, float yawDegrees) const;

	// 归属(owner/stated，二者互斥)、住户(households)、当前在场的人(occupants)是3个独立概念，见room.md
	Citizen* GetOwner() const;
	void SetOwner(Citizen* value);
	bool GetStated() const;
	void SetStated(bool value);

	// AddResident按亲属关系并入已有的户/合并相连的户/新建一户，见populace/household.h；读档重建户用AddHousehold
	const std::vector<Household*>& GetHouseholds() const;
	Household* AddResident(Citizen* citizen);
	Household* AddHousehold();
	// 住户搬走/死亡/离城：移出所在的户并SetHousehold(nullptr)，户空了就删掉。不在这里的人什么都不做。
	// 不改市民记的家(GetRoom)，调用方决定。
	void RemoveResident(Citizen* citizen);
	// 住宅里还有没有活着、没离城的住户(没有任何户的住宅=空房，见Map::PickVacantResidence)。
	bool HasActiveResident() const;

	const std::vector<Citizen*>& GetOccupants() const;
	void AddOccupant(Citizen* citizen);
	void RemoveOccupant(Citizen* citizen);

	// 房间地上摆放的资产，房间独占持有(析构时delete)，key=Asset::GetName()
	const std::unordered_map<std::string, Asset*>& GetAssets() const;
	// 把一件物品放到这个房间的地上(房间从此持有它)。已经有同名物品时拒绝并返回false——不能覆盖，
	// 旧的那件会没人释放，而且UE里可能还有Actor指着它。物品名由mod保证全局唯一，正常不会撞。
	bool AddAsset(Asset* asset);
	Asset* RemoveAsset(const std::string& name); // 摘除不delete，调用方接手所有权

private:
	RoomMod* mod;

	// mod->SetProperty()产出的属性快照，见IsResidence()注释。
	struct Properties {
		bool isResidence = false;
		int residenceCapacity = 0;
		bool isWorkspace = false;
		int workspaceCapacity = 0;
		bool isStorage = false;
		std::string storageType;
		int64_t rent = 0;
		bool isManufacture = false;
		std::vector<std::string> manufactureTypes;
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
	std::vector<ParkingSlot*> parkingSlots; // 持有所有权

	std::vector<Door*> doorEntities;
	std::vector<DecorationInstance> furniture; // 世界摆位(LayoutFurniture算好)，存档

	// RoomMod::scriptModName/milestoneNames的快照(构造函数里SetProperty()之后拷贝，读档重建时同样会跑)。
	std::string scriptModName; // 空=脚本没有ScriptMod(逻辑全在里程碑文件里)
	std::vector<std::string> scriptMilestones;
	Script* script = nullptr;

	Citizen* owner = nullptr;
	bool stated = false;
	std::vector<Household*> households; // 持有所有权
	std::vector<Citizen*> occupants;

	std::unordered_map<std::string, Asset*> assets;
};
