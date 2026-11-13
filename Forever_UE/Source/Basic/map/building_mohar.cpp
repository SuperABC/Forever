#include "building_mohar.h"

#include "common/utility.h"

#include <cmath>
#include <algorithm>


using namespace std;

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
// 和模型、楼层模板一起转；模型本身不动。layout模板3rdparty_mohar_fg1按这个footprint拟合一楼范围。
// 数值是按模型实测调出来的。
#define MOHAR_BODY_WEST -1.55f
#define MOHAR_BODY_EAST 1.25f
#define MOHAR_BODY_NORTH -0.95f
#define MOHAR_BODY_SOUTH 1.5f

// 层高(地图单位，0.55=5.5m)：一楼5.5m，二楼4m，三楼先按4m试。
#define MOHAR_FLOOR1_HEIGHT 0.55f
#define MOHAR_FLOOR2_HEIGHT 0.4f
#define MOHAR_FLOOR3_HEIGHT 0.4f
#define MOHAR_FLOOR1_TEMPLATE "3rdparty_mohar_fg1"

// 一楼用fg1，二楼用fu2，三楼用fu3，各层模板独立。
#define MOHAR_FLOOR2_TEMPLATE "3rdparty_mohar_fu2"
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

	// 各层用3rdparty_mohar_fg1/fu2/fu3模板拟合模型里的房间；楼体外观由下面的成品模型提供，每层的
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
	if (area == AREA_RESIDENCE_HIGH) {
		return 0.5f;
	}
	return 0.f;
}
