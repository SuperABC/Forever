#include "component.h"


using namespace std;

std::string FlatComponent::GetId() {
	return "flat";
}

std::string FlatComponent::GetType() const {
	return "flat";
}

std::string FlatComponent::GetName() const {
	return "公寓组合";
}

std::string HotelComponent::GetId() {
	return "hotel";
}

std::string HotelComponent::GetType() const {
	return "hotel";
}

std::string HotelComponent::GetName() const {
	return "酒店组合";
}
