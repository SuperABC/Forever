#include "building_residence_high.h"

#include "common/utility.h"

#include <cmath>
#include <algorithm>


using namespace std;

namespace {
	// 楼体摆法(wing/side/twin)：朝向轴(进深)上最多maxDepth(地图单位)，地块没这么深就占满，更深就取
	// maxDepth并居中；垂直于进深的方向(开间)居中，两侧各留kSideGap的空隙。
	BuildingFootprintSpec DepthLimitedFootprint(int direction, const Quad& quad, float maxDepth) {
		constexpr float kSideGap = 0.2f;
		BuildingFootprintSpec spec{ 0.5f, 0.5f, 1.f, 1.f };
		if (direction == FACE_WEST || direction == FACE_EAST) {
			spec.sizeRatioX = min(maxDepth, quad.GetSizeX()) / quad.GetSizeX();
			spec.sizeRatioY = max(0.f, quad.GetSizeY() - 2.f * kSideGap) / quad.GetSizeY();
		} else {
			spec.sizeRatioY = min(maxDepth, quad.GetSizeY()) / quad.GetSizeY();
			spec.sizeRatioX = max(0.f, quad.GetSizeX() - 2.f * kSideGap) / quad.GetSizeX();
		}
		return spec;
	}
	constexpr const char* kComponent = "component_residence";
	constexpr int kComponentId = 0;
}

int ResidenceHighBuilding::count = 0;

ResidenceHighBuilding::ResidenceHighBuilding() : id(count++) {
}

const char* ResidenceHighBuilding::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

void ResidenceHighBuilding::Layout(int& direction, const Quad& quad,
	const std::unordered_map<int, Road*>& boundaryRoads) {
	// 挂示例脚本entity_event(进出事件只发给这个实体自己的Script)，见Resource/Story/entity_event.script。
	milestoneNames = { "entity_event" };
	// direction对显式占位是真实方向，权重CDF/FillRemainder落地时是-1：-1时从boundaryRoads随机
	// 挑一个有真实边界路的方向回写，行人/车辆导航的"outside"端点要靠它找到该连的路，见
	// building_mod.h的Layout()注释。
	if (direction < 0) {
		std::vector<int> candidates;
		for (auto& [dir, road] : boundaryRoads) {
			if (road) candidates.push_back(dir);
		}
		if (!candidates.empty()) {
			direction = candidates[GetRandom(static_cast<int>(candidates.size()))];
		}
	}
	// 偏好方向(四种布局相同)：长边朝向正对的路，
	// 只有boundaryRoads里确实有非空条目时才采用，否则保留上面兜底出来的direction(否则outside端点会查不到路)。
	int preferredDirection = quad.GetSizeX() > quad.GetSizeY() ? FACE_NORTH + GetRandom(2) : FACE_WEST + GetRandom(2);
	auto roadIt = boundaryRoads.find(preferredDirection);
	if (roadIt != boundaryRoads.end() && roadIt->second) {
		direction = preferredDirection;
	}

	// 四种布局用到的墙面/地面/屋顶标签 -> 材质实例，统一登记。
	SetSurfaceMaterial("roof", "/Game/Asset/Building/Material/Black.Black");
	SetSurfaceMaterial("brick", "/Game/Asset/Building/Material/Brick.Brick");
	SetSurfaceMaterial("concrete", "/Game/Asset/Building/Material/Concrete.Concrete");
	SetSurfaceMaterial("clay", "/Game/Asset/Building/Material/Clay.Clay");
	SetSurfaceMaterial("pavement", "/Game/Asset/Building/Material/Pavement.Pavement");
	SetSurfaceMaterial("specular", "/Game/Asset/Building/Material/Specular.Specular");
	SetSurfaceMaterial("stone", "/Game/Asset/Building/Material/Stone.Stone");
	// 楼体与建筑范围之间的空隙铺人行道材质。
	gapMaterial = "/Game/Asset/Building/Material/Pavement.Pavement";

	// 按面积选布局：4000~8000在side/nshape里等概率选，8000~12000四种等概率，12000~16000在twin/wing里等概率选。
	const float acreage = quad.GetAcreage();
	int variant; // 0 wing、1 side、2 twin、3 nshape
	if (acreage < 8000.f) {
		variant = GetRandom(2) == 0 ? 1 : 3;
	} else if (acreage < 12000.f) {
		variant = GetRandom(4);
	} else {
		variant = GetRandom(2) == 0 ? 2 : 0;
	}
	switch (variant) {
	case 0: LayoutWing(direction, quad); break;
	case 1: LayoutSide(direction, quad); break;
	case 2: LayoutTwin(direction, quad); break;
	default: LayoutNshape(direction, quad); break;
	}
}

