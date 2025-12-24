#include "component.h"


using namespace std;

string FlatComponent::GetId() {
	return "flat";
}

string FlatComponent::GetType() const {
	return "flat";
}

string FlatComponent::GetName() const {
	return "公寓组合";
}

string HotelComponent::GetId() {
	return "hotel";
}

string HotelComponent::GetType() const {
	return "hotel";
}

string HotelComponent::GetName() const {
	return "酒店组合";
}
