#pragma once

#include "../common/utility.h"
#include "../common/error.h"

#include <vector>
#include <unordered_map>
#include <functional>
#include <array>
#include <string>

// 由长宽计算默认面积时的换算系数
#define ACREAGE_SCALE_FACTOR 100.f


// 边方向
enum FACE_DIRECTION : int {
	FACE_WEST,
	FACE_EAST,
	FACE_NORTH,
	FACE_SOUTH
};

class Node {
public:
	// 禁止默认构造
	Node() = delete;

	// 显式构造分配新id
	explicit Node(std::string category, float x, float y, float z = 0.f);

	// 拷贝构造使用相同id
	Node(const Node& other);

	// 赋值构造使用相同id
	Node& operator=(const Node& other);

	// 无析构
	~Node();

	// 获取id
	int GetId() const;

	// 获取坐标x
	float GetX() const;

	// 获取坐标y
	float GetY() const;

	// 获取坐标z
	float GetZ() const;

	// 获取类别
	std::string GetCategory() const;

	// 获取计数器
	static int GetCount();

	// 设置计数器
	static void SetCount(int c);

private:
	// 唯一id
	int id;

	// 坐标x
	float posX;

	// 坐标y
	float posY;

	// 坐标z
	float posZ;

	std::string category;

	// id分配计数器
	static int count;
};

class Connection {
public:
	// 禁止默认构造
	Connection() = delete;

	// 构造连接
	explicit Connection(Node n1, Node n2, float begin = 0.0f, float end = 1.0f);

	// 拷贝构造新创建nodes
	Connection(const Connection& other);

	// 赋值构造新创建nodes
	Connection& operator=(const Connection& other);

	// 无析构
	~Connection();

	// 添加控制点
	void AddControls(std::vector<std::pair<Node, float>> controls);

	// 获取控制点及其权重（拷贝）
	std::vector<std::pair<Node, float>> GetControls() const;

	// 根据n1.id, n2.id判断是否相同
	bool operator==(const Connection& other) const;

	// 获取start对应的起点（非实际端点)
	Node GetStart() const;

	// 获取end对应的终点（非实际端点)
	Node GetEnd() const;

	// 获取任一连线点
	Node GetPoint(float f) const;

	// 获取任一连线点处的切线方向（非单位向量，方向及模长由曲线参数化决定）
	void GetTangent(float f, float& dx, float& dy, float& dz) const;

	// 获取begin和end之间的距离（非两端点距离)
	float CalcDistance() const;

	// 获连线任一两点距离
	float CalcDistance(float f1, float f2) const;

protected:
	// 起点
	OBJECT_HOLDER Node* beginVertex;

	// 终点
	OBJECT_HOLDER Node* endVertex;

	// 起点参数
	float begin;

	// 终点参数
	float end;

	// 控制点及其权重
	OBJECT_HOLDER std::vector<std::pair<Node*, float>> controlVertices;

private:
	// 弧长参数表缓存，惰性计算；控制点变化后由AddControls清空
	mutable std::vector<double> arcLengthCache;

	// 按弧长比例f反查对应的原始Bezier多项式参数u，首次调用时惰性建表并缓存到arcLengthCache
	double ResolveArcLengthParam(const std::vector<std::pair<Node*, float>>& allPoints, int m, float f) const;
};

class Intersection : public Node {
public:
	// 禁止默认构造
	Intersection() = delete;

	// 显式构造分配新id
	explicit Intersection(float x, float y, float z = 0.f);

	// 由Node构造，使用相同id
	explicit Intersection(const Node& node);

	// 拷贝构造使用相同id
	Intersection(const Intersection& other);

	// 赋值构造使用相同id
	Intersection& operator=(const Intersection& other);

	// 无析构
	~Intersection();
};

// 车道分裂/开口标记，直接挂在Road自己身上，不在Map侧另开表。两个产出方：①Map::
// AddRoadAccessNode——同时断开导航图车道贯通线、用forwardSide/isVehicle选中具体车道；
// ②Lot::SplitWithPath——小路接到一条大路上时标一个纯几何/渲染意义的开口，不碰导航图，这种
// 情况forwardSide/isVehicle没有意义(没有消费方会读，只是保留默认值)。
// t: 沿Road弧长比例位置；width: 开口沿道路方向的长度；forwardSide: 取用road哪一侧
// （vehicleLanes[0]/pedestrianLanes[0]为true，[1]为false）；isVehicle: 车行(true)还是行人(false)开口。
struct RoadOpening {
	float t = 0.f;
	float width = 0.f;
	bool forwardSide = true;
	bool isVehicle = true;
};

