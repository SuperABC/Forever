#include "storage_burger.h"

using namespace std;

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
