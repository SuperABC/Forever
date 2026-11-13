#include "room_shop.h"

using namespace std;

int ShopRoom::count = 0;

ShopRoom::ShopRoom() : id(count++) {
}

const char* ShopRoom::GetName() {
	name = "ShopRoom" + to_string(id);
	return name.data();
}

void ShopRoom::SetProperty() {
	isWorkspace = true;
	workspaceCapacity = 5;
	rent = 2000;

	doorSpecs["room"].mesh = "/Game/3rdParty/Door_with_cardreader/StaticMeshes/SKM_door_with_cardreader.SKM_door_with_cardreader";
	doorSpecs["room"].animClass = "/Game/3rdParty/Door_with_cardreader/StaticMeshes/ABP_Door.ABP_Door_C";
	// 商店房间的门有名字、挂脚本，可以交互("敲门")。商店用的一楼模板(preset_lobby_linear_fg+/
	// preset_circle_double_fg+-)里门标签全是"room"、没有"entrance"，ShopBuilding的
	// doorSpecs["entrance"]对不上任何门，所以挂在这里，见Resource/Story/door_shop_entrance.script。
	doorSpecs["room"].name = "商店大门";
	doorSpecs["room"].scriptModName = "script_empty";
	doorSpecs["room"].milestoneNames = { "door_shop_entrance" };
}