class Road : public Connection {
public:
	// 禁止默认构造
	Road() = delete;

	// 构造连接
	explicit Road(std::string name, Node n1, Node n2, std::string mesh, float unit, float begin = 0.0f, float end = 1.0f);

	// 由Connection构造，附加名称与半径
	explicit Road(const Connection& connection, std::string name, std::string mesh, float unit);

	// 拷贝构造新创建nodes
	Road(const Road& other);

	// 赋值构造新创建nodes
	Road& operator=(const Road& other);

	// 无析构
	~Road();

	// 获取道路名称
	std::string GetName() const;

	// 获取道路3D资产路径（沿路重复摆放的mesh，见vehicleLanes等注释）
	std::string GetMesh() const;

	// 获取道路mesh基准长度（沿Connection弧长按此长度重复摆放GetMesh()资产一次）
	float GetUnit() const;

	// 车道数据：每个数组下标0=沿Connection方向前进的一侧，1=反向一侧；vector每个元素是一条车道的宽度，
	// 数组大小即车道数，0=不存在。视觉效果由GetMesh()/GetUnit()整体承担（该mesh资产本身已经画好了
	// 车道+人行道横断面），这几组数据只用于结构计算（lot margin、路口路缘角点、导航锚点偏移）。
	// parking/pedestrian两侧只代表物理位置，不代表通行方向；vehicle两侧的方向性由side本身隐含。
	void AddVehicleLane(int side, float width);
	const std::vector<float>& GetVehicleLanes(int side) const;
	void AddParkingLane(int side, float width);
	const std::vector<float>& GetParkingLanes(int side) const;
	void AddPedestrianLane(int side, float width);
	const std::vector<float>& GetPedestrianLanes(int side) const;

	// 车道分裂/开口标记，见RoadOpening注释。
	void AddOpening(const RoadOpening& opening);
	const std::vector<RoadOpening>& GetOpenings() const;

	// side(0/1)车行+停车+人行道宽度总和。
	float GetSideWidth(int side) const;

	// 是否是Zone/Building裁剪Lot自由空间时自动生成的小路（Lot::SplitWithPath创建），
	// 不是RoadnetMod铺设的正式路。分类信息记在Road自己身上——持有Road*的调用方（比如以后
	// 要重新给小路接导航图时）直接问这条Road自己就够了，不需要额外拿着Map的某个列表去做
	// 成员检查（之前ConnectPathRoad那版就是反面教材：查Map::pathRoads.find()才知道
	// 一条Road是不是小路，被撤销的同时也带出了这个设计问题）。
	void SetPathRoad(bool isPath = true);
	bool IsPathRoad() const;

	// 两侧宽度相加——Connection连线代表的是整条车道横断面的**几何中心**，不是两侧的分界线
	// （哪怕side0/side1车道数、宽度完全不对称，甚至单行道只有一侧有车道，连线也严格居中），
	// 这个值就是这条路在路口/lot边界处需要让出的横向总宽度。详见roadnet.md"车道居中"一节。
	float GetTotalWidth() const;

private:
	// 道路名称
	std::string name;

	// 道路3D资产路径
	std::string mesh;

	// 道路mesh基准长度
	float unit;

	std::vector<float> vehicleLanes[2];
	std::vector<float> parkingLanes[2];
	std::vector<float> pedestrianLanes[2];
	std::vector<RoadOpening> openings;
	bool isPathRoad = false;
};

class Quad {
public:
	// 构造空矩形
	Quad();

	// 构造矩形
	Quad(float x, float y, float w, float h);

	// 无析构
	virtual ~Quad();

	// 获取中心点坐标x
	float GetPosX() const;

	// 设置中心点坐标x
	void SetPosX(float x);

	// 获取中心点坐标y
	float GetPosY() const;

	// 设置中心点坐标y
	void SetPosY(float y);

	// 获取长宽尺寸x
	float GetSizeX() const;

