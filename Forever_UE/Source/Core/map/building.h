#pragma once

#include "class.h"

#include "map/building_mod.h"
#include "map/building_factory.h"
#include "map/room_factory.h"
#include "map/component_factory.h"
#include "map/general_geometry.h"
#include "map/cabin.h"

#include <string>
#include <unordered_map>
#include <array>


// 楼梯/电梯/坡道：矩形+朝向+四面是否有墙。params是ratio+offset相对参数，InstanciateQuad(width,height)按楼体尺寸
// 换算成楼体局部坐标(原点楼体左下角，未旋转)的绝对Quad，世界坐标转换见Building::LocalToWorld
class Stair : public Quad {
	// 存档：MapSaveCodec直接读写私有字段(map_save.md)。
	friend class MapSaveCodec;
public:
	explicit Stair(RectParams params);

	int GetDirection() const;
	void SetDirection(int direction);
	bool GetWall(int direction) const;
	void AddWall(int direction);
	// 每面墙(按墙朝向FACE_DIRECTION)四个侧面的标签/材质，见general_geometry.h WallStyle。
	const WallStyle& GetWallStyle(int direction) const;
	const WallStyle* GetWallStyles() const { return wallStyles; } // 4项，下标=墙朝向
	WallStyle& MutableWallStyle(int direction);
	void InstanciateQuad(float width, float height);

private:
	int direction = FACE_WEST;
	bool walls[4] = { false, false, false, false };
	WallStyle wallStyles[4];
	RectParams params;
};

class Elevator : public Quad {
	// 存档：MapSaveCodec直接读写私有字段(map_save.md)。
	friend class MapSaveCodec;
public:
	explicit Elevator(RectParams params);

	int GetDirection() const;
	void SetDirection(int direction);
	bool GetWall(int direction) const;
	void AddWall(int direction);
	// 每面墙(按墙朝向FACE_DIRECTION)四个侧面的标签/材质，见general_geometry.h WallStyle。
	const WallStyle& GetWallStyle(int direction) const;
	const WallStyle* GetWallStyles() const { return wallStyles; } // 4项，下标=墙朝向
	WallStyle& MutableWallStyle(int direction);
	void InstanciateQuad(float width, float height);

private:
	int direction = FACE_WEST;
	bool walls[4] = { false, false, false, false };
	WallStyle wallStyles[4];
	RectParams params;
};

class Ramp : public Quad {
	// 存档：MapSaveCodec直接读写私有字段(map_save.md)。
	friend class MapSaveCodec;
public:
	explicit Ramp(RectParams params);

	int GetDirection() const;
	void SetDirection(int direction);
	bool GetWall(int direction) const;
	void AddWall(int direction);
	// 每面墙(按墙朝向FACE_DIRECTION)四个侧面的标签/材质，见general_geometry.h WallStyle。
	const WallStyle& GetWallStyle(int direction) const;
	const WallStyle* GetWallStyles() const { return wallStyles; } // 4项，下标=墙朝向
	WallStyle& MutableWallStyle(int direction);
	void InstanciateQuad(float width, float height);

private:
	int direction = FACE_WEST;
	bool walls[4] = { false, false, false, false };
	WallStyle wallStyles[4];
	RectParams params;
};

// 天花板/地板：纯矩形，没有方向/墙的概念——照抄老工程Ceiling/Ground，渲染层统一用一份
// 薄slab表示(见ForeverBuildingFrameworkComponent.md)。
class Ceiling : public Quad {
	// 存档：MapSaveCodec直接读写私有字段(map_save.md)。
	friend class MapSaveCodec;
public:
	explicit Ceiling(RectParams params);
	void InstanciateQuad(float width, float height);

	// .layout里的标签和mod(BuildingMod::SetSurfaceMaterial)按标签查到的材质实例软路径，都可为空(空=白色默认)。
	const std::string& GetTag() const { return tag; }
	void SetTag(std::string value) { tag = std::move(value); }
	const std::string& GetMaterial() const { return material; }
	void SetMaterial(std::string value) { material = std::move(value); }

private:
	RectParams params;
	std::string tag;
	std::string material;
};

