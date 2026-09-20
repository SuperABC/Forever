#pragma once

#include "class.h"

#include "map/geometry.h"
#include "map/terrain_factory.h"
#include "map/roadnet_factory.h"
#include "map/zone_factory.h"
#include "map/building_factory.h"

#include "map/terrain.h"
#include "map/roadnet.h"
#include "map/zone.h"
#include "map/building.h"

#include <string>
#include <vector>
#include <utility>
#include <unordered_map>
#include <array>
#include <tuple>


// 10m*10m地图元素。当前只有Terrain域需要的字段;zone/building字段等Zone/Building阶段
// 迁移时再补。hatches现在就接好(挖洞用),但在Roadnet/Building迁移前始终为空,详见map.md。
struct Element {
	std::string terrain = "plain";
	float height = 0.f;
	std::pair<bool, float> water{ false, 0.f };
	std::vector<std::pair<Quad, float>> hatches;
};

// 阶段4-1 Map域的Map(聚合)雏形:只承担Terrain域需要的职责(尺寸、Element存储、
// TerrainFactory归属、地形分发、plain->construction晋升规则)。Zone/Block/Component/
// Room/Building/Roadnet迁移时会在这个类基础上继续扩展,不是最终形态,详见map.md。
class Map {
public:
	Map(int width, int height);
	~Map();

	// 按GetPriority()降序对所有已注册地形执行DistributeTerrain，再执行plain/construction的
	// 3x3晋升规则、重建terrainTextures索引表——纯粹是"用已经注册好的terrainFactory生成这次
	// 地图内容"，不包含mod dll的发现/注册：那一步现在归`Registry`全局管，只在整个UE进程
	// 生命周期里跑一次，和这里"每次开局都要重新生成一遍地图内容"的调用频率完全不同，详见
	// Source/Core/common/registry.md。
	void InitTerrains();

	std::pair<int, int> GetSize() const;

	std::string GetTerrain(int x, int y) const;
	bool SetTerrain(int x, int y, const std::string& terrain, std::pair<bool, float> water = { false, 0.f });
	float GetHeight(int x, int y) const;
	bool SetHeight(int x, int y, float height);
	std::pair<bool, float> GetWater(int x, int y) const;
	const std::vector<std::pair<Quad, float>>& GetHatches(int x, int y) const;

	// 自动分发到所有与q重叠的element(按q的旋转AABB计算重叠范围),和旧工程Map::AddHatch同语义。
	void AddHatch(Quad q, float rotation);

	// 地形类型 -> {纹理数组槽位索引, diffuse资产路径}
	const std::unordered_map<std::string, std::pair<int, std::string>>& GetTerrainTextures() const;

	// 用已经注册好的roadnetFactory构建路网(mod dll的发现/注册归`Registry`全局管,不在这里
	// 做),假定InitTerrains已经跑完(DistributeRoadnet要采样已生成好的地形/水面)。构建顺序见
	// map.md:深拷贝路网数据->
	// 地址编号->每个Intersection建RoadJunction(车行/行人锚点+路缘角点)->车行/行人双导航图
	// (每条Road的"最内侧车道贯通线"+每个RoadJunction的路口内部连接)。
	void InitRoadnet();

	const std::vector<Road*>& GetRoads() const;
	const std::vector<Intersection*>& GetIntersections() const;
	const std::vector<Node*>& GetExterns() const;
	const std::vector<Lot*>& GetLots() const;
	const std::vector<RoadJunction*>& GetJunctions() const;
	Lot* LocateLot(const std::string& road, int index) const;