	// 设置长宽尺寸x
	void SetSizeX(float w);

	// 获取长宽尺寸y
	float GetSizeY() const;

	// 设置长宽尺寸y
	void SetSizeY(float h);

	// 获取左边坐标x
	float GetLeft() const;

	// 获取右边坐标x
	float GetRight() const;

	// 获取下边坐标y
	float GetBottom() const;

	// 获取上边坐标y
	float GetTop() const;

	// 通过四边坐标指定位置
	void SetVertices(float x1, float y1, float x2, float y2);

	// 通过中心和尺寸指定位置
	void SetPosition(float x, float y, float w, float h);

	// 获取面积
	float GetAcreage() const;

	// 设置面积（在指定坐标之前设置期望面积）
	void SetAcreage(float a);

protected:
	// 中心点坐标x
	float posX;

	// 中心点坐标y
	float posY;

	// 长宽尺寸x
	float sizeX;

	// 长宽尺寸y
	float sizeY;

	// 面积
	float acreage;
};

enum AREA_TYPE : int {
	AREA_NONE,
	AREA_RESIDENTIAL_HIGH,
	AREA_RESIDENTIAL_MIDDLE,
	AREA_RESIDENTIAL_LOW,
	AREA_COMMERCIAL_HIGH,
	AREA_COMMERCIAL_MIDDLE,
	AREA_COMMERCIAL_LOW,
	AREA_INDUSTRIAL_HIGH,
	AREA_INDUSTRIAL_MIDDLE,
	AREA_INDUSTRIAL_LOW,
	AREA_OFFICIAL_HIGH,
	AREA_OFFICIAL_MIDDLE,
	AREA_OFFICIAL_LOW,
	AREA_GREEN,
	AREA_END
};

class Lot;

// 小路车道配置：车行vehicleWidth+人行pedestrianWidth各两侧，无停车道，宽度固定
// vehicleWidth*2+pedestrianWidth*2（默认0.3/0.2，两侧共1单位）。小路的材质是全图统一的一个
// 值（Roadnet::GetPathRoadMaterial()），不需要每条小路自己记一份，因此不在这个结构体里。
struct PathLaneSpec {
	float vehicleWidth = 0.3f;
	float pedestrianWidth = 0.2f;
};

// Zone/Building的mod往Lot里指定一块贴着某条边界路的矩形区域时使用：direction是贴哪一侧
// (FACE_DIRECTION)，marginStart/marginEnd是沿该路方向距两端的距离，depth是离路的进深。
struct LotPlacementRequest {
	Lot* lot = nullptr;
	int direction = FACE_WEST;
	float marginStart = 0.f;
	float marginEnd = 0.f;
	float depth = 0.f;

	// 非空时说明lot不是调用方传进Assign()的那个顶层Lot本身，而是Lot::FindAdaptivePlacement在
	// 它的freeLots里点中的一个子块——RequestPlacement成功之后，调用方(Map::InitZones()/
	// InitBuildings())要把lot自己剩下的freeLots/pathRoadLinks搬回adaptiveParent、并把lot从
	// adaptiveParent->GetFreeLots()里摘掉再delete，这样下一个mod类型的FindAdaptivePlacement才能
	// 看到真正还剩多少空间（否则lot会继续以"看起来完整未使用"的样子留在adaptiveParent里）。这个
	// 搬运步骤必须在Core编译的代码里做，不能让mod自己做，见FindAdaptivePlacement声明处的说明。
	Lot* adaptiveParent = nullptr;
};

// Zone/BuildingMod的Assign()一次性扫描全地图的lot列表、决定要显式占位哪些lot时，通过这个回调把
// 每条LotPlacementRequest交回调用方——回调函数本身（emit指向的代码）由调用方(Core编译的
// Map::InitZones()/InitBuildings())提供，mod只是调用它，不持有/不增长/不返回任何容器，跨DLL安全
// (原因见BuildingFactory/ZoneFactory的AssignFunc注释)。裸函数指针，和creator/deleter同一个机制。
using PlacementEmitFunc = void(*)(void* context, const LotPlacementRequest& request);

