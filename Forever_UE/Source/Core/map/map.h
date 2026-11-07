#pragma once

#include "class.h"

#include "map/general_geometry.h"
#include "map/terrain_factory.h"
#include "map/roadnet_factory.h"
#include "map/zone_factory.h"
#include "map/building_factory.h"

#include "map/terrain.h"
#include "map/roadnet.h"
#include "map/zone.h"
#include "map/building.h"
#include "map/door.h"
#include "map/vegetation.h"
#include "map/lamp.h"

#include "common/handle.h"

#include <string>
#include <vector>
#include <utility>
#include <unordered_map>
#include <array>
#include <tuple>
#include <memory>
#include <mutex>


// 10m*10m地图元素；hatches是地形挖洞标记(隧道口/地下室)，详见map.md。
struct Element {
	std::string terrain = "plain";
	float height = 0.f;
	std::pair<bool, float> water{ false, 0.f };
	std::vector<TerrainHatch> hatches;
	// 细化高度数组(17x17,覆盖[x,x+1]x[y,y+1],约定见terrain_mod.h的TERRAIN_FINE_SUBDIV),
	// 默认为空——只有起伏大的格子(目前是河岸)才分配,用unique_ptr让没有数组的格子只多8字节。
	std::unique_ptr<std::array<float, TERRAIN_FINE_SIZE * TERRAIN_FINE_SIZE>> fineHeights;
};

// 地图聚合：Element存储、地形/路网/Zone/Building/植被/路灯的生成流程、导航图、寻址、门/电梯实体，详见map.md
class Map {
	// 存档：MapSaveCodec直接读写私有字段(map_save.md)。
	friend class MapSaveCodec;
public:
	Map(int width, int height);
	~Map();

	// 按GetPriority()降序对所有已注册地形执行DistributeTerrain，再做plain->construction晋升、重建terrainTextures。
	// 不含mod dll的发现/注册(归Registry，整个进程只跑一次)，见map.md。
	void InitTerrains();

	std::pair<int, int> GetSize() const;

	std::string GetTerrain(int x, int y) const;
	// 同GetTerrain，但返回地图自己存的那份(越界返回空串)——给跨DLL的回调用，见Map::InitTerrains。
	const std::string& GetTerrainRef(int x, int y) const;

	bool SetTerrain(int x, int y, const std::string& terrain);
	float GetHeight(int x, int y) const;
	// 除了改粗高度，还会清掉共用格点(x,y)的4块细化数组((x,y)/(x-1,y)/(x,y-1)/(x-1,y-1))，
	// 防止细化数据角点和新的粗高度对不上——所以mod必须先SetHeight再SetFineHeights。
	bool SetHeight(int x, int y, float height);
	// 没有细化数组时返回nullptr；否则指向TERRAIN_FINE_SIZE^2个float(行主序)。
	const float* GetFineHeights(int x, int y) const;
	// 拷贝data(TERRAIN_FINE_SIZE^2个float)进Core自己分配的数组；data为nullptr时清除。
	bool SetFineHeights(int x, int y, const float* data);
	// 连续坐标(地图element单位)高度采样：落在有细化数组的格子里，就在1/16细化网格上双线性
	// (坐标正好在细化网格点上时等于直接查表——UE各级LOD顶点都是这种情况)；否则退回4个格点
	// 粗高度的双线性。UE地形网格/出生点都走这个函数，见map.md"细化高度"一节。
	float SampleHeight(float x, float y) const;
	std::pair<bool, float> GetWater(int x, int y) const;
	// 每个element的水面独立设置(同一次DistributeTerrain里不同格子可有不同水位)，见terrain_mod.md"水面"一节。
	bool SetWater(int x, int y, bool hasWater, float height);
	const std::vector<TerrainHatch>& GetHatches(int x, int y) const;

	// 自动分发到所有与q重叠的element(按q的旋转AABB计算重叠范围)。
	void AddHatch(Quad q, float rotation);
	void AddHatch(const TerrainHatch& hatch);

