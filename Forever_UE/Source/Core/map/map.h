#pragma once

#include "class.h"

#include "map/general_geometry.h"
#include "map/terrain_factory.h"
#include "map/roadnet_factory.h"
#include "map/zone_factory.h"
#include "map/green_factory.h"
#include "map/building_factory.h"

#include "map/terrain.h"
#include "map/roadnet.h"
#include "map/zone.h"
#include "map/green.h"
#include "map/building.h"
#include "map/door.h"
#include "map/vegetation.h"
#include "map/lamp.h"

#include "common/handle.h"

#include <string>
#include <vector>
#include <utility>
#include <unordered_map>
#include <unordered_set>
#include <array>
#include <tuple>
#include <memory>
#include <mutex>
#include <atomic>
#include <algorithm>


// 10m*10m地图元素；hatches是地形挖洞标记(隧道口/地下室)，详见map.md。
struct Element {
	std::string terrain = "plain";
	// 可建设标记(InitTerrains标出)：3x3范围内高度都为0。和地形类型无关，terrain始终是原始地形名。
	bool construction = false;
	float height = 0.f;
	std::pair<bool, float> water{ false, 0.f };
	std::vector<TerrainHatch> hatches;
	// 细化高度数组(17x17,覆盖[x,x+1]x[y,y+1],约定见terrain_mod.h的TERRAIN_FINE_SUBDIV),
	// 默认为空——只有起伏大的格子(目前是河岸)才分配,用unique_ptr让没有数组的格子只多8字节。
	std::unique_ptr<std::array<float, TERRAIN_FINE_SIZE * TERRAIN_FINE_SIZE>> fineHeights;
};

// 导航图邻接表：节点id -> (邻居id, 边)。读接口和unordered_map一致(begin/end/find/count/size，都是const)，
// 修改只能走下面的成员函数，每个都让revision+1：寻路的"编译图"和行人路线缓存据此O(1)判断图有没有变
// (原来每次寻路遍历所有边算校验和，约0.5ms，比命中缓存本身贵得多)。
// 故意不继承unordered_map、也不暴露内部容器的非const引用：这样没有任何办法绕过revision去改图。
class NavGraph {
public:
	using Edges = std::vector<std::pair<int, Connection*>>;
	using Container = std::unordered_map<int, Edges>;
	using const_iterator = Container::const_iterator;

	// ---- 只读 ----
	const_iterator begin() const { return data.begin(); }
	const_iterator end() const { return data.end(); }
	const_iterator find(int id) const { return data.find(id); }
	size_t count(int id) const { return data.count(id); }
	size_t size() const { return data.size(); }
	bool empty() const { return data.empty(); }
	const Edges& at(int id) const { return data.at(id); }
	uint64_t Revision() const { return revision; }
	// 全部节点id升序(存档按key排序写，保证字节稳定)。
	std::vector<int> SortedIds() const { std::vector<int> ids; ids.reserve(data.size()); for (const auto& entry : data) ids.push_back(entry.first); std::sort(ids.begin(), ids.end()); return ids; }

	// ---- 修改(都会revision+1) ----
	void AddEdge(int from, int to, Connection* connection) { revision++; data[from].emplace_back(to, connection); }
	// 取(没有就建)from的出边表，供读档按顺序往里填；返回的引用只在本次填充期间用，不要保存。
	Edges& EdgesOf(int from) { revision++; return data[from]; }
	// 删from->to的所有边(只删这个方向)。
	void RemoveEdges(int from, int to) {
		revision++;
		auto it = data.find(from);
		if (it == data.end()) return;
		Edges& edges = it->second;
		edges.erase(std::remove_if(edges.begin(), edges.end(), [to](const std::pair<int, Connection*>& e) { return e.first == to; }), edges.end());
	}
	// 整张图里删掉所有满足pred(fromId, edge)的边。
	template <class Pred> void RemoveEdgesIf(Pred pred) {
		revision++;
		for (auto& [from, edges] : data) {
			edges.erase(std::remove_if(edges.begin(), edges.end(), [&](const std::pair<int, Connection*>& e) { return pred(from, e); }), edges.end());
		}
	}
	// 删掉节点自己的出边表(别的节点指向它的边另外用RemoveEdges/RemoveEdgesIf删)。
	void EraseNode(int id) { revision++; data.erase(id); }

private:
	Container data;
	uint64_t revision = 0;
};