void ResidenceHighBuilding::LayoutWing(int direction, const Quad& quad) {
	basements = 1;
	layers = 12 + GetRandom(6); // 地上12~17层随机(含最顶上只有屋面的fr层)，另有地下1层
	floorHeights.clear();
	floorHeights.push_back(0.35f); // 地下室
	for (int i = 0; i < layers; i++) {
		floorHeights.push_back(0.4f);
	}
	constexpr float size = 300.f; // 房间面积
	footprint = DepthLimitedFootprint(direction, quad, 4.f);

	// 地下1层pg+(停车库)，1楼fg+-，2楼fl+-，3楼及以上fu+-。
	AssignFloor(-1, "preset_residence_wing_pg+", direction);
	AssignRoom(-1, 0, "room_parking", kComponent, kComponentId);
	AssignFloor(0, "preset_residence_wing_fg+-", direction);
	AssignFloor(1, "preset_residence_wing_fl+-", direction);
	for (int level = 2; level < layers; level++) {
		// 最顶上再加一层屋面层fr(只有地面、没有房间)；其下一层(最高住人层)换成顶层模板fr-(没有楼梯、天花板是屋顶)，single/row数量与fu+-一致。
		if (level == layers - 1) {
			AssignFloor(level, "preset_residence_wing_fr", direction);
			continue;
		}
		AssignFloor(level, level == layers - 2 ? "preset_residence_wing_fr-" : "preset_residence_wing_fu+-", direction);
		AssignRoom(level, 0, "room_residence", kComponent, kComponentId); // fu+-多出的两个single
		AssignRoom(level, 1, "room_residence", kComponent, kComponentId);
	}
	for (int level = 0; level < layers - 1; level++) { // 最顶上的屋面层fr没有房间
		for (int row = 0; row < 4; row++) {
			ArrangeRow(level, row, "room_residence", size, kComponent, kComponentId);
		}
	}
	// 电梯到最高住人层(fr-)，不到屋面层。
	AssignElevatorCabin(0, -basements, layers - 2, "", "script_empty", { "elevator_basic" });
	AssignElevatorCabin(1, -basements, layers - 2, "", "script_empty", { "elevator_basic" });
}

void ResidenceHighBuilding::LayoutSide(int direction, const Quad& quad) {
	basements = 0; // 没有地下室
	layers = 5 + GetRandom(5);
	floorHeights.clear();
	for (int i = 0; i < layers; i++) {
		floorHeights.push_back(0.4f);
	}
	footprint = DepthLimitedFootprint(direction, quad, 3.f);

	// 1楼fg+，顶层fu-，中间fu+-；每层8个single，4个电梯井道贯穿全楼。
	FloorAssetSpec assets;
	assets.stairMeshPath = "/Game/Asset/Building/Mesh/Stair_Line.Stair_Line"; // 直跑楼梯，不用默认的Stair_Return
	for (int level = 0; level < layers; level++) {
		const char* templateName = level == 0 ? "preset_residence_side_fg+"
			: level == layers - 1 ? "preset_residence_side_fu-" : "preset_residence_side_fu+-";
		AssignFloor(level, templateName, direction, assets);
		for (int slot = 0; slot < 8; slot++) {
			AssignRoom(level, slot, "room_residence", kComponent, kComponentId);
		}
	}
	for (int elevator = 0; elevator < 4; elevator++) {
		AssignElevatorCabin(elevator, 0, layers - 1, "", "script_empty", { "elevator_basic" });
	}
}

void ResidenceHighBuilding::LayoutTwin(int direction, const Quad& quad) {
	basements = 1;
	layers = 12 + GetRandom(6);
	floorHeights.clear();
	floorHeights.push_back(0.35f); // 地下室
	for (int i = 0; i < layers; i++) {
		floorHeights.push_back(0.4f);
	}
	constexpr float size = 200.f; // 联排房间面积
	constexpr float lowSize = 400.f; // 1、2楼联排房间面积
	footprint = DepthLimitedFootprint(direction, quad, 4.f);

	// 地下1层停车库pg+；1楼fg+-、2楼fl+-各2个single+1个row(row切成400㎡的房间，1楼的row是餐厅，每个房间独立一个component_restaurant)；3楼起奇数楼fo+-、偶数楼fe+-，各4个row(联排)。电梯贯穿地下到顶层。
	AssignFloor(-1, "preset_residence_twin_pg+", direction); // 楼梯用默认的Stair_Return
	AssignRoom(-1, 0, "room_parking", kComponent, kComponentId);
	for (int level = 0; level < layers; level++) {
		// level 0是1楼：3楼(level 2)起奇数楼fo+-、偶数楼fe+-。
		const char* templateName = level == 0 ? "preset_residence_twin_fg+-"
			: level == 1 ? "preset_residence_twin_fl+-"
			: (level + 1) % 2 == 1 ? "preset_residence_twin_fo+-" : "preset_residence_twin_fe+-";
		AssignFloor(level, templateName, direction);
		if (level <= 1) {
			for (int slot = 0; slot < 2; slot++) {
				AssignRoom(level, slot, "room_residence", kComponent, kComponentId);
			}
			if (level == 0) ArrangeRowEach(level, 0, "room_restaurant", lowSize, "component_restaurant");
			else ArrangeRow(level, 0, "room_residence", lowSize, kComponent, kComponentId);
		} else {
			for (int row = 0; row < 4; row++) {
				ArrangeRow(level, row, "room_residence", size, kComponent, kComponentId);
			}
		}
	}
	for (int elevator = 0; elevator < 2; elevator++) {
		AssignElevatorCabin(elevator, -basements, layers - 1, "", "script_empty", { "elevator_basic" });
	}
}

