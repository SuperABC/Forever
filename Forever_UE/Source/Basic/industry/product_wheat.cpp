#include "product_wheat.h"

using namespace std;

int WheatProduct::count = 0;

WheatProduct::WheatProduct() : id(count++) {
}

const char* WheatProduct::GetName() {
	name = "WheatProduct" + to_string(id);
	return name.data();
}

void WheatProduct::SetProperty() {
	categories = { "grain" };
	batchSize = 10.f; // 一批10份，无原料无副产品
	// 价格(分)：零售8元>进口6元>出口4元。
	retailPrice = 800;
	importPrice = 600;
	exportPrice = 400;
}
