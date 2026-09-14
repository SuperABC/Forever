#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <tuple>
#include <utility>

#include "map/geometry.h"

// (component名字, id)分组key的hash函数对象——AssignRoom/ArrangeRow用这个组合key把散落在
// 不同楼层/槽位的Room归到同一个Component下，见下面singles/rows两个容器的注释。
struct BuildingComponentKeyHash {
	size_t operator()(const std::pair<std::string, int>& key) const {
		return std::hash<std::string>()(key.first) ^ (std::hash<int>()(key.second) << 1);
	}
};

// 楼体在Building自身Quad里的位置——两个比例float表示楼体中心在Quad里的位置(0.5,0.5=居中，同
// Quad::posX/posY"矩形中心点"的语义)，两个比例float表示楼体长宽相对Quad长宽的比例。不复用Quad
// 类型本身(那样会把Quad正常的"中心点+尺寸"语义挪用成"比例袋子"，容易搞混)。
struct BuildingFootprintSpec {
	float centerRatioX = 0.5f;
	float centerRatioY = 0.5f;
	float sizeRatioX = 1.f;
	float sizeRatioY = 1.f;
};

// 一层楼可配置的渲染资产——留空的字段由Forever层用组件级默认值兜底(墙/地/顶默认White，
// 楼梯/坡道默认老工程同款的Stair.Stair/Ramp.Ramp)。这次不区分外墙/内墙材质，统一用
// wallMaterial一份(以后要分的话在这里加字段即可)。.layout模板本身不携带任何材质/网格
// 信息(和老工程一致，模板只描述2D位置/朝向/哪几侧有墙)，楼梯/坡道用什么3D网格必须由mod
// 在这里显式指定，不能指望"选了某个模板名字"就间接带出3D资产。
struct FloorAssetSpec {
	std::string wallMaterial;
	std::string floorMaterial;
	std::string ceilingMaterial;
	std::string stairMeshPath;
	std::string rampMeshPath;
};

// 一层楼要用哪个.layout模板、朝向、资产——AssignFloor/AssignFloors记录进BuildingMod::floors，
// Building::Layout()读出来查BuildingLayoutLibrary实例化。
struct FloorLayoutSpec {
	std::string templateName;
	int face = FACE_SOUTH;
	FloorAssetSpec assets;
};

// BuildingMod：Building这次有两种生成方式（显式占位 + 权重CDF随机填充，照抄老工程
// Map::InitContents对Building的处理，见Source/Core/map/map.md"InitBuildings"一节）。
//
// 这次会话改回和Zone一样的"一个本体独占一个mod实例"模型（撤销早前的"按类型共享"设计）：
// RandomAcreage/GetAcreageMin/GetAcreageMax/GetPower/Assign全部不再是需要实例的虚方法，改成
// 每个具体子类必须实现的static方法，通过BuildingFactory::RegisterBuilding的额外函数指针参数
// 注册（和creator/deleter同样的裸函数指针机制，见building_factory.h）——这样查询这些"按类型
// 固定"的信息完全不需要构造任何BuildingMod实例，只有真正落地成功时才会CreateBuilding一次。
class BuildingMod {
public:
	BuildingMod() = default;
	virtual ~BuildingMod() = default;

	virtual const char* GetType() const = 0;

	// 每个mod实例独占服务一个Building，这个名字在构造函数里定死即可（老工程"mod自己维护static
	// 计数器保证实例名字唯一"的模式，见Map::AddBuilding的寻址唯一性说明）——GetName()是普通的、
	// 无副作用的只读getter，不需要在调用时才计算。
	virtual const char* GetName() = 0;

	// config.json里"building_mods"数组中该mod id后面的命令行式参数字符串(如
	// "pengzhan --density 1.0"里的"--density 1.0"),由<Concept>Factory::Create<Concept>
	// 创建实例后立刻调用一次。默认空实现,不需要参数的mod不用重写,格式解析完全由重写者
	// 自己决定。
	virtual void ApplyArgs(const std::string& args) {}