	// 路网里带侧壁的hatch(隧道口，wall=true)，UE层据此生成侧壁mesh。roadnet为空返回空；读档后
	// roadnet的hatches还在，所以也能拿到。
	std::vector<TerrainHatch> GetTunnelWallHatches() const;

	// 地形类型 -> {纹理数组槽位索引, diffuse/normal/roughness三套资产路径}
	struct TerrainTextureInfo {
		int slice = 0;
		std::string diffuse;
		std::string normal;
		std::string roughness;
	};
	const std::unordered_map<std::string, TerrainTextureInfo>& GetTerrainTextures() const;

	// 全图植被(InitVegetation里展开)，UE层UForeverVegetationFrameworkComponent一次性读取
	// 渲染，见vegetation.md。
	const Vegetation& GetVegetation() const;
	// 全图路灯(InitLamps里展开)，UE层UForeverLampFrameworkComponent读取后逐盏生成Actor，见lamp.md。
	const Lamps& GetLamps() const;
	// 这个点(地图element单位)能不能种树：越界、有水面、有挖洞(隧道口等)的格子不种。
	bool CanPlaceTree(float x, float y) const;

	// 用已注册的roadnetFactory构建路网、建车行/行人导航图，假定InitTerrains已跑完，流程见map.md"InitRoadnet"。
	void InitRoadnet();

	// 把路网产出的地形贴合请求烤进地形高度(InitRoadnet内调用)，不经SetHeight，详见map.md"地形贴合"。
	void ApplyTerrainFits(const std::vector<TerrainFitRequest>& fits);

	const std::vector<Road*>& GetRoads() const;
	const std::vector<Intersection*>& GetIntersections() const;
	const std::vector<Node*>& GetExterns() const;
	const std::vector<Lot*>& GetLots() const;
	const std::vector<RoadJunction*>& GetJunctions() const;
	Lot* LocateLot(const std::string& road, int index) const;

	// 在road的弧长比例t处为车行/行人图新增访问点(参数是Road*不是名字)，详见map.md"导航图与贯通线"
	Node* AddRoadAccessNode(Road* road, float t, bool isVehicle, bool useForwardSide, float openingWidth);

	// 把Lot::SplitWithPath产出的一条小路接入车行/行人导航图，详见map.md"ConnectPathRoad"
	void ConnectPathRoad(const PathRoadLink& link);

	// 自适应占位成功后把usedChild的残余freeLots搬回parent，详见map.md"自适应占位"
	void PromoteAdaptiveRemainder(Lot* parent, Lot* usedChild);

	// 用已注册的zoneFactory按显式占位生成zone(Assign->RequestPlacement->CreateZone->Layout)，假定InitRoadnet已跑完，
	// 详见map.md"InitZones"。
	void InitZones();

	// 按显式占位+权重CDF/FillRemainder落地Building，假定InitZones已跑完，详见map.md"InitBuildings"
	void InitBuildings();

	// 植被是最后一个阶段，必须在InitBuildings()之后(行道树要避开Road::GetOpenings)：路网/building/zone各自展开树请求，
	// 过滤规则见CanPlaceTree，详见vegetation.md。
	void InitVegetation();

	// 路灯：紧跟InitVegetation()之后调用(开口同样已经齐全)。目前只有路网宿主：
	// roadnet->LayoutLamps()展开后交给Lamps，过滤规则同CanPlaceTree，见lamp.md。
	void InitLamps();

	// 住宅房间折算的目标人口(累计容量<住宅房间数两倍时取房间数，否则取容量一半)，供Populace::Init，假定InitBuildings已跑完。
	int ComputeAccommodationTarget() const;
	// 全部工位房间(IsWorkspace)的WorkspaceCapacity之和，不参与人口随机采样，Populace::Init在采样之后用它
	// 给人数封顶，避免劳动力远多于岗位导致大面积失业。
	int ComputeWorkCapacity() const;

