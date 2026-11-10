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
	// 存档：MapSaveCodec直接读写私有字段(map_save.md)。
	friend class MapSaveCodec;
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
	// 存档：MapSaveCodec直接读写私有字段(map_save.md)。
	friend class MapSaveCodec;
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

// 车道分裂/开口标记,挂在Road自身。产出方:Map::AddRoadAccessNode(断开导航图车道并选中车道)、
// Lot::SplitWithPath(纯几何/渲染开口,此时forwardSide/isVehicle无意义)。
// t:沿Road弧长比例;width:开口沿路长度;forwardSide:vehicleLanes[0]/pedestrianLanes[0]为true;isVehicle:车行/行人。
struct RoadOpening {
	float t = 0.f;
	float width = 0.f;
	bool forwardSide = true;
	bool isVehicle = true;
};

class Road : public Connection {
	// 存档：MapSaveCodec直接读写私有字段(map_save.md)。
	friend class MapSaveCodec;
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

	// 车道数据:下标0=沿Connection方向一侧,1=反向一侧;元素是车道宽度,数组大小=车道数。
	// 这些数据只用于结构计算(lot margin、路口路缘、导航锚点偏移),视觉由GetMesh()/GetUnit()承担。
	// parking/pedestrian两侧只代表物理位置,不代表通行方向。
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

	// 是否是Lot::SplitWithPath裁剪出的小路(不是RoadnetMod铺设的正式路),分类记在Road自身。
	void SetPathRoad(bool isPath = true);
	bool IsPathRoad() const;

	// 路口缩进：这条路在起点端(atStart)/终点端离路口中心沿路中轴的缩进距离，默认0。由RoadnetMod在所有路的车道
	// 配好之后按路口实际情况统一写入(宽度是后改的)，Core的RoadJunction::Build和mod的行道树/路灯布局直接读它。
	void SetSetback(bool atStart, float value);
	float GetSetback(bool atStart) const;

	// 两侧宽度相加。Connection连线是车道横断面的几何中心(两侧不对称也严格居中),见roadnet.md"车道居中"。
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
	float setbacks[2] = { 0.f, 0.f };
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

// 地形挖洞标记。quad.sizeX=沿rotation(长边方向,弧度)的长度,sizeY=宽度。wall=true(隧道口,
// RoadnetMod::AddTunnelHatch)时在两条长边加竖直侧壁,floorStart/floorEnd是洞底在起/终点端的绝对高度
// (地图element单位,沿路smoothstep过渡)。
struct TerrainHatch {
	Quad quad;
	float rotation = 0.f;
	bool wall = false;
	float floorStart = 0.f;
	float floorEnd = 0.f;
};

// 地形贴合请求(BuildTerrainFit产出),宿主Map::ApplyTerrainFits把面下和边上的地形贴到线高度,
// 边缘向外falloff内渐变。存采样好的折线而非Connection指针(mod的容器会扩容)。RoadnetMod和RouteMod共用。
struct TerrainFitSample {
	float x = 0.f;
	float y = 0.f;
	float z = 0.f; // 这一点的线高度(地图element单位)
};
struct TerrainFitRequest {
	std::vector<TerrainFitSample> centerline;
	float width = 0.f;   // 整宽(地图element单位)
	float falloff = 0.f; // 边缘向外渐变回原地形的距离
};

// 把connection上[t1,t2]包成Quad追加进out(挖地形洞)。tunnel=true时带侧壁,仅隧道口用。
// RoadnetMod::AddHatch/AddTunnelHatch、RouteMod的同名辅助都转调这里。
void BuildHatch(const Connection* connection, float t1, float t2, float width, bool tunnel, std::vector<TerrainHatch>& out);

// 把connection上[t1,t2]这一段沿弧长按固定间距采样成折线，追加一个TerrainFitRequest进out。
void BuildTerrainFit(const Connection* connection, float t1, float t2, float width, float falloff, std::vector<TerrainFitRequest>& out);

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
	AREA_TEST,	// 测试分区：井字路网中心lot专用，原先放在AREA_OFFICIAL_HIGH里的建筑都改放这里
	AREA_END
};

class Lot;

