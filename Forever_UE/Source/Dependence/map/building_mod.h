#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "map/geometry.h"

// 楼体在Building自身Quad里的位置——两个比例float表示楼体中心在Quad里的位置(0.5,0.5=居中，同
// Quad::posX/posY"矩形中心点"的语义)，两个比例float表示楼体长宽相对Quad长宽的比例。不复用Quad
// 类型本身(那样会把Quad正常的"中心点+尺寸"语义挪用成"比例袋子"，容易搞混)。
struct BuildingFootprintSpec {
	float centerRatioX = 0.5f;
	float centerRatioY = 0.5f;
	float sizeRatioX = 1.f;
	float sizeRatioY = 1.f;
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
	// footprint/basements/layers/floorHeights/lodMaterial——构造函数此时还不知道任何落地
	// 上下文，不能在构造函数里定这些值。direction对显式占位(Assign产出)落地的Building是Assign
	// 选中的真实方向；对权重CDF/FillRemainder落地的Building没有方向概念，传-1作为"没有方向"的
	// 哨兵值，具体怎么摆由子类自己决定。quad/boundaryRoads是这个Building真正落地的矩形和四周
	// 道路——纯只读输入，不涉及任何跨DLL容器所有权问题(Quad没有动态容器，unordered_map<int,
	// Road*>只是读指针值)，Building本身就是Quad的子类，调用方直接传*this即可。
	virtual void Layout(int direction, const Quad& quad,
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
};