// Lot::SplitWithPath产出的一条小路及其两端各自连到的路+弧长位置，供Core层(Map::ConnectPathRoad)
// 接导航图用。endRoad1/endRoad2可能为nullptr(小路这一端没有可连的路，见SplitWithPath"关键设计
// 决策8"——但两端不能同时为空)，此时Map按孤立端点处理，不接到任何已有贯通线上。
struct PathRoadLink {
	Road* road = nullptr;
	Road* endRoad1 = nullptr;
	float endT1 = 0.f;
	Road* endRoad2 = nullptr;
	float endT2 = 0.f;
};

// Building内部布局模板(.layout文件)里，一个矩形/点在"某个尺寸的房间里"的相对位置描述——
// ratio+offset两部分：worldCoord = ratio*floor自身宽或高 + offset，同一份模板换到不同实际
// 楼层尺寸时套用同一个公式即可复用。矩形用8个float(两个对角点各自的x/y两套ratio+offset)，
// 点用4个float(一个点的x/y两套ratio+offset)——照抄老工程语义，纯数学，和UE/引擎无关，
// 见Source/Core/map/building.md"模板数据模型"一节。
using RectParams = std::array<float, 8>;
using PointParams = std::array<float, 4>;

// 一面墙上的门/窗开口列表，按FACE_DIRECTION(0-3)分组——每个开口只保留位置ratio参数
// (RectParams，沿墙方向+垂直方向各一对ratio+offset)，照抄老工程Corridor/Single/Row的
// doors/windows字段；老工程原本每个开口还搭配一个Quad(供以后扩展用)，但从未被任何消费方
// 读取过(BuildingBase.cpp::ConstructQuad解构时直接把Quad那一半丢掉)，这次不带这个用不到
// 的字段。Building(Corridor/Single/Row)和Room都要用这个类型，放在geometry.h这个公共
// 底层头，不需要互相include对方。
using WallHole = std::unordered_map<int, std::vector<RectParams>>;

// 一面墙上每个门洞的layout标签——下标和WallHole的下标(FACE_DIRECTION)一一对应，每个
// direction下的vector和对应WallHole[direction]逐个元素对齐(同一次AddDoor调用追加的
// position/tag一一配对)。没有标签的门洞对应空字符串，ResolveDoorSpec按这个空字符串
// 判定"不可选中，永远只是个洞"，见Core/map/door.h。和WallHole同样放在这个公共底层头，
// Building(Corridor/Single/Row)和Room都要用，不需要互相include对方。
using DoorTags = std::unordered_map<int, std::vector<std::string>>;

// Room内部一个停车位——位置是ratio+offset(相对房间自身宽高，和PointParams同一套约定)，
// 但这里的ratio是**相对房间中心**的偏移比例：0.5表示偏移0(正中心)，0/1分别是房间沿这个轴
// 两侧的边缘(-/+半边长)，不是相对房间角点的0~1——因为Room::GetPosX()/GetPosY()(Quad的约定)
// 本身就是房间的中心点，不是角点，见Traffic::Init()换算room-local坐标的实现。
// rotationDegrees是车辆停在这个车位时相对房间自身朝向的旋转角(角度制，房间作者填起来直观；
// 实际换算成世界旋转时会转成弧度、叠加到Building::GetRotation()上，和
// ComputeDirectionalMeshTransform()处理楼梯朝向的做法一致)。老工程RoomMod::parkingSpaces
// 只有位置、没有旋转(Vehicle::SetPosition接受旋转参数但调用点永远传0.f)，这次把这个缺口
// 补上，见Source/Core/traffic/traffic.md。
struct ParkingSpot {
	PointParams position{};
	float rotationDegrees = 0.f;
};

// Building楼层导航模板里的一个固定锚点(和Stair/Row这类槽位无关，模板作者自己在画布上点出来
// 的点，比如走廊拐角)。
struct NavigationNodeTemplate {
	PointParams position{};
};

// Building楼层导航模板里的一条"贯通线"(通常代表一条走廊的可通行中轴线)，两端是固定点；
// NavigationConnectionTemplate里type=="line"、vertex==-1的连接会动态投影到这条线上，
// 不是端点本身。
struct NavigationLineTemplate {
	PointParams begin{};
	PointParams end{};
};

