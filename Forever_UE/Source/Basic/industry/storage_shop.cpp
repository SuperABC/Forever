#include "storage_shop.h"

using namespace std;

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