	// 车道分裂/开口(要求4/6/5):在road这条路上、距起点forward弧长比例t处，为vehicle(true)
	// 或pedestrian(false)图新增一个访问点。useForwardSide含义分两种情况：该侧(side0/1)本身
	// 就有对应类别车道时，效果和原来一样(true=侧0/右手边，false=侧1/左手边)；该侧是单行道的
	// 空侧(对侧才有车道)时，重新解释成"要连最靠右(true)还是最靠左(false)的车道"，从对侧的
	// 车道里按实际横向位置挑——单行道不存在"正向/反向"这个参照了，只能按左右分。openingWidth
	// 是开口沿道路方向的长度，供Forever层mesh生成时挖空对应长度、放置开口cube(记在road自己的
	// openings里，见geometry.h的Road::AddOpening)。
	// 分裂规则：每条车道现在都有自己专属的贯通线（见ThroughLine注释），所以不管双向/单行，
	// 都是先选出目标车道再直接把它自己的贯通线在t处切两段——双向路车道数>=2时固定选最外侧
	// 车道（和原始设计"新访问点代表外侧车道"一致，不影响其余车道各自的贯通线）；单行路按
	// useForwardSide要求的左右方向选最靠右/最靠左的车道，见map.md"单行道开口"一节。返回
	// 新创建的访问点Node，road为空或两侧都没有对应类别车道时返回nullptr。
	// **参数是Road*而不是名字**：小路(`Lot::SplitWithPath`产的path road)全部共用同一个
	// 名字`"path"`(不像大路每条名字唯一)，早先按`roadName`在`roadnet->GetRoads()`里线性
	// 查找的实现——①对小路完全找不到(`roadnet->GetRoads()`只装大路，小路挂在各自`Lot::
	// GetPathRoads()`下，从不在这个列表里)，②即使把小路也塞进查找范围，按名字查也无法在
	// 多条同名small road之间区分——building落地在小路边时"outside"端点接路网直接静默失败，
	// PIE验证发现。调用方(`Map::FlushPendingBuildingRoadAccess`)本来就已经手上有确定的
	// `Road*`(`Building::GetBoundaryRoad`/`ThroughLine`本身也是按指针不是按名字索引)，
	// 直接传指针既修好小路场景又省掉一次线性查找。
	Node* AddRoadAccessNode(Road* road, float t, bool isVehicle, bool useForwardSide, float openingWidth);

	// 把Lot::SplitWithPath产出的一条小路正式接入vehicleNavGraph/pedestrianNavGraph：先按
	// link.endRoad1(小路Start端)/endRoad2(小路End端)各自解出小路在该端的4个锚点(车行side0/1、
	// 人行side0/1)——endRoad为空就是孤立端点；endRoad是"大路"(非小路)就在它最靠近小路的那一侧
	// 断出2个node分别接小路车行/人行两条车道/人行道；endRoad本身也是小路就在它两侧车道/人行道
	// 各断出2个node(近侧2+远侧2)，远侧单向搭桥到近侧、近侧再接新小路——再用两端的4+4个锚点建
	// 小路自己的4条贯通线(车行side0 Start->End、side1 End->Start，人行两侧各自双向)。由
	// InitZones()/InitBuildings()对每条新产生的PathRoadLink调用一次，调用顺序必须是link产生
	// 的顺序(更早创建的小路可能被更晚创建的小路当成endRoad，必须先接完前者才能接后者)，具体
	// 规则见map.md"ConnectPathRoad"一节。
	void ConnectPathRoad(const PathRoadLink& link);

	// 用已经注册好的zoneFactory生成zone(mod dll的发现/注册归`Registry`全局管,不在这里
	// 做)。Zone这次只有"显式指定矩形"一种生成方式：按注册顺序对每个类型调一次
	// static ZoneMod::Assign(排好序的GetLots(), emit,
	// context)，一次性扫完全部lot拿到这个类型想要的全部LotPlacementRequest(不存在任何
	// ZoneMod实例)，逐条调用lot->RequestPlacement(...)，真正成功了才CreateZone一次、
	// Layout(direction)、存进zones。假定InitRoadnet()已经跑完(要用到GetLots())。
	void InitZones();

	// 结构和InitZones类似：先对每个building mod类型调一次static Assign扫描全部lot的显式占位
	// 请求，再用static GetPower(area)给每个lot登记权重，供lot->FillRemainder(...)做权重CDF
	// 随机填充；真正落地(显式占位/FillRemainder结果/园区内部建筑)才CreateBuilding一次。假定
	// InitZones()已经跑完(要用GetLots()裁剪完的freeLots)。三段落地循环各自调用Layout+
	// MergeBuildingNavigation之后，最后统一调一次FlushPendingBuildingRoadAccess()，把这次
	// 收集到的所有building outside端点按物理顺序接上道路网(不能在每个building落地时立即接，
	// 见FlushPendingBuildingRoadAccess()注释)。building/room/component三个mod的注册(mod dll
	// 发现+RegisterConcept)不在这里做，属于`Registry`全局一次性注册的范围，详见
	// Source/Core/common/registry.md——Layout()内部创建Room/Component时直接用的是
	// `roomFactory`/`componentFactory`这两个引用成员，不需要这里再另外确认注册没跑。
	void InitBuildings();

