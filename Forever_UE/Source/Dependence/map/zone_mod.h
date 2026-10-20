#pragma once

#include "common/mod_state.h"
#include "map/feature_spec.h"
#include "map/general_geometry.h"

#include <string>
#include <unordered_map>
#include <vector>


// 围墙：沿direction这条参考边铺设，marginStart/marginEnd是距参考边两端的距离，depth是进深，
// depthInward控制进深往矩形内(true)还是外(false)量——具体哪个是"正确"朝向由围墙资产的实际
// 朝向决定，默认true。mesh/unit和Road::GetMesh()/GetUnit()
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

// 大门：位置/朝向数据(围墙据此让出这段空当)+door字段——门扇外观/开关方式，door.mesh为空=
// 只是空当，不放门，和建筑门/房间门同一个"mesh为空就是洞"的约定，见feature_spec.h。
struct ZoneGateSpec {
	int direction = FACE_WEST;
	float marginStart = 0.f;
	float marginEnd = 0.f;
	float depth = 0.f;
	bool depthInward = true;
	DoorSpec door;
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
// 再解析成Road*。direction传给这个内部建筑自己的BuildingMod::Layout()（园区内部建筑没有
// Assign()挑选的方向，由mod在这里直接声明，默认FACE_WEST，和ZoneWallSpec/ZoneGateSpec同款
// 默认值）。
struct ZoneInternalBuildingSpec {
	std::string type;
	float x = 0.f, y = 0.f, sizeX = 0.f, sizeY = 0.f;
	float relativeRotation = 0.f;
	int direction = FACE_WEST;
	std::unordered_map<int, int> roadIndices;
};

// ZoneMod：Zone这次只有"显式指定矩形"一种生成方式(不参与权重/CDF随机填充，见
// Source/Core/map/map.md"InitZones"一节)。
//
// 和BuildingMod同样改回"一个本体独占一个mod实例"模型：Distribute()/explicitPlacements不再是
// 需要实例的虚方法+成员，改成子类必须实现的static Assign()（见下），一次调用扫完全地图的lot，
// 通过PlacementEmitFunc回调交回想要的显式占位请求，不需要构造任何ZoneMod实例；只有真正落地
// 成功才会CreateZone一次。
class ZoneMod {
public:
	ZoneMod() = default;
	virtual ~ZoneMod() = default;

	virtual const char* GetType() const = 0;

	// 每个mod实例独占服务一个Zone，这个名字在构造函数里定死即可（老工程"mod自己维护static
	// 计数器保证实例名字唯一"的模式）——GetName()是普通的、无副作用的只读getter。
	virtual const char* GetName() = 0;

	// 真正创建出Zone实例、SetPosition/SetBoundaryRoad都设好之后，Map::InitZones()会通过
	// Zone::Layout()调用这一个虚方法一次，用来在这个真实例上填好walls/gates/vehicleEntries/
	// vehicleExits/pedestrianAccess/internalRoads/internalBuildings——这几个字段本身不变，
	// 只是从"边显式占位边算围墙"改成拆开两步：Assign只决定"要不要、往哪摆"，Layout只管"摆下去
	// 之后长什么样"，且只在真正会被保留的实例上跑一次，不会有任何浪费。direction是Assign为这次
	// 占位选中的真实方向；quad/boundaryRoads是这个Zone真正落地的矩形和四周道路——纯只读输入，
	// 不涉及跨DLL容器所有权问题，Zone本身就是Quad的子类，调用方直接传*this即可。默认空实现，
	// 不需要围墙类布局的类型可以不重写。
	virtual void Layout(int direction, const Quad& quad,
		const std::unordered_map<int, Road*>& boundaryRoads) {}

