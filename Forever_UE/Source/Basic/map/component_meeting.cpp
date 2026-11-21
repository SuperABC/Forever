#include "component_meeting.h"

using namespace std;

int MeetingComponent::count = 0;

MeetingComponent::MeetingComponent() : id(count++) {
}

const char* MeetingComponent::GetName() {
	name = "MeetingComponent" + to_string(id);
	return name.data();
}
