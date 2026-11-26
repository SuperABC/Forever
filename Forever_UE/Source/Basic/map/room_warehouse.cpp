#include "room_warehouse.h"

using namespace std;

int WarehouseRoom::count = 0;

WarehouseRoom::WarehouseRoom() : id(count++) {
}

const char* WarehouseRoom::GetName() {
	name = "WarehouseRoom" + to_string(id);
	return name.data();
}

void WarehouseRoom::SetProperty() {
	isStorage = true;
	storageType = "storage_shop";
	// 仓库也有管理员：容纳量1，Organization按容纳量满编定员。
	isWorkspace = true;
	workspaceCapacity = 1;
	rent = 2000;
}

void WarehouseRoom::Layout(const RoomLayoutInfo& info) {
	// 测试用：房间中央摆10个各自独立的汉堡(2排x5个，间距0.5米，贴地放)。不用随机数，所以不消耗这个mod的随机流。
	// asset_burger没启用时Core跳过并打一条警告，房间照常生成。
	const int columns = 5, rows = 2;
	const float spacing = 0.5f, margin = 0.3f;
	auto clampInside = [margin](float value, float extent) {
		if (extent <= margin * 2.f) return extent * 0.5f;
		return value < margin ? margin : (value > extent - margin ? extent - margin : value);
	};
	for (int i = 0; i < columns * rows; i++) {
		RoomAssetSpec burger;
		burger.assetType = "asset_burger";
		burger.count = 1;
		burger.u = clampInside(info.width * 0.5f + (static_cast<float>(i % columns) - (columns - 1) * 0.5f) * spacing, info.width);
		burger.v = clampInside(info.depth * 0.5f + (static_cast<float>(i / columns) - (rows - 1) * 0.5f) * spacing, info.depth);
		PlaceAsset(burger);
	}
}