class Ground : public Quad {
	// 存档：MapSaveCodec直接读写私有字段(map_save.md)。
	friend class MapSaveCodec;
public:
	explicit Ground(RectParams params);
	void InstanciateQuad(float width, float height);

	// 同Ceiling。
	const std::string& GetTag() const { return tag; }
	void SetTag(std::string value) { tag = std::move(value); }
	const std::string& GetMaterial() const { return material; }
	void SetMaterial(std::string value) { material = std::move(value); }

private:
	RectParams params;
	std::string tag;
	std::string material;
};

// 地板/天花板洞口：只有位置矩形。只有地下一层(离地表最近的basement)的Hatch会被Map转发进Map::AddHatch挖世界地形，见building.md
class Hatch : public Quad {
	// 存档：MapSaveCodec直接读写私有字段(map_save.md)。
	friend class MapSaveCodec;
public:
	explicit Hatch(RectParams params);
	void InstanciateQuad(float width, float height);

private:
	RectParams params;
};

// 走廊：矩形+四面是否有墙+门/窗开口，没有朝向概念(老工程Corridor没有direction字段，
// 门/窗方向各自独立指定)。
class Corridor : public Quad {
	// 存档：MapSaveCodec直接读写私有字段(map_save.md)。
	friend class MapSaveCodec;
public:
	explicit Corridor(RectParams params);

	bool GetWall(int direction) const;
	void AddWall(int direction);
	const WallHole& GetDoors() const;
	const DoorTags& GetDoorTags() const;
	void AddDoor(int direction, std::vector<RectParams> positions, std::string tag = "");
	const WallHole& GetWindows() const;
	void AddWindow(int direction, std::vector<RectParams> positions);
	// 每面墙(按墙朝向FACE_DIRECTION)四个侧面的标签/材质，见general_geometry.h WallStyle。
	const WallStyle& GetWallStyle(int direction) const;
	const WallStyle* GetWallStyles() const { return wallStyles; } // 4项，下标=墙朝向
	WallStyle& MutableWallStyle(int direction);
	void InstanciateQuad(float width, float height);

private:
	bool walls[4] = { false, false, false, false };
	WallHole doors;
	DoorTags doorTags;
	WallHole windows;
	WallStyle wallStyles[4];
	RectParams params;
};

// 独立房间槽位：矩形+朝向+门/窗开口，隐含四面都有墙(不需要walls数组)——照抄老工程Single，
// AssignRoom按这个槽位实例化出一个真正的Room。
class Single : public Quad {
	// 存档：MapSaveCodec直接读写私有字段(map_save.md)。
	friend class MapSaveCodec;
public:
	explicit Single(RectParams params);

	int GetDirection() const;
	void SetDirection(int direction);
	const WallHole& GetDoors() const;
	const DoorTags& GetDoorTags() const;
	void AddDoor(int direction, std::vector<RectParams> positions, std::string tag = "");
	const WallHole& GetWindows() const;
	void AddWindow(int direction, std::vector<RectParams> positions);
	// 每面墙(按墙朝向FACE_DIRECTION)四个侧面的标签/材质，见general_geometry.h WallStyle。
	const WallStyle& GetWallStyle(int direction) const;
	const WallStyle* GetWallStyles() const { return wallStyles; } // 4项，下标=墙朝向
	WallStyle& MutableWallStyle(int direction);
	void InstanciateQuad(float width, float height);

private:
	int direction = FACE_WEST;
	WallHole doors;
	DoorTags doorTags;
	WallHole windows;
	WallStyle wallStyles[4];
	RectParams params;
};

// 联排房间槽位：和Single同结构，ArrangeRow按目标面积把它切成若干个真正的Room。
class Row : public Quad {
	// 存档：MapSaveCodec直接读写私有字段(map_save.md)。
	friend class MapSaveCodec;
public:
	explicit Row(RectParams params);

