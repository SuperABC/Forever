#pragma once

#include "map/building_mod.h"
#include "map/building_factory.h"
#include "map/room_factory.h"
#include "map/component_factory.h"
#include "map/geometry.h"

#include <string>
#include <unordered_map>
#include <array>

class Zone;
class Component;
class Room;

// 楼梯/电梯/坡道：都是"矩形+朝向+四面是否有墙"，照抄老工程Stair/Elevator/Ramp(字段/方法
// 完全一样，只是分成三个类型名以后如果需要各自加字段更清楚)。params是从.layout模板解析出的
// ratio+offset相对参数，InstanciateQuad(width,height)按楼层实际宽高换算成绝对Quad——width/
// height就是这栋Building的楼体尺寸(GetBodySizeX/Y())，换算结果是"以楼体左下角为原点、未旋转"
// 的局部坐标，不是世界坐标(世界坐标转换见Building::LocalToWorld)。
class Stair : public Quad {
public:
	explicit Stair(RectParams params);

	int GetDirection() const;
	void SetDirection(int direction);
	bool GetWall(int direction) const;
	void AddWall(int direction);
	void InstanciateQuad(float width, float height);

private:
	int direction = FACE_WEST;
	bool walls[4] = { false, false, false, false };
	RectParams params;
};

class Elevator : public Quad {
public:
	explicit Elevator(RectParams params);

	int GetDirection() const;
	void SetDirection(int direction);
	bool GetWall(int direction) const;
	void AddWall(int direction);
	void InstanciateQuad(float width, float height);

private:
	int direction = FACE_WEST;
	bool walls[4] = { false, false, false, false };
	RectParams params;
};

class Ramp : public Quad {
public:
	explicit Ramp(RectParams params);

	int GetDirection() const;
	void SetDirection(int direction);
	bool GetWall(int direction) const;
	void AddWall(int direction);
	void InstanciateQuad(float width, float height);

private:
	int direction = FACE_WEST;
	bool walls[4] = { false, false, false, false };
	RectParams params;
};

// 天花板/地板：纯矩形，没有方向/墙的概念——照抄老工程Ceiling/Ground，渲染层统一用一份
// 薄slab表示(见ForeverBuildingFrameworkComponent.md)。
class Ceiling : public Quad {
public:
	explicit Ceiling(RectParams params);
	void InstanciateQuad(float width, float height);

private:
	RectParams params;
};

class Ground : public Quad {
public:
	explicit Ground(RectParams params);
	void InstanciateQuad(float width, float height);

private:
	RectParams params;
};

// 地板/天花板洞口：只有位置矩形，没有方向/墙——这次不用来裁剪Building自己楼层的Ceiling/
// Ground(那些照模板原样绘制，不做运行时裁剪)，只有地下一层(离地表最近的那层basement)的
// Hatch会被Map::InitBuildings()取出来转发进Map::AddHatch去挖世界地形，见building.md
// "地下室与地表hatch"一节。
class Hatch : public Quad {
public:
	explicit Hatch(RectParams params);
	void InstanciateQuad(float width, float height);

private:
	RectParams params;
};

// 走廊：矩形+四面是否有墙+门/窗开口，没有朝向概念(老工程Corridor没有direction字段，
// 门/窗方向各自独立指定)。
class Corridor : public Quad {
public:
	explicit Corridor(RectParams params);

	bool GetWall(int direction) const;
	void AddWall(int direction);
	const WallHole& GetDoors() const;
	void AddDoor(int direction, std::vector<RectParams> positions);
	const WallHole& GetWindows() const;
	void AddWindow(int direction, std::vector<RectParams> positions);
	void InstanciateQuad(float width, float height);

private:
	bool walls[4] = { false, false, false, false };
	WallHole doors;
	WallHole windows;
	RectParams params;
};

// 独立房间槽位：矩形+朝向+门/窗开口，隐含四面都有墙(不需要walls数组)——照抄老工程Single，
// AssignRoom按这个槽位实例化出一个真正的Room。
class Single : public Quad {
public:
	explicit Single(RectParams params);

	int GetDirection() const;
	void SetDirection(int direction);
	const WallHole& GetDoors() const;
	void AddDoor(int direction, std::vector<RectParams> positions);
	const WallHole& GetWindows() const;
	void AddWindow(int direction, std::vector<RectParams> positions);
	void InstanciateQuad(float width, float height);

private:
	int direction = FACE_WEST;
	WallHole doors;
	WallHole windows;
	RectParams params;
};