void ResidenceHighBuilding::LayoutNshape(int direction, const Quad& quad) {
	basements = 1;
	layers = 5 + GetRandom(5);
	floorHeights.clear();
	floorHeights.push_back(0.35f); // 地下室
	for (int i = 0; i < layers; i++) {
		floorHeights.push_back(0.4f);
	}
	constexpr float size = 300.f; // 联排房间面积

	// 楼体贴着朝向的那一边占满，其余三边各留kSideGap的空隙(空隙铺gapMaterial)。
	// quad局部系里北边y=0、西边x=0：朝向的那一边起点/终点不收缩，对边和两侧各收缩kSideGap。
	constexpr float kSideGap = 0.2f;
	const float quadX = quad.GetSizeX(), quadY = quad.GetSizeY();
	float x0 = kSideGap, x1 = quadX - kSideGap, y0 = kSideGap, y1 = quadY - kSideGap;
	if (direction == FACE_WEST) x0 = 0.f;
	else if (direction == FACE_EAST) x1 = quadX;
	else if (direction == FACE_NORTH) y0 = 0.f;
	else if (direction == FACE_SOUTH) y1 = quadY;
	x1 = max(x0, x1);
	y1 = max(y0, y1);
	footprint = BuildingFootprintSpec{ (x0 + x1) * 0.5f / quadX, (y0 + y1) * 0.5f / quadY, (x1 - x0) / quadX, (y1 - y0) / quadY };

	// 地下1层bg+(4个single、3个row)；1楼fg+-(4个single、4个row)；2楼起fu+-(4个single、3个row)。
	AssignFloor(-1, "preset_residence_nshape_bg+", direction);
	for (int slot = 0; slot < 4; slot++) {
		AssignRoom(-1, slot, "room_residence", kComponent, kComponentId);
	}
	for (int row = 0; row < 3; row++) {
		ArrangeRow(-1, row, "room_residence", size, kComponent, kComponentId);
	}
	for (int level = 0; level < layers; level++) {
		AssignFloor(level, level == 0 ? "preset_residence_nshape_fg+-" : "preset_residence_nshape_fu+-", direction);
		for (int slot = 0; slot < 4; slot++) {
			AssignRoom(level, slot, "room_residence", kComponent, kComponentId);
		}
		const int rows = level == 0 ? 4 : 3;
		for (int row = 0; row < rows; row++) {
			ArrangeRow(level, row, "room_residence", size, kComponent, kComponentId);
		}
	}
	for (int elevator = 0; elevator < 2; elevator++) {
		AssignElevatorCabin(elevator, -basements, layers - 1, "", "script_empty", { "elevator_basic" });
	}
}

void ResidenceHighBuilding::Assign(const vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {
	// 在井字中心lot(AREA_TEST)的某个角上占一块12x8(沿路x纵深，地图单位)，保证开局就有住宅、有市民可控制。
	// 同TrainStationBuilding::Assign的自适应思路：依次尝试4个方向，先找到可用空间的方向就用；
	// 看到的是先处理的建筑(火车站/机场)留下的最新freeLots状态。
	constexpr float kFrontage = 12.f;
	constexpr float kDepth = 8.f;

	for (Lot* lot : lots) {
		if (!lot || lot->GetArea() != AREA_TEST) continue;

		for (int dir = FACE_WEST; dir <= FACE_SOUTH; dir++) {
			Lot* targetLot;
			float marginStart, marginEnd, depth;
			if (!lot->FindAdaptivePlacement(dir, kFrontage, kDepth, targetLot, marginStart, marginEnd, depth)) continue;

			// FindAdaptivePlacement给的是居中的margin：余量全挪到marginEnd，贴这条边的起点角。
			marginEnd += marginStart;
			marginStart = 0.f;

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
		return; // 中心lot只有一个
	}
}

float ResidenceHighBuilding::RandomAcreage() {
	const float r = GetRandom(1000) / 1000.f;
	return 4000.f + 12000.f * r * r; // 4000~16000按平方随机(偏向小面积)
}

float ResidenceHighBuilding::GetAcreageMin() {
	return 4000.f;
}

float ResidenceHighBuilding::GetAcreageMax() {
	return 16000.f;
}

float ResidenceHighBuilding::GetPower(AREA_TYPE area) {
	// 只在住宅高密度分区上有权重，和其余类型的分区互斥，见building_residence_high.md。
	if (area == AREA_RESIDENCE_HIGH) {
		return 1.f;
	}
	return 0.f;
}