// 小路车道配置:车行vehicleWidth+人行pedestrianWidth各两侧,无停车道(默认0.3/0.2,总宽1单位)。
// 材质全图统一(Roadnet::GetPathRoadMaterial()),不在此结构体里。
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

	// 非空=lot是FindAdaptivePlacement在该顶层Lot的freeLots里命中的嵌套子块;
	// RequestPlacement成功后由Core(Map::InitZones/InitBuildings)把残余搬回adaptiveParent,见general_geometry.md。
	Lot* adaptiveParent = nullptr;
};

// Assign()把每条LotPlacementRequest交回调用方的回调(体编译在调用方一侧,mod不持有任何容器,
// 跨DLL安全)。裸函数指针,与creator/deleter同机制。
using PlacementEmitFunc = void(*)(void* context, const LotPlacementRequest& request);

// SplitWithPath产出的小路及两端连到的路+弧长位置,供Map::ConnectPathRoad接导航图。
// endRoad1/endRoad2可为nullptr(两端不能同时为空),此时按孤立端点处理。
struct PathRoadLink {
	Road* road = nullptr;
	Road* endRoad1 = nullptr;
	float endT1 = 0.f;
	Road* endRoad2 = nullptr;
	float endT2 = 0.f;
};

// Building内部布局模板(.layout)里矩形/点的相对位置:worldCoord = ratio*floor宽或高 + offset。
// 矩形8个float(两个对角点各x/y两套ratio+offset),点4个float,见Source/Core/map/building.md"模板数据模型"。
using RectParams = std::array<float, 8>;
using PointParams = std::array<float, 4>;

// 一面墙上的门/窗开口列表,按FACE_DIRECTION(0-3)分组,每个开口只保留位置ratio参数(RectParams)。
// 放在公共底层头,Building(Corridor/Single/Row)和Room共用。
using WallHole = std::unordered_map<int, std::vector<RectParams>>;

// 每个门洞的layout标签,下标与WallHole一一对应(同一次AddDoor追加的position/tag配对)。
// 无标签=空字符串,ResolveDoorSpec据此判定"只是个洞",见Core/map/door.h。
using DoorTags = std::unordered_map<int, std::vector<std::string>>;

// Room内部一个停车位。position用PointParams约定:第一对=u轴(沿正面墙)、第二对=v轴(朝正面为正),
// 原点在房间角点,坐标系由房间自己的朝向(Room::GetDirection())定义,详见general_geometry.md。
// rotationDegrees:相对房间朝向的旋转角(度,0=朝向房间正面,180=朝房间深处)。
// categories:允许停入的载具种类(VehicleMod::GetCategory()的名字,如"car"/"bus"/"plane"),
// Traffic::InitParkedVehicles对每个车位随机选种类再选车型。
struct ParkingSpot {
	PointParams position{};
	float rotationDegrees = 0.f;
	std::vector<std::string> categories;
};

// Building楼层导航模板里的一个固定锚点(和Stair/Row这类槽位无关，模板作者自己在画布上点出来
// 的点，比如走廊拐角)。
struct NavigationNodeTemplate {
	PointParams position{};
};

// Building楼层导航模板里的"贯通线"(走廊可通行中轴线),两端固定;
// NavigationConnectionTemplate里type=="line"、vertex==-1的连接动态投影到这条线上。
struct NavigationLineTemplate {
	PointParams begin{};
	PointParams end{};
	// 仅车行导航用的通行方向(行人恒双向):forward=沿begin->end,backward=反向,都true=双向;layout未写时只顺向。
	bool forward = true;
	bool backward = false;
};

// 导航连接的端点:type取"outside"(连到building朝向的边界路)/"node"(固定锚点)/"line"(贯通线,
// vertex=0取begin、1取end、-1动态投影)/"single"或"row"(对应Room自己的锚点)/"upstair"或"downstair"
// (跨楼层竖向通道匹配,见Building::BuildPedestrianNavigation)。idx是node/line/single/row数组下标。
struct NavigationEndpointTemplate {
	std::string type;
	int idx = -1;
	int vertex = 0;
};

struct NavigationConnectionTemplate {
	NavigationEndpointTemplate begin;
	NavigationEndpointTemplate end;
	// 仅车行导航用的通行方向,含义同NavigationLineTemplate;对只做标记的端点(outside/upstair等)无意义。
	bool forward = true;
	bool backward = false;
};