// 地图聚合：Element存储、地形/路网/Zone/Building/植被/路灯的生成流程、导航图、寻址、门/电梯实体，详见map.md
class Map {
	// 存档：MapSaveCodec直接读写私有字段(map_save.md)。
	friend class MapSaveCodec;
public:
	Map(int width, int height);
	~Map();

	// 按GetPriority()降序对所有已注册地形执行DistributeTerrain，再按"3x3范围内高度都为0"标记construction(可建设)。
	// 不含mod dll的发现/注册(归Registry，整个进程只跑一次)，见map.md。
	void InitTerrains();

	std::pair<int, int> GetSize() const;

	std::string GetTerrain(int x, int y) const;
	// 同GetTerrain，但返回地图自己存的那份(越界返回空串)——给跨DLL的回调用，见Map::InitTerrains。
	const std::string& GetTerrainRef(int x, int y) const;
	// 该element是否可建设(Element::construction)，越界返回false。
	bool IsConstruction(int x, int y) const;

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
	// 粗高度：每个格子按"自适应对角线"分成两个三角形线性插值(见UsesAntiDiagonal)，而不是双线性。
	// UE地形网格/出生点都走这个函数，见map.md"细化高度"一节。
	float SampleHeight(float x, float y) const;
	// 格子(ex,ey)的粗高度插值用哪条对角线：false=主对角线(左上(x,y)-右下(x+1,y+1))，true=副对角线((x+1,y)-(x,y+1))。
	// 取两端高度差较小的那条：斜着走的地形交界(锯齿状台阶)上，格子往往是"三高一低"，对角线选在两个高点之间，
	// 整条交界就是一道笔直的斜坡，而不是双线性的锯齿鞍面。UE网格的小格拆三角形必须用同一个判断，网格和采样才一致。
	bool UsesAntiDiagonal(int ex, int ey) const;
	std::pair<bool, float> GetWater(int x, int y) const;
	// 每个element的水面独立设置(同一次DistributeTerrain里不同格子可有不同水位)，见terrain_mod.md"水面"一节。
	bool SetWater(int x, int y, bool hasWater, float height);
	const std::vector<TerrainHatch>& GetHatches(int x, int y) const;

	// 自动分发到所有与q重叠的element(按q的旋转AABB计算重叠范围)。
	void AddHatch(Quad q, float rotation);
	void AddHatch(const TerrainHatch& hatch);
	// 线路(火车隧道)用：AddHatch之外，带侧壁的hatch(wall=true)再记进roadnet的hatches，这样GetTunnelWallHatches拿得到、存档里也有。
	void AddRouteHatch(const TerrainHatch& hatch);

	// 路网里带侧壁的hatch(隧道口，wall=true)，UE层据此生成侧壁mesh。roadnet为空返回空；读档后
	// roadnet的hatches还在，所以也能拿到。
	std::vector<TerrainHatch> GetTunnelWallHatches() const;

	// 路网(公路桥)的桥墩，UE层据此生成墩的mesh。roadnet为空返回空；读档后来自存档。
	std::vector<Pier> GetRoadPiers() const;

	// 地形类型 -> 外观(地图界面颜色+地表材质声明)。向已注册的地形mod收集(调SetupTexture)后缓存，所以新建的世界和读档后的世界都能用；
	// 另含默认地形"plain"(没有mod，不声明材质和地图颜色，由渲染层/界面兜底)。
	const std::unordered_map<std::string, TerrainAppearance>& GetTerrainAppearances() const;

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

