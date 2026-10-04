#include "storage_basic.h"

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

int BurgerStorage::count = 0;

BurgerStorage::BurgerStorage() : id(count++) {
}

const char* BurgerStorage::GetName() {
	name = "BurgerStorage" + to_string(id);
	return name.data();
}

void BurgerStorage::SetProperty() {
	categories = { "food" };
	capacity = 100.f;
}