	int GetDirection() const;
	void SetDirection(int direction);
	const WallHole& GetDoors() const;
	const DoorTags& GetDoorTags() const;
	void AddDoor(int direction, std::vector<RectParams> positions, std::string tag = "");
	const WallHole& GetWindows() const;
	void AddWindow(int direction, std::vector<RectParams> positions);
	// 每面墙(按墙朝向FACE_DIRECTION)四个侧面的标签/材质，见general_geometry.h WallStyle。
	const WallStyle& GetWallStyle(int direction) const;
	const WallStyle* GetWallStyles() const { return wallStyles; } // 4项，下标=墙朝向
	WallStyle& MutableWallStyle(int direction);
	void InstanciateQuad(float width, float height);

private:
	int direction = FACE_WEST;
	WallHole doors;
	DoorTags doorTags;
	WallHole windows;
	WallStyle wallStyles[4];
	RectParams params;
};

// 一层楼实例化后的完整数据：局部坐标(以Building楼体左下角为原点，宽高=GetBodySizeX/Y())，
// 未做building自身旋转/世界偏移——渲染层/Map::InitBuildings()按需转换成世界坐标
// (Building::LocalToWorld)。
class Floor : public Quad {
	// 存档：MapSaveCodec直接读写私有字段(map_save.md)。
	friend class MapSaveCodec;
public:
	Floor(int level, float width, float height);

	int GetLevel() const;

	const std::vector<Stair>& GetStairs() const;
	const std::vector<Elevator>& GetElevators() const;
	const std::vector<Ramp>& GetRamps() const;
	const std::vector<Ceiling>& GetCeilings() const;
	const std::vector<Ground>& GetGrounds() const;
	const std::vector<Corridor>& GetCorridors() const;
	const std::vector<Single>& GetSingles() const;
	const std::vector<Row>& GetRows() const;
	const std::vector<Hatch>& GetHatches() const;

	void AddStair(Stair stair);
	void AddElevator(Elevator elevator);
	void AddRamp(Ramp ramp);
	void AddCeiling(Ceiling ceiling);
	void AddGround(Ground ground);
	void AddCorridor(Corridor corridor);
	void AddSingle(Single single);
	void AddRow(Row row);
	void AddHatch(Hatch hatch);

	// 分配并返回下一个房间门牌序号(照抄老工程Floor::AssignNumber，每层楼独立计数)。
	int AssignNumber();

private:
	int level;
	int number = 0;
	std::vector<Stair> stairs;
	std::vector<Elevator> elevators;
	std::vector<Ramp> ramps;
	std::vector<Ceiling> ceilings;
	std::vector<Ground> grounds;
	std::vector<Corridor> corridors;
	std::vector<Single> singles;
	std::vector<Row> rows;
	std::vector<Hatch> hatches;
};

// 一个方向(0-3)的行人/车辆导航模板，结构相同——见general_geometry.h的NavigationXxxTemplate。
struct NavigationTemplate {
	std::vector<NavigationNodeTemplate> nodes;
	std::vector<NavigationLineTemplate> lines;
	std::vector<NavigationConnectionTemplate> connections;
};

// 全局共享的建筑内部布局模板仓库(.layout解析结果)，Map::InitBuildings加载一次传给每个Building::Layout()；
// 故意不叫Layout，避免与各类Layout()虚方法混淆，详见building.md
class BuildingLayoutLibrary {
public:
	// 扫描paths指向的每个.layout文件，解析并预算好4个朝向(InverseParams/InverseDirection/
	// InverseWall/InversePoint，照抄老工程Building::ReadTemplates的算法)，重复调用会清空
	// 之前解析的内容重新来一遍。
	void ReadTemplates(const std::vector<std::string>& paths);