	// 房产归属(owner/stated)与住处分配(households/occupants)是两个独立步骤，详见map.md"房产归属与住处分配"
	void Checkin(const Populace& populace);
	// 让citizen住进room：家(Lot/Zone/Building/Room+户)和当前位置(CurrentRoom+occupants)都设成这里。
	// Checkin和spawn_npc的home字段共用。
	void SettleCitizen(Citizen* citizen, Room* room);

	// 创建一扇门并登记进三份索引(id/交互名/allDoors)，详见map.md"门系统"
	Door* CreateDoor(DOOR_KIND_TYPE kind, Zone* zone, Building* building, Room* room,
		const std::string& tag, const DoorSpec& spec,
		float x, float y, float z, float width, float height, float yaw, int level, bool isVehicleGate);

	// 按交互名查门(只有spec.name非空的门有交互名)，找不到返回nullptr。
	Door* FindDoorByName(const std::string& interactName) const;

	// 创建一台电梯(Building::BuildCabins调用)：交互名"<building名> 电梯<n>"(n按这栋楼已建的电梯数+1)，
	// 初始停在1楼(夹到范围内)，登记进cabinsByInteractName。电梯归building的cabinEntities持有。
	Cabin* CreateCabin(Building* building, int specIndex, const ElevatorCabinSpec& spec);

	// 实体脚本(Zone/Building/Room各自挂的Script)：已有Script的实体跳过，重复调用安全；读档重建后用BuildAllEntityScripts补建，
	// 详见map.md"实体脚本"。
	void BuildZoneScript(Zone* zone);
	void BuildBuildingScripts(Building* building);
	void BuildAllEntityScripts();
	// 按交互名查电梯，找不到返回nullptr。
	Cabin* FindCabinByName(const std::string& interactName) const;
	// 按名字找房间里的资产(含容器内容)，遍历全部房间；找不到返回nullptr。玩家身上/地上的见Player::FindAssetByName。
	Asset* FindAssetByName(const std::string& name) const;

	// 所有真正放了门扇的门——不分三类，供UForeverStoryFrameworkComponent::
	// BroadcastGameStart遍历+过滤GetScript()!=nullptr。
	const std::vector<Door*>& GetAllDoors() const;

	const std::unordered_map<std::string, Zone*>& GetZones() const;
	const std::unordered_map<std::string, Building*>& GetBuildings() const;

	// 按Zone/Building自己的唯一name做扁平查找(唯一性由mod自己的GetName()/构造函数计数器
	// 保证，Map只在AddZone/AddBuilding发现重名时拒绝加入+debugf警告作为兜底，不主动生成
	// 名字)。找不到返回nullptr。
	Zone* GetZone(const std::string& name) const;
	Building* GetBuilding(const std::string& name) const;

	// 分层地址查找("Block"即顶层Lot)："<road> <index> <zoneName>"、"<road> <index> <buildingName>"(无parentZone)、
	// "<road> <index> <zoneName> <buildingName>"，找不到返回nullptr，见map.md"寻址"。
	Zone* LocateZone(const std::string& address) const;
	Building* LocateBuilding(const std::string& address) const;

	// 地址在LocateBuilding的格式末尾再加房间号，找不到返回nullptr，见map.md"寻址"。
	Room* LocateRoom(const std::string& address) const;

	// 汇总每个顶层Lot自己的GetPathRoads()(小路归Lot所有，Map不持有)，按值返回。
	std::vector<Road*> GetPathRoads() const;

	// 转发roadnet->GetPathRoadMaterial()，供InitZones/InitBuildings构造PathLaneSpec、
	// Forever层渲染小路时查。roadnet为空(InitRoadnet没跑或没有可用mod)时返回空字符串。
	const std::string& GetPathRoadMaterial() const;

