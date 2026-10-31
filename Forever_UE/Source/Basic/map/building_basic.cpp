#include "building_basic.h"

#include "common/utility.h"

#include <cmath>
#include <algorithm>


using namespace std;

namespace {
	// 把楼体压到靠近道路的一侧，深度方向只占bodyDepthSizeRatio这么大一块、贴着道路那条边
	// (bodyDepthCenterRatio是这块body在深度方向上的中心、相对"贴道路=0、远离道路=1"这同一套
	// 0~1比例，和station_basic.cpp的NearRoadPoint用的是完全一致的direction->深度轴映射约定，
	// 这里单独写一份是因为BuildingFootprintSpec用(centerRatio,sizeRatio)描述一个区间而不是
	// NearRoadPoint的(alongRatio,depthRatio)单点坐标，换算方式不同，没法直接调用它)——剩下的
	// 深处空间留给StationMod的站台/跑道接口用，避免公共交通连线穿过楼体实体(用户实测反馈
	// "建筑楼体没有偏移，还位于建筑矩形的中心，导致公共交通路线穿过楼体"就是因为之前
	// centerRatio固定0.5、bodyOffsetX/Y恒为0，见Building::Layout()里
	// bodyOffsetX=(fp.centerRatioX-0.5)*GetSizeX()这个换算)。bodyFrontageSizeRatio是沿
	// 道路方向(frontage轴)的占比，不需要跟着direction做任何换算。
	BuildingFootprintSpec NearRoadFootprint(int direction, float bodyDepthCenterRatio,
		float bodyDepthSizeRatio, float bodyFrontageSizeRatio) {
		bool depthAlongX = (direction == FACE_WEST || direction == FACE_EAST);
		bool flip = (direction == FACE_EAST || direction == FACE_NORTH);
		float depthCenter = flip ? 1.f - bodyDepthCenterRatio : bodyDepthCenterRatio;

		BuildingFootprintSpec spec;
		if (depthAlongX) {
			spec.centerRatioX = depthCenter; spec.sizeRatioX = bodyDepthSizeRatio;
			spec.centerRatioY = 0.5f; spec.sizeRatioY = bodyFrontageSizeRatio;
		} else {
			spec.centerRatioY = depthCenter; spec.sizeRatioY = bodyDepthSizeRatio;
			spec.centerRatioX = 0.5f; spec.sizeRatioX = bodyFrontageSizeRatio;
		}
		return spec;
	}
}

int ResidenceBuilding::count = 0;

ResidenceBuilding::ResidenceBuilding() : id(count++) {
}

const char* ResidenceBuilding::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