	// 模板名对应的.layout是否已被解析。Building::Layout()用它拒绝不存在的模板名(抛异常)，和读档前
	// 的"missing layout"检查保持一致。
	bool HasTemplate(const std::string& name) const;

	// 按模板名+朝向(0-3)查询这9类元素+2套导航模板，查不到返回空(引用一个静态空实例，不是
	// 抛异常；模板名是否存在由Building::Layout()用HasTemplate先校验)。
	const std::vector<Stair>& GetStairs(const std::string& name, int face) const;
	const std::vector<Elevator>& GetElevators(const std::string& name, int face) const;
	const std::vector<Ramp>& GetRamps(const std::string& name, int face) const;
	const std::vector<Ceiling>& GetCeilings(const std::string& name, int face) const;
	const std::vector<Ground>& GetGrounds(const std::string& name, int face) const;
	const std::vector<Corridor>& GetCorridors(const std::string& name, int face) const;
	const std::vector<Single>& GetSingles(const std::string& name, int face) const;
	const std::vector<Row>& GetRows(const std::string& name, int face) const;
	const std::vector<Hatch>& GetHatches(const std::string& name, int face) const;
	const NavigationTemplate& GetPedestrianNavigation(const std::string& name, int face) const;
		// 车行导航模板，由Building::BuildVehicleNavigation使用。
	const NavigationTemplate& GetVehicleNavigation(const std::string& name, int face) const;

	// 4个方向转换辅助函数：纯函数，设成public是因为building.cpp里的ParseNavigationBlock也要用
	static RectParams InverseParams(const RectParams& params, int face);
	static int InverseDirection(int direction, int face);
	static PointParams InversePoint(const PointParams& point, int face);
	// 按朝北(face=2)定义的yaw在face朝向下要叠加的偏移(度)——和InversePoint的点映射是同一个旋转：
	// face0把(1,0)转到(0,-1)即-90°(=270°)，face1转到(0,1)即+90°，face3是180°。建筑/园区的
	// 自定义模型共用。face不在[0,3]时按0处理(原样)。
	static float FaceYawDegrees(int face);
	static RectParams InverseWall(const RectParams& pos, int direction, int face);

private:
	std::unordered_map<std::string, std::array<std::vector<Stair>, 4>> stairTemplates;
	std::unordered_map<std::string, std::array<std::vector<Elevator>, 4>> elevatorTemplates;
	std::unordered_map<std::string, std::array<std::vector<Ramp>, 4>> rampTemplates;
	std::unordered_map<std::string, std::array<std::vector<Ceiling>, 4>> ceilingTemplates;
	std::unordered_map<std::string, std::array<std::vector<Ground>, 4>> groundTemplates;
	std::unordered_map<std::string, std::array<std::vector<Corridor>, 4>> corridorTemplates;
	std::unordered_map<std::string, std::array<std::vector<Single>, 4>> singleTemplates;
	std::unordered_map<std::string, std::array<std::vector<Row>, 4>> rowTemplates;
	std::unordered_map<std::string, std::array<std::vector<Hatch>, 4>> hatchTemplates;
	std::unordered_map<std::string, std::array<NavigationTemplate, 4>> pedestrianNavigationTemplates;
	std::unordered_map<std::string, std::array<NavigationTemplate, 4>> vehicleNavigationTemplates;
};

// Building::Layout()构建内部导航图的结果，交还给Map::InitBuildings()合并，见building.md"行人导航"
struct BuildingNavResult {
	// 需要登记进Map::navAnchorNodes统一管理生命周期/可视化的新建节点——包含building内部
	// 导航图专属的固定/投影锚点，也包含每个Room自己的导航节点(Building::Layout()里创建、
	// BuildPedestrianNavigation()结尾统一收进这里，不在别处另外登记)。
	std::vector<Node*> nodes;
	// 需要登记进Map::pedestrianNavGraph(双向)的新建连接。
	std::vector<Connection*> connections;
	// "outside"类型的端点节点，Map据此接上道路网。每个成员同时也在上面的nodes里，只是单独拎出清单——
	// 不能再push进navAnchorNodes，否则同一个Node*登记两次，~Map()会double free
	std::vector<Node*> outsideNodes;

