#include "component_restaurant.h"

using namespace std;

int RestaurantComponent::count = 0;

RestaurantComponent::RestaurantComponent() : id(count++) {
}

const char* RestaurantComponent::GetName() {
	name = "RestaurantComponent" + to_string(id);
	return name.data();
}