	// 围墙/大门/车行出入口(入/出分开)/行人出入口/内部道路/内部建筑——都是纯值数据，供
	// Map::InitZones()读回后自己调用Zone/Building的Core编译setter去真正写入(跨DLL所有权
	// 铁律，和candidateWeights同一个模式)。这次范围内对同一个ZoneMod产出的所有Zone一视同仁地
	// 生效(每个mod实际只会产出同一种形状的Zone，按每次显式占位单独配置留到以后真的需要时再做)。
	// 在Layout(...)里设置。
	std::vector<ZoneWallSpec> walls;
	std::vector<ZoneGateSpec> gates;
	std::vector<ZoneAccessPoint> vehicleEntries;
	std::vector<ZoneAccessPoint> vehicleExits;
	std::vector<ZoneAccessPoint> pedestrianAccess;
	std::vector<ZoneInternalRoadSpec> internalRoads;
	std::vector<ZoneInternalBuildingSpec> internalBuildings;

	// 这个园区(Zone)挂的Script：scriptModName是ScriptMod的id，milestoneNames是要读取的里程碑脚本名
	// 数组(Config::GetScriptPath解析)，和DoorSpec/ElevatorCabinSpec同一个约定——milestoneNames
	// 非空才创建Script，空=没有脚本(默认)。玩家进出这个园区(Zone)的enter/leave事件只发给它自己的
	// Script，不再广播给全部脚本，见Core/map/map.md"实体脚本"一节。在Layout(...)里设置。
	std::string scriptModName = "script_empty";
	std::vector<std::string> milestoneNames;

	// 在园区里放一个自定义StaticMesh——在Layout()里调用。和建筑的AddDecoration同一套约定，只是
	// 园区没有footprint：DecorationSpec::position是相对园区quad(原点在西北角)的PointParams，按
	// "朝北"(大门在北)定义，Core按园区实际朝向(direction)和自身旋转换算。常驻显示(园区没有LOD)。
	void AddDecoration(const DecorationSpec& spec);
	std::vector<DecorationSpec> decorations;

	// 布局园区里的树：在整张地图(路网+园区+建筑+房间)全部生成完之后由Map::InitVegetation调用一次，
	// 参数和Layout()一样(纯只读输入)。在这里用AddTreeSingle/AddTreeLine/AddTreeArea提交请求。默认不种树。
	virtual void LayoutTrees(int direction, const Quad& quad,
		const std::unordered_map<int, Road*>& boundaryRoads) {}

	// 在LayoutTrees里提交单棵/一排/一片树的请求，只记进trees，随机旋转/缩放、跳过水面和挖洞格子
	// 都由Core统一处理，见feature_spec.h的TreeRequests。参考系是园区quad，按朝北(大门在北)写，Core
	// 按大门方向和园区旋转换算。
	void AddTreeSingle(const PointParams& position, float scale, const std::string& meshPath);
	void AddTreeLine(const PointParams& from, const PointParams& to, int num, float scale,
		const std::string& meshPath);
	void AddTreeArea(const PointParams& from, const PointParams& to, int num, float scale,
		const std::string& meshPath);
	TreeRequests trees;

	// 不再是虚方法——不需要任何实例状态，改成子类必须实现的static方法，通过
	// ZoneFactory::RegisterZone的额外参数注册，详见zone_factory.h：
	//   static void Assign(const std::vector<Lot*>& lots, PlacementEmitFunc emit, void* context);
	//     —— 一次调用扫完全地图的lot，决定这个类型想要显式占位哪些lot（自己决定
	//        direction/margin/depth，也就是自己决定这块Zone的尺寸），想要哪块就调一次
	//        emit(context, request)，request.lot必须设成对应的那个lot。

	// ---- 存档(见Source/Dependence/common/mod_state.md) ----
	// mod私有数据的存档/读档钩子，默认不存任何东西。基类里的公共声明字段由Core直接快照，
	// 这里只放mod自己的私有数据。
	virtual void SaveState(StateWriter&) const {}
	virtual void LoadState(StateReader&) {}
	// SaveState写出数据的格式版本：改了自己SaveState的格式就加一，读档时从
	// StateReader::GetSavedVersion()拿到当时的版本自己迁移。默认0。
	virtual int GetStateVersion() const { return 0; }
	// 这个mod类用来生成唯一实例名的静态计数器——存档保存、读档后恢复，避免读档后新建的实例和
	// 恢复出来的名字撞车。没有计数器的类保持默认(返回-1、Set不做事)。
	virtual int GetInstanceCounter() const { return -1; }
	virtual void SetInstanceCounter(int) {}
};