	// 拆掉所有被strips条带(折线+宽度)扫到的楼体；命中园区内部建筑则整个园区(含内部建筑/小路)一起拆。keep返回true的建筑
	// 不拆(带stationMod的建筑：它的Station已建好)；园区里有keep的建筑则整个园区也不拆。只能在生成阶段、
	// InitVegetation/InitLamps之后、网格生成之前调用，返回拆掉的建筑数(含园区内部建筑)，见map.md"拆除"。
	int DemolishAlongStrips(const std::vector<DemolishRequest>& strips, const std::function<bool(const Building*)>& keep);

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

	// 路网预设(Roadnet::GetPresets)：把"请求→RequestPlacement→CreateZone/CreateBuilding→Layout"拆成占位和落地两步，
	// Assign和预设共用落地函数。预设在InitZones开头一次性按顺序占位(无论id是否有效都占，保证和编辑器按创建顺序重放的结果一致)，
	// zone预设立即落地，building预设存进pendingBuildingPresets，InitBuildings开头(模板库加载后)落地。详见map.md"路网预设"。
	struct PlacedPreset {
		std::string id;
		Lot* lot = nullptr;            // 落地用的父lot(占位发生的那块lot，预设落在子块时是子块)
		Lot* adaptiveParent = nullptr; // 非空=lot是嵌套子块，落地时继承它的地址
		Quad quad;
		std::unordered_map<int, Road*> boundaryRoads;
		int direction = 0;
		int outputIndex = -1;          // Roadnet::Preset::outputIndex，落地后登记进presetOfBuilding
		AREA_TYPE area = AREA_NONE;    // 顶层lot的分区：id落空时按它的权重随机退化(PickRandomBuildingId)
	};
	std::string PickRandomBuildingId(AREA_TYPE area);
	std::vector<PlacedPreset> pendingBuildingPresets;
	// 预设落地成功的建筑 -> 预设在.roadnet output.presets里的原下标。线路环按它找到站(Route::Build填RouteStationInfo::presetIndex)。不存档(读档不重新Build线路)。
	std::unordered_map<const Building*, int> presetOfBuilding;
	void PlacePresets();
	bool CreateZoneAt(const std::string& id, Lot* lot, const Quad& placedQuad, const std::unordered_map<int, Road*>& boundaryRoads, int direction);
	bool CreateBuildingAt(const std::string& id, Lot* lot, Lot* adaptiveParent, const Quad& placedQuad, const std::unordered_map<int, Road*>& boundaryRoads, int direction, Building** created = nullptr);

	// 用已注册的zoneFactory按显式占位生成zone(Assign->RequestPlacement->CreateZone->Layout)，假定InitRoadnet已跑完，
	// 详见map.md"InitZones"。
	void InitZones();

	// 按显式占位+权重CDF/FillRemainder落地Building，假定InitZones已跑完，详见map.md"InitBuildings"
	void InitBuildings();

	// 路网指定的绿化多边形(Roadnet::GetGreenSites)逐个落地：按"有没有连路"选一套权重挑mod，mod布置活动圆/树/装饰物，
	// 通过检查的圆接入行人导航图。必须在InitBuildings()之后(接路要用FlushPendingBuildingRoadAccess的断开顺序)、
	// InitVegetation()之前，详见map.md"InitGreens"。
	void InitGreens();

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
	// 不动产资产：按Checkin写下的归属(owner/stated)建立EstateAsset，只记"最高层"——园区整体归属就只有园区
	// 一个资产，不再给下属建筑/房间各建一个；建筑整体归属同理。私有的加进该Citizen的资产列表，公有(stated)的
	// 收进GetStatedEstateAssets()由框架交给政府(Society::SetGovernmentAssets)。EstateAsset由Map持有，
	// 重复调用会先清掉旧的。必须在Checkin(新游戏)或读档恢复归属(populace段)之后调用，详见map.md。
	void BuildEstateAssets();
	const std::vector<Asset*>& GetStatedEstateAssets() const { return statedEstateAssets; }

