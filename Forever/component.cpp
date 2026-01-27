#include "component.h"


using namespace std;

string DefaultResidentialComponent::GetId() {
	return "default_residential";
}

string DefaultResidentialComponent::GetType() const {
	return "default_residential";
}

string DefaultResidentialComponent::GetName() const {
	return "默认住宅组合";
}

string DefaultWorkingComponent::GetId() {
	return "default_working";
}

string DefaultWorkingComponent::GetType() const {
	return "default_working";
}

string DefaultWorkingComponent::GetName() const {
	return "默认工作组合";
}