void ResidenceBuilding::Layout(int& direction, const Quad& quad,
	const std::unordered_map<int, Road*>& boundaryRoads) {
	// 挂示例脚本entity_event(进出事件只发给这个实体自己的Script)，见Resource/Story/entity_event.script。
	milestoneNames = { "entity_event" };
	// direction对显式占位是真实方向，对权重CDF/FillRemainder落地传进来是-1——这个类型的
	// 楼层内部布局(AssignFloor选的模板)按direction摆朝向，所以-1时要从boundaryRoads里随机
	// 挑一个有真实边界路的方向回写，保证这栋building最终有确定方向可用(行人/车辆导航的
	// "outside"端点要靠这个方向找到该连去哪条路，见building_mod.h的Layout()注释)。
	if (direction < 0) {
		std::vector<int> candidates;
		for (auto& [dir, road] : boundaryRoads) {
			if (road) candidates.push_back(dir);
		}
		if (!candidates.empty()) {
			direction = candidates[GetRandom(static_cast<int>(candidates.size()))];
		}
	}

	footprint = BuildingFootprintSpec{ 0.5f, 0.5f, 0.8f, 0.8f }; // 楼体占地块80%，居中
	basements = 1;
	// 楼层数按地块面积分档。
	if (quad.GetAcreage() < 5000.f) {
		layers = 3 + GetRandom(3);
	} else if (quad.GetAcreage() < 10000.f) {
		layers = 6 + GetRandom(4);
	} else {
		layers = 10 + GetRandom(5);
	}
	floorHeights.clear();
	floorHeights.push_back(0.35f); // 地下室
	// 地上每层高度统一0.4，不循环取不同值。
	for (int i = 0; i < layers; i++) {
		floorHeights.push_back(0.4f);
	}
	// lodMaterial留空 -> 用渲染层默认灰色Pure MID

	// 建筑大门：随机左右滑动的单开门，占位先用项目自己的Cube资产，见door_system_plan.md
	// "用户追加的具体门型要求"一节。只有corridor的门洞带了"entrance"这个tag才会用到这份
	// spec，见Core/map/door.h的ResolveDoorSpec。
	doorSpecs["entrance"].mesh = "/Game/Asset/Meshes/Cube.Cube";
	doorSpecs["entrance"].style = 0; // Slide
	doorSpecs["entrance"].leaves = 1;
	doorSpecs["entrance"].randomSide = true;

	constexpr const char* kComponent = "component_residence";
	constexpr int kComponentId = 0;

	// 按quad尺寸分档随机选一套模板组合(layout 0-3)+对应的房间槽位密度(size)，layout==1/3
	// 自带电梯井道模板数据(preset_lobby_wing_*/preset_nshape_double_*)，AssignElevatorCabin
	// 靠这两个分支才有真正的Elevator几何可用。
	//
	// 方向：按quad尺寸/朝向算出的"偏好方向"覆盖掉一开始GetRandom(4)的初始值，但只有偏好
	// 方向在boundaryRoads里确实有非空条目时才采用，否则保留上面已经兜底好的direction——
	// 不检查就直接覆盖会导致上面费心兜底出来的、行人/车辆导航outside端点能用的direction
	// 被换成一个查不到路的方向。
	int layout = 0;
	float size = 120.f;
	int preferredDirection = direction;
	if (quad.GetSizeX() <= 3.f || quad.GetSizeY() <= 3.f) {
		layout = 0;
		size = 40.f;
		if (quad.GetSizeX() > 3.f) preferredDirection = FACE_WEST + GetRandom(2);
		if (quad.GetSizeY() > 3.f) preferredDirection = FACE_NORTH + GetRandom(2);
	} else if (quad.GetSizeX() <= 6.f || quad.GetSizeY() <= 6.f) {
		layout = GetRandom(2);
		size = 160.f;
		if (quad.GetSizeX() > 6.f) preferredDirection = layout * 2 + GetRandom(2);
		if (quad.GetSizeY() > 6.f) preferredDirection = 2 - layout * 2 + GetRandom(2);
	} else if (quad.GetSizeX() <= 9.f || quad.GetSizeY() <= 9.f) {
		layout = 2;
		size = 120.f;
		if (quad.GetSizeX() > 9.f) preferredDirection = FACE_WEST + GetRandom(2);
		if (quad.GetSizeY() > 9.f) preferredDirection = FACE_NORTH + GetRandom(2);
	} else {
		layout = 3;
		size = 120.f;
		if (quad.GetSizeX() > quad.GetSizeY()) preferredDirection = FACE_WEST + GetRandom(2);
		else preferredDirection = FACE_NORTH + GetRandom(2);
	}
	auto roadIt = boundaryRoads.find(preferredDirection);
	if (roadIt != boundaryRoads.end() && roadIt->second) {
		direction = preferredDirection;
	}

	if (layout == 0) {
		AssignFloor(-1, "preset_straight_linear_bg+", direction);
		ArrangeRow(-1, 0, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(-1, 1, "room_residence", size, kComponent, kComponentId);
		AssignFloor(0, "preset_straight_linear_fg+-", direction);
		ArrangeRow(0, 0, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(0, 1, "room_residence", size, kComponent, kComponentId);
		for (int i = 1; i < layers; i++) {
			AssignFloor(i, "preset_straight_linear_fu+-", direction);
			ArrangeRow(i, 0, "room_residence", size, kComponent, kComponentId);
			ArrangeRow(i, 1, "room_residence", size, kComponent, kComponentId);
		}
	} else if (layout == 1) {
		AssignFloor(-1, "preset_lobby_wing_bg+", direction);
		ArrangeRow(-1, 0, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(-1, 1, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(-1, 2, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(-1, 3, "room_residence", size, kComponent, kComponentId);
		AssignFloor(0, "preset_lobby_wing_fg+-", direction);
		ArrangeRow(0, 0, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(0, 1, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(0, 2, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(0, 3, "room_residence", size, kComponent, kComponentId);
		for (int i = 1; i < layers; i++) {
			AssignFloor(i, "preset_lobby_wing_fu+-", direction);
			ArrangeRow(i, 0, "room_residence", size, kComponent, kComponentId);
			ArrangeRow(i, 1, "room_residence", size, kComponent, kComponentId);
			ArrangeRow(i, 2, "room_residence", size, kComponent, kComponentId);
			ArrangeRow(i, 3, "room_residence", size, kComponent, kComponentId);
		}
		AssignElevatorCabin(0, -basements, layers - 1, "", "script_empty", { "elevator_basic" });
		AssignElevatorCabin(1, -basements, layers - 1, "", "script_empty", { "elevator_basic" });
	} else if (layout == 2) {
		AssignFloor(-1, "preset_lshape_double_bg+", direction);
		AssignRoom(-1, 0, "room_residence", kComponent, kComponentId);
		AssignRoom(-1, 1, "room_residence", kComponent, kComponentId);
		ArrangeRow(-1, 0, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(-1, 1, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(-1, 2, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(-1, 3, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(-1, 4, "room_residence", size, kComponent, kComponentId);
		AssignFloor(0, "preset_lshape_double_fg+-", direction);
		AssignRoom(0, 0, "room_residence", kComponent, kComponentId);
		AssignRoom(0, 1, "room_residence", kComponent, kComponentId);
		ArrangeRow(0, 0, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(0, 1, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(0, 2, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(0, 3, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(0, 4, "room_residence", size, kComponent, kComponentId);
		for (int i = 1; i < layers; i++) {
			AssignFloor(i, "preset_lshape_double_fu+-", direction);
			AssignRoom(i, 0, "room_residence", kComponent, kComponentId);
			AssignRoom(i, 1, "room_residence", kComponent, kComponentId);
			ArrangeRow(i, 0, "room_residence", size, kComponent, kComponentId);
			ArrangeRow(i, 1, "room_residence", size, kComponent, kComponentId);
			ArrangeRow(i, 2, "room_residence", size, kComponent, kComponentId);
			ArrangeRow(i, 3, "room_residence", size, kComponent, kComponentId);
			ArrangeRow(i, 4, "room_residence", size, kComponent, kComponentId);
		}
	} else { // layout == 3
		AssignFloor(-1, "preset_nshape_double_bg+", direction);
		AssignRoom(-1, 0, "room_empty", kComponent, kComponentId); // 占位槽位，不铺真实房间
		AssignRoom(-1, 1, "room_residence", kComponent, kComponentId);
		AssignRoom(-1, 2, "room_residence", kComponent, kComponentId);
		ArrangeRow(-1, 0, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(-1, 1, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(-1, 2, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(-1, 3, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(-1, 4, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(-1, 5, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(-1, 6, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(-1, 7, "room_residence", size, kComponent, kComponentId);
		AssignFloor(0, "preset_nshape_double_fg+-", direction);
		AssignRoom(0, 0, "room_empty", kComponent, kComponentId);
		AssignRoom(0, 1, "room_residence", kComponent, kComponentId);
		AssignRoom(0, 2, "room_residence", kComponent, kComponentId);
		ArrangeRow(0, 0, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(0, 1, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(0, 2, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(0, 3, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(0, 4, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(0, 5, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(0, 6, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(0, 7, "room_residence", size, kComponent, kComponentId);
		for (int i = 1; i < layers; i++) {
			AssignFloor(i, "preset_nshape_double_fu+-", direction);
			AssignRoom(i, 0, "room_empty", kComponent, kComponentId);
			AssignRoom(i, 1, "room_residence", kComponent, kComponentId);
			AssignRoom(i, 2, "room_residence", kComponent, kComponentId);
			ArrangeRow(i, 0, "room_residence", size, kComponent, kComponentId);
			ArrangeRow(i, 1, "room_residence", size, kComponent, kComponentId);
			ArrangeRow(i, 2, "room_residence", size, kComponent, kComponentId);
			ArrangeRow(i, 3, "room_residence", size, kComponent, kComponentId);
			ArrangeRow(i, 4, "room_residence", size, kComponent, kComponentId);
			ArrangeRow(i, 5, "room_residence", size, kComponent, kComponentId);
			ArrangeRow(i, 6, "room_residence", size, kComponent, kComponentId);
			ArrangeRow(i, 7, "room_residence", size, kComponent, kComponentId);
		}
		AssignElevatorCabin(0, -basements, layers - 1, "", "script_empty", { "elevator_basic" });
		AssignElevatorCabin(1, -basements, layers - 1, "", "script_empty", { "elevator_basic" });
	}
}

void ResidenceBuilding::Assign(const vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {
	// 不用显式占位。
}

float ResidenceBuilding::RandomAcreage() {
	return 2000.f * powf(1.f + GetRandom(1000) / 1000.f * 2.f, 2);
}

float ResidenceBuilding::GetAcreageMin() {
	return 2000.f;
}

float ResidenceBuilding::GetAcreageMax() {
	return 18000.f;
}

float ResidenceBuilding::GetPower(AREA_TYPE area) {
	// 只在住宅分区(高/中/低密度)上有权重，和ShopBuilding/FactoryBuilding各自只在商业/工业
	// 分区有权重(见building_basic.md)保持互斥——否则会在商业/工业分区上和Shop/Factory一起
	// 参与FillRemainder的权重CDF竞争，导致商业区/工业区里混入住宅建筑。这个通用占位类型不
	// 细分三档密度权重，统一按1.f处理。办公/商业/工业分区上返回0，意味着没有对应类型建筑
	// 竞争的分区(目前只有办公区，还没有Office建筑类型)那块lot会暂时保持空地，属于预期行为。
	if (area == AREA_RESIDENTIAL_HIGH || area == AREA_RESIDENTIAL_MIDDLE || area == AREA_RESIDENTIAL_LOW) {
		return 1.f;
	}
	return 0.f;
}

int ShopBuilding::count = 0;

ShopBuilding::ShopBuilding() : id(count++) {
}

const char* ShopBuilding::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

void ShopBuilding::Layout(int& direction, const Quad& quad,
	const std::unordered_map<int, Road*>& boundaryRoads) {
	// direction对显式占位是真实方向，对权重CDF/FillRemainder落地传进来是-1——照抄
	// ResidenceBuilding同款兜底：先从boundaryRoads里随机挑一个有真实边界路的方向，保证这栋
	// building最终有确定方向可用(行人/车辆导航的"outside"端点要靠它找到该连去哪条路)。
	if (direction < 0) {
		std::vector<int> candidates;
		for (auto& [dir, road] : boundaryRoads) {
			if (road) candidates.push_back(dir);
		}
		if (!candidates.empty()) {
			direction = candidates[GetRandom(static_cast<int>(candidates.size()))];
		}
	}

	float scaleX = 0.6f;
	float scaleY = 0.6f;
	if (quad.GetSizeX() < 6.f) scaleX = (quad.GetSizeX() - 2.4f) / quad.GetSizeX();
	if (quad.GetSizeY() < 6.f) scaleY = (quad.GetSizeY() - 2.4f) / quad.GetSizeY();
	footprint = BuildingFootprintSpec{ 0.5f, 0.5f, scaleX, scaleY };

	// 和ResidenceBuilding一样，只有偏好方向在boundaryRoads里确实有非空条目时才采用，否则
	// 保留上面已经兜底好的direction，避免行人/车辆导航outside端点查不到路。
	int preferredDirection = (quad.GetSizeX() > quad.GetSizeY())
		? (FACE_WEST + GetRandom(2))
		: (FACE_NORTH + GetRandom(2));
	auto roadIt = boundaryRoads.find(preferredDirection);
	if (roadIt != boundaryRoads.end() && roadIt->second) {
		direction = preferredDirection;
	}

	// 商店大门：随机左右滑动的单开门，有名字可以交互("敲门")，照抄vehicle_basic.script的
	// game_start->add_option模式，见Resource/Story/door_shop_entrance.script。
	doorSpecs["entrance"].mesh = "/Game/Asset/Meshes/Cube.Cube";
	doorSpecs["entrance"].name = "商店大门";
	doorSpecs["entrance"].scriptModName = "script_empty";
	doorSpecs["entrance"].milestoneNames = { "door_shop_entrance" };
	doorSpecs["entrance"].style = 0; // Slide
	doorSpecs["entrance"].leaves = 1;
	doorSpecs["entrance"].randomSide = true;

	constexpr const char* kComponent = "component_shop";
	constexpr int kComponentId = 0;
	FloorAssetSpec assets;
	assets.wallMaterial = "/Game/Asset/Materials/White.White";

	// layers固定2层(不像Residence那样按acreage随机分档)，见building_basic.md。
	layers = 2;

	if (quad.GetAcreage() < 6000.f) {
		basements = 0;
		floorHeights.assign(layers, 0.4f);

		AssignFloor(0, "preset_lobby_linear_fg+", direction, assets);
		AssignRoom(0, 0, "room_shop", kComponent, kComponentId);
		ArrangeRow(0, 0, "room_warehouse", 200.f, kComponent, kComponentId);
		ArrangeRow(0, 1, "room_warehouse", 200.f, kComponent, kComponentId);
		// layers==2时这个循环体是no-op(1 < layers-1 == 1 < 1为false)，保留循环结构不简化，
		// layers以后如果改成可变值这段逻辑会自动生效。
		for (int i = 1; i < layers - 1; i++) {
			AssignFloor(i, "preset_lobby_linear_fu+-", direction, assets);
			AssignRoom(i, 0, "room_warehouse", kComponent, kComponentId);
			ArrangeRow(i, 0, "room_warehouse", 200.f, kComponent, kComponentId);
			ArrangeRow(i, 1, "room_warehouse", 200.f, kComponent, kComponentId);
		}
		AssignFloor(layers - 1, "preset_lobby_linear_fu-", direction, assets);
		AssignRoom(layers - 1, 0, "room_warehouse", kComponent, kComponentId);
		ArrangeRow(layers - 1, 0, "room_warehouse", 200.f, kComponent, kComponentId);
		ArrangeRow(layers - 1, 1, "room_warehouse", 200.f, kComponent, kComponentId);
	} else {
		basements = 1;
		floorHeights.assign(basements + layers, 0.4f);

		if (GetRandom(2)) {
			AssignFloor(-1, "preset_circle_double_pg+", direction, assets);
			AssignRoom(-1, 0, "room_parking", kComponent, kComponentId);
		} else {
			AssignFloor(-1, "preset_circle_double_bg+", direction, assets);
			AssignRoom(-1, 0, "room_warehouse", kComponent, kComponentId);
			AssignRoom(-1, 1, "room_warehouse", kComponent, kComponentId);
			AssignRoom(-1, 2, "room_warehouse", kComponent, kComponentId);
			AssignRoom(-1, 3, "room_warehouse", kComponent, kComponentId);
			AssignRoom(-1, 4, "room_warehouse", kComponent, kComponentId);
			ArrangeRow(-1, 0, "room_warehouse", 100.f, kComponent, kComponentId);
			ArrangeRow(-1, 1, "room_warehouse", 100.f, kComponent, kComponentId);
			ArrangeRow(-1, 2, "room_warehouse", 100.f, kComponent, kComponentId);
			ArrangeRow(-1, 3, "room_warehouse", 100.f, kComponent, kComponentId);
			ArrangeRow(-1, 4, "room_warehouse", 100.f, kComponent, kComponentId);
			ArrangeRow(-1, 5, "room_warehouse", 100.f, kComponent, kComponentId);
			ArrangeRow(-1, 6, "room_warehouse", 100.f, kComponent, kComponentId);
		}

		AssignFloor(0, "preset_circle_double_fg+-", direction, assets);
		AssignRoom(0, 0, "room_shop", kComponent, kComponentId);
		AssignRoom(0, 1, "room_shop", kComponent, kComponentId);
		AssignRoom(0, 2, "room_warehouse", kComponent, kComponentId);
		AssignRoom(0, 3, "room_warehouse", kComponent, kComponentId);
		AssignRoom(0, 4, "room_warehouse", kComponent, kComponentId);
		AssignRoom(0, 5, "room_warehouse", kComponent, kComponentId);
		ArrangeRow(0, 0, "room_warehouse", 100.f, kComponent, kComponentId);
		ArrangeRow(0, 1, "room_warehouse", 100.f, kComponent, kComponentId);
		ArrangeRow(0, 2, "room_warehouse", 100.f, kComponent, kComponentId);
		ArrangeRow(0, 3, "room_warehouse", 100.f, kComponent, kComponentId);
		ArrangeRow(0, 4, "room_warehouse", 100.f, kComponent, kComponentId);
		ArrangeRow(0, 5, "room_warehouse", 100.f, kComponent, kComponentId);

		for (int i = 1; i < layers; i++) {
			AssignFloor(i, "preset_circle_double_fu+-", direction, assets);
			AssignRoom(i, 0, "room_warehouse", kComponent, kComponentId);
			AssignRoom(i, 1, "room_warehouse", kComponent, kComponentId);
			AssignRoom(i, 2, "room_warehouse", kComponent, kComponentId);
			AssignRoom(i, 3, "room_warehouse", kComponent, kComponentId);
			AssignRoom(i, 4, "room_warehouse", kComponent, kComponentId);
			ArrangeRow(i, 0, "room_warehouse", 100.f, kComponent, kComponentId);
			ArrangeRow(i, 1, "room_warehouse", 100.f, kComponent, kComponentId);
			ArrangeRow(i, 2, "room_warehouse", 100.f, kComponent, kComponentId);
			ArrangeRow(i, 3, "room_warehouse", 100.f, kComponent, kComponentId);
			ArrangeRow(i, 4, "room_warehouse", 100.f, kComponent, kComponentId);
			ArrangeRow(i, 5, "room_warehouse", 100.f, kComponent, kComponentId);
			ArrangeRow(i, 6, "room_warehouse", 100.f, kComponent, kComponentId);
		}
	}
}

void ShopBuilding::Assign(const vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {
	// 不用显式占位。
}

float ShopBuilding::RandomAcreage() {
	return 4000.f * powf(1.f + GetRandom(1000) / 1000.f * 1.f, 2);
}

float ShopBuilding::GetAcreageMin() {
	return 4000.f;
}

float ShopBuilding::GetAcreageMax() {
	return 16000.f;
}

float ShopBuilding::GetPower(AREA_TYPE area) {
	// 只在商业分区(高/中/低密度)上有权重。
	if (area == AREA_COMMERCIAL_HIGH || area == AREA_COMMERCIAL_MIDDLE || area == AREA_COMMERCIAL_LOW) {
		return 1.f;
	}
	return 0.f;
}

int FactoryBuilding::count = 0;

FactoryBuilding::FactoryBuilding() : id(count++) {
}

const char* FactoryBuilding::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

void FactoryBuilding::Layout(int& direction, const Quad& quad,
	const std::unordered_map<int, Road*>& boundaryRoads) {
	if (direction < 0) {
		std::vector<int> candidates;
		for (auto& [dir, road] : boundaryRoads) {
			if (road) candidates.push_back(dir);
		}
		if (!candidates.empty()) {
			direction = candidates[GetRandom(static_cast<int>(candidates.size()))];
		}
	}

	// direction引用参数是唯一的输出，这个偏好方向要真正生效：同ResidenceBuilding/
	// ShopBuilding一样，只有boundaryRoads里确实有这个方向的路时才采用。
	int preferredDirection;
	if (quad.GetSizeX() > quad.GetSizeY()) {
		preferredDirection = FACE_NORTH + GetRandom(2);
		if (quad.GetSizeX() > 4.f) basements = 1;
	} else {
		preferredDirection = FACE_WEST + GetRandom(2);
		if (quad.GetSizeY() > 4.f) basements = 1;
	}
	auto roadIt = boundaryRoads.find(preferredDirection);
	if (roadIt != boundaryRoads.end() && roadIt->second) {
		direction = preferredDirection;
	}

	footprint = BuildingFootprintSpec{ 0.5f, 0.5f, 0.6f, 0.6f };

	constexpr const char* kComponent = "component_factory";
	constexpr int kComponentId = 0;
	FloorAssetSpec assets;
	assets.wallMaterial = "/Game/Asset/Materials/White.White";

	// 工厂地上固定1层楼，显式赋值不依赖BuildingMod基类默认值。basements是否有地下室由上面
	// quad尺寸判定，和地上层数无关。
	layers = 1;
	floorHeights.assign(basements + layers, 0.6f);

	if (basements > 0) {
		AssignFloor(-1, "preset_single_room_pg+", direction, assets);
		AssignRoom(-1, 0, "room_parking", kComponent, kComponentId);
		AssignFloor(0, "preset_single_room_fg-", direction, assets);
	} else {
		AssignFloor(0, "preset_single_room_fg", direction, assets);
	}
	AssignRoom(0, 0, "room_factory", kComponent, kComponentId);
}

void FactoryBuilding::Assign(const vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {
	// 不用显式占位。
}

// 公式基数2000和min/max常量4000/16000不一致是既有数值，不做"修正"。
float FactoryBuilding::RandomAcreage() {
	return 2000.f * powf(1.f + GetRandom(1000) / 1000.f * 1.f, 2);
}

float FactoryBuilding::GetAcreageMin() {
	return 4000.f;
}

float FactoryBuilding::GetAcreageMax() {
	return 16000.f;
}

float FactoryBuilding::GetPower(AREA_TYPE area) {
	// 只在工业分区(高/中/低密度)上有权重。
	if (area == AREA_INDUSTRIAL_HIGH || area == AREA_INDUSTRIAL_MIDDLE || area == AREA_INDUSTRIAL_LOW) {
		return 1.f;
	}
	return 0.f;
}

int TrainStationBuilding::count = 0;

TrainStationBuilding::TrainStationBuilding() : id(count++) {
}

const char* TrainStationBuilding::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

void TrainStationBuilding::Layout(int& direction, const Quad& quad,
	const std::unordered_map<int, Road*>& boundaryRoads) {
	// 楼体占满整个Building区域(不像机场那样只占贴路一侧)。注意TrainStation::Layout的两条轨道/
	// 四个接口(depthRatio 0.55/0.65)因此落在楼体范围之内，见station_basic.md。
	stationMod = "station_train";
	footprint = BuildingFootprintSpec();   // 默认就是中心(0.5,0.5)、占满(1,1)
	basements = 1;
	layers = 1;
	floorHeights.assign(basements + layers, 0.6f);

	constexpr const char* kComponent = "component_train_station";
	constexpr int kComponentId = 0;
	FloorAssetSpec assets;
	assets.stairMeshPath = "/Game/Asset/Meshes/Stair_Line.Stair_Line";
	// 地下1层(站台层，楼梯通上来)+地面1层(大厅，左右两间站房)，两层各有2台电梯
	AssignFloor(-1, "preset_train_station_bg+", direction, assets);
	AssignFloor(0, "preset_train_station_fg-", direction, assets);
	AssignRoom(0, 0, "room_train_station", kComponent, kComponentId);
	AssignRoom(0, 1, "room_train_station", kComponent, kComponentId);
	AssignElevatorCabin(0, -basements, layers - 1, "", "script_empty", { "elevator_basic" });
	AssignElevatorCabin(1, -basements, layers - 1, "", "script_empty", { "elevator_basic" });
}

void TrainStationBuilding::Assign(const vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {
	// 测试布局：固定在井字中心正方形lot(AREA_OFFICIAL_HIGH)上找一块地方，和机场
	// (AirportBuilding::Assign)、围墙测试场景(ResidenceZone)共存——不再手动猜哪个方向/
	// margin刚好能避开别人已经占掉的区域，而是用Lot::FindAdaptivePlacement按"现在实际还剩
	// 多少自由空间"自适应选：依次尝试4个方向，每个方向用当前freeLots真实算出一组保证命中
	// 的margin/depth，哪个方向先找到可用空间就用哪个，不挑剔具体是哪一侧。
	constexpr float kMinFrontage = 7.f;
	constexpr float kMinDepth = 4.f;

	for (Lot* lot : lots) {
		if (!lot || lot->GetArea() != AREA_OFFICIAL_HIGH) continue;

		for (int dir = FACE_WEST; dir <= FACE_SOUTH; dir++) {
			Lot* targetLot;
			float marginStart, marginEnd, depth;
			if (!lot->FindAdaptivePlacement(dir, kMinFrontage, kMinDepth, targetLot, marginStart, marginEnd, depth)) continue;

			// 贴到命中子块这条边"起点"那个角：FindAdaptivePlacement给的是居中的margin，两边余量
			// 合并全部挪到marginEnd，让火车站挤在角落，把完整的大块空地留给后面的建筑。
			marginEnd += marginStart;
			marginStart = 0.f;

			// request.lot是targetLot(FindAdaptivePlacement命中的那个自由子块)，不是lot本身——
			// 子块贴的道路可能是切割产生的内部小路，margin/depth是相对子块自己的尺寸算的，必须
			// 让RequestPlacement在子块自己的坐标系里裁剪，见FindAdaptivePlacement的说明。
			// adaptiveParent只在targetLot真的是lot freeLots里的一个嵌套子块时才设(targetLot!=lot
			// 说明命中的不是lot自己，而是PeekFreeLots()返回的某个子块)——Core侧处理这条request
			// 成功之后要靠这个字段把targetLot自己的残余空间/新增小路搬回lot，不能在这里(Basic.dll)
			// 自己搬，见FindAdaptivePlacement声明处的说明。
			LotPlacementRequest request;
			request.lot = targetLot;
			request.adaptiveParent = (targetLot != lot) ? lot : nullptr;
			request.direction = dir;
			request.marginStart = marginStart;
			request.marginEnd = marginEnd;
			request.depth = depth;
			emit(context, request);
			return;
		}
		return; // 中心lot只有一个，4个方向都试过就不用再看其它lot
	}
}

float TrainStationBuilding::RandomAcreage() { return 2000.f; }
float TrainStationBuilding::GetAcreageMin() { return 2000.f; }
float TrainStationBuilding::GetAcreageMax() { return 2000.f; }
float TrainStationBuilding::GetPower(AREA_TYPE area) { return 0.f; }

int AirportBuilding::count = 0;

AirportBuilding::AirportBuilding() : id(count++) {
}

const char* AirportBuilding::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

void AirportBuilding::Layout(int& direction, const Quad& quad,
	const std::unordered_map<int, Road*>& boundaryRoads) {
	// 和TrainStationBuilding同样贴路一侧的footprint，理由见那边的注释——AirStation::Layout的
	// 两条跑道depthRatio是0.65/0.85，同一组数值。
	stationMod = "station_air";
	footprint = NearRoadFootprint(direction, 0.18f, 0.35f, 0.6f);
	basements = 0;
	layers = 1;
	floorHeights.assign(1, 0.6f);

	constexpr const char* kComponent = "component_airport";
	constexpr int kComponentId = 0;
	AssignFloor(0, "preset_single_room_fg", direction);
	AssignRoom(0, 0, "room_airport", kComponent, kComponentId);
}

void AirportBuilding::Assign(const vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {
	// 和TrainStationBuilding::Assign同一个自适应思路——这个Assign在Building阶段的处理顺序
	// 排在TrainStationBuilding之后(谁先谁后由BuildingFactory::GetRegisteredIds()的枚举顺序
	// 决定，这里不关心)，FindAdaptivePlacement会自动看到火车站(如果已经成功)留下的最新
	// freeLots状态，自己找一块不冲突的空间，不需要硬编码"对面那一侧"。
	// 占地(沿路x纵深，地图单位)比火车站(7x4)大：机场要放跑道和两侧停机位。放不下大的就退回7x4。
	constexpr float kFrontages[] = { 12.f, 7.f };
	constexpr float kDepths[] = { 8.f, 4.f };

	for (Lot* lot : lots) {
		if (!lot || lot->GetArea() != AREA_OFFICIAL_HIGH) continue;

		for (int size = 0; size < 2; size++)
		for (int dir = FACE_WEST; dir <= FACE_SOUTH; dir++) {
			Lot* targetLot;
			float marginStart, marginEnd, depth;
			if (!lot->FindAdaptivePlacement(dir, kFrontages[size], kDepths[size], targetLot, marginStart, marginEnd, depth)) continue;

			// 贴到另一端的角(和火车站相反)：余量全部挪到marginStart，marginEnd=0，这样两者即使
			// 同在一条边上也占据这条边的两个对角，中间留下整块连续空地。
			marginStart += marginEnd;
			marginEnd = 0.f;

			LotPlacementRequest request;
			request.lot = targetLot;
			request.adaptiveParent = (targetLot != lot) ? lot : nullptr;
			request.direction = dir;
			request.marginStart = marginStart;
			request.marginEnd = marginEnd;
			request.depth = depth;
			emit(context, request);
			return;
		}
		return;
	}
}

float AirportBuilding::RandomAcreage() { return 2000.f; }
float AirportBuilding::GetAcreageMin() { return 2000.f; }
float AirportBuilding::GetAcreageMax() { return 2000.f; }
float AirportBuilding::GetPower(AREA_TYPE area) { return 0.f; }

// 公园种树用的mesh(和行道树同一棵测试树)和密度：每PARK_ELEMENTS_PER_TREE个element²一棵
// (约每200m²一棵)，先粗估。
#define PARK_TREE_MESH_PATH "/Game/3rdParty/Mossy_Tree/StaticMeshes/mossy-tree.mossy-tree"
#define PARK_TREE_SCALE 1.f
#define PARK_ELEMENTS_PER_TREE 2.f

int ParkBuilding::count = 0;

ParkBuilding::ParkBuilding() : id(count++) {
}

const char* ParkBuilding::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

void ParkBuilding::Layout(int& direction, const Quad& quad,
	const std::unordered_map<int, Road*>& boundaryRoads) {
	// 0层：Core不会为它建任何Floor，UE层也不会生成楼体(见building.md"允许0层建筑")。
	basements = 0;
	layers = 0;
	floorHeights.clear();
}

void ParkBuilding::LayoutTrees(const Quad& quad, const std::unordered_map<int, Road*>& boundaryRoads) {
	int num = max(1, static_cast<int>(quad.GetSizeX() * quad.GetSizeY() / PARK_ELEMENTS_PER_TREE));
	// 建筑植被参考系是整块quad：两个对角(0,0)-(1,1)就是整块地。
	AddTreeArea({ 0.f, 0.f, 0.f, 0.f }, { 1.f, 0.f, 1.f, 0.f }, num, PARK_TREE_SCALE, PARK_TREE_MESH_PATH);
}

void ParkBuilding::Assign(const vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {
}

float ParkBuilding::RandomAcreage() {
	return 3000.f + GetRandom(1000) / 1000.f * 5000.f;
}

float ParkBuilding::GetAcreageMin() {
	return 3000.f;
}

float ParkBuilding::GetAcreageMax() {
	return 8000.f;
}

float ParkBuilding::GetPower(AREA_TYPE area) {
	// 绿地分区本来就该是公园；住宅分区给一个较小的权重，让测试时住宅区里也能刷出几块公园
	// (现在的井字路网不产出AREA_GREEN的lot)。
	if (area == AREA_GREEN) return 1.f;
	if (area == AREA_RESIDENTIAL_HIGH || area == AREA_RESIDENTIAL_MIDDLE || area == AREA_RESIDENTIAL_LOW) {
		return 0.25f;
	}
	return 0.f;
}

// Mohar模型和它的默认摆位(先给默认值，看过效果再调)：位置是楼体局部的PointParams
// (xRatio,xOffset,yRatio,yOffset)，按face=2(朝北)定义；Z是离地高度(地图单位)；缩放是资产原尺寸
// 的倍数；YAW是度，同样按朝北定义——实际朝向不同时Core会自动旋转，见feature_spec.h的DecorationSpec。
#define MOHAR_MESH_PATH "/Game/3rdParty/Mohar/StaticMeshes/mohar.mohar"
#define MOHAR_POS_X_RATIO 0.5f
#define MOHAR_POS_X_OFFSET 0.f
#define MOHAR_POS_Y_RATIO 0.5f
#define MOHAR_POS_Y_OFFSET 0.f
#define MOHAR_Z 0.f
#define MOHAR_SCALE_XY 0.06f
#define MOHAR_SCALE_Z 0.04f
// 模型初始旋转：资产自身的大门朝向和楼层模板的约定差90度，逆时针转90度(俯视)之后，朝北(face=2)
// 时模型大门也朝北，和模板一致。UE的正yaw俯视是顺时针，所以逆时针90度是-90。
#define MOHAR_YAW -90.f
// 楼体footprint的四条边(地图单位，1=10m)，按"朝北"(face=2，模板原样)定义、相对模型中心(=地块
// 中心)：+X是东、+Y是南(大门在北墙)，所以WEST/NORTH是负数。其余朝向按InversePoint同一套旋转换算，
// 和模型、楼层模板一起转；模型本身不动。layout模板3rdparty_mohar_fu1按这个footprint拟合一楼范围。
// 当前值：目测30m*30m起步，往远离大门的方向(南)挪5m，再按实测：南边向北收3m+1m、西边向西扩0.5m+0.5m、
// 东边向西收1.5m+1m；之后北边向南收0.5m、西边向东收0.5m、南边再向北收1m。
#define MOHAR_BODY_WEST -1.55f
#define MOHAR_BODY_EAST 1.25f
#define MOHAR_BODY_NORTH -0.95f
#define MOHAR_BODY_SOUTH 1.5f
// 层高(地图单位，0.55=5.5m)：一楼5.5m，二楼4m，三楼先按4m试。
#define MOHAR_FLOOR1_HEIGHT 0.55f
#define MOHAR_FLOOR2_HEIGHT 0.4f
#define MOHAR_FLOOR3_HEIGHT 0.4f
#define MOHAR_FLOOR1_TEMPLATE "3rdparty_mohar_fu1"
// 二楼先试着复用一楼的模板(对不上再换fu2)；三楼用单独的fu3。
#define MOHAR_FLOOR2_TEMPLATE "3rdparty_mohar_fu1"
#define MOHAR_FLOOR3_TEMPLATE "3rdparty_mohar_fu3"
// 每层要不要由框架构造墙壁/地板/天花板(FloorAssetSpec::buildShell)：成品模型已经包含这些，三层
// 都关掉；房间/Room碰撞盒照常生成。核对某一层的房间范围时把那一层临时改成true就能看到框架画的楼层。
#define MOHAR_FLOOR1_BUILD_SHELL false
#define MOHAR_FLOOR2_BUILD_SHELL false
#define MOHAR_FLOOR3_BUILD_SHELL false

int MoharBuilding::count = 0;

MoharBuilding::MoharBuilding() : id(count++) {
}

const char* MoharBuilding::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

void MoharBuilding::Layout(int& direction, const Quad& quad,
	const std::unordered_map<int, Road*>& boundaryRoads) {
	// 和ResidenceBuilding一样：FillRemainder落地传进来的是-1，模型朝向要靠direction决定，
	// 所以从有真实边界路的方向里随机挑一个回写。
	if (direction < 0) {
		std::vector<int> candidates;
		for (auto& [dir, road] : boundaryRoads) {
			if (road) candidates.push_back(dir);
		}
		if (!candidates.empty()) {
			direction = candidates[GetRandom(static_cast<int>(candidates.size()))];
		}
	}

	// 各层用3rdparty_mohar_fu*模板拟合模型里的房间；楼体外观由下面的成品模型提供，每层的
	// buildShell控制框架要不要再构造近处的墙壁/地板/天花板(远处方块照常画)，见MOHAR_FLOOR*_BUILD_SHELL。
	int face = direction >= 0 ? direction : FACE_NORTH;
	// 朝北坐标系里的楼体尺寸和中心(相对模型)，再换算到实际朝向：尺寸在朝东/西时宽和进深互换，
	// 中心位移和InversePoint对offset的映射一致：face0 (dx,dy)->(dy,-dx)，face1 ->(-dy,dx)，
	// face3 ->(-dx,-dy)。
	const float northSizeX = MOHAR_BODY_EAST - MOHAR_BODY_WEST;
	const float northSizeY = MOHAR_BODY_SOUTH - MOHAR_BODY_NORTH;
	const float northCenterX = (MOHAR_BODY_WEST + MOHAR_BODY_EAST) * 0.5f;
	const float northCenterY = (MOHAR_BODY_NORTH + MOHAR_BODY_SOUTH) * 0.5f;
	bool axesSwapped = (face == FACE_WEST || face == FACE_EAST);
	float bodySizeX = axesSwapped ? northSizeY : northSizeX;
	float bodySizeY = axesSwapped ? northSizeX : northSizeY;
	float shiftX = northCenterX, shiftY = northCenterY;
	switch (face) {
	case FACE_WEST: { float t = shiftX; shiftX = shiftY; shiftY = -t; break; }
	case FACE_EAST: { float t = shiftX; shiftX = -shiftY; shiftY = t; break; }
	case FACE_SOUTH: shiftX = -shiftX; shiftY = -shiftY; break;
	default: break;
	}
	footprint.centerRatioX = quad.GetSizeX() > 0.f ? 0.5f + shiftX / quad.GetSizeX() : 0.5f;
	footprint.centerRatioY = quad.GetSizeY() > 0.f ? 0.5f + shiftY / quad.GetSizeY() : 0.5f;
	footprint.sizeRatioX = quad.GetSizeX() > 0.f ? min(1.f, bodySizeX / quad.GetSizeX()) : 1.f;
	footprint.sizeRatioY = quad.GetSizeY() > 0.f ? min(1.f, bodySizeY / quad.GetSizeY()) : 1.f;

	basements = 0;
	layers = 3;
	floorHeights = { MOHAR_FLOOR1_HEIGHT, MOHAR_FLOOR2_HEIGHT, MOHAR_FLOOR3_HEIGHT };
	// 各层的房间挂在同一个component下(一栋独栋住宅)。
	const char* floorTemplates[3] = { MOHAR_FLOOR1_TEMPLATE, MOHAR_FLOOR2_TEMPLATE, MOHAR_FLOOR3_TEMPLATE };
	const bool floorBuildShell[3] = { MOHAR_FLOOR1_BUILD_SHELL, MOHAR_FLOOR2_BUILD_SHELL, MOHAR_FLOOR3_BUILD_SHELL };
	for (int level = 0; level < 3; level++) {
		FloorAssetSpec assets;
		assets.buildShell = floorBuildShell[level];
		AssignFloor(level, floorTemplates[level], face, assets);
		AssignRoom(level, 0, "room_residence", "component_residence", 0);
	}

	DecorationSpec spec;
	spec.meshPath = MOHAR_MESH_PATH;
	// 模型位置是相对楼体中心的：楼体中心相对模型偏了(northCenterX, northCenterY)，模型反向补回，
	// 保持在地块中心不动(InversePoint按朝向旋转这个offset，和楼体位移同一套映射)。
	spec.position = { MOHAR_POS_X_RATIO, MOHAR_POS_X_OFFSET - northCenterX, MOHAR_POS_Y_RATIO, MOHAR_POS_Y_OFFSET - northCenterY };
	spec.z = MOHAR_Z;
	spec.scaleX = spec.scaleY = MOHAR_SCALE_XY;
	spec.scaleZ = MOHAR_SCALE_Z;
	spec.yaw = MOHAR_YAW;
	AddDecoration(spec);
}

void MoharBuilding::Assign(const vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {
}

float MoharBuilding::RandomAcreage() {
	return 3000.f + GetRandom(1000) / 1000.f * 5000.f;
}

float MoharBuilding::GetAcreageMin() {
	return 3000.f;
}

float MoharBuilding::GetAcreageMax() {
	return 8000.f;
}

float MoharBuilding::GetPower(AREA_TYPE area) {
	if (area == AREA_RESIDENTIAL_HIGH || area == AREA_RESIDENTIAL_MIDDLE || area == AREA_RESIDENTIAL_LOW) {
		return 1.f;
	}
	return 0.f;
}
