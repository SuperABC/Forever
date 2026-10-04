#pragma once

#include "map/geometry.h"

#include <string>
#include <vector>


// 一个站点接口——位置用建筑占地矩形(Building自身的Quad，不是楼体footprint)的局部坐标声明，
// 原点在占地矩形左下角、未旋转(和NavigationNodeTemplate同一套ratio+offset约定，不是
// ParkingSpot那种"相对中心"的约定，因为轨道/跑道接口经常落在楼体外、靠近占地矩形边缘的空地
// 上，用左下角做原点换算更直观)。headingDegrees是接口朝向，相对建筑自身旋转
// (Building::GetRotation())，角度制；z是地图单位，相对地面(配合Building::GetFloorBaseZ(0)
// 换算成世界高度，机场跑道/火车站台都贴地，这个字段主要给以后非贴地站点(比如高架)留口子)。
struct StationInterfaceSpec {
	PointParams position{};
	float headingDegrees = 0.f;
	float z = 0.f;
};

// 不挂建筑、直接贴着道路摆的站点请求(目前只有公交站用，见StationMod::AssignRoads)——road是
// 贴哪条路，t是沿road弧长比例(0~1)的位置，rightSide决定贴在road哪一侧(沿Road Start->End
// 方向，true=右手边，和Map::ResolveAccessLane"side0=右手边"的既有约定一致)。Core负责把
// (road,t,rightSide)换算成世界坐标+朝向，mod自己不用管Node/Connection这些几何细节。
struct RoadStationRequest {
	Road* road = nullptr;
	float t = 0.5f;
	bool rightSide = true;
};

// AssignRoads()往外层交回结果用的回调——和BuildingMod::Assign的PlacementEmitFunc同一个机制
// (裸函数指针，调用方Core编译的那份代码，跨DLL安全)。
using RoadStationEmitFunc = void(*)(void* context, const RoadStationRequest& request);

// StationMod：一个站点承载的上下车接口定义，两种挂载方式二选一：
// 1. 挂在建筑上——BuildingMod::stationMod这个字符串单向声明"这栋楼配哪个站点"
//    (building_mod.h)，Traffic::InitStations()据此创建Station、调一次Layout()填好
//    interfaces(相对建筑占地矩形的局部坐标)，见火车站/机场。
// 2. 直接贴着道路摆，不占用任何Lot面积——AssignRoads()声明想贴哪些路、哪一侧，
//    Traffic::InitRoadsideStations()据此直接在道路旁边创建Station(只有一个世界坐标接口，
//    不经过Layout()/interfaces这套建筑相对坐标系)，见公交站。两种方式用的是同一个
//    StationMod基类/同一套config.json"station_mods"注册机制，只是走的Core创建路径不同。
//
// stationType是通用交通类别词(比如"bus"/"train"/"plane")，不是mod名——Core只做字符串相等
// 匹配，RouteMod::stationType声明自己要接哪类站点，同一个类别词可以对应多种StationMod(不同
// 外观/不同接口布局)，解耦站点外观和线路连接逻辑，见traffic/route_mod.h、Core/traffic/
// traffic.md"站点与线路"一节。
class StationMod {
public:
	StationMod() = default;
	virtual ~StationMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// 具体子类必须在这里填stationType，不要在构造函数里赋值——构造函数只负责id/count
	// 这类登记，和StorageMod::SetProperty()同一套"两段式"约定。注意不能指望在AssignRoads()
	// 里设stationType：AssignRoads()是static方法(见下)，根本没有实例可以赋值，必须在这个
	// SetProperty()里设好。Core在创建完mod实例后会立刻调一次这个方法，再读stationType/调
	// Layout()。
	virtual void SetProperty() {}

	std::string stationType;
	std::vector<StationInterfaceSpec> interfaces;

	// 建筑Layout()之后Core调用一次，用来按这栋楼的实际朝向/占地尺寸(地图单位)填好
	// interfaces——direction是Building::GetDirection()缓存下来的值(落地时已经解析过，不是
	// -1)，sizeX/sizeY是这栋楼占地矩形(Quad)的实际尺寸，不是楼体footprint尺寸。不关心朝向/
	// 尺寸、固定几个接口的站点类型可以不实现这个方法，直接在SetProperty()里填好interfaces。
	// 挂道路的站点类型(实现AssignRoads的那些)不需要override这个，留空即可。
	virtual void Layout(int direction, float sizeX, float sizeY) {}

	// 不挂建筑、贴着道路摆的站点类型实现这个：扫一遍全图所有Road，想贴哪条路、哪一侧就调一次
	// emit(context, request)。这个方法只用来跑一次性的"全图探测"，不需要读/写任何实例状态，
	// 所以不是虚方法——和BuildingMod::Assign/ZoneMod::Layout里RandomAcreage/GetAcreageMin/
	// GetAcreageMax/GetPower/Assign同一个"按类型固定、不需要任何实例"的static方法约定：基类
	// 不声明，每个具体子类必须实现同名static方法，通过StationFactory::RegisterStation的额外
	// 函数指针参数注册，详见station_factory.h。Traffic::InitRoadsideStations()直接调
	// StationFactory::AssignRoads(id, ...)转发到这个注册的函数指针，不需要创建/销毁任何
	// StationMod实例做"探测"(这是2026-10-04从"创建临时实例调虚方法、用完销毁"改过来的，当时
	// 没有现成的static函数指针注册机制，图省事复用了CreateStation/DestroyStation；后来发现
	// BuildingMod/ZoneMod已经有现成的机制，照抄即可，不需要为了"图省事"留着多余的实例创建/
	// 销毁)。挂建筑的站点类型(实现Layout()的那些)不需要这个，给个空实现即可：
	//   static void AssignRoads(const std::vector<Road*>&, RoadStationEmitFunc, void*) {}
};
