#include "component_shop.h"

using namespace std;

int ShopComponent::count = 0;

ShopComponent::ShopComponent() : id(count++) {
}

const char* ShopComponent::GetName() {
	name = "ShopComponent" + to_string(id);
	return name.data();
}
