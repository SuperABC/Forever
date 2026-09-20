#include "building_basic.h"

#include "common/utility.h"

#include <cmath>


using namespace std;

int ResidenceBuilding::count = 0;

ResidenceBuilding::ResidenceBuilding() {
	lastName = string(GetType()) + std::to_string(count++);
}

void ResidenceBuilding::Layout(int& direction, const Quad& quad,
	const std::unordered_map<int, Road*>& boundaryRoads) {
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
	// 楼层数分档照抄老工程ResidentialBuilding::LayoutBuilding原始数值。
	if (quad.GetAcreage() < 5000.f) {
		layers = 3 + GetRandom(3);
	} else if (quad.GetAcreage() < 10000.f) {
		layers = 6 + GetRandom(4);
	} else {
		layers = 10 + GetRandom(5);
	}
	floorHeights.clear();
	floorHeights.push_back(0.35f); // 地下室
	// "每层高度循环取值、不完全一样"这个特性已经验证过(以及验证过程中发现并修复的"循环表
	// 固定相位导致全地图第3层系统性偏高"那个问题)，现在恢复成地上每层统一0.4，不再循环取值。
	for (int i = 0; i < layers; i++) {
		floorHeights.push_back(0.4f);
	}
	// lodMaterial留空 -> 用渲染层默认灰色Pure MID

	constexpr const char* kComponent = "component_residence";
	constexpr int kComponentId = 0;

	// 按quad尺寸分档随机选一套模板组合(layout 0-3)+对应的房间槽位密度(size)——完整照抄老工程
	// ResidentialBuilding::LayoutBuilding的选型逻辑(building_basic.cpp:47-199)，layout==1/3
	// 自带电梯井道模板数据(preset_lobby_wing_*/preset_nshape_double_*)，这次会话新增的
	// AssignElevatorCabin就是靠这两个分支才有真正的Elevator几何可用。
	//
	// 方向：老工程这里直接用quad尺寸/朝向算出的"偏好方向"覆盖掉一开始GetRandom(4)的初始值，
	// 不检查这个方向是否真的有边界路——这次不能照抄这一点，偏好方向如果没有对应的真实边界路，
	// 会导致上面费心兜底出来的、行人/车辆导航outside端点能用的direction被覆盖成一个查不到路
	// 的方向。这里做法：只有偏好方向在boundaryRoads里确实有非空条目时才采用，否则保留上面
	// 已经兜底好的direction。
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
		AssignElevatorCabin(0, -basements, layers - 1, "");
		AssignElevatorCabin(1, -basements, layers - 1, "");
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
		AssignRoom(-1, 0, "empty", kComponent, kComponentId); // 老工程这里也是"empty"占位槽位
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
		AssignRoom(0, 0, "empty", kComponent, kComponentId);
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
			AssignRoom(i, 0, "empty", kComponent, kComponentId);
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
		AssignElevatorCabin(0, -basements, layers - 1, "");
		AssignElevatorCabin(1, -basements, layers - 1, "");
	}
}

void ResidenceBuilding::Assign(const vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {
	// 不用显式占位。
}

// 老工程ResidentialBuilding::RandomAcreage/minAcreage/maxAcreage原始数值，照抄不改。
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
	// RoadnetMod(JingRoadnet::DistributeRoadnet)已经会给每个lot调用Lot::SetArea()标好实际的
	// 分区类型，不是AREA_NONE——之前这里错误地假设所有lot都是默认值AREA_NONE，导致GetPower对
	// 每个真实lot都返回0，FillRemainder链路上的候选权重从未被登记过，一个独立building都生成
	// 不出来(PIE验证发现)。当时"对所有分区一视同仁给权重1"是因为这个占位类型是唯一注册的
	// building类型，怎么给权重都无所谓——引入ShopBuilding/FactoryBuilding(各自只在商业/工业
	// 分区有权重，见building_basic.md)之后，这个"一视同仁"就变成bug了：商业/工业分区上
	// ResidenceBuilding和Shop/Factory一起参与FillRemainder的权重CDF竞争，导致商业区/工业区
	// 里混入了住宅建筑(PIE验证发现)。改成只在住宅分区(高/中/低密度)上有权重，照抄老工程真正
	// 参与竞争的ResidentialLow/Middle/HighBuilding三个具体类型只在住宅分区有非零权重的做法
	// (老工程真正注册的是这三个，从未注册这个通用ResidentialBuilding基类，见
	// E:\Projects\Forever_UE\Source\Basic\basic.cpp:92-102)——这个通用占位类型这次不细分
	// 三档密度权重，统一按1.f处理，等以后设计具体建筑类型时再细分。办公/商业/工业分区上返回0，
	// 意味着没有对应类型建筑竞争的分区(目前只有办公区，还没有Office建筑类型)那块lot会暂时
	// 保持空地，不会被随便一种建筑顶上，属于预期行为。
	if (area == AREA_RESIDENTIAL_HIGH || area == AREA_RESIDENTIAL_MIDDLE || area == AREA_RESIDENTIAL_LOW) {
		return 1.f;
	}
	return 0.f;
}

