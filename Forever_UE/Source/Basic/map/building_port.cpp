#include "building_port.h"

using namespace std;

namespace {
	// preset_port_fg.layout里船位(parkings)的个数。
	constexpr int kBoatSlotCount = 8;
	// preset_port_fg.layout里single(房间)的个数。
	constexpr int kSingleCount = 4;
}

int PortBuilding::count = 0;

PortBuilding::PortBuilding() : id(count++) {
}

const char* PortBuilding::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

void PortBuilding::Layout(int& direction, const Quad& quad,
	const std::unordered_map<int, Road*>& boundaryRoads) {
	// 楼体占满整个Building区域(默认footprint)；建筑本身不重要，只是把船放出来：靠路一侧一间小房，
	// 远离路的一侧(水面)8个船位。模板preset_port_fg(地上)/preset_port_bg(地下)见Resource/Layouts。
	footprint = BuildingFootprintSpec();
	// 地下1层20米(2.0地图单位)只是一圈墙(layout里的corridor)，不是房间：让墙插到水面以下，一楼不会浮在水面上。
	// 这一层不AssignRoom、不分船位。
	basements = 1;
	layers = 1;
	floorHeights = { 2.0f, 0.6f }; // 下标=basements+level：地下1层、地上1层

	constexpr const char* kComponent = "component_port";
	constexpr int kComponentId = 0;
	AssignFloor(-1, "preset_port_bg", direction);
	AssignFloor(0, "preset_port_fg", direction);
	// 一楼4个single(preset_port_fg的singles 0~3)都是空房间；船位归入0号。
	for (int singleIndex = 0; singleIndex < kSingleCount; singleIndex++) {
		AssignRoom(0, singleIndex, "room_empty", kComponent, kComponentId);
	}
	// 8个船位(fg layout的parkings 0~7)都归入这间小房；stocked：开局每个船位停一艘船(船没有主人，不走AssignVehicles)。
	// spacing=-1=整个矩形一个船位，矩形多大都只停一艘。offRoad：船位在水里，不接公路(layout里没有车行连接)。
	for (int parkingIndex = 0; parkingIndex < kBoatSlotCount; parkingIndex++) {
		ParkingSlotSpec spec;
		spec.parkingIndex = parkingIndex;
		spec.spacing = -1.f;
		spec.categories = { "boat" };
		spec.stocked = true;
		spec.offRoad = true;
		AssignParking(0, 0, spec);
	}
}

void PortBuilding::Assign(const vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {
	// 港口只靠路网预设落地(Map::PlacePresets)，不参与自动选址。
}

float PortBuilding::RandomAcreage() { return 2000.f; }

float PortBuilding::GetAcreageMin() { return 2000.f; }

float PortBuilding::GetAcreageMax() { return 2000.f; }

float PortBuilding::GetPower(AREA_TYPE area) { return 0.f; }