// 导航连接的一个端点：type取"outside"(连到building朝向的边界路，见map.md"寻址"/
// building.md"行人导航"一节)/"node"(NavigationNodeTemplate固定锚点)/"line"(贯通线，
// vertex=0取begin端、1取end端、-1表示动态投影到线上离对端最近的点)/"single"或"row"
// (对应槽位实例化出的Room自己的导航锚点)/"upstair"或"downstair"(和上一层/下一层对应的
// 竖向通道锚点做跨楼层匹配，见Building::BuildPedestrianNavigation)。idx是node/line/
// single/row数组里的下标，outside/upstair/downstair不需要idx。
struct NavigationEndpointTemplate {
	std::string type;
	int idx = -1;
	int vertex = 0;
};

struct NavigationConnectionTemplate {
	NavigationEndpointTemplate begin;
	NavigationEndpointTemplate end;
};

class Lot : public Quad {
public:
	// 构造空地块
	Lot();

	// 根据中心点和长宽构造地块
	Lot(float x, float y, float w, float h, float r);

	// 根据连续三个端点构造地块；boundary可选，直接在构造时按FACE_DIRECTION登记边界Road
	// （等价于构造完再逐个调SetBoundaryRoad，只是不用在调用方那边另开一个pair/map），
	// 指针必须由调用方保证有效性（RoadnetMod侧必须指向自己roads数组里的元素本身，语义
	// 和SetBoundaryRoad完全一致，见该方法注释）。
	Lot(Node n1, Node n2, Node n3, std::vector<float> margin = std::vector<float>(4, 0.f),
		const std::unordered_map<int, Road*>& boundary = {});

	// 根据连续四个端点构造地块；boundary同上一个构造函数。
	Lot(Node n1, Node n2, Node n3, Node n4, std::vector<float> margin = std::vector<float>(4, 0.f),
		const std::unordered_map<int, Road*>& boundary = {});

	// 释放freeLots里持有的每个子Lot*、以及pathRoadLinks里持有的每条小路Road*（其余成员都是非
	// 持有指针，不需要额外清理）。
	virtual ~Lot();

	// 获取旋转
	float GetRotation() const;

	// 设置旋转
	void SetRotation(float r);

	// 获取地块类型
	AREA_TYPE GetArea() const;

	// 设置地块类型
	void SetArea(AREA_TYPE area);

	// 获取端点坐标（idx：0=WEST-NORTH, 1=EAST-NORTH, 2=EAST-SOUTH, 3=WEST-SOUTH）
	std::pair<float, float> GetVertex(int idx) const;

	// 获取矩形内任意点坐标
	std::pair<float, float> GetPosition(float x, float y) const;

	// 引入基类的 float 版本，避免名称隐藏
	using Quad::SetPosition;

	// 通过逆时针顺序三个顶点设置矩形
	void SetPosition(Node n1, Node n2, Node n3, const std::vector<float>& margin);

	// 通过顺序无关四个顶点设置矩形
	void SetPosition(Node n1, Node n2, Node n3, Node n4, const std::vector<float>& margin);

	// 地址编号：一个lot可能临街多条路（角地块），各自记一个(路名,序号)。由Roadnet::AllocateAddress
	// 按lot的边界Road映射填入，不是构造时就有的数据。
	void AddAddress(const std::string& road, int index);
	const std::vector<std::pair<std::string, int>>& GetAddresses() const;

	// 格式"<road> <index>"，取GetAddresses()[0]（角地块有多个地址时任选其一即可——和
	// LocateLot本来就是"用哪个地址都能查到同一个Lot"的语义一致）。没有任何地址(理论上不会，
	// AllocateAddress保证每个临街Lot至少有一个)时返回空串。
	std::string GetAddress() const;

	// 四周边界Road：下标按FACE_DIRECTION(0-3)，不是每个方向都一定有entry(挨着相邻lot的
	// 内部分界线方向没有对应的路)。Lot不持有这些指针的生命周期，由构造方(RoadnetMod/Roadnet)
	// 各自管理，和AddAddress一样是构造完lot几何之后再由调用方填入的数据。
	void SetBoundaryRoad(int direction, Road* road);
	Road* GetBoundaryRoad(int direction) const;
	const std::unordered_map<int, Road*>& GetBoundaryRoads() const;

