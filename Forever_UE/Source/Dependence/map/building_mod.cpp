#include "building_mod.h"


using namespace std;

void BuildingMod::AssignFloor(int level, const string& templateName, int face, FloorAssetSpec assets) {
	floors[level] = FloorLayoutSpec{ templateName, face, std::move(assets) };
}

void BuildingMod::AssignFloors(const string& templateName, int face, FloorAssetSpec assets) {
	for (int level = -basements; level < layers; level++) {
		floors[level] = FloorLayoutSpec{ templateName, face, assets };
	}
}

void BuildingMod::AssignFloors(const vector<string>& templateNames, int face, const vector<FloorAssetSpec>& assets) {
	for (size_t i = 0; i < templateNames.size(); i++) {
		int level = static_cast<int>(i) - basements;
		FloorAssetSpec spec = (i < assets.size()) ? assets[i] : FloorAssetSpec{};
		floors[level] = FloorLayoutSpec{ templateNames[i], face, std::move(spec) };
	}
}

void BuildingMod::AssignRoom(int level, int slot, const string& room, const string& component, int id) {
	singles[{component, id}].push_back({ level, slot, room });
}

void BuildingMod::ArrangeRow(int level, int slot, const string& room, float acreage, const string& component, int id) {
	rows[{component, id}].push_back({ level, slot, room, acreage });
}

void BuildingMod::AssignElevatorCabin(int shaftIndex, int minFloor, int maxFloor, string cabinMeshPath) {
	cabins.push_back(ElevatorCabinSpec{ shaftIndex, minFloor, maxFloor, std::move(cabinMeshPath) });
}