	// 遍历所有building的所有room，IsResidential()的加ResidentialCapacity()，除以2——
	// 老工程Map::InitContents()对"accomodation"的统计口径(先求和再减半)。供调用方
	// (AForeverFrameworkActor::EnsurePopulaceGenerated())传给Populace::Init()。假定
	// InitBuildings()已经跑完。
	int ComputeAccommodationTarget() const;

	// 遍历所有building的所有room，IsResidential()的每个room贡献1个名额(不按
	// ResidentialCapacity()重复贡献——老工程Checkin本来就是"一个room一个名额"的口径，
	// capacity只用来算ComputeAccommodationTarget()的城市级目标，不在这一步重复消费)，
	// 名额池随机打乱后按populace.GetCitizens()顺序依次消费；citizens比名额多时多出的人
	// 分不到住处(GetRoom()==nullptr)，这次也没有"一并塞进空room"的例外处理——正常情况下
	// target本来就≈capacity/2，名额池大小=room数量本身(明显大于target)，不会出现名额
	// 不够。成功分配的citizen同时设置Lot/Zone/Building/Room。**不做老工程"配偶/未成年
	// 子女一起搬进同一间"的逻辑**——这次Citizen没有配偶/亲子字段，这一步简化成纯粹的
	// "一人一间随机分配"，见Source/Core/populace/populace.md。
	void Checkin(const Populace& populace);

	const std::unordered_map<std::string, Zone*>& GetZones() const;
	const std::unordered_map<std::string, Building*>& GetBuildings() const;

	// 按Zone/Building自己的唯一name做扁平查找(唯一性由mod自己的GetName()/构造函数计数器
	// 保证，Map只在AddZone/AddBuilding发现重名时拒绝加入+debugf警告作为兜底，不主动生成
	// 名字)。找不到返回nullptr。
	Zone* GetZone(const std::string& name) const;
	Building* GetBuilding(const std::string& name) const;

	// 分层地址查找，"Block"概念在这个新工程里等价于顶层Lot：地址格式
	// "<road> <index> <zoneName>"定位直接落在某个Lot上的Zone；
	// "<road> <index> <buildingName>"定位直接落在某个Lot上、没有parentZone的Building；
	// "<road> <index> <zoneName> <buildingName>"定位某个Zone内部的Building。(road,index)
	// 直接复用已有的LocateLot(road,index)反查Lot，角地块的多个(road,index)地址都指向同一个
	// Lot，用哪一个都能查到同样的结果。找不到返回nullptr。
	Zone* LocateZone(const std::string& address) const;
	Building* LocateBuilding(const std::string& address) const;

	// 地址格式在LocateBuilding的基础上再加一段房间号("... <buildingName> <number>"或
	// "... <zoneName> <buildingName> <number>")——取最后一个空格分隔的token当房间号，
	// 剩下部分拼回字符串交给LocateBuilding定位building，再在building->GetRooms()里线性找
	// GetNumber()匹配的Room。不需要LocateComponent：Component没有GetAddress，只能通过
	// Building/Room间接找到（照抄老工程，Component本来就不参与地址体系）。找不到返回nullptr。
	Room* LocateRoom(const std::string& address) const;

	// 汇总GetLots()里每个顶层Lot自己的GetPathRoads()——小路是RequestPlacement/FillRemainder
	// 裁剪某个顶层Lot的空闲空间时的副产品，归属和生命周期都记在那个顶层Lot自己身上（构造它的
	// 正是RoadnetMod），Map不重复持有一份，这里只是遍历汇总供Forever层渲染用，按值返回。
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

private:
	bool CheckXY(int x, int y) const;
	Element& At(int x, int y);
	const Element& At(int x, int y) const;

	int width;
	int height;
	std::vector<Element> elements; // 行主序扁平数组(y*width+x),取代旧工程Chunk分块

