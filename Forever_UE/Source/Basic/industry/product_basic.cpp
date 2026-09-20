#include "product_basic.h"

using namespace std;

int ProductBasic::count = 0;

ProductBasic::ProductBasic() : id(count++) {
}

const char* ProductBasic::GetName() {
	name = "ProductBasic" + to_string(id);
	return name.data();
}
