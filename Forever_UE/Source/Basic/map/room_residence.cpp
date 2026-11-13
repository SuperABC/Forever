#include "room_residence.h"

using namespace std;

int ResidenceRoom::count = 0;

ResidenceRoom::ResidenceRoom() : id(count++) {
}

const char* ResidenceRoom::GetName() {
	name = "ResidenceRoom" + to_string(id);
	return name.data();
}

void ResidenceRoom::SetProperty() {
	isResidence = true;
	residenceCapacity = 1;
	rent = 2000; // 每日房租20元
	// 挂示例脚本entity_event(进出事件只发给这个实体自己的Script)，见Resource/Story/entity_event.script。
	milestoneNames = { "entity_event" };
	// 住宅房间的门：四种高密度住宅layout里single/row的门洞标签统一是"home"，用和其他房间一样的读卡器门。
	doorSpecs["home"].mesh = "/Game/3rdParty/Door_with_cardreader/StaticMeshes/SKM_door_with_cardreader.SKM_door_with_cardreader";
	doorSpecs["home"].animClass = "/Game/3rdParty/Door_with_cardreader/StaticMeshes/ABP_Door.ABP_Door_C";
}