	// 车行导航图的平行字段(来源是vehicleNavigation模板)。车行图和行人图是两张独立的图，合并方式也不同
	// (车行按入度/出度判定单向，行人固定双向)
	std::vector<Node*> vehicleNodes;
	std::vector<Connection*> vehicleConnections;
	// 车行导航的显式入口/出口端点(layout里的"entrance"/"exit")，已包含在vehicleNodes里；方向由类型决定，
	// 车行导航不支持"outside"
	std::vector<Node*> vehicleEntranceNodes;
	std::vector<Node*> vehicleExitNodes;
};

// 已落地的Building(继承Quad)，独占一个BuildingMod实例，设计见building.md。
// rotation不自存：转发parentLot->GetRotation()再叠加relativeRotation(园区内部建筑用，见SetParentLot)
class Building : public Quad {
	// 存档：MapSaveCodec直接读写私有字段(map_save.md)。
	friend class MapSaveCodec;
	// 实体脚本：Map::BuildZoneScript/BuildBuildingScripts直接读写快照字段并挂Script。
	friend class Map;
public:
	Building() = delete;

	Building(BuildingFactory* factory, BuildingMod* mod);
	~Building();

	std::string GetType() const;
	std::string GetName() const;

	BuildingMod* GetMod() const;

	// 落地入口：在SetPosition/SetBoundaryRoad之后调用一次，转发mod->Layout并解析楼体/楼层/房间/Component/导航，见building.md。
	// direction语义同BuildingMod::Layout(显式占位是Assign选中的方向，其余传-1/spec.direction)；
	// roomFactory/componentFactory只在本次调用里临时使用；navOut由Map::InitBuildings合并进全局导航图
	void Layout(int direction, const BuildingLayoutLibrary& library,
		RoomFactory& roomFactory, ComponentFactory& componentFactory, BuildingNavResult& navOut);

	// mod->Layout()实际确定下来的方向(处理过-1兜底之后的最终值)，供Map::InitBuildings()
	// 接building外部导航用；FillRemainder落地且mod没有兜底逻辑时仍然可能是-1(表示这栋
	// building确实没有可用的边界路)。
	int GetDirection() const;

	float GetBodyOffsetX() const; // 楼体中心相对Building自身中心的偏移(地图单位，未旋转局部坐标)
	float GetBodyOffsetY() const;
	float GetBodySizeX() const;   // 楼体绝对尺寸(地图单位)
	float GetBodySizeY() const;
	int GetBasementCount() const;
	int GetLayerCount() const;
	const std::vector<float>& GetFloorHeights() const; // 长度basements+layers，从下到上
	const std::string& GetLodMaterialPath() const;
	// mod->decorations按最终朝向换算后的世界摆位(DecorationInstance见feature_spec.h)，UE层只在
	// 近处LOD按这份数据生成组件，见building.md"自定义模型"一节。
	const std::vector<DecorationInstance>& GetDecorations() const;

	// mod声明式字段在Layout()时拷贝下来的快照——UE层/Traffic读这几份，不直接读mod：读档时mod是
	// 重新创建的空实例(Layout不会再跑，见map_save.md)，mod->floors/cabins/stationMod都是空的。
	// 查不到这一层的模板记录时GetFloorSpec返回nullptr。
	const FloorLayoutSpec* GetFloorSpec(int level) const;
	const std::unordered_map<int, FloorLayoutSpec>& GetFloorSpecs() const;
	const std::vector<ElevatorCabinSpec>& GetCabins() const;
	const std::string& GetStationMod() const;