int ShopBuilding::count = 0;

ShopBuilding::ShopBuilding() {
	lastName = string(GetType()) + std::to_string(count++);
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

	// 照抄老工程ShopBuilding::PlaceConstruction的占地比例公式(building_basic.cpp:315-328)。
	float scaleX = 0.6f;
	float scaleY = 0.6f;
	if (quad.GetSizeX() < 6.f) scaleX = (quad.GetSizeX() - 2.4f) / quad.GetSizeX();
	if (quad.GetSizeY() < 6.f) scaleY = (quad.GetSizeY() - 2.4f) / quad.GetSizeY();
	footprint = BuildingFootprintSpec{ 0.5f, 0.5f, scaleX, scaleY };

	// 老工程PlaceConstruction直接用这个偏好方向覆盖direction、不检查边界路是否存在——这次和
	// ResidenceBuilding一样，只有偏好方向在boundaryRoads里确实有非空条目时才采用，否则保留
	// 上面已经兜底好的direction，避免行人/车辆导航outside端点查不到路。
	int preferredDirection = (quad.GetSizeX() > quad.GetSizeY())
		? (FACE_WEST + GetRandom(2))
		: (FACE_NORTH + GetRandom(2));
	auto roadIt = boundaryRoads.find(preferredDirection);
	if (roadIt != boundaryRoads.end() && roadIt->second) {
		direction = preferredDirection;
	}

	constexpr const char* kComponent = "component_shop";
	constexpr int kComponentId = 0;
	FloorAssetSpec assets;
	assets.wallMaterial = "/Game/Asset/Materials/White.White"; // 照抄老工程wallTexture

	// layers=2是老工程ShopBuilding::LayoutBuilding的固定值(不像Residence那样按acreage随机
	// 分档)，完整照抄，见building_basic.md。
	layers = 2;

	if (quad.GetAcreage() < 6000.f) {
		basements = 0;
		floorHeights.assign(layers, 0.4f);

		AssignFloor(0, "preset_lobby_linear_fg+", direction, assets);
		AssignRoom(0, 0, "room_shop", kComponent, kComponentId);
		ArrangeRow(0, 0, "room_warehouse", 200.f, kComponent, kComponentId);
		ArrangeRow(0, 1, "room_warehouse", 200.f, kComponent, kComponentId);
		// layers==2时这个循环体是no-op(1 < layers-1 == 1 < 1为false)，照抄老工程原始写法
		// 保留，不做"简化成没有循环"的改写，行为和老工程逐行一致。
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

// 照抄老工程ShopBuilding::RandomAcreage/minAcreage/maxAcreage原始数值。
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
	// 照抄老工程ShopBuilding::GetPowers()：只在商业分区(高/中/低密度)上有权重。
	if (area == AREA_COMMERCIAL_HIGH || area == AREA_COMMERCIAL_MIDDLE || area == AREA_COMMERCIAL_LOW) {
		return 1.f;
	}
	return 0.f;
}

int FactoryBuilding::count = 0;

FactoryBuilding::FactoryBuilding() {
	lastName = string(GetType()) + std::to_string(count++);
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

	// 照抄老工程FactoryBuilding::LayoutBuilding的方向/地下室判定(building_basic.cpp:464-476)。
	// 老工程这里用一个局部变量`direction`覆盖同名成员、从未真正回写方向(疑似遗留bug)——这次
	// 新API的direction就是唯一的输出，所以这个偏好方向要真正生效，同ResidenceBuilding/
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

	// 照抄老工程PlaceConstruction固定0.6x0.6占地比例(building_basic.cpp:456-458)。
	footprint = BuildingFootprintSpec{ 0.5f, 0.5f, 0.6f, 0.6f };

	constexpr const char* kComponent = "component_factory";
	constexpr int kComponentId = 0;
	FloorAssetSpec assets;
	assets.wallMaterial = "/Game/Asset/Materials/White.White"; // 照抄老工程wallTexture

	// 工厂地上固定1层楼(应用户要求显式写出，不再依赖BuildingMod基类默认值1——老工程
	// FactoryBuilding::LayoutBuilding没有显式设layers，靠基类默认值凑出同样的1层，这次
	// 显式赋值，行为不变但不再依赖那个隐式默认值)。basements是否有地下室由上面quad尺寸
	// 判定，和地上层数无关。
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

// 照抄老工程FactoryBuilding::RandomAcreage/minAcreage/maxAcreage原始数值——公式基数2000
// 和min/max常量4000/16000不一致是老工程本来的数值，照抄不做"修正"。
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
	// 照抄老工程FactoryBuilding::GetPowers()：只在工业分区(高/中/低密度)上有权重。
	if (area == AREA_INDUSTRIAL_HIGH || area == AREA_INDUSTRIAL_MIDDLE || area == AREA_INDUSTRIAL_LOW) {
		return 1.f;
	}
	return 0.f;
}