	// 6个map域concept(terrain/roadnet/zone/building/component/room)的Factory不再是Map自己
	// 持有的成员，而是引用`Registry::Get()`那份全局只注册一次的实例(所有20个concept集中
	// 注册的地方，不只是map域，见Source/Core/common/registry.md)——mod dll的发现/注册
	// (LoadLibrary+RegisterConcept)和"实际生成/销毁地图对象"完全分开：注册只在整个UE进程
	// 生命周期里发生一次(第一次有人调用Registry::Get()时)，Map每次开局构造/结束析构都
	// 不会重新扫描dll，也不会丢失已经注册好的mod类型。引用成员没有默认值，只能在构造函数
	// 初始化列表里绑定，所以Map(int,int)的实现文件(map.cpp)必须include common/registry.h；
	// 这几个成员在类里的声明顺序必须跟构造函数初始化列表里的顺序一致(C++按声明顺序初始化，
	// 不是按初始化列表书写顺序)。
	TerrainFactory& terrainFactory;
	RoadnetFactory& roadnetFactory;
	ZoneFactory& zoneFactory;
	BuildingFactory& buildingFactory;
	RoomFactory& roomFactory;
	ComponentFactory& componentFactory;

	std::unordered_map<std::string, std::pair<int, std::string>> terrainTextures;

	Roadnet* roadnet = nullptr;
	std::vector<RoadJunction*> junctions;

	// 从磁盘.layout文件解析出来的建筑内部布局模板仓库，InitBuildings()加载一次，
	// 传给每个Building::Layout()查询，详见building.md。
	BuildingLayoutLibrary buildingLayoutLibrary;
	// 老工程Map::zones/buildings同款存储方式(unordered_map，不是vector)——寻址用的
	// GetZone(name)/GetBuilding(name)直接find即可，不需要另外挂一张单独的"名字->指针"表。
	std::unordered_map<std::string, Zone*> zones;
	std::unordered_map<std::string, Building*> buildings;

	// 按name插入，重名返回false并debugf警告、不插入(不生成消歧名字——唯一性是mod自己的
	// GetName()/构造函数计数器的责任，这里只是兜底)；成功插入返回true。
	bool AddZone(Zone* zone);
	bool AddBuilding(Building* building);

	// 车行/行人导航图：key是锚点Node::GetId()，value是(邻接锚点id, 边)列表。车行边只按实际
	// 通行方向单向插入；行人边(横道/转角/贯通线)双向插入，可能出现同一个Connection*被两条
	// 邻接记录共同引用，析构时用set去重删除一次。
	std::unordered_map<int, std::vector<std::pair<int, Connection*>>> vehicleNavGraph;
	std::unordered_map<int, std::vector<std::pair<int, Connection*>>> pedestrianNavGraph;

	// InitRoadnet/AddRoadAccessNode过程中创建的所有锚点Node(路口锚点+车道分裂新增的访问点)，
	// 析构时统一释放；导航图本身的Connection*从vehicleNavGraph/pedestrianNavGraph遍历去重释放。
	std::vector<Node*> navAnchorNodes;

	// 每条Road当前的"贯通线"记录(建图时创建，AddRoadAccessNode拆分某条车道时会清空对应
	// entry的edge)，下标0=车行side0，1=车行side1，2=行人side0，3=行人side1，每个下标
	// 对应一个vector，元素数量等于该side该类别的车道数——**每条物理车道都有自己独立的
	// entry**（第九轮迁移，起因是PIE导航图可视化验证时发现多车道路段只画出一条线：早期
	// 版本只保存"最内侧车道"或"单行道两端车道"，其余车道完全没有贯通线，车道数据和
	// 导航图对不上）。fromAnchor/toAnchor按该方向实际通行方向排列(side0:沿Road
	// Start->End；side1:沿End->Start)，车行取该车道在`RoadJunctionApproach::
	// vehicleInbound`/`vehicleOutbound`(现在也是逐车道的vector)里的专属锚点，不再是
	// 整个side共用一个锚点——否则哪怕每条车道各有一条Connection，几何上仍然会因为共用
	// 端点而重叠成一条看不出区别的线。laneIndex是这条贯通线对应该side车道数组
	// (vehicleLanes[side]/pedestrianLanes[side])里的下标，供AddRoadAccessNode按目标
	// 车道直接找到并断开对应entry，不再需要"内侧线不动、外侧新增分支"那套workaround
	// （每条车道现在都已经有自己专属的贯通线可以断），见roadnet.md"车道级导航锚点"一节。
	struct ThroughLine {
		Connection* edge = nullptr;
		Node* fromAnchor = nullptr;
		Node* toAnchor = nullptr;
		int laneIndex = 0;
	};
	std::unordered_map<Road*, std::array<std::vector<ThroughLine>, 4>> throughLines;