	// 把当前矩形沿splitAlongX方向、在局部坐标splitCoordinate处（原点在左下角，参照GetPosition
	// 的局部坐标系）切成两段，中间嵌入一条按spec配置车道的1单位宽小路Road（构造出来后立刻
	// SetPathRoad(true)标记自己，其余调用方以后可以直接问这条Road自己，不需要另外记表）。
	// splitAlongX为true时切割线垂直于局部X轴（南北向小路，两段都保留原NORTH/SOUTH边界，各自的
	// WEST/EAST一个继承原边界、一个指向新小路）；为false时反过来（东西向小路，两段都保留原
	// WEST/EAST边界，各自的NORTH/SOUTH一个继承原边界、一个指向新小路）。小路两端如果落在一条
	// "大路"（非小路的边界Road）上，就给那条大路加一个RoadOpening标记路面缺口（宽度=小路总宽，
	// t按直线投影近似算，见实现的ProjectT）；落在另一条小路上则不标（两条小路的路口不算"大路
	// 被开口"）。这一步（SplitWithPath自己）只是几何/渲染标记，不接入导航图——两端原有的边界
	// Road不会被这次切割改动任何Connection数据；真正把小路接进vehicleNavGraph/
	// pedestrianNavGraph是Core层Map::ConnectPathRoad的职责（读下面endRoad1/endT1/endRoad2/
	// endT2这几个字段），见map.md"ConnectPathRoad"一节，不在这个函数里做。
	// 如果this在splitAlongX对应的两个端面方向（splitAlongX时是NORTH/SOUTH，否则WEST/EAST）
	// 都没有边界Road，直接拒绝，返回{nullptr,nullptr,nullptr}——不产生两端都不挨路的孤岛小路。
	// splitCoordinate不在有效范围内（切不出两段有效尺寸）同样返回{nullptr,nullptr,nullptr}。
	// endRoad1/endT1对应splitAlongX时的NORTH端(否则WEST端)，endRoad2/endT2对应SOUTH端(否则
	// EAST端)——和pathRoad自己的Start/End一一对应(Start落在endRoad1上，End落在endRoad2上)。
	// endT按Start->End直线投影近似算(ProjectT)，endRoad为空时endT无意义(留默认值0)。这两组
	// 字段供Core层Map::ConnectPathRoad接导航图用；RoadOpening.t用的是同一次ProjectT计算结果。
	struct SplitResult {
		Lot* lowerLot = nullptr;
		Lot* upperLot = nullptr;
		Road* pathRoad = nullptr;
		Road* endRoad1 = nullptr;
		float endT1 = 0.f;
		Road* endRoad2 = nullptr;
		float endT2 = 0.f;
	};
	SplitResult SplitWithPath(bool splitAlongX, float splitCoordinate, const PathLaneSpec& spec);

	// 自由子地块池：只读枚举，惰性初始化——第一次通过RequestPlacement/FillRemainder访问时，
	// 如果freeLots还是空的，先塞入一个和this自身范围重合、边界Road直接继承this->boundaryRoads
	// 的初始元素。子地块不会再有自己的子地块，只有顶层Lot会真正用到这个池。
	//
	// 注意：这个惰性初始化本身会new一个Lot、push_back进this->freeLots——这个分配/容器增长动作
	// 必须只在Core编译的代码(Map::InitZones()/InitBuildings()，运行时属于UnrealEditor-Forever.dll
	// 这个真正的UE模块，operator new/delete被PerModuleInline.inl接到FMemory)里发生，绝对不能从
	// mod DLL(Basic.dll等，用的是普通CRT分配器)的调用栈里触发——哪怕调用的是完全同一份Lot::
	// GetFreeLots()源码，Dependence.lib被两个模块各自静态链接一份，这次调用实际执行哪一份取决于
	// 调用指令本身编译进了哪个最终binary，不是看代码写在哪个.cpp里。一旦this->freeLots的buffer
	// 由一个模块的分配器分配、又被另一个模块的分配器reallocate/free，就会堆损坏(exit时或者下一次
	// 任意堆操作时crash，现象是STATUS_HEAP_CORRUPTION，参见geometry.md"跨DLL分配器"一节/
	// cross-dll-allocator-crash笔记——这正是FindAdaptivePlacement v3那次踩到的坑：直接从
	// TrainStationBuilding::Assign(Basic.dll)里调了RequestPlacement，表现为PIE一进场景就
	// 整个UnrealEditor.exe直接退出)。mod DLL只能调下面的PeekFreeLots()这个纯读版本。
	std::vector<Lot*>& GetFreeLots();
	float GetFreeAcreage();