	// 车行/行人导航图只读访问，供Forever层可视化/未来Traffic域寻路使用。key/邻接id都是锚点
	// Node::GetId()，锚点本身的坐标通过GetNavAnchorNodes()（路口/车道分裂新增锚点）+
	// GetExterns()（地图边缘残端，也可能是graph里的端点）查到。
	const std::unordered_map<int, std::vector<std::pair<int, Connection*>>>& GetVehicleNavGraph() const;
	const std::unordered_map<int, std::vector<std::pair<int, Connection*>>>& GetPedestrianNavGraph() const;
	const std::vector<Node*>& GetNavAnchorNodes() const;

	// 对pedestrianNavGraph做A*寻路(边权=Connection::CalcDistance())，返回含起终点的路径点；不连通/id不存在返回空。
	// 供市民走路用，见Source/Forever/Framework/ForeverPopulaceFrameworkComponent.md"市民走路"。
	std::vector<const Node*> FindPedestrianPath(int fromNodeId, int toNodeId) const;

	// 对vehicleNavGraph寻路(共享私有FindPath)，返回依次经过的Connection*序列(公交线路要拼接真实车道几何，见traffic/route.md)；找不到返回空。
	std::vector<Connection*> FindVehiclePath(int fromNodeId, int toNodeId) const;

	// 纯只读查询(公交站旁路用)：(x,y)沿(dirX,dirY)方向看，落在road哪一条车行贯通线段上、段两端锚点是谁。
	// 不修改throughLines/导航图；road为空、没有车行道或t不在任何段内返回false。
	struct LaneSegment {
		Connection* edge = nullptr;
		Node* fromAnchor = nullptr;
		Node* toAnchor = nullptr;
		float t = 0.f;
		float worldX = 0.f, worldY = 0.f, worldZ = 0.f;
	};
	bool GetLaneSegmentAt(Road* road, float x, float y, float dirX, float dirY, LaneSegment& out) const;

	// 车辆/行人导航测试用，详见map.md"路径规划"
	struct NavPoint { float x = 0.f, y = 0.f, z = 0.f; };
	struct VehicleRoutePlan {
		enum Status { OK, NO_PATH_TO_DESTINATION, NO_ROAD_CONNECTION };
		Status status = NO_ROAD_CONNECTION;
		std::vector<NavPoint> points;
	};
	VehicleRoutePlan PlanVehicleRoute(Room* startRoom, float x, float y, Room* destRoom) const;

	// 公交站辅助导航节点：stationNode不在vehicleNavGraph里(Station自己拥有，导航图参与存档/析构)，在此单独登记(运行时、不存档、不拥有)。
	// 站作终点=路径到laneFrom再走inEdge；作起点=先走outEdge再从laneTo出发；同段车道上起点在上游=两个stationNode直线弦(见route.md"同车道特例")。
	// Station::EnsureRoadLink调用Register，~Station调用Unregister。
	struct AuxNavNode {
		Node* node = nullptr;        // stationNode
		Node* laneFrom = nullptr;
		Connection* inEdge = nullptr;  // laneFrom -> node
		Node* laneTo = nullptr;
		Connection* outEdge = nullptr; // node -> laneTo
		Connection* sourceEdge = nullptr;
		float t = 0.f;
	};
	void RegisterAuxNavNode(const AuxNavNode& aux);
	void UnregisterAuxNavNode(int nodeId);
	const AuxNavNode* FindAuxNavNode(int nodeId) const;

	// 同PlanVehicleRoute，但终点是vehicleNavGraph节点id或已登记的辅助节点(公交站)id，不要求是房间。
	// useHeading为true时，室外起点接入车道会把车道方向与车头(headingX,headingY)的夹角折算成额外距离(180度=15m)参与比较，避免选到反向车道。
	// destNodeId既不在图里也不是辅助节点 -> NO_PATH_TO_DESTINATION，points为空。
	VehicleRoutePlan PlanVehicleRouteToNode(Room* startRoom, float x, float y, int destNodeId,
		bool useHeading = false, float headingX = 0.f, float headingY = 0.f) const;