	// 在throughLines[road][idx]里找到laneIndex对应的贯通线，把它当前的fromAnchor->toAnchor
	// 一条边断成fromAnchor->N->toAnchor两段(车行只插入该车道自己的实际通行方向；人行两段都
	// 双向插入)，在(worldX,worldY,worldZ)新建node N，返回N。**并把这条ThroughLine的fromAnchor
	// 更新成N、edge更新成新的N->toAnchor那条**——这样如果同一条车道之后还要被再断一次(小路的
	// 场景里同一条临街大路很可能被沿线好几条小路连续断开)，下次断的是"剩下还没断过的那一截
	// 尾巴"，不会因为找到的还是最初那条整段edge而和已有node脱节。找不到对应贯通线(laneIndex
	// 越界/road不在throughLines里)返回nullptr。是AddRoadAccessNode和ConnectPathRoad共用的
	// 底层原语——AddRoadAccessNode原来自己内联做这件事，但没有这份"回写"逻辑，因为它目前还没有
	// 真正的调用方，从没暴露过"同一车道断第二次"这个问题；这次给小路接图必然会撞上，顺手把
	// AddRoadAccessNode也改成调用这个统一实现，不留两份逻辑。
	Node* BreakThroughLine(Road* road, bool isVehicle, int side, int laneIndex, float worldX, float worldY, float worldZ);

	// 算road在弧长比例t处、(isVehicle,side,laneIndex)这条具体车道/人行道的车道中心世界坐标——
	// 和AddRoadAccessNode算nx/ny用的是同一套公式(perp0偏移+shift居中换算，见
	// Source/Core/map/roadnet.md"车道居中"一节)，行人道额外加上同侧车行+停车道的总宽度当基准
	// (人行道在车行道外侧，不是从中轴线直接量，和RoadJunction::Build的pedestrianSide锚点算法
	// 一致)。ConnectPathRoad在大路/小路上breakout新node时必须用这个算出实际车道位置，不能直接
	// 用road->GetPoint(t)这个纯中轴线坐标——否则新node会紧贴中轴线，和原来车道自己真实的
	// fromAnchor/toAnchor连起来变成从车道位置抖到中轴线又抖回去的锯齿，PIE验证发现过这个问题。
	// 返回值只有x/y，z直接取road->GetPoint(t)的高度(车道横向偏移不影响高度)。
	std::pair<float, float> ComputeLaneAnchorPosition(Road* road, float t, bool isVehicle, int side, int laneIndex) const;

	// 新建一个不接入任何既有贯通线的孤立锚点node(小路端点没有可连的路时用)，登记进
	// navAnchorNodes，返回新node。
	Node* MakeIsolatedAnchor(float worldX, float worldY, float worldZ, const char* category);

	// ConnectPathRoad的核心：解出小路path在isStartEnd(true=Start端,false=End端)这一端的4个
	// 锚点(outVeh[0]/[1]=车行side0/1，outPed[0]/[1]=人行side0/1)。hostRoad为空按孤立端点处理；
	// 非空时用小路自身连接方向和hostRoad在hostT处的perp0做点积判断"近侧"(dot>=0是hostRoad的
	// side0，否则side1)，hostRoad不是小路("大路")时只断近侧一条车行道(优先选最外侧车道，某侧
	// 没有车道就退化用另一侧)+近侧一条人行道各出2个node直接对应小路的2条车道/人行道；hostRoad
	// 也是小路时近侧、远侧各自的车行道/人行道都断出2个node，远侧单向搭桥到近侧对应node
	// (远进入->近进入、近合并->远合并，人行道双向搭桥)，近侧2个node才是实际对应小路的锚点。
	void ResolvePathEndAnchors(Road* path, bool isStartEnd, Road* hostRoad, float hostT,
		std::array<Node*, 2>& outVeh, std::array<Node*, 2>& outPed);

	// 把zone局部坐标(x,y)(原点在zone矩形中心，和ZoneAccessPoint/ZoneInternalRoadSpec同一套
	// 约定)转换成世界坐标——标准2D旋转，和Lot::GetPosition同一套cos/sin写法(去掉了Lot那边
	// "局部原点在WEST-NORTH角"需要先减半尺寸再旋转那一步，因为zone局部坐标已经是中心原点)。
	std::pair<float, float> ZoneLocalToWorld(const Zone* zone, float x, float y) const;