	// GetFreeLots()的纯只读版本，专供mod DLL在Assign()探测阶段调用：freeLots还没真正初始化过
	// (freeLotsInitialized==false)时直接返回空vector，绝不触发那个会new的lazy-init；已经初始化
	// 过(说明之前已经有某次Core侧的RequestPlacement调用安全地做过lazy-init了)时按值拷贝一份
	// 返回——拷贝本身分配在调用方自己的模块里，不和this->freeLots共享buffer，不会把两个模块的
	// 分配器搅到一起。
	std::vector<Lot*> PeekFreeLots() const;

	// 在freeLots中找一块贴着direction方向道路、放得下[marginStart,marginEnd]x[0,depth]矩形的
	// 自由子块，精确裁剪出来。direction在this(顶层Lot)自己的边界Road表里没有对应Road时直接
	// 返回false，不做任何回退。裁剪通过最多3次SplitWithPath调用完成，新增小路记进this自己的
	// pathRoadLinks（见下GetPathRoadLinks注释——this就是RoadnetMod初始化的那个顶层Lot，调用方
	// 不需要另外传引用出参收集）；不满足最小2x2单位或不可达的子块被丢弃，不追加回freeLots。成功
	// 返回true，*outPlaced写入裁出的世界坐标矩形；失败返回false，freeLots不变。outBoundaryRoads
	// 非空时，成功后写入裁出的这块地实际靠着的边界Road（真正被裁出来的Lot在返回前会被delete，
	// 这份信息不写出来的话调用方就再也拿不到——Zone/Building要记录自己四周的road需要这个）。
	bool RequestPlacement(int direction, float marginStart, float marginEnd, float depth,
		const PathLaneSpec& spec, Quad* outPlaced,
		std::unordered_map<int, Road*>* outBoundaryRoads = nullptr);

	// 自适应版本：不要求调用方自己猜一组marginStart/marginEnd/depth去刚好避开别的mod已经占掉
	// 的区域——用PeekFreeLots()(纯读)在当前freeLots里找一块贴着direction方向道路、frontage
	// (沿道路方向)至少minFrontage、depth(垂直道路方向)至少minDepth的自由子块(挑面积最大的
	// 一个)，margin取居中(多余的frontage两侧平分)。
	//
	// 这条路贴的可能是原始lot自己的四条边界路，也可能是之前某次切割(SplitWithPath)在切割线上
	// 插入的内部"path"小路——后者同样是真实的、会接入导航图的Road，贴着它盖房子完全合理(比如
	// 临街小巷两侧盖房子)，不要求必须贴着最外层那四条原始边。
	//
	// 这是一个纯查询——const成员函数，不碰任何会导致分配/释放的路径(用PeekFreeLots()而不是
	// GetFreeLots()，freeLots一个指针都不会动，更不会调RequestPlacement)，可以安全地从mod
	// DLL(Basic.dll等)的Assign()探测阶段直接调用，见GetFreeLots()/PeekFreeLots()声明处的说明。
	// *outLot*就是PeekFreeLots()返回列表里命中的那个指针本身(如果freeLots还没初始化过，退化为
	// this自己)，margin/depth相对outLot自己的尺寸(不是this的)——调用方(mod的Assign())要把
	// outLot原样设进LotPlacementRequest::lot，并在outLot!=this(即命中的是一个真正的嵌套子块，
	// 不是this自身)时把LotPlacementRequest::adaptiveParent设成this，供Core编译的处理阶段
	// (Map::InitZones()/InitBuildings())在RequestPlacement成功之后把outLot自己的残余freeLots/
	// pathRoadLinks搬回this、把outLot从this->GetFreeLots()里摘掉再delete——这个搬运步骤同样
	// 必须在Core侧做，不能在这个函数里提前做掉(FindAdaptivePlacement v3那版的错误：直接在这里
	// 调outLot->RequestPlacement()，结果是从Basic.dll的调用栈触发了this->freeLots的分配器
	// 操作，PIE一进场景整个UnrealEditor.exe直接退出，STATUS_HEAP_CORRUPTION，见geometry.md
	// "跨DLL分配器"一节/cross-dll-allocator-crash笔记)。
	//
	// 找不到满足条件的自由子块返回false，out参数不修改。
	bool FindAdaptivePlacement(int direction, float minFrontage, float minDepth,
		Lot*& outLot, float& outMarginStart, float& outMarginEnd, float& outDepth) const;

