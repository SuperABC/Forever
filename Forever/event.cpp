#include "event.h"


using namespace std;

EVENT_TYPE Event::GetType() {
	return type;
}

void Event::SetCondition(Condition condition) {
	this->condition = condition;
}

Condition& Event::GetCondition() {
	return condition;
}


