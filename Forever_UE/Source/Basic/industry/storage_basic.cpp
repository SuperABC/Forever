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

int ShopStorage::count = 0;

ShopStorage::ShopStorage() : id(count++) {
}

const char* ShopStorage::GetName() {
	name = "ShopStorage" + to_string(id);
	return name.data();
}

void ShopStorage::SetProperty() {
	categories = { "food" };
	capacity = 30.f; // 单个仓库房间的容量;一栋商店有很多仓库房间,全城总容量要覆盖日需求(每人1份)
	canTake = false; // 商店仓库的货只能被市民买走，物流不能搬出去
	canPut = true;
	serviceTypeDemand = { { "food", 1.f } };
}