	// 让citizen住进room：家(Lot/Zone/Building/Room+户)和当前位置(CurrentRoom+occupants)都设成这里。
	// Checkin和spawn_npc的home字段共用。
	void SettleCitizen(Citizen* citizen, Room* room);
	// 搬家(成年搬出/婚后同居，见populace/demography.md)：从原来的家移出(Room::RemoveResident，户空了房子就空出来)，
	// SettleCitizen到newHome，并瞬移过去(结束行走、清掉自由位置)；有工作的刷新工作脚本里的home地址。
	// UE层另外按WorldStepHooks::onRelocated挪它的Actor。
	void MoveCitizen(Citizen* citizen, Room* newHome);
	// 随机一间空住宅(住宅房间且没有任何户)，没有返回nullptr。迁入和人口推进共用。调用方负责随机作用域。
	Room* PickVacantResidence() const;

	// 创建一扇门并登记进三份索引(id/交互名/allDoors)，详见map.md"门系统"
	Door* CreateDoor(DOOR_KIND_TYPE kind, Zone* zone, Building* building, Room* room,
		const std::string& tag, const DoorSpec& spec,
		float x, float y, float z, float width, float height, float yaw, int level, bool isVehicleGate);

	// 按交互名查门(只有spec.name非空的门有交互名)，找不到返回nullptr。
	Door* FindDoorByName(const std::string& interactName) const;
	// 按id查门(Door::GetId，计时器归属键"door:<id>"用)，找不到返回nullptr。
	Door* FindDoorById(const std::string& id) const;

	// 创建一台电梯(Building::BuildCabins调用)：交互名"<building名> 电梯<n>"(n按这栋楼已建的电梯数+1)，
	// 初始停在1楼(夹到范围内)，登记进cabinsByInteractName。电梯归building的cabinEntities持有。
	Cabin* CreateCabin(Building* building, int specIndex, const ElevatorCabinSpec& spec);

	// 实体脚本(Zone/Building/Room各自挂的Script)：已有Script的实体跳过，重复调用安全；读档重建后用BuildAllEntityScripts补建，
	// 详见map.md"实体脚本"。
	void BuildZoneScript(Zone* zone);
	void BuildBuildingScripts(Building* building);
	void BuildAllEntityScripts();
	// 房间mod在自己房间里放的物品(RoomMod::PlaceAsset/PlaceFixture)：建筑落地、地址确定之后对它的每个房间调
	// Room::CreatePlacedAssets。只在新开局的三条落地路径里调；读档时物品随player段恢复，不重建。
	void BuildRoomAssets(Building* building);
	// 按交互名查电梯，找不到返回nullptr。
	Cabin* FindCabinByName(const std::string& interactName) const;
	// 按名字找房间里的资产(含容器内容)，遍历全部房间；找不到返回nullptr。玩家身上/地上的见Player::FindAssetByName。
	Asset* FindAssetByName(const std::string& name) const;

	// 所有真正放了门扇的门——不分三类，供UForeverStoryFrameworkComponent::
	// BroadcastGameStart遍历+过滤GetScript()!=nullptr。
	const std::vector<Door*>& GetAllDoors() const;

	const std::unordered_map<std::string, Zone*>& GetZones() const;
	// 绿化区按生成顺序排列(存档下标就是这个顺序)。
	const std::vector<Green*>& GetGreens() const;
	Green* GetGreen(const std::string& name) const;
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

