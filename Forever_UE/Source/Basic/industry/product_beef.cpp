#include "product_beef.h"

using namespace std;

int BeefProduct::count = 0;

BeefProduct::BeefProduct() : id(count++) {
}

const char* BeefProduct::GetName() {
	name = "BeefProduct" + to_string(id);
	return name.data();
}

void BeefProduct::SetProperty() {
	categories = { "meat" };
	batchSize = 10.f; // 一批10份，无原料无副产品
	retailPrice = 1600; // 零售16元>进口12元>出口8元
	importPrice = 1200;
	exportPrice = 800;
}
