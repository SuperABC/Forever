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