	// 绿化区地址："<路名> -1 <绿化名> <圆序号>"(连了路的绿化区)。-1代表目标不在lot里，LocateZone/Building/Room
	// 遇到它都返回nullptr。circleIndex输出圆序号(地址里没写则-1)，找不到返回nullptr。
	// 没连路的绿化区地址就是名字本身，不支持寻址，见map.md"寻址"。
	Green* LocateGreen(const std::string& address, int& circleIndex) const;

	// 汇总每个顶层Lot自己的GetPathRoads()(小路归Lot所有，Map不持有)，按值返回。
	std::vector<Road*> GetPathRoads() const;

	// 转发roadnet->GetPathRoadMaterial()，供InitZones/InitBuildings构造PathLaneSpec、
	// Forever层渲染小路时查。roadnet为空(InitRoadnet没跑或没有可用mod)时返回空字符串。
	const std::string& GetPathRoadMaterial() const;

	// 路网里编辑器手工设计的独立线路(见transit_data.h)，roadnet为空时返回空数据；Route::Build在--skip的route mod里用。
	const TransitData& GetTransit() const;
	// 该建筑是不是路网预设放出来的：是返回预设在output.presets里的原下标，否则-1。
	int GetPresetIndexOfBuilding(const Building* building) const;

	// 车行/行人导航图只读访问，供Forever层可视化/未来Traffic域寻路使用。key/邻接id都是锚点
	// Node::GetId()，锚点本身的坐标通过GetNavAnchorNodes()（路口/车道分裂新增锚点）+
	// GetExterns()（地图边缘残端，也可能是graph里的端点）查到。
	const NavGraph& GetVehicleNavGraph() const;
	const NavGraph& GetPedestrianNavGraph() const;
	const std::vector<Node*>& GetNavAnchorNodes() const;

	// 对pedestrianNavGraph做A*寻路(边权=Connection::CalcDistance())，返回含起终点的路径点；不连通/id不存在返回空。
	// 供市民走路用，见Source/Forever/Framework/ForeverPopulaceFrameworkComponent.md"市民走路"。
	std::vector<const Node*> FindPedestrianPath(int fromNodeId, int toNodeId) const;

	// 对vehicleNavGraph寻路(共享私有FindPath)，返回依次经过的Connection*序列(公交线路要拼接真实车道几何，见traffic/route.md)；找不到返回空。
	std::vector<Connection*> FindVehiclePath(int fromNodeId, int toNodeId) const;

	// 逃跑选路(市民逃跑，见Forever层UForeverPopulaceFrameworkComponent::RequestFlee)：不规划整条路线，一个节点一个节点地选。
	// FleeStep：ok=false表示选不出(行人图里没有线/节点)；否则nodeId/x/y/z是下一个要去的行人导航节点，prevNodeId是"刚来的节点"
	// (下一步不回头走它)，没有时为-1。坐标是地图单位。
	struct FleeStep {
		bool ok = false;
		int nodeId = -1;
		int prevNodeId = -1;
		float x = 0.f, y = 0.f, z = 0.f;
	};
	// 起点：找离(x,y)最近的行人导航线(点到线段水平距离最小)，在它的两个端点里选"更背离(awayFromX,awayFromY)"的那个当下一个节点，
	// 另一个端点记作prevNodeId。(取线不取节点：市民通常站在两个节点之间的线上，取最近节点可能要先朝目标方向跑过去。)
	// 一次要遍历全部行人边，只在开始逃跑时调用，不在每帧热路径上。
	FleeStep StartPedestrianFlee(float x, float y, float awayFromX, float awayFromY) const;
	// 到达nodeId之后：在它所有邻接线里选终点方向最背离(awayFromX,awayFromY)的那个节点，不回头走prevNodeId，
	// 死胡同(只剩prevNodeId)才回头。节点不在行人图里返回ok=false。
	FleeStep NextPedestrianFlee(int nodeId, int prevNodeId, float awayFromX, float awayFromY) const;