	// 行人导航里"outside"端点对应的1楼外部连接点（Map::MergeBuildingNavigation在init时记下，进存档）。
	// 节点由Map::navAnchorNodes持有，这里只是引用。导航线用它当"到不了房间的车"的目的地，见Map::PlanVehicleRouteToPoint。
	const std::vector<Node*>& GetOutsideNodes() const;
	void SetOutsideNodes(const std::vector<Node*>& nodes);

	// 这栋建筑自己的Script(BuildingMod::milestoneNames非空才有，否则nullptr)，Map::BuildBuildingScripts创建，析构时释放。
	Script* GetScript() const;

	// 按level(0=1楼，-1=地下一层……)查一层的实例化数据，查不到返回nullptr。
	const Floor* GetFloor(int level) const;
	const std::vector<Component*>& GetComponents() const;
	const std::vector<Room*>& GetRooms() const;

	// 这层楼底部的Z(地图单位，相对地坪grade=0)：basements往下累减floorHeights，地上楼层
	// 往上累加，和渲染层ComputeFloorZRange同一套grade-相对的堆叠约定，只是这里只算一个
	// 代表性的"楼层底部"值(用于导航节点的近似高度，不是渲染用的精确范围)。
	float GetFloorBaseZ(int level) const;

	// 把楼体局部坐标(原点在楼体左下角，未旋转，即GetFloor(level)/Room矩形自己的坐标系)
	// 转换成世界坐标(地图单位)——按bodyOffset+自身旋转换算，和渲染层
	// ComputeBodyWorldCenter同一套公式，只是这里额外支持任意局部点(不止楼体中心)。
	std::pair<float, float> LocalToWorld(float localX, float localY) const;

	// 建筑植被用的参考系是整块建筑quad(不是楼体footprint)：QuadNorthPointToLocal把按朝北定义的
	// quad上PointParams(原点quad西北角)按GetDirection()用InversePoint旋转，换成quad局部坐标(中心
	// 原点、沿建筑自身轴向)；QuadLocalToWorld按GetRotation()旋转加上quad中心。direction为-1时按朝北。
	std::pair<float, float> QuadNorthPointToLocal(const PointParams& p) const;
	std::pair<float, float> QuadLocalToWorld(float localX, float localY) const;

	// 转发parentLot->GetRotation()+relativeRotation；parentLot为空时按0.f+relativeRotation算。
	float GetRotation() const;

	// 有parentZone时"<road> <index> <zoneName> <buildingName>"，否则
	// "<road> <index> <buildingName>"——和Map::LocateBuilding的两种解析格式一一对应
	// （照抄老工程Building::GetAddress的两分支逻辑）。
	std::string GetAddress() const;

	Lot* GetParentLot() const;
	void SetParentLot(Lot* lot, float relativeRotation = 0.f);

	// 归属哪个Zone——只是纯粹的反向查询登记(以后"这个建筑在哪个园区里"之类的功能用)，
	// 不参与GetRotation()计算(旋转转发走的是parentLot那条链路，见上)。和parentLot同时设置，
	// 不是二选一：园区内部building两个都要设。
	Zone* GetParentZone() const;
	void SetParentZone(Zone* zone);

	// 四周边界Road：下标按FACE_DIRECTION(0-3)，和Lot::boundaryRoads语义完全一致，不持有
	// 指针生命周期。
	void SetBoundaryRoad(int direction, Road* road);
	Road* GetBoundaryRoad(int direction) const;
	const std::unordered_map<int, Road*>& GetBoundaryRoads() const;

	// 房产归属：owner(私有)与stated(公有)互斥；内部各room独立归属时两者保持默认值，见building.md
	Citizen* GetOwner() const;
	void SetOwner(Citizen* value);
	bool GetStated() const;
	void SetStated(bool value);

	// 建筑门(corridor门洞，登记进doorEntities)和房间门(登记进对应room)，见building.md"建筑门"
	void BuildDoors(Map& map);
	const std::vector<Door*>& GetDoorEntities() const;

