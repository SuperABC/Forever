#include "component_basic.h"

using namespace std;

int ResidenceComponent::count = 0;

ResidenceComponent::ResidenceComponent() : id(count++) {
}

const char* ResidenceComponent::GetName() {
	name = "ResidenceComponent" + to_string(id);
	return name.data();
}

int ShopComponent::count = 0;

ShopComponent::ShopComponent() : id(count++) {
}

const char* ShopComponent::GetName() {
	name = "ShopComponent" + to_string(id);
	return name.data();
}

int FactoryComponent::count = 0;

FactoryComponent::FactoryComponent() : id(count++) {
}

const char* FactoryComponent::GetName() {
	name = "FactoryComponent" + to_string(id);
	return name.data();
}
