#include "storage_beef.h"

using namespace std;

int BeefStorage::count = 0;

BeefStorage::BeefStorage() : id(count++) {
}

const char* BeefStorage::GetName() {
	name = "BeefStorage" + to_string(id);
	return name.data();
}

void BeefStorage::SetProperty() {
	categories = { "meat" };
	capacity = 200.f;
}