	// 行人图里fromNodeId与toNodeId之间的边(双向边，两个方向查到同一个Connection*)；没有直接相连返回nullptr。
	// 供FindPedestrianPath的结果回查边(路口管理按Connection*限行，见traffic/junction.md)，只读、线程安全。
	Connection* FindPedestrianEdge(int fromNodeId, int toNodeId) const;

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
	// points[firstPoint..lastPoint]这一段折线走的是edge(含车道边和路口连线)；相邻两段共用端点。
	// 室外起点接入车道时第一段是车道边的一部分(垂足到下游锚点)，起点到垂足那一小段不属于任何edge。
	struct NavEdge {
		Connection* edge = nullptr;
		int firstPoint = 0;
		int lastPoint = 0;
	};
	struct VehicleRoutePlan {
		enum Status { OK, NO_PATH_TO_DESTINATION, NO_ROAD_CONNECTION };
		Status status = NO_ROAD_CONNECTION;
		std::vector<NavPoint> points;
		std::vector<NavEdge> edges; // 按行进顺序，与points的下标对应
		// 第一段(points[0]->points[1])是倒车：车头朝向与行进方向相反。只有PlanVehicleRouteFromSlot的出库路线会置位。
		bool reverseFirstSegment = false;
	};
	VehicleRoutePlan PlanVehicleRoute(Room* startRoom, float x, float y, Room* destRoom) const;

	// 车停在room的车位slot里出库：从车位导航节点出发规划到destNodeId(语义同PlanVehicleRouteToNode)，并在最前面
	// 加一段倒车：车沿车位开口方向倒出，越过与车位相连的connection另一端N再多倒一个车长vehicleLength(地图单位)，之后再向前沿导航线拐弯。
	// 车位节点到路网之间那段与倒车方向共线的点会被丢弃(倒车终点之后直接接路网上的点)；vehicleLength<=0不加倒车段。
	VehicleRoutePlan PlanVehicleRouteFromSlot(const Room* room, const ParkingSlot* slot, int destNodeId,
		float vehicleLength) const;

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