	// 给zone的一个车行/行人出入口点接图：ZoneLocalToWorld算世界坐标后，比较到zone四条边
	// (WEST/EAST/NORTH/SOUTH)的局部距离找到最近的一条，取zone->GetBoundaryRoad(direction)；
	// 没有对应边界Road、或该侧没有对应类别(isVehicle)车道，返回nullptr(出入口一定要连到真实
	// 道路，连不上说明mod配置有问题，不静默退化成孤立锚点)。找到host road后用ResolvePathEndAnchors
	// 同一套"入zone方向和host的perp0点积判断近侧"方法选side，取该侧最外侧车道，
	// ComputeLaneAnchorPosition算车道中心世界坐标，BreakThroughLine断出一个node并给road补一个
	// RoadOpening标记；再在zone自己的世界坐标点MakeIsolatedAnchor一个"zone侧"锚点(登记进
	// anchorCache，供内部道路端点复用)，两个锚点间建一条Connection——isEntry为true时单向
	// road->zone，为false(出口)时单向zone->road；isVehicle为false(行人)时双向都插入
	// pedestrianNavGraph，isEntry参数被忽略。anchorCache记录(x,y,isVehicle,Node*)，同一个zone
	// 在同一次InitZones()调用期间由出入口/内部道路共用，坐标(含类别)重合的点直接复用同一个
	// node，不重复建。
	Node* ConnectZoneAccessPoint(Zone* zone, float x, float y, float width, bool isVehicle, bool isEntry,
		std::vector<std::tuple<float, float, bool, Node*>>& anchorCache);

	// 把ZoneInternalRoadSpec实例化成一条真正的Road(mesh=""/unit=0.f，和SplitWithPath产的
	// 小路同样的"不参与BuildRoadInstances铺设、只连导航图"约定)：车行side0(Start->End)/
	// side1(End->Start)各建一条单向贯通线(每条车道各自的贯通线)，人行两侧都双向——比照
	// InitRoadnet给普通Road建图同样的逐车道模型，但不经过RoadJunction，端点直接用
	// anchorCache里坐标(含类别)重合就复用、否则MakeIsolatedAnchor新建。返回的Road*由调用方
	// (Map::InitZones)收集后交给zone->SetInternalRoads持有生命周期。
	Road* ConnectZoneInternalRoad(Zone* zone, const ZoneInternalRoadSpec& spec,
		std::vector<std::tuple<float, float, bool, Node*>>& anchorCache);

	// 把ZoneInternalBuildingSpec实例化成Building：ZoneLocalToWorld算世界坐标中心，
	// building->SetPosition(...)，building->SetParentZone(zone)，
	// building->SetParentLot(zone->GetParentLot(), spec.relativeRotation)——parentLot直接
	// 复用zone自己的parentLot(GetRotation()转发基准天然和zone一致)，relativeRotation叠加一个
	// 偏移。再用builtInternalRoads(按spec.roadIndices的FACE_DIRECTION->下标)解析出Road*逐个
	// SetBoundaryRoad。mod是调用方(Map::InitBuildings())为这个spec单独CreateBuilding出来的、
	// 独占的BuildingMod实例——Building析构时会DestroyBuilding(mod)(和普通顶层Building同一个
	// 模式，见building.h)。返回的Building*由调用方登记进zone->AddInternalBuilding和
	// Map::buildings(所有权在后者，和其余顶层Building一致)。不在这里调用Layout()——调用方
	// (Map::InitBuildings())在拿到返回的Building*之后统一调用一次building->Layout(spec.direction)。
	Building* PlaceZoneInternalBuilding(Zone* zone, const ZoneInternalBuildingSpec& spec,
		const std::vector<Road*>& builtInternalRoads, BuildingMod* mod);

	// 把Building::Layout()产出的行人导航结果合并进Map自己的pedestrianNavGraph/
	// navAnchorNodes：①result.nodes直接登记进navAnchorNodes；②result.connections按两个
	// 方向都插入pedestrianNavGraph(行人边双向，和InitRoadnet建图同一个约定)；③对每个
	// result.outsideNodes，用building->GetBoundaryRoad(building->GetDirection())找到
	// building朝向的边界Road，投影求弧长比例t、判断building在Road哪一侧，**这次不在这里
	// 立即调用AddRoadAccessNode**，而是记进pendingBuildingRoadAccess，真正断开延后到
	// InitBuildings()三段落地循环全部跑完之后统一调用FlushPendingBuildingRoadAccess()——
	// 原因见该方法注释(多个building在同一条路上断开的顺序必须和它们的物理位置一致，不能是
	// building落地/遍历的顺序)。building没有可用的边界Road(GetDirection()仍然是-1，mod
	// 没能兜底选出任何方向)时，outsideNode只登记进navAnchorNodes，不产生任何pending项。
	void MergeBuildingNavigation(Building* building, const BuildingNavResult& result);

