#include "room_restaurant.h"

using namespace std;

int RestaurantRoom::count = 0;

RestaurantRoom::RestaurantRoom() : id(count++) {
}

const char* RestaurantRoom::GetName() {
	name = "RestaurantRoom" + to_string(id);
	return name.data();
}

void RestaurantRoom::SetProperty() {
	isWorkspace = true;
	workspaceCapacity = 5;
	rent = 2000;
}