	// 节点到节点（任意一端可以是辅助节点）。OK时points是完整折线（含起点终点），from==to时只有一个点；
	// 连不通 -> NO_PATH_TO_DESTINATION，points为空。公交循环里站到站用。
	VehicleRoutePlan PlanVehicleLeg(int fromNodeId, int toNodeId) const;

	// 市民导航测试用：从任意起点规划一条到destRoom的行人折线（地图单位）。startRoom非空且有
	// 行人导航节点 -> 从该房间节点出发；否则从(x,y)最近的几个行人图节点里挑第一个能走到
	// 目的地的。points是各导航节点坐标（含起点节点和目的房间节点），起点==终点时只有一个点。
	//   OK：points是完整折线。
	//   NO_PATH_TO_DESTINATION：找到了起点节点但图不连通，points只含这个起点节点。
	//   NO_ROAD_CONNECTION：行人图里没有任何可用起点节点，points为空。
	VehicleRoutePlan PlanPedestrianRoute(Room* startRoom, float x, float y, Room* destRoom) const;

	// 拍平所有building的component，供Society::Init(Society不知道Map/Building)。
	std::vector<Component*> GetAllComponents() const;

	// 空实现：Map域没有每帧逻辑，保持各域统一的Tick形状，见map.md。
	void Tick(const Time& currentTime, bool crossedDay, PostHandle* post);

	// 处理门/电梯/资产选项/record_object相关的Change，返回是否认识该类型，详见map.md"ApplyChange"
	bool ApplyChange(const Change* change, const ScriptContext& context);

private:
	bool CheckXY(int x, int y) const;
	Element& At(int x, int y);
	const Element& At(int x, int y) const;

	int width;
	int height;
	std::vector<Element> elements; // 行主序扁平数组(y*width+x)

	// Registry里的Factory引用成员：只能在构造函数初始化列表绑定，声明顺序必须与初始化列表一致，详见map.md"Registry引用成员"
	TerrainFactory& terrainFactory;
	RoadnetFactory& roadnetFactory;
	ZoneFactory& zoneFactory;
	BuildingFactory& buildingFactory;
	RoomFactory& roomFactory;
	ComponentFactory& componentFactory;
	// 门/电梯/实体Script创建用。
	ScriptFactory& scriptFactory;

	std::unordered_map<std::string, TerrainTextureInfo> terrainTextures;

	Vegetation vegetation;
	Lamps lamps;

	Roadnet* roadnet = nullptr;
	std::vector<RoadJunction*> junctions;

	// 从磁盘.layout文件解析出来的建筑内部布局模板仓库，InitBuildings()加载一次，
	// 传给每个Building::Layout()查询，详见building.md。
	BuildingLayoutLibrary buildingLayoutLibrary;
	// 按name索引，GetZone/GetBuilding直接find。
	std::unordered_map<std::string, Zone*> zones;
	std::unordered_map<std::string, Building*> buildings;

	// 读档时重建出来的"游离"Lot：自适应占位(PromoteAdaptiveRemainder)用过的嵌套子块，生成时
	// 已经从父块的freeLots里摘掉、不归任何Lot持有，但还被building->GetParentLot()引用着。新开局
	// 时这类子块没有任何持有者(一直存活到进程结束)；读档时由Map持有、析构时释放，见map_save.md。
	std::vector<Lot*> orphanLots;

	// 门索引——不持有所有权(Door*的生命周期归Zone/Building/Room各自的doorEntities管)，
	// 这几个容器只是方便按id/交互名查找+遍历全部。doorCount是全局门计数器("Door"+n)；
	// doorNameSequence按"所属对象名+spec.name"分组计数交互名的编号(从1开始)，见
	// door.md"tag是一类门，不是身份"一节。
	int doorCount = 0;
	std::unordered_map<std::string, int> doorNameSequence;
	std::unordered_map<std::string, Door*> doorsById;
	std::unordered_map<std::string, Door*> doorsByInteractName;
	std::vector<Door*> allDoors;