// 联排房间槽位：和Single同结构，ArrangeRow按目标面积把它切成若干个真正的Room。
class Row : public Quad {
public:
	explicit Row(RectParams params);

	int GetDirection() const;
	void SetDirection(int direction);
	const WallHole& GetDoors() const;
	void AddDoor(int direction, std::vector<RectParams> positions);
	const WallHole& GetWindows() const;
	void AddWindow(int direction, std::vector<RectParams> positions);
	void InstanciateQuad(float width, float height);

private:
	int direction = FACE_WEST;
	WallHole doors;
	WallHole windows;
	RectParams params;
};

// 一层楼实例化后的完整数据：局部坐标(以Building楼体左下角为原点，宽高=GetBodySizeX/Y())，
// 未做building自身旋转/世界偏移——渲染层/Map::InitBuildings()按需转换成世界坐标
// (Building::LocalToWorld)。
class Floor : public Quad {
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

// 一个方向(0-3)的行人/车辆导航模板，结构相同——见geometry.h的NavigationXxxTemplate。
struct NavigationTemplate {
	std::vector<NavigationNodeTemplate> nodes;
	std::vector<NavigationLineTemplate> lines;
	std::vector<NavigationConnectionTemplate> connections;
};

// 全局共享的建筑内部布局模板仓库：从磁盘.layout文件解析出来，Map::InitBuildings()加载
// 一次，传给每个Building::Layout()查询。命名上和ZoneMod::Layout()/BuildingMod::Layout()/
// Building::Layout()这一整套"落地后摆布局"虚方法区分开(不叫老工程的"Layout"这个名字，避免
// 看错成同一个概念)，详见building.md。
class BuildingLayoutLibrary {
public:
	// 扫描paths指向的每个.layout文件，解析并预算好4个朝向(InverseParams/InverseDirection/
	// InverseWall/InversePoint，照抄老工程Building::ReadTemplates的算法)，重复调用会清空
	// 之前解析的内容重新来一遍。
	void ReadTemplates(const std::vector<std::string>& paths);

	// 按模板名+朝向(0-3)查询这9类元素+2套导航模板，查不到返回空(引用一个静态空实例，不是
	// 抛异常——mod自己保证传的模板名字存在，Building::Layout()这一层不需要重复校验)。
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
	// 这次只解析、不使用，留给以后车辆域真正做"车辆能进建筑内部"这个玩法时再消费。
	const NavigationTemplate& GetVehicleNavigation(const std::string& name, int face) const;

	// 4个方向转换辅助函数——本来应该是private(只在ReadTemplates内部解析用)，但
	// ParseNavigationBlock(building.cpp里的匿名namespace自由函数，不是成员)也要用
	// InversePoint，干脆整体设成public：都是纯函数(没有任何实例状态)，公开出去没有
	// 封装上的坏处。
	static RectParams InverseParams(const RectParams& params, int face);
	static int InverseDirection(int direction, int face);
	static PointParams InversePoint(const PointParams& point, int face);
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

// Building::Layout()内部构建行人导航图的结果，交还给Map::InitBuildings()合并进全局图——
// Building自己不直接碰Map的私有成员，见building.md"行人导航"一节。
struct BuildingNavResult {
	// 需要登记进Map::navAnchorNodes统一管理生命周期/可视化的新建节点——包含building内部
	// 导航图专属的固定/投影锚点，也包含每个Room自己的导航节点(Building::Layout()里创建、
	// BuildPedestrianNavigation()结尾统一收进这里，不在别处另外登记)。
	std::vector<Node*> nodes;
	// 需要登记进Map::pedestrianNavGraph(双向)的新建连接。
	std::vector<Connection*> connections;
	// "outside"类型的端点节点——Map::InitBuildings()负责用building->GetBoundaryRoad
	// (building->GetDirection())+投影求t+AddRoadAccessNode把它们接上道路网，见
	// building.md"行人导航"一节；Building自己不知道怎么连Road。**这里的每个指针本来就已经
	// 是上面`nodes`的成员之一**(它们本来就是resolveEndpoint()解出的某个"node"/"line"锚点
	// 或Room导航节点，创建/收集时已经无条件进了`nodes`)，只是单独拎出来给Map一份"这些需要
	// 接道路网"的清单，不是一份独立于`nodes`之外的新节点集合——Map合并时只应该把这份清单
	// 当成"nodes的子集,用来找该连哪条路"来读，不能再把它们重新push进navAnchorNodes一次，
	// 否则同一个Node*登记两次，~Map()清理时按navAnchorNodes遍历delete会对它double free
	// (PIE验证发现的退出崩溃，见map.md"InitBuildings"一节)。
	std::vector<Node*> outsideNodes;
};

// Building：持有一个具体BuildingMod实例，代表一栋已经落地的Building（继承Quad表示自己
// 占据的矩形）。楼体footprint(基本形状)+楼层高度是Building自己的字段；每层楼内部具体
// 长什么样(墙/走廊/房间/楼梯/电梯/坡道/导航)由mod在Layout()里通过AssignFloor/AssignRoom/
// ArrangeRow声明，这里解析成真正的Floor/Room/Component数据，详见map.md"InitBuildings"
// 一节+building.md。
//
// 仿照老工程、和Zone同一个模式：Building独占持有一个mod实例的生命周期，构造时创建、析构时
// factory->DestroyBuilding(mod)。
//
// 不自己存一份rotation，直接转发parentLot->GetRotation()再叠加relativeRotation——普通(非
// 园区内部)building的relativeRotation恒为0，等价于原来纯转发的行为；园区内部building由
// Map::PlaceZoneInternalBuilding用SetParentLot(zone->GetParentLot(), spec.relativeRotation)
// 设置，parentLot直接复用zone自己的parentLot(所以转发基准天然和zone一致)，relativeRotation
// 是相对zone自身旋转的附加偏移。
class Building : public Quad {
public:
	Building() = delete;