	// 建筑地下一层(离地表最近的那层basement)的Hatch(楼梯/电梯/坡道井道正上方的洞口)转发进
	// 世界地形——只处理`building->GetFloor(-1)`这一层，和building.md"地下室与地表hatch"
	// 一节的设计一致(basement越往下离地表越远，只有最上面那层basement会真正凿穿室外地表)；
	// building没有basement(GetBasementCount()<=0)时直接返回，不做任何事。每个Hatch的局部
	// ratio坐标(原点在楼体左下角)按`building->LocalToWorld`换算成世界坐标中心，尺寸不需要
	// 跟着换算(旋转由`AddHatch`的rotation参数单独处理，和Hatch自己不携带旋转的约定一致)，
	// 逐个转发给`this->AddHatch(quad, building->GetRotation())`——复用Terrain已有的挖洞
	// 渲染机制，和roadnet隧道口(`InitRoadnet()`里`roadnet->GetHatches()`那段)完全同一套调用
	// 方式。由`InitBuildings()`三段落地循环各自在`MergeBuildingNavigation`之后调用一次。
	void ForwardBuildingHatches(Building* building);

	// 一个还没真正执行的"building outside端点->道路访问点"断开请求，由MergeBuildingNavigation
	// 收集，FlushPendingBuildingRoadAccess()按物理顺序排序后统一执行。
	struct PendingRoadAccess {
		Road* road;
		float t;
		bool useForwardSide;
		Node* outsideNode;
		bool isVehicle = false;
		// 仅isVehicle==true时有意义：true=这个outsideNode是"出口"(按入度出度判定，见
		// MergeBuildingNavigation注释)，最终连接方向"outsideNode->access node"；false=
		// "入口"，方向"access node->outsideNode"。行人分支不看这个字段(固定双向)。
		bool isExit = false;
	};
	std::vector<PendingRoadAccess> pendingBuildingRoadAccess;

	// 按pendingBuildingRoadAccess实际会解析到的(road,side,laneIndex)分组，同一组内部按这条
	// 车道自己的实际通行方向排序(side0沿Start->End即t升序，side1沿End->Start即t降序)——和
	// ThroughLine/BreakThroughLine"每次都断当前剩余尾巴、fromAnchor跟着往通行方向前进"这个
	// 假设对齐，再依次调用AddRoadAccessNode真正断开+建Connection接上outsideNode。不这样做的
	// 后果：Map::InitBuildings()里三段building落地循环各自的处理顺序(显式占位按id/权重CDF
	// 按Lot哈希/园区内部按zones这个unordered_map)和building在路上的实际物理位置完全无关，
	// 如果直接按building处理到的顺序断，物理上靠后的building可能先断、把物理上靠前的
	// building还没轮到的那一段"剩余尾巴"抢先切掉，导致断点次序和物理顺序错位——行人贯通线在
	// 可视化上出现连线交叉/跳跃，看起来像整条路的导航图被搞乱了(PIE验证发现)。清空
	// pendingBuildingRoadAccess。
	void FlushPendingBuildingRoadAccess();

	// 从AddRoadAccessNode里拆出来的纯查询版本(不产生任何副作用、不新建/断开任何东西)：给定
	// useForwardSide，按AddRoadAccessNode同一套单行道退化规则解出实际会用的side/laneIndex。
	// FlushPendingBuildingRoadAccess()排序前用它预判"这一项最终会落在哪条车道"，必须和
	// AddRoadAccessNode真正执行时的判定完全一致，因此两处共用同一份实现，不重复维护逻辑。
	// road两侧都没有对应类别车道时返回false。
	static bool ResolveAccessLane(Road* road, bool isVehicle, bool useForwardSide, int& outSide, int& outLaneIndex);

	// 把一个点投影到road的中心线上，返回对应弧长比例t(裁剪到[0,1])——没有中间控制点(直线，
	// 井字路网里绝大多数路段)时直接解析算垂足，O(1)精确；有控制点(曲线，比如隧道引道的S形
	// 下坡)才退化成数值采样+局部细化找最近点这套更贵但通用的算法。
	static float ProjectPointOntoRoad(Road* road, float px, float py);
};