class Lot : public Quad {
	// 存档：MapSaveCodec直接读写私有字段(map_save.md)。
	friend class MapSaveCodec;
public:
	// 构造空地块
	Lot();

	// 根据中心点和长宽构造地块
	Lot(float x, float y, float w, float h, float r);

	// 根据连续三个端点构造地块;boundary可选,等价于构造后逐个SetBoundaryRoad,指针有效性由调用方保证。
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

	// 格式"<road> <index>",取GetAddresses()[0];没有地址时返回空串。
	std::string GetAddress() const;

	// 四周边界Road,下标为FACE_DIRECTION(0-3),某方向可能没有。Lot不持有指针的生命周期。
	void SetBoundaryRoad(int direction, Road* road);
	Road* GetBoundaryRoad(int direction) const;
	const std::unordered_map<int, Road*>& GetBoundaryRoads() const;

	// 沿局部坐标(原点左下角,同GetPosition)splitCoordinate处切成两段,中间嵌入一条1单位宽小路Road
	// (已SetPathRoad(true))。两端面方向都没有边界Road或切不出两段有效尺寸时返回{nullptr,nullptr,nullptr}。
	// endRoad1/endT1对应splitAlongX时的NORTH端(否则WEST端),endRoad2/endT2对应另一端;endT按直线投影近似,
	// 供Map::ConnectPathRoad接导航图。几何/开口标记规则见general_geometry.md。
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

	// 自由子地块池,惰性初始化(首次访问时new一个Lot并push_back)。该分配只能发生在Core编译的代码里,
	// mod DLL只能用下面的PeekFreeLots(),原因见general_geometry.md"跨DLL分配器"。
	std::vector<Lot*>& GetFreeLots();
	float GetFreeAcreage();

	// GetFreeLots()的纯只读版本,供mod DLL在Assign()里调用:未初始化时返回空,否则按值拷贝。
	std::vector<Lot*> PeekFreeLots() const;

	// 在freeLots中找贴着direction道路、放得下[marginStart,marginEnd]x[0,depth]的自由子块并精确裁剪。
	// 该方向无边界Road返回false。新增小路记进this的pathRoadLinks;成功时*outPlaced为世界矩形,
	// outBoundaryRoads非空则写入实际靠着的边界Road(裁出的临时Lot返回前会被delete)。
	bool RequestPlacement(int direction, float marginStart, float marginEnd, float depth,
		const PathLaneSpec& spec, Quad* outPlaced,
		std::unordered_map<int, Road*>* outBoundaryRoads = nullptr);

	// 自适应版本:纯查询(const,只用PeekFreeLots(),可从mod DLL的Assign()调用),找frontage>=minFrontage、
	// depth>=minDepth的最大自由子块,margin居中。*outLot是命中的子块,margin/depth相对它自己;
	// outLot!=this时调用方须把LotPlacementRequest::adaptiveParent设成this。找不到返回false。
	// 真正的裁剪/搬运必须在Core侧做,见general_geometry.md。
	bool FindAdaptivePlacement(int direction, float minFrontage, float minDepth,
		Lot*& outLot, float& outMarginStart, float& outMarginEnd, float& outDepth) const;

	// 对freeLots按候选权重CDF随机填充,递归二分定位(流程见general_geometry.md),剩余空间丢弃。新增小路记进pathRoadLinks。
	struct FillResult {
		std::string type;
		Quad footprint;
		std::unordered_map<int, Road*> boundaryRoads;   // 这块footprint实际靠着的边界Road，
		                                                  // 语义和RequestPlacement的outBoundaryRoads一致
	};
	std::vector<FillResult> FillRemainder(const PathLaneSpec& spec,
		const std::function<float(const std::string&)>& randomAcreage,
		const std::function<std::pair<float, float>(const std::string&)>& acreageMinMax);

	// RequestPlacement/FillRemainder在this的freeLots里切出的小路及连接信息,析构时一并delete link.road。
	const std::vector<PathRoadLink>& GetPathRoadLinks() const;

	// 把pathRoadLinks搬出并清空自己;仅Map的adaptive placement流程用(否则子块析构会删掉仍被引用的Road*)。
	std::vector<PathRoadLink> TakePathRoadLinks();

	// TakePathRoadLinks()的反向操作,仅Map用。
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