	// 真正创建出Building实例、SetPosition/SetBoundaryRoad都设好之后，Map::InitZones()/
	// InitBuildings()会通过Building::Layout()调用这一个虚方法一次，用来在这个真实例上填好
	// footprint/basements/layers/floorHeights/lodMaterial，以及调AssignFloor/AssignRoom/
	// ArrangeRow记录楼层/房间布局——构造函数此时还不知道任何落地上下文，不能在构造函数里定
	// 这些值。direction是**引用参数**：对显式占位(Assign产出)落地的Building，传入的是Assign
	// 选中的真实方向；对权重CDF/FillRemainder落地的Building传入-1(没有方向概念)，这种情况下
	// 如果这个mod关心方向(比如需要靠GetBoundaryRoad(direction)找一条边界路)，应该从
	// boundaryRoads里挑一个非空的key用GetRandom(...)随机选一个方向回写进direction，让
	// 这栋building最终有一个确定的、有真实边界路的方向(building内部行人导航的"outside"
	// 端点需要靠这个方向找到应该连去哪条路，见Source/Core/map/building.md)；boundaryRoads
	// 一个非空的都没有时保持-1即可(building四周确实没有任何边界路)。不关心方向的mod
	// (比如footprint/楼层数据是固定值、不随方向变化的测试类型)不用管这个参数，保持传入值
	// 不变没有任何副作用。quad/boundaryRoads是这个Building真正落地的矩形和四周道路——纯
	// 只读输入，不涉及任何跨DLL容器所有权问题(Quad没有动态容器，unordered_map<int,Road*>
	// 只是读指针值)，Building本身就是Quad的子类，调用方直接传*this即可。
	virtual void Layout(int& direction, const Quad& quad,
		const std::unordered_map<int, Road*>& boundaryRoads) {}

	// 楼体位置/楼层/LOD材质——按类型固定的配置，在Layout(...)里设置。
	BuildingFootprintSpec footprint;
	int basements = 0;
	int layers = 1;
	// 楼层高度，地图单位，从最底下的地下室到最高层，长度必须是basements+layers；由
	// Building::Layout()读取校验，长度不对会用默认值补齐。
	std::vector<float> floorHeights;
	// 远处LOD单个cube用的材质软路径；留空时渲染层用默认灰色材质。
	std::string lodMaterial;

	// 声明第level层用哪个模板+朝向+资产。level：0-based，0=1楼，负数=地下室(-1=地下一层，
	// 离地表最近的那层)——和floorHeights下标同一套人类直觉编号(不是老工程"basements偏移后
	// 的数组下标"，Building::Layout()内部自己做偏移换算)。同一个level重复调用会覆盖之前
	// 记录的那份。
	void AssignFloor(int level, const std::string& templateName, int face, FloorAssetSpec assets = {});
	// 所有层(从-basements到layers-1)都用同一个模板+资产。
	void AssignFloors(const std::string& templateName, int face, FloorAssetSpec assets = {});
	// 每层各自一个模板+资产，templateNames/assets按"从最深地下室到最高层"的顺序排列，
	// 长度必须等于basements+layers(assets留空则每层都用默认FloorAssetSpec{})；长度不对
	// 是mod自己的bug，多出来的条目被忽略，不够的层级不会被赋值(等价于没调AssignFloor)。
	void AssignFloors(const std::vector<std::string>& templateNames, int face,
		const std::vector<FloorAssetSpec>& assets = {});

	// 把第level层第slot个single槽位实例化成一个类型为room的Room，挂到(component,id)这个
	// 组合(Component)上——同一个(component,id)多次调用会把Room都归到同一个Component下，
	// 哪怕它们分布在不同楼层。
	void AssignRoom(int level, int slot, const std::string& room, const std::string& component, int id);
	// 把第level层第slot个row槽位按目标面积acreage切成若干个类型为room的Room，挂到组合上。
	void ArrangeRow(int level, int slot, const std::string& room, float acreage,
		const std::string& component, int id);

	// 以下四个不再是虚方法——它们不需要任何实例状态，改成每个具体子类的static方法，通过
	// BuildingFactory::RegisterBuilding的额外参数注册，详见building_factory.h。子类必须实现
	// 同名static方法(不是override，因为基类不声明它们)：
	//   static void Assign(const std::vector<Lot*>& lots, PlacementEmitFunc emit, void* context);
	//     —— 一次调用扫完全地图的lot，决定这个类型想要显式占位哪些lot，想要哪块就调一次
	//        emit(context, request)，request.lot必须设成对应的那个lot。不用显式占位的类型给
	//        空实现即可。
	//   static float RandomAcreage();
	//   static float GetAcreageMin();
	//   static float GetAcreageMax();
	//   static float GetPower(AREA_TYPE area); // 这个类型在某个分区类型的lot上的权重，0表示不考虑

	// 供Building::Layout()读取，mod自己不需要直接操作这几个容器(全部通过上面AssignFloor/
	// AssignFloors/AssignRoom/ArrangeRow写入)。
	std::unordered_map<int, FloorLayoutSpec> floors;
	// (component,id) -> [(level, slot, roomType)]，AssignRoom记录的single槽位。
	std::unordered_map<std::pair<std::string, int>, std::vector<std::tuple<int, int, std::string>>,
		BuildingComponentKeyHash> singles;
	// (component,id) -> [(level, slot, roomType, acreage)]，ArrangeRow记录的row槽位。
	std::unordered_map<std::pair<std::string, int>, std::vector<std::tuple<int, int, std::string, float>>,
		BuildingComponentKeyHash> rows;
};