	// 对freeLots和候选权重表做CDF随机填充，每确定一个候选的目标面积后用SplitWithPath递归二分
	// 定位到某个freeLot里。分割轴优先选择能让两侧都保住可达性的那个，只有在按这个轴切会导致
	// 某一侧宽度不足2单位时才被迫换轴（阈值用7留余量），换轴后产生的不可达一侧被丢弃。没有分配
	// 出去的剩余空间直接丢弃（对应"设成空地"）。新增小路记进this自己的pathRoadLinks，同
	// RequestPlacement。
	struct FillResult {
		std::string type;
		Quad footprint;
		std::unordered_map<int, Road*> boundaryRoads;   // 这块footprint实际靠着的边界Road，
		                                                  // 语义和RequestPlacement的outBoundaryRoads一致
	};
	std::vector<FillResult> FillRemainder(const PathLaneSpec& spec,
		const std::function<float(const std::string&)>& randomAcreage,
		const std::function<std::pair<float, float>(const std::string&)>& acreageMinMax);

	// RequestPlacement/FillRemainder在this(RoadnetMod初始化的顶层Lot)自己的freeLots里切出来的
	// 每一条小路(连同两端连接信息)都记在这里——小路是Zone/Building裁剪这个顶层Lot的空闲空间时
	// 产生的副产品，归属关系上本来就该跟着这个顶层Lot走，不需要另外找个地方(比如Map)单独维护
	// 一份"这些小路是谁的"记录。析构时一并delete每条link.road。
	const std::vector<PathRoadLink>& GetPathRoadLinks() const;

	// 把pathRoadLinks整体搬出去并清空自己这边：只给Map(Core编译的代码)在"adaptive placement"
	// 流程里把一个已经被消耗过的嵌套子块(FindAdaptivePlacement命中的那个outLot)的新增小路转交
	// 给它的上级顶层Lot用——转交之后这个子块对象很快就会被delete，如果不先清空，~Lot()会把这些
	// 小路的Road*也删掉，而这些Road*可能正被刚创建的Building/Zone自己的boundaryRoads引用着，
	// 变成悬空指针。不对mod DLL公开调用约定(虽然签名是public，但只有Map会用)。
	std::vector<PathRoadLink> TakePathRoadLinks();

	// TakePathRoadLinks()的反向操作：把一批(通常是从某个嵌套子块身上Take出来的)PathRoadLink
	// 追加进pathRoadLinks——同样只给Map用，让"adaptive placement"流程里真正切出来的小路能被
	// 记到正确的顶层Lot身上，供GetPathRoadLinks()/GetPathRoads()查到。
	void AdoptPathRoadLinks(std::vector<PathRoadLink>&& links);

	// 从GetPathRoadLinks()按值筛出.road，纯供Forever层遍历渲染用；Map::GetPathRoads()汇总所有
	// 顶层Lot的这个列表。按值返回(不是引用)——底层存储是PathRoadLink，这里每次现筛一份。
	std::vector<Road*> GetPathRoads() const;

	// 权重候选表：ZoneMod/BuildingMod的Distribute()对这个lot感兴趣时调用，供FillRemainder用；
	// Zone阶段和Building阶段之间要显式Clear，避免Zone没用完的权重错误地参与Building阶段抽签。
	void AddCandidate(const std::string& type, float weight);
	const std::vector<std::pair<std::string, float>>& GetCandidates() const;
	void ClearCandidates();

protected:
	// 旋转角度
	float rotation;

	// 地块类型
	AREA_TYPE area;

private:
	std::vector<std::pair<std::string, int>> addresses;
	std::unordered_map<int, Road*> boundaryRoads;
	std::vector<Lot*> freeLots;
	bool freeLotsInitialized = false;
	std::vector<std::pair<std::string, float>> candidates;
	std::vector<PathRoadLink> pathRoadLinks;
};