	// 同PlanVehicleRouteToNode，但终点是室外任意一点(destX,destY)：找离它最近的车道段，路线终止在车道上的垂足
	// （导航线用：车到不了的房间改去建筑1楼外部连接点附近的车道）。终点锚=该车道边的上游锚点，再沿边走到垂足。
	// OK：points是完整折线(最后一点是垂足)；其余状态同PlanVehicleRouteToNode。
	VehicleRoutePlan PlanVehicleRouteToPoint(Room* startRoom, float x, float y, float destX, float destY,
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

	// 同PlanPedestrianRoute，但起点/终点直接给节点：startNode非空且有出边就从它出发，否则按(x,y)找最近的行人节点；destNode必须非空。
	VehicleRoutePlan PlanPedestrianRouteFromNode(const Node* startNode, float x, float y, const Node* destNode) const;

	// 行人路线缓存(只缓存行人，车辆因为车位每次不同不缓存)：按(起点节点,终点节点)存完整折线，A->B和B->A共用一条(行人边都是双向的，
	// 反向命中时把点序和edges倒过来)，不连通的结果也缓存。25游戏小时(kPedestrianRouteCacheLifetime)内没被命中的条目由Tick清掉，
	// 暂不设数量/内存上限。导航图内容变了(SearchGraph重建)缓存随之整体作废。线程安全。
	static constexpr double kPedestrianRouteCacheLifetime = 25.0 * 3600.0; // 游戏秒
	struct RouteCacheStats {
		size_t entries = 0;
		size_t bytes = 0;      // 条目占用的堆内存估算(points/edges的capacity + 表项)
		size_t hits = 0;       // 累计(Clear不归零)
		size_t misses = 0;
		size_t purged = 0;     // 累计因过期清掉的条目数
	};
	RouteCacheStats GetPedestrianRouteCacheStats() const;
	void ClearPedestrianRouteCache() const;

	// 到绿化区第circleIndex个圆：图上走到圆心，再追加最后一段圆心->(targetX,targetY)(圆内随机点，调用方用
	// Green::RandomPointInCircle取)。startRoom/(x,y)含义同PlanPedestrianRoute。绿化区没连路/圆没有导航节点返回NO_ROAD_CONNECTION。
	VehicleRoutePlan PlanPedestrianRouteToGreen(Room* startRoom, float x, float y, const Green* green, int circleIndex,
		float targetX, float targetY) const;

	// 从绿化区第circleIndex个圆出发(离开绿化区：先到圆心再按导航图走)，终点是room。
	VehicleRoutePlan PlanPedestrianRouteFromGreen(const Green* green, int circleIndex, Room* destRoom) const;

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
	GreenFactory& greenFactory;
	BuildingFactory& buildingFactory;
	RoomFactory& roomFactory;
	ComponentFactory& componentFactory;
	// 门/电梯/实体Script创建用。
	ScriptFactory& scriptFactory;
	std::unordered_set<std::string> warnedPlacedAssetMods; // BuildRoomAssets里"物品mod没启用"按id去重

	mutable std::unordered_map<std::string, TerrainAppearance> terrainAppearances;
	mutable bool terrainAppearancesBuilt = false;

	Vegetation vegetation;
	Lamps lamps;

	Roadnet* roadnet = nullptr;
	std::vector<RoadJunction*> junctions;

	// 从磁盘.layout文件解析出来的建筑内部布局模板仓库，InitBuildings()加载一次，
	// 传给每个Building::Layout()查询，详见building.md。
	BuildingLayoutLibrary buildingLayoutLibrary;
	// 按name索引，GetZone/GetBuilding直接find。
	std::unordered_map<std::string, Zone*> zones;
	std::vector<Green*> greens;
	std::vector<EstateAsset*> estateAssets;   // 持有所有权，BuildEstateAssets重建
	std::vector<Asset*> statedEstateAssets;   // 公有的那部分，不持有
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
	NavGraph vehicleNavGraph;
	NavGraph pedestrianNavGraph;

	// 公交站辅助导航节点（见AuxNavNode），key是stationNode的id。运行时登记，不存档。
	std::unordered_map<int, AuxNavNode> auxNavNodes;

	// from/to任意一端可以是辅助节点；把from到to的折线追加进points（起点在points为空时一起加入）。
	// 连不通返回false（points保持原样）。
	// edges非空时同步追加每段折线对应的Connection(下标按points)。
	bool AppendLegPoints(int fromId, int toId, std::vector<NavPoint>& points, std::vector<NavEdge>* edges = nullptr) const;

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

	// 曲线路(有控制点)上车道贯通线的拆分点：在[tA,tB]内按路面单元(Forever BuildRoadInstances)同样的等弧长网格取点，
	// 再按转角(>约17度)/节数上限把相邻单元合并成一段，返回内部拆分点t(不含tA、tB)。直线路/不需要拆返回空。
	// 拆出来的每一段由MakeLaneConnection拟合成一条带控制点的曲线，所以不用采得和路面单元一样密。
	static std::vector<float> LaneSplitKnots(Road* road, float tA, float tB);

	// 建车道贯通线的一段Connection(from->to)：路是直线=两点直线；路有控制点=加一个控制点，让曲线经过车道中心线在两端中间t处的点
	// (二次Bezier)，这样导航线跟着路的弯走而不是切弦。两端t由端点投影回road得到。
	Connection* MakeLaneConnection(Road* road, bool isVehicle, int side, int laneIndex, Node* from, Node* to) const;

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
		// 发起接入的Building，拆除时据此撤销开口。
		const void* owner = nullptr;
	};
	std::vector<PendingRoadAccess> pendingBuildingRoadAccess;

