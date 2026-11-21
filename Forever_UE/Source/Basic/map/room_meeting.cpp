#include "room_meeting.h"

using namespace std;

int MeetingRoom::count = 0;

MeetingRoom::MeetingRoom() : id(count++) {
}

const char* MeetingRoom::GetName() {
	name = "MeetingRoom" + to_string(id);
	return name.data();
}

void MeetingRoom::SetProperty() {
	// 占的是residence twin 1/2楼的single房间(门洞标签是"home")，门和住宅一样用读卡器门。
	doorSpecs["home"].mesh = "/Game/3rdParty/Door_with_cardreader/StaticMeshes/SKM_door_with_cardreader.SKM_door_with_cardreader";
	doorSpecs["home"].animClass = "/Game/3rdParty/Door_with_cardreader/StaticMeshes/ABP_Door.ABP_Door_C";
}
