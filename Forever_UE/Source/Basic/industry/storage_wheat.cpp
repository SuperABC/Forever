#include "storage_wheat.h"

using namespace std;

int WheatStorage::count = 0;

WheatStorage::WheatStorage() : id(count++) {
}

const char* WheatStorage::GetName() {
	name = "WheatStorage" + to_string(id);
	return name.data();
}

void WheatStorage::SetProperty() {
	categories = { "grain" };
	capacity = 200.f;
}
