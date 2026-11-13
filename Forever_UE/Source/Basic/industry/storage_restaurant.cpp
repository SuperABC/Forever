#include "storage_restaurant.h"

using namespace std;

int RestaurantStorage::count = 0;

RestaurantStorage::RestaurantStorage() : id(count++) {
}

const char* RestaurantStorage::GetName() {
	name = "RestaurantStorage" + to_string(id);
	return name.data();
}

void RestaurantStorage::SetProperty() {
	categories = { "food" };
	capacity = 10.f; // 单个餐厅房间的容量，只放一点
	canTake = false; // 餐厅仓库的货只能被市民买走，物流不能搬出去(餐厅之间不会互搬)
	canPut = true;
	serviceTypeDemand = { { "food", 1.f } };
}