	// 电梯索引，不持有(电梯归Building::cabinEntities)。
	std::unordered_map<std::string, Cabin*> cabinsByInteractName;

	// 按name插入，重名返回false并debugf警告、不插入(不生成消歧名字——唯一性是mod自己的
	// GetName()/构造函数计数器的责任，这里只是兜底)；成功插入返回true。
	bool AddZone(Zone* zone);
	bool AddBuilding(Building* building);

	// 车行/行人导航图：key是锚点Node::GetId()，value是(邻接锚点id, 边)列表。车行边只按实际
	// 通行方向单向插入；行人边(横道/转角/贯通线)双向插入，可能出现同一个Connection*被两条
	// 邻接记录共同引用，析构时用set去重删除一次。
	std::unordered_map<int, std::vector<std::pair<int, Connection*>>> vehicleNavGraph;
	std::unordered_map<int, std::vector<std::pair<int, Connection*>>> pedestrianNavGraph;

	// 公交站辅助导航节点（见AuxNavNode），key是stationNode的id。运行时登记，不存档。
	std::unordered_map<int, AuxNavNode> auxNavNodes;

	// from/to任意一端可以是辅助节点；把from到to的折线追加进points（起点在points为空时一起加入）。
	// 连不通返回false（points保持原样）。
	bool AppendLegPoints(int fromId, int toId, std::vector<NavPoint>& points) const;

	// InitRoadnet/AddRoadAccessNode过程中创建的所有锚点Node(路口锚点+车道分裂新增的访问点)，
	// 析构时统一释放；导航图本身的Connection*从vehicleNavGraph/pedestrianNavGraph遍历去重释放。
	std::vector<Node*> navAnchorNodes;

	// 每条Road的车道贯通线记录，详见map.md"导航图与贯通线"
	struct ThroughLine {
		Connection* edge = nullptr;
		Node* fromAnchor = nullptr;
		Node* toAnchor = nullptr;
		int laneIndex = 0;
		float tLo = 0.f;
		float tHi = 1.f;
	};
	std::unordered_map<Road*, std::array<std::vector<ThroughLine>, 4>> throughLines;

	// 把世界坐标投影回road弧长参数t，在贯通线上断出新node，详见map.md"导航图与贯通线"
	Node* BreakThroughLine(Road* road, bool isVehicle, int side, int laneIndex, float worldX, float worldY, float worldZ);

	// 算road在t处某条车道/人行道的中心世界坐标，详见map.md"导航图与贯通线"
	std::pair<float, float> ComputeLaneAnchorPosition(Road* road, float t, bool isVehicle, int side, int laneIndex) const;

	// 新建一个不接入任何既有贯通线的孤立锚点node(小路端点没有可连的路时用)，登记进
	// navAnchorNodes，返回新node。
	Node* MakeIsolatedAnchor(float worldX, float worldY, float worldZ, const char* category);

	// ConnectPathRoad的核心：解出小路path在isStartEnd这一端的4个锚点(outVeh[0]/[1]=车行side0/1，outPed[0]/[1]=人行side0/1)；
	// hostRoad为空按孤立端点处理，规则见map.md"ConnectPathRoad"。
	void ResolvePathEndAnchors(Road* path, bool isStartEnd, Road* hostRoad, float hostT,
		std::array<Node*, 2>& outVeh, std::array<Node*, 2>& outPed);

	// zone局部坐标(原点在矩形中心)转世界坐标，标准2D旋转。
	std::pair<float, float> ZoneLocalToWorld(const Zone* zone, float x, float y) const;

	// 给zone的一个车行/行人出入口点接图，详见map.md"Zone内部布局"
	Node* ConnectZoneAccessPoint(Zone* zone, float x, float y, float width, bool isVehicle, bool isEntry,
		std::vector<std::tuple<float, float, bool, Node*>>& anchorCache);

