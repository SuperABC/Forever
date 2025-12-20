#include "room.h"


using namespace std;

std::string FlatRoom::GetId() {
	return "flat";
}

std::string FlatRoom::GetType() const {
	return "flat";
}

std::string FlatRoom::GetName() const {
	return "公寓房间";
}

bool FlatRoom::IsResidential() const {
	return true;
}

bool FlatRoom::IsWorkspace() const {
	return false;
}

int FlatRoom::GetLivingCapacity() const {
	return 2;
}

int FlatRoom::GetPersonnelCapacity() const {
	return 0;
}