	// 一栋楼/一个园区(key=Building*/Zone*)往全局导航图里加的东西，拆除用(只在生成阶段维护，读档不重建，读档后不再拆)：
	// 它独占的导航节点(导航图里与这些节点相邻的边都是它的)，以及它在路上挖的车行开口(road,t)。
	struct EntityNav {
		std::vector<Node*> nodes;
		std::vector<std::pair<Road*, float>> openings;
	};
	std::unordered_map<const void*, EntityNav> entityNav;
	// 园区内部小路 -> 所属园区：BreakThroughLine在内部小路上新建的访问点归园区。
	std::unordered_map<Road*, Zone*> internalRoadOwner;
	// ForwardBuildingHatches的逆操作：把building地下一层转发进地形的挖洞再删掉。
	void RemoveBuildingHatches(Building* building);
	void RemoveHatch(const TerrainHatch& hatch);

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
	// 不连通/id不存在返回空。节点坐标由编译图(SearchGraph)自带：全部节点都有坐标时按A*运算(启发值=缩放后到终点的水平直线距离，
	// 不超过真实路径长度，保证最优)，否则是Dijkstra。
	std::vector<int> FindPath(const NavGraph& graph,
		int fromNodeId, int toNodeId) const;

	// 寻路用的"编译图"：把unordered_map邻接表一次性转成按下标的数组，边权(CalcDistance要采样17个曲线点，很贵)只算一次，
	// 并带节点坐标做A*启发、行人完整路线缓存。NavGraph::revision变了会自动重建(缓存随之作废)。
	// 多线程(异步行人寻路)会同时用，所以重建/取用加锁，缓存自己另有锁。
	struct SearchGraph {
		struct Arc { int to; float weight; };
		std::unordered_map<int, int> index;      // 节点id -> 下标
		std::vector<int> ids;                    // 下标 -> 节点id
		std::vector<float> x, y;                 // 节点坐标(启发值用)
		std::vector<std::vector<Arc>> adjacency;
		float heuristicScale = 0.f;              // 启发值=scale*水平直线距离；0=不用启发(缺坐标)，scale<=1保证不高估
		uint64_t revision = 0;                   // 编译时NavGraph::revision，和当前不同就重建
		// 行人完整路线缓存(只行人图用)：key=(min(a,b),max(a,b))，plan按min->max方向存；status!=OK表示不连通(也缓存)。lastHit=游戏秒。
		struct RouteEntry { VehicleRoutePlan plan; double lastHit = 0; };
		mutable std::mutex cacheMutex;
		mutable std::unordered_map<uint64_t, RouteEntry> routes;
	};
	// 路线缓存的游戏时钟(秒)：Tick里更新，缓存命中/过期都用它。线程安全。
	mutable std::atomic<double> routeClock{ 0.0 };
	mutable std::atomic<double> routeLastPurge{ 0.0 };
	mutable std::atomic<size_t> routeHits{ 0 }, routeMisses{ 0 }, routePurged{ 0 };
	std::shared_ptr<SearchGraph> AcquireSearchGraph(const NavGraph& graph,
		std::shared_ptr<SearchGraph>& slot) const;
	mutable std::mutex searchGraphMutex;
	mutable std::shared_ptr<SearchGraph> vehicleSearchGraph;
	mutable std::shared_ptr<SearchGraph> pedestrianSearchGraph;

	// FindPedestrianPath用的"id -> Node*"索引缓存(节点上万，不能每次寻路重建)。线程池里的多个任务会同时调用，所以检查/重建时加锁；
	// 节点数量变了会自动重建。
	// 按需(重)建nodeIndex(锚点/地图边缘残端数量变了才重建)，FindPedestrianPath和逃跑选路共用。
	void EnsureNodeIndex() const;
	mutable std::mutex nodeIndexMutex;
	mutable std::unordered_map<int, const Node*> nodeIndex;
	mutable size_t nodeIndexAnchorCount = (size_t)-1;
	mutable size_t nodeIndexExternCount = (size_t)-1;
};
