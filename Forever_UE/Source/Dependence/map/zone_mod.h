#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "map/geometry.h"

// 围墙：沿direction这条参考边铺设，marginStart/marginEnd是距参考边两端的距离，depth是进深，
// depthInward控制进深往矩形内(true)还是外(false)量——具体哪个是"正确"朝向由围墙资产的实际
// 朝向决定，先默认true，效果不对由PIE验证后再翻转。mesh/unit和Road::GetMesh()/GetUnit()
// 同一套语义，沿边重复铺设(算法见ForeverRoadnetFrameworkComponent::BuildRoadInstances的
// tileRange)。
struct ZoneWallSpec {
	int direction = FACE_WEST;
	float marginStart = 0.f;
	float marginEnd = 0.f;
	float depth = 0.f;
	bool depthInward = true;
	std::string mesh;
	float unit = 0.f;
};

// 大门：只记录位置/朝向数据(围墙据此让出这段空当)，目前没有大门资产，不带mesh字段，也不
// 生成任何渲染——字段和ZoneWallSpec的位置部分同构，以后有资产了直接加mesh字段即可。
struct ZoneGateSpec {
	int direction = FACE_WEST;
	float marginStart = 0.f;
	float marginEnd = 0.f;
	float depth = 0.f;
	bool depthInward = true;
};

// 出入口：局部坐标，原点在Zone矩形中心(和Lot局部坐标系原点在WEST-NORTH角不同，这是专属于
// 这几个zone内部相关结构体的新约定)。
struct ZoneAccessPoint {
	float x = 0.f;
	float y = 0.f;
	float width = 0.f;
};

// 园区内部道路：局部坐标(同ZoneAccessPoint坐标系)两端点+单一车道，只能是"一条车辆单行道"
// 或"一条人行道"二选一——和vehicleEntries/vehicleExits/pedestrianAccess分开指定的模型
// 保持一致(车行入口/出口/行人各自独立，不像真实Road那样一个对象打包多条车道)，不支持
// 多车道/双向车行道混在同一个spec里。isVehicle=true时沿Start->End单向通行；isVehicle=false
// 时是人行道(双向)。要双向车行，用两条spec各自反向；要行人+车行都通，用两条spec各给一个
// 类型，端点坐标相同的话会在anchorCache里自动按坐标+类别合并。mesh留空(现在没有园区内道路
// 模型)，只连导航图。
struct ZoneInternalRoadSpec {
	float x1 = 0.f, y1 = 0.f, x2 = 0.f, y2 = 0.f;
	bool isVehicle = false;
	float width = 0.f;
};

// 园区内部建筑：<building,rotation,roads>三元组。type供Core构造Building(factory,type)用；
// x/y/sizeX/sizeY是局部坐标+尺寸；relativeRotation是相对zone自身rotation的附加旋转；
// roadIndices是FACE_DIRECTION->internalRoads下标的映射(和Lot::boundaryRoads同样"每个方向
// 最多一条"的形状)——mod执行阶段道路还没实例化，先存下标，Core实例化完internalRoads后
// 再解析成Road*。
struct ZoneInternalBuildingSpec {
	std::string type;
	float x = 0.f, y = 0.f, sizeX = 0.f, sizeY = 0.f;
	float relativeRotation = 0.f;
	std::unordered_map<int, int> roadIndices;
};

// ZoneMod：Zone这次只有"显式指定矩形"一种生成方式(不参与权重/CDF随机填充，见
// Source/Core/map/map.md"InitZones"一节)。
class ZoneMod {
public:
	ZoneMod() = default;
	virtual ~ZoneMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// config.json里"zone_mods"数组中该mod id后面的命令行式参数字符串(如
	// "pengzhan --density 1.0"里的"--density 1.0"),由<Concept>Factory::Create<Concept>
	// 创建实例后立刻调用一次。默认空实现,不需要参数的mod不用重写,格式解析完全由重写者
	// 自己决定。
	virtual void ApplyArgs(const std::string& args) {}

	// Distribute()里push显式占位请求，引擎读取后逐条调用对应lot->RequestPlacement(...)。
	std::vector<LotPlacementRequest> explicitPlacements;

	// 围墙/大门/车行出入口(入/出分开)/行人出入口/内部道路/内部建筑——都是纯值数据，供
	// Map::InitZones()读回后自己调用Zone/Building的Core编译setter去真正写入(跨DLL所有权
	// 铁律，和上面explicitPlacements、BuildingMod::candidateWeights同一个模式)。这次范围内
	// 对同一个ZoneMod产出的所有Zone一视同仁地生效(每个mod实际只会产出同一种形状的Zone，
	// 按每次显式占位单独配置留到以后真的需要时再做)。
	std::vector<ZoneWallSpec> walls;
	std::vector<ZoneGateSpec> gates;
	std::vector<ZoneAccessPoint> vehicleEntries;
	std::vector<ZoneAccessPoint> vehicleExits;
	std::vector<ZoneAccessPoint> pedestrianAccess;
	std::vector<ZoneInternalRoadSpec> internalRoads;
	std::vector<ZoneInternalBuildingSpec> internalBuildings;

	// 引擎按当前全图lot列表（剩余空闲面积降序，用Lot::GetFreeAcreage()排序）调用一次。mod
	// 在其中对自己想要的lot直接push一条explicitPlacements请求（自己决定direction/margin/
	// depth，也就是自己决定这块Zone的尺寸），引擎逐条尝试裁剪，不额外做权重/随机面积填充。
	virtual void Distribute(const std::vector<Lot*>& lots) = 0;
};
