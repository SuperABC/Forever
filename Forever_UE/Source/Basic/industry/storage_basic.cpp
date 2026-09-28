#include "storage_basic.h"

using namespace std;

int WheatStorage::count = 0;

WheatStorage::WheatStorage() : id(count++) {
	categories = { "grain" };
	capacity = 200.f;
}

const char* WheatStorage::GetName() {
	name = "WheatStorage" + to_string(id);
	return name.data();
}

int BeefStorage::count = 0;

BeefStorage::BeefStorage() : id(count++) {
	categories = { "meat" };
	capacity = 200.f;
}

const char* BeefStorage::GetName() {
	name = "BeefStorage" + to_string(id);
	return name.data();
}

int BurgerStorage::count = 0;

BurgerStorage::BurgerStorage() : id(count++) {
	categories = { "food" };
	capacity = 100.f;
}

const char* BurgerStorage::GetName() {
	name = "BurgerStorage" + to_string(id);
	return name.data();
}