	// 把ZoneInternalRoadSpec实例化成Road(mesh=""/unit=0.f，只连导航图不参与路面铺设，同SplitWithPath的小路)；
	// 返回的Road*由调用方收集后交给zone->SetInternalRoads持有，见map.md"Zone内部布局"。
	Road* ConnectZoneInternalRoad(Zone* zone, const ZoneInternalRoadSpec& spec,
		std::vector<std::tuple<float, float, bool, Node*>>& anchorCache);

	// 把ZoneInternalBuildingSpec实例化成Building，详见map.md"Zone内部布局"
	Building* PlaceZoneInternalBuilding(Zone* zone, const ZoneInternalBuildingSpec& spec,
		const std::vector<Road*>& builtInternalRoads, BuildingMod* mod);

	// 合并Building::Layout()产出的导航结果进Map的导航图/navAnchorNodes，详见map.md"Building内部布局"
	void MergeBuildingNavigation(Building* building, const BuildingNavResult& result);

	// 把建筑地下一层的Hatch转发进世界地形，详见map.md"Building内部布局"
	void ForwardBuildingHatches(Building* building);

	// 一个尚未执行的"building outside端点->道路访问点"断开请求，由MergeBuildingNavigation收集，FlushPendingBuildingRoadAccess()按物理顺序统一执行。
	struct PendingRoadAccess {
		Road* road;
		float t;
		bool useForwardSide;
		Node* outsideNode;
		bool isVehicle = false;
		// 仅isVehicle==true时有意义：true=出口，连接方向"outsideNode->access node"；false=入口，方向"access node->outsideNode"。
		// 行人分支不看这个字段(固定双向)。
		bool isExit = false;
	};
	std::vector<PendingRoadAccess> pendingBuildingRoadAccess;

	// 按(road,side,laneIndex)分组统一接入building的outside端点，详见map.md"Building内部布局"
	void FlushPendingBuildingRoadAccess();

	// AddRoadAccessNode用的纯查询版本：按同一套单行道退化规则解出实际会用的side/laneIndex。FlushPendingBuildingRoadAccess()排序前
	// 用它预判落点，必须与AddRoadAccessNode一致，所以两处共用这份实现。road两侧都没有对应类别车道时返回false。
	static bool ResolveAccessLane(Road* road, bool isVehicle, bool useForwardSide, int& outSide, int& outLaneIndex);

	// 待处理的小路接入host道路请求，详见map.md"ConnectPathRoad"
	std::vector<PathRoadLink> pendingPathRoadLinks;

	// 统一接入pendingPathRoadLinks里小路的两端，详见map.md"ConnectPathRoad"
	void FlushPendingPathRoadLinks();

	// 把点投影到road中心线，返回弧长比例t(裁剪到[0,1])：直线O(1)解析求垂足，有控制点的曲线退化成数值采样+局部细化。
	static float ProjectPointOntoRoad(Road* road, float px, float py);

	// FindPedestrianPath/FindVehiclePath共享的寻路核心，只按节点id运算，边权=Connection::CalcDistance()；返回含起终点的id序列，
	// 不连通/id不存在返回空。coords非空时按A*运算(启发值=到终点的水平直线距离，不超过真实路径长度，保证最优)。
	std::vector<int> FindPath(const std::unordered_map<int, std::vector<std::pair<int, Connection*>>>& graph,
		int fromNodeId, int toNodeId, const std::unordered_map<int, const Node*>* coords = nullptr) const;

	// FindPedestrianPath用的"id -> Node*"索引缓存(节点上万，不能每次寻路重建)。线程池里的多个任务会同时调用，所以检查/重建时加锁；
	// 节点数量变了会自动重建。
	mutable std::mutex nodeIndexMutex;
	mutable std::unordered_map<int, const Node*> nodeIndex;
	mutable size_t nodeIndexAnchorCount = (size_t)-1;
	mutable size_t nodeIndexExternCount = (size_t)-1;
};
