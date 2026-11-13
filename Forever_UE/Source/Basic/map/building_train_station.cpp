#include "building_train_station.h"

#include "common/utility.h"

#include <cmath>
#include <algorithm>


using namespace std;

namespace {
	constexpr const char* kComponent = "component_residence";
	constexpr int kComponentId = 0;
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
	// 楼体占满整个Building区域(不像机场只占贴路一侧)，TrainStation::Layout的两条轨道/四个接口
	// (depthRatio 0.55/0.65)因此落在楼体范围之内，见station_*.md。
	stationMod = "station_train";
	footprint = BuildingFootprintSpec();   // 默认就是中心(0.5,0.5)、占满(1,1)
	basements = 1;
	layers = 1;
	floorHeights.assign(basements + layers, 0.6f);

	constexpr const char* kComponent = "component_train_station";
	constexpr int kComponentId = 0;
	FloorAssetSpec assets;
	assets.stairMeshPath = "/Game/Asset/Building/Mesh/Stair_Line.Stair_Line";
	// 地下1层(站台层，楼梯通上来)+地面1层(大厅，左右两间站房)，两层各有2台电梯
	AssignFloor(-1, "preset_train_station_bg+", direction, assets);
	AssignFloor(0, "preset_train_station_fg-", direction, assets);
	AssignRoom(0, 0, "room_train_station", kComponent, kComponentId);
	AssignRoom(0, 1, "room_train_station", kComponent, kComponentId);
	AssignElevatorCabin(0, -basements, layers - 1, "", "script_empty", { "elevator_basic" });
	AssignElevatorCabin(1, -basements, layers - 1, "", "script_empty", { "elevator_basic" });
}

void TrainStationBuilding::Assign(const vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {
	// 在井字中心lot(AREA_TEST)上找一块地方，和机场/园区共存：用
	// Lot::FindAdaptivePlacement按当前剩余自由空间自适应选，依次尝试4个方向，先找到可用空间的方向就用。
	constexpr float kMinFrontage = 7.f;
	constexpr float kMinDepth = 4.f;

	for (Lot* lot : lots) {
		if (!lot || lot->GetArea() != AREA_TEST) continue;

		for (int dir = FACE_WEST; dir <= FACE_SOUTH; dir++) {
			Lot* targetLot;
			float marginStart, marginEnd, depth;
			if (!lot->FindAdaptivePlacement(dir, kMinFrontage, kMinDepth, targetLot, marginStart, marginEnd, depth)) continue;

			// FindAdaptivePlacement给的是居中的margin：余量全挪到marginEnd，让火车站贴在这条边的起点角。
			marginEnd += marginStart;
			marginStart = 0.f;

			// request.lot是命中的自由子块targetLot而不是lot本身：margin/depth是相对子块算的，
			// 要在子块自己的坐标系里裁剪。adaptiveParent仅在targetLot!=lot时设，Core处理成功后
			// 靠它把子块的残余空间/新增小路搬回lot(不能在Basic.dll里搬)，见FindAdaptivePlacement声明。
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
