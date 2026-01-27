#include "room.h"


using namespace std;

string DefaultResidentialRoom::GetId() {
	return "default_residential";
}

string DefaultResidentialRoom::GetType() const {
	return "default_residential";
}

string DefaultResidentialRoom::GetName() const {
	return "默认住宅房间";
}

bool DefaultResidentialRoom::IsResidential() const {
	return true;
}

bool DefaultResidentialRoom::IsWorkspace() const {
	return false;
}

int DefaultResidentialRoom::GetLivingCapacity() const {
	return 1;
}

int DefaultResidentialRoom::GetPersonnelCapacity() const {
	return 0;
}

string DefaultWorkingRoom::GetId() {
	return "default_working";
}

string DefaultWorkingRoom::GetType() const {
	return "default_working";
}

string DefaultWorkingRoom::GetName() const {
	return "默认工作房间";
}

bool DefaultWorkingRoom::IsResidential() const {
	return false;
}

bool DefaultWorkingRoom::IsWorkspace() const {
	return true;
}

int DefaultWorkingRoom::GetLivingCapacity() const {
	return 0;
}

int DefaultWorkingRoom::GetPersonnelCapacity() const {
	return 10;
}