	// 按GetCabins()逐条创建Cabin，登记进cabinEntities，见cabin.md
	void BuildCabins(Map& map);
	const std::vector<Cabin*>& GetCabinEntities() const;

private:
	// 按mod->floors实例化每一层的Floor，塞进floors数组。
	void ReadFloor(int level, int face, const std::string& templateName, const BuildingLayoutLibrary& library);

	// 按mod->singles/rows实例化Component+Room。
	void AssignRoom(int level, int slot, const std::string& roomType, Component* component, RoomFactory& roomFactory);
	void ArrangeRow(int level, int slot, const std::string& roomType, float acreage, Component* component,
		RoomFactory& roomFactory);

	// 行人导航图(转发BuildNavigationGraph，isVehicle=false, registerRoomNodes=true)，见BuildingNavResult
	void BuildPedestrianNavigation(const BuildingLayoutLibrary& library, int direction, BuildingNavResult& navOut);

	// 车行导航图，写进navOut的vehicle*字段。registerRoomNodes必须为false，否则Room导航节点重复登记会让~Map() double free
	void BuildVehicleNavigation(const BuildingLayoutLibrary& library, int direction, BuildingNavResult& navOut);

	// 行人/车行共用的算法：isVehicle只决定读哪套导航模板；registerRoomNodes控制是否把Room导航节点登记进newNodes(只能登记一次)
	void BuildNavigationGraph(const BuildingLayoutLibrary& library, int inputDirection, bool isVehicle,
		std::vector<Node*>& newNodes, std::vector<Connection*>& newConnections,
		std::vector<Node*>& outsideNodes, bool registerRoomNodes,
		std::vector<Node*>* entranceNodes = nullptr, std::vector<Node*>* exitNodes = nullptr);

	static float ProjectOntoLine(float px, float py, float ax, float ay, float bx, float by);

	BuildingMod* mod;
	BuildingFactory* factory;
	std::string type;
	std::string name;
	Lot* parentLot = nullptr;
	Zone* parentZone = nullptr;
	float relativeRotation = 0.f;
	std::unordered_map<int, Road*> boundaryRoads;

	int direction = -1;
	float bodyOffsetX = 0.f;
	float bodyOffsetY = 0.f;
	float bodySizeX = 0.f;
	float bodySizeY = 0.f;
	int basements = 0;
	int layers = 1;
	std::vector<float> floorHeights;
	std::string lodMaterialPath;
	std::vector<DecorationInstance> decorations;
	// mod->floors/cabins/stationMod的快照，见GetFloorSpec注释。
	std::unordered_map<int, FloorLayoutSpec> floorSpecs;
	std::vector<ElevatorCabinSpec> cabins;
	std::string stationMod;
	// BuildingMod::scriptModName/milestoneNames的快照(读档时Layout不会再跑，存档里直接存这两个)。
	std::string scriptModName = "script_empty";
	std::vector<std::string> scriptMilestones;
	Script* script = nullptr;
	std::vector<Node*> outsideNodes;

	std::vector<Floor> floors;
	std::vector<Component*> components;
	std::vector<Room*> rooms;

	// 独立房间导航映射：[楼层编号][独立房间序号] -> 生成的房间（非持有引用）；联排房间同理，
	// 一个槽位对应一组房间——照抄老工程singleRoomBySlot/rowRoomBySlot，BuildPedestrianNavigation
	// 解析"single"/"row"类型的导航端点时用。
	std::unordered_map<int, std::unordered_map<int, Room*>> singleRoomBySlot;
	std::unordered_map<int, std::unordered_map<int, std::vector<Room*>>> rowRoomBySlot;

	Citizen* owner = nullptr;
	bool stated = false;

	// corridor门洞上的门，BuildDoors填入，析构时释放(room的门归对应Room)；tag在doorSpecs里解析不出
	// spec的门洞不会生成Door。
	std::vector<Door*> doorEntities;

	// 电梯实体，见BuildCabins。
	std::vector<Cabin*> cabinEntities;
};
