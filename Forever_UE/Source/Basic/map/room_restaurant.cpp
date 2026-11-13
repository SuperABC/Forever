#include "room_restaurant.h"

using namespace std;

int RestaurantRoom::count = 0;

RestaurantRoom::RestaurantRoom() : id(count++) {
}

const char* RestaurantRoom::GetName() {
	name = "RestaurantRoom" + to_string(id);
	return name.data();
}

void RestaurantRoom::SetProperty() {
	isWorkspace = true;
	workspaceCapacity = 5;
	rent = 2000;
	isStorage = true;
	storageType = "storage_restaurant"; // 一点food库存，向汉堡厂拉货
	// 门：twin布局一楼联排的门洞标签是"home"，和住宅一样用读卡器门(doorSpecs里没有对应标签的门洞不生成门)。
	doorSpecs["home"].mesh = "/Game/3rdParty/Door_with_cardreader/StaticMeshes/SKM_door_with_cardreader.SKM_door_with_cardreader";
	doorSpecs["home"].animClass = "/Game/3rdParty/Door_with_cardreader/StaticMeshes/ABP_Door.ABP_Door_C";
}