	// @factory: building工厂(用于~Building()里DestroyBuilding); @mod: 这个Building独占持有的
	// mod实例(调用方保证不会再有别的Building共用同一个mod指针)。
	Building(BuildingFactory* factory, BuildingMod* mod);
	~Building();

	std::string GetType() const;
	std::string GetName() const;

	// 这个Building持有的mod实例——不需要另外拷贝一份。
	BuildingMod* GetMod() const;

	// 调用方在SetPosition/SetBoundaryRoad都设好之后调用一次：内部先调
	// mod->Layout(direction, *this, GetBoundaryRoads())（this已经是Quad、边界路也已经是真实
	// 数据；direction是引用参数，mod可能把-1改写成一个真实方向，见building_mod.h），再把
	// mod->footprint/basements/layers/floorHeights/lodMaterial解析成Building自己的绝对
	// (相对自身中心)数值并缓存，然后按mod记录的floors/singles/rows实例化每一层的Floor+
	// Room+Component，最后构建行人内部导航图。direction对显式占位落地是Assign选中的真实
	// 方向；对权重CDF/FillRemainder落地传-1（没有方向概念，交给mod自己兜底）；对园区内部
	// 建筑传spec.direction。roomFactory/componentFactory是Map持有的工厂实例，Building自己
	// 不持有(只在这一次调用里临时用来创建Room/Component，不需要跟Building自己持有的
	// BuildingFactory一样长期存一份指针——Room/Component创建完之后靠自己的factory指针
	// 析构，不依赖这里传的引用继续存活)。navOut收集这次调用产出的行人导航结果，供调用方
	// (Map::InitBuildings())合并进全局导航图。
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

	// 转发parentLot->GetRotation()+relativeRotation；parentLot为空时按0.f+relativeRotation算。
	float GetRotation() const;

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

private:
	// 按mod->floors实例化每一层的Floor，塞进floors数组。
	void ReadFloor(int level, int face, const std::string& templateName, const BuildingLayoutLibrary& library);

	// 按mod->singles/rows实例化Component+Room。
	void AssignRoom(int level, int slot, const std::string& roomType, Component* component, RoomFactory& roomFactory);
	void ArrangeRow(int level, int slot, const std::string& roomType, float acreage, Component* component,
		RoomFactory& roomFactory);

	// 按各楼层的pedestrianNavigation模板构建building内部导航图数据，见BuildingNavResult注释。
	void BuildPedestrianNavigation(const BuildingLayoutLibrary& library, int direction, BuildingNavResult& navOut);

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

	std::vector<Floor> floors;
	std::vector<Component*> components;
	std::vector<Room*> rooms;

	// 独立房间导航映射：[楼层编号][独立房间序号] -> 生成的房间（非持有引用）；联排房间同理，
	// 一个槽位对应一组房间——照抄老工程singleRoomBySlot/rowRoomBySlot，BuildPedestrianNavigation
	// 解析"single"/"row"类型的导航端点时用。
	std::unordered_map<int, std::unordered_map<int, Room*>> singleRoomBySlot;
	std::unordered_map<int, std::unordered_map<int, std::vector<Room*>>> rowRoomBySlot;
};
